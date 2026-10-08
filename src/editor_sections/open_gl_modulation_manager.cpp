/* Copyright 2025 Marc Scheffer
 *
 * helmBoy is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 *
 * This work is based on bepzi's Helm project, <https://github.com/bepzi/helm>,
 * itself based on Matt Tytel's Helm <https://tytel.org/helm/>
 *
 * helmBoy is distributedin the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with helmBoy.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "open_gl_modulation_manager.h"

#include "button_modulation_slider.h"
#include "full_interface.h"
#include "helmBoy_common.h"
#include "modulation_highlight.h"
#include "modulation_look_and_feel.h"
#include "open_gl_modulation_meter.h"
#include "modulation_slider.h"
#include "shaders.h"
#include "synth_base.h"

using namespace juce::gl;
#include "synth_gui_interface.h"

/**
 * @file open_gl_modulation_manager.cpp
 * @brief Implementation of OpenGLModulationManager (OpenGL visualization of modulations).
 */

#define FLOATS_PER_METER 24
#define INDICES_PER_METER 6
#define POINTS_PER_METER 4

OpenGLModulationManager::OpenGLModulationManager(
  SynthBase* synth,
    mopo::output_map modulation_sources,
    std::map<std::string, ModulationButton*> modulation_buttons,
    std::map<std::string, SynthSlider*> sliders,
  std::map<std::string, SynthButton*> buttons,
    mopo::output_map mono_modulations,
    mopo::output_map poly_modulations) {
  static const int quad_triangles[6] {
    0, 1, 2,
    2, 3, 0
  };

  modulation_buttons_ = modulation_buttons;
  synth_ = synth;
  modulation_sources_ = modulation_sources;
  setInterceptsMouseClicks(false, true);

  current_modulator_ = "";

  polyphonic_destinations_ = std::make_unique<Component>();
  polyphonic_destinations_->setInterceptsMouseClicks(false, true);

  monophonic_destinations_ = std::make_unique<Component>();
  monophonic_destinations_->setInterceptsMouseClicks(false, true);

  for (auto& mod_button : modulation_buttons_) {
    mod_button.second->addListener(this);
    mod_button.second->addDisconnectListener(this);

    // Create modulation highlight overlays.
    std::string name = mod_button.second->getName().toStdString();
    ModulationHighlight* overlay = new ModulationHighlight();
    addChildComponent(overlay);
    overlay_lookup_[name] = overlay;
    overlay->setName(name);
    Rectangle<int> local_bounds = mod_button.second->getBoundsInParent();
    overlay->setBounds(mod_button.second->getParentComponent()->localAreaToGlobal(local_bounds));
  }

  slider_model_lookup_ = sliders;
  button_model_lookup_ = buttons;
  vertices_ = new float[FLOATS_PER_METER * slider_model_lookup_.size()];
  triangles_ = new int[INDICES_PER_METER * slider_model_lookup_.size()];

  int i = 0;
  for (auto& slider : slider_model_lookup_) {
    std::string name = slider.first;
    const mopo::Output* mono_total = mono_modulations[name];
    const mopo::Output* poly_total = poly_modulations[name];

    float* meter_vertices = vertices_ + (i * FLOATS_PER_METER);
    memset(meter_vertices, 0, FLOATS_PER_METER * sizeof(float));

    int* meter_triangles = triangles_ + (i * INDICES_PER_METER);
    for (int t = 0; t < INDICES_PER_METER; ++t)
      meter_triangles[t] = i * POINTS_PER_METER + quad_triangles[t];

    slider.second->addSliderListener(this);

    // Create modulation meter.
    if (mono_total) {
      std::string name = slider.second->getName().toStdString();
  OpenGLModulationMeter* meter = new OpenGLModulationMeter(synth_, mono_total, poly_total,
                       slider.second, std::span<float>(meter_vertices, FLOATS_PER_METER));
      addChildComponent(meter);
      meter_lookup_[name] = meter;
      meter->setName(name);
      Rectangle<int> local_bounds = slider.second->getBoundsInParent();
      meter->setBounds(slider.second->getParentComponent()->localAreaToGlobal(local_bounds));
    }

    // Create modulation slider.
    ModulationSlider* mod_slider = new ModulationSlider(slider.second);
    mod_slider->setLookAndFeel(ModulationLookAndFeel::instance());
    mod_slider->addListener(this);
    if (poly_total)
      polyphonic_destinations_->addAndMakeVisible(mod_slider);
    else
      monophonic_destinations_->addAndMakeVisible(mod_slider);

    slider_lookup_[name] = mod_slider;
    owned_sliders_.push_back(mod_slider);

    ++i;
  }

  for (auto& button : button_model_lookup_) {
    const mopo::Output* poly_total = poly_modulations[button.first];

    button.second->addButtonListener(this);

    ButtonModulationSlider* mod_slider = new ButtonModulationSlider(button.second);
    mod_slider->setLookAndFeel(ModulationLookAndFeel::instance());
    mod_slider->addListener(this);
    if (poly_total)
      polyphonic_destinations_->addAndMakeVisible(mod_slider);
    else
      monophonic_destinations_->addAndMakeVisible(mod_slider);

    slider_lookup_[button.first] = mod_slider;
    owned_sliders_.push_back(mod_slider);
  }

  addAndMakeVisible(polyphonic_destinations_.get());
  addAndMakeVisible(monophonic_destinations_.get());

  forgetModulator();
}

OpenGLModulationManager::~OpenGLModulationManager() {
  for (auto& [name, button] : modulation_buttons_) {
    button->removeListener(this);
    button->removeDisconnectListener(this);
  }
  for (auto& [name, slider] : slider_model_lookup_)
    slider->removeSliderListener(this);
  for (auto& [name, button] : button_model_lookup_)
    button->removeButtonListener(this);
  for (auto& meter : meter_lookup_)
    delete meter.second;
  for (auto& overlay : overlay_lookup_)
    delete overlay.second;
  for (Slider* slider : owned_sliders_) {
    slider->removeListener(this);
    delete slider;
  }
  delete[] vertices_;
  delete[] triangles_;
}

void OpenGLModulationManager::paint(Graphics& g) {
}

void OpenGLModulationManager::resized() {
  SynthGuiInterface* parent = findParentComponentOfClass<SynthGuiInterface>();
  polyphonic_destinations_->setBounds(getBounds());
  monophonic_destinations_->setBounds(getBounds());

  // Update modulation slider locations.
  for (auto& slider : slider_lookup_) {
    setDestinationSliderBounds(slider.first, slider.second);
  }

  // Update modulation meter locations.
  for (auto& meter : meter_lookup_) {
    Slider* model = slider_model_lookup_[meter.first];
    Point<float> local_top_left = getLocalPoint(model, Point<float>(0.0f, 0.0f));
    meter.second->setBounds(local_top_left.x, local_top_left.y,
                            model->getWidth(), model->getHeight());
    if (parent) {
      int num_modulations = parent->getSynth()->getNumModulations(meter.first);
      meter.second->setModulated(num_modulations);
      meter.second->setVisible(num_modulations && model->isVisible());
    }
  }

  // Update modulation highlight overlay locations.
  for (auto& overlay : overlay_lookup_) {
    ModulationButton* model = modulation_buttons_[overlay.first];
    Point<float> local_top_left = getLocalPoint(model, Point<float>(0.0f, 0.0f));
    overlay.second->setBounds(local_top_left.x, local_top_left.y,
                              model->getWidth(), model->getHeight());
  }

  OpenGLComponent::resized();
}

void OpenGLModulationManager::buttonClicked(juce::Button *clicked_button) {
  std::string name = clicked_button->getName().toStdString();
  if (clicked_button->getToggleState()) {
    if (current_modulator_ != "") {
      Button* modulator = modulation_buttons_[current_modulator_];
      modulator->setToggleState(false, NotificationType::dontSendNotification);
    }
    changeModulator(name);
  }
  else
    forgetModulator();
}

void OpenGLModulationManager::sliderValueChanged(juce::Slider *moved_slider) {
  std::string destination_name = moved_slider->getName().toStdString();
  setModulationAmount(current_modulator_, destination_name, moved_slider->getValue());

  modulation_buttons_[current_modulator_]->repaint();
  last_value_ = moved_slider->getValue();
}

void OpenGLModulationManager::guiChanged(SynthSlider* slider) {
  auto meter = meter_lookup_.find(slider->getName().toStdString());
  if (meter != meter_lookup_.end())
    meter->second->updateGeometry();
}

void OpenGLModulationManager::modulationDisconnected(mopo::ModulationConnection* connection, bool last) {
  if (connection->source == current_modulator_) {
    Slider* slider = slider_lookup_[connection->destination];
    slider->setValue(slider->getDoubleClickReturnValue());
  }

  if (meter_lookup_.count(connection->destination)) {
    OpenGLModulationMeter* meter = meter_lookup_[connection->destination];
    meter->setModulated(!last);
    SynthSlider* model = slider_model_lookup_[connection->destination];
    meter->setVisible(!last && model->isVisible());
  }

  if (button_model_lookup_.count(connection->destination))
    button_model_lookup_[connection->destination]->setModulated(!last);
}

void OpenGLModulationManager::init(OpenGLContext& open_gl_context) {
  gl_ready_ = false;
  position_.reset();
  coordinates_.reset();
  range_.reset();
  radius_uniform_.reset();
  open_gl_context.extensions.glGenBuffers(1, &vertex_buffer_);
  open_gl_context.extensions.glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);

  int num_meters = slider_model_lookup_.size();
  GLsizeiptr vert_size = static_cast<GLsizeiptr>(num_meters * FLOATS_PER_METER * sizeof(float));
  open_gl_context.extensions.glBufferData(GL_ARRAY_BUFFER, vert_size,
                                          vertices_, GL_STATIC_DRAW);

  open_gl_context.extensions.glGenBuffers(1, &triangle_buffer_);
  open_gl_context.extensions.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangle_buffer_);

  GLsizeiptr tri_size = static_cast<GLsizeiptr>(num_meters * INDICES_PER_METER * sizeof(int));
  open_gl_context.extensions.glBufferData(GL_ELEMENT_ARRAY_BUFFER, tri_size,
                                          triangles_, GL_STATIC_DRAW);

  const char* vertex_shader = Shaders::getShader(Shaders::kModulationVertex);
  const char* fragment_shader = Shaders::getShader(Shaders::kModulationFragment);

  shader_ = std::make_unique<OpenGLShaderProgram>(open_gl_context);

  if (shader_->addVertexShader(OpenGLHelpers::translateVertexShaderToV3(vertex_shader)) &&
      shader_->addFragmentShader(OpenGLHelpers::translateFragmentShaderToV3(fragment_shader)) &&
      shader_->link()) {
    shader_->use();
    position_ = std::make_unique<OpenGLShaderProgram::Attribute>(*shader_, "position");
    coordinates_ = std::make_unique<OpenGLShaderProgram::Attribute>(*shader_, "coordinates");
    range_ = std::make_unique<OpenGLShaderProgram::Attribute>(*shader_, "range");
    radius_uniform_ = std::make_unique<OpenGLShaderProgram::Uniform>(*shader_, "radius");
    gl_ready_ = vertex_buffer_ != 0 && triangle_buffer_ != 0 &&
                position_->attributeID != static_cast<GLuint>(-1) &&
                coordinates_->attributeID != static_cast<GLuint>(-1) &&
                range_->attributeID != static_cast<GLuint>(-1) && radius_uniform_->uniformID >= 0;
  }
  if (!gl_ready_)
    Logger::writeToLog("[OpenGL] Modulation manager initialization failed: " + shader_->getLastError());
}

void OpenGLModulationManager::render(OpenGLContext& open_gl_context, bool animate) {
  if (!gl_ready_ || !animate)
    return;

  for (auto& meter : meter_lookup_) {
    meter.second->updateDrawing();
  }

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  setViewPort(open_gl_context);

  shader_->use();
  radius_uniform_->set(0.9f);

  open_gl_context.extensions.glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);

  int num_meters = slider_model_lookup_.size();
  GLsizeiptr vert_size = static_cast<GLsizeiptr>(num_meters * FLOATS_PER_METER * sizeof(float));
  open_gl_context.extensions.glBufferData(GL_ARRAY_BUFFER, vert_size,
                                          vertices_, GL_STATIC_DRAW);
  open_gl_context.extensions.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangle_buffer_);

  if (position_ != nullptr) {
    open_gl_context.extensions.glVertexAttribPointer(position_->attributeID, 2, GL_FLOAT,
                                                     GL_FALSE, 6 * sizeof(float), 0);
    open_gl_context.extensions.glEnableVertexAttribArray(position_->attributeID);
  }

  if (coordinates_ != nullptr) {
    open_gl_context.extensions.glVertexAttribPointer(coordinates_->attributeID, 2, GL_FLOAT,
                                                     GL_FALSE, 6 * sizeof(float),
                                                     (GLvoid*)(2 * sizeof(float)));
    open_gl_context.extensions.glEnableVertexAttribArray(coordinates_->attributeID);
  }

  if (range_ != nullptr) {
    open_gl_context.extensions.glVertexAttribPointer(range_->attributeID, 2, GL_FLOAT,
                                                     GL_FALSE, 6 * sizeof(float),
                                                     (GLvoid*)(4 * sizeof(float)));
    open_gl_context.extensions.glEnableVertexAttribArray(range_->attributeID);
  }

  glDrawElements(GL_TRIANGLES, num_meters * INDICES_PER_METER, GL_UNSIGNED_INT, 0);

  if (position_ != nullptr)
    open_gl_context.extensions.glDisableVertexAttribArray(position_->attributeID);

  if (coordinates_ != nullptr)
    open_gl_context.extensions.glDisableVertexAttribArray(coordinates_->attributeID);

  if (range_ != nullptr)
    open_gl_context.extensions.glDisableVertexAttribArray(range_->attributeID);

  open_gl_context.extensions.glBindBuffer(GL_ARRAY_BUFFER, 0);
  open_gl_context.extensions.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void OpenGLModulationManager::destroy(OpenGLContext& open_gl_context) {
  gl_ready_ = false;
  shader_ = nullptr;
  position_ = nullptr;
  coordinates_ = nullptr;
  range_ = nullptr;
  radius_uniform_ = nullptr;

  open_gl_context.extensions.glDeleteBuffers(1, &vertex_buffer_);
  open_gl_context.extensions.glDeleteBuffers(1, &triangle_buffer_);
  vertex_buffer_ = 0;
  triangle_buffer_ = 0;
}

void OpenGLModulationManager::hoverStarted(const std::string& name) {
  makeModulationsVisible(name, true);
}

void OpenGLModulationManager::hoverEnded(const std::string& name) {
  makeModulationsVisible(name, false);
}

bool OpenGLModulationManager::isDestinationModulated(const std::string& name) const {
  auto meter = meter_lookup_.find(name);
  return meter != meter_lookup_.end() && meter->second->isModulated();
}

bool OpenGLModulationManager::copyModulatedValue(const std::string& name, float& value) const {
  auto meter = meter_lookup_.find(name);
  return meter != meter_lookup_.end() && meter->second->copyModulatedValue(value);
}

void OpenGLModulationManager::modulationsChanged(const std::string& destination) {
  SynthGuiInterface* parent = findParentComponentOfClass<SynthGuiInterface>();
  if (parent == nullptr)
    return;

  SynthBase* synth = parent->getSynth();
  int num_modulations = synth->getNumModulations(destination);

  float modulation_amount = 0.0f;
  for (auto* connection : synth->getDestinationConnections(destination)) {
    if (connection->source == current_modulator_) {
      modulation_amount = connection->amount.value();
      break;
    }
  }
  auto modulation_slider = slider_lookup_.find(destination);
  if (modulation_slider != slider_lookup_.end()) {
    modulation_slider->second->setValue(modulation_amount, NotificationType::dontSendNotification);
    modulation_slider->second->repaint();
  }

  if (meter_lookup_.count(destination)) {
    meter_lookup_[destination]->setModulated(num_modulations);
    meter_lookup_[destination]->setVisible(num_modulations && slider_model_lookup_[destination]->isVisible());
  }
  if (button_model_lookup_.count(destination))
    button_model_lookup_[destination]->setModulated(num_modulations);
}

void OpenGLModulationManager::modulationsChanged(SynthButton* button) {
  if (button != nullptr)
    modulationsChanged(button->getName().toStdString());
}

void OpenGLModulationManager::setModulationAmount(std::string source, std::string destination,
                                                  mopo::mopo_float amount) {
  SynthGuiInterface* parent = findParentComponentOfClass<SynthGuiInterface>();
  if (parent == nullptr)
    return;

  parent->getSynth()->changeModulationAmount(source, destination, amount);
  modulationsChanged(destination);
}

void OpenGLModulationManager::promptForModulationValueEntry(const std::string& source,
                                                           const std::string& destination) {
  const auto slider = slider_lookup_.find(destination);
  if (slider == slider_lookup_.end())
    return;
  auto* modulation_slider = dynamic_cast<SynthSlider*>(slider->second);
  if (modulation_slider == nullptr)
    return;

  Component::SafePointer<OpenGLModulationManager> safe_this(this);
  modulation_slider->promptForModulationValueEntry(source,
      [safe_this, source, destination](double amount) {
        if (safe_this == nullptr)
          return;
        auto* parent = safe_this->findParentComponentOfClass<SynthGuiInterface>();
        if (parent == nullptr)
          return;
        bool connection_exists = false;
        for (auto* connection : parent->getSynth()->getDestinationConnections(destination)) {
          if (connection->source == source) {
            connection_exists = true;
            break;
          }
        }
        if (!connection_exists)
          return;
        safe_this->setModulationAmount(source, destination, amount);
        if (safe_this->current_modulator_ == source) {
          const auto current_slider = safe_this->slider_lookup_.find(destination);
          if (current_slider != safe_this->slider_lookup_.end()) {
            current_slider->second->setValue(amount, dontSendNotification);
            current_slider->second->repaint();
          }
        }
        const auto button = safe_this->modulation_buttons_.find(source);
        if (button != safe_this->modulation_buttons_.end())
          button->second->repaint();
        parent->notifyChange();
      }, findParentComponentOfClass<FullInterface>());
}

void OpenGLModulationManager::forgetModulator() {
  polyphonic_destinations_->setVisible(false);
  monophonic_destinations_->setVisible(false);
  current_modulator_ = "";
}

void OpenGLModulationManager::reset(const SynthGuiStateSnapshot& snapshot) {
  for (auto& meter : meter_lookup_) {
    int num_modulations = static_cast<int>(std::count_if(
        snapshot.modulations.begin(), snapshot.modulations.end(),
        [&meter](const auto& connection) { return connection.destination == meter.first; }));
    meter.second->setModulated(num_modulations);
    meter.second->setVisible(num_modulations && slider_model_lookup_[meter.first]->isVisible());
  }

  for (auto& button : button_model_lookup_) {
    int num_modulations = static_cast<int>(std::count_if(
        snapshot.modulations.begin(), snapshot.modulations.end(),
        [&button](const auto& connection) { return connection.destination == button.first; }));
    button.second->setModulated(num_modulations);
  }

  for (auto& slider : slider_lookup_) {
    float value = 0.0f;
    const std::string destination = slider.second->getName().toStdString();
    for (const auto& connection : snapshot.modulations) {
      if (connection.source == current_modulator_ && connection.destination == destination) {
        value = static_cast<float>(connection.amount);
        break;
      }
    }
    slider.second->setValue(value, NotificationType::dontSendNotification);
    slider.second->repaint();
  }
}

void OpenGLModulationManager::makeModulationsVisible(std::string destination, bool visible) {
  SynthGuiInterface* parent = findParentComponentOfClass<SynthGuiInterface>();
  if (parent == nullptr)
    return;

  std::vector<mopo::ModulationConnection*> connections =
      parent->getSynth()->getDestinationConnections(destination);

  for (mopo::ModulationConnection* connection : connections)
    overlay_lookup_[connection->source]->setVisible(visible);
}

void OpenGLModulationManager::setSliderValues() {
  SynthGuiInterface* parent = findParentComponentOfClass<SynthGuiInterface>();
  if (parent == nullptr)
    return;

  std::vector<mopo::ModulationConnection*> connections =
      parent->getSynth()->getSourceConnections(current_modulator_);
  for (auto& slider : slider_lookup_) {
    std::string destination_name = slider.second->getName().toStdString();
    float value = 0.0f;

    for (mopo::ModulationConnection* connection : connections) {
      if (connection->destination == destination_name) {
        value = connection->amount.value();
        break;
      }
    }
    slider.second->setValue(value, NotificationType::dontSendNotification);
    slider.second->repaint();
  }
}

void OpenGLModulationManager::changeModulator(std::string new_modulator) {
  current_modulator_ = new_modulator;
  setSliderValues();

  for (auto& slider : slider_lookup_)
    setDestinationSliderBounds(slider.first, slider.second);

  polyphonic_destinations_->setVisible(true);
  polyphonic_destinations_->repaint();
  monophonic_destinations_->setVisible(true);
  monophonic_destinations_->repaint();
}

bool OpenGLModulationManager::modulationGesture(SynthButton* button, const ModifierKeys& mods) {
  if (current_modulator_.empty())
    return false;

  std::string destination = button->getName().toStdString();
  if (!button_model_lookup_.count(destination) || !slider_lookup_.count(destination))
    return false;

  Slider* slider = slider_lookup_[destination];
  double threshold = mods.isShiftDown() ? -0.5 : 0.5;
  if (std::abs(slider->getValue() - threshold) < 0.0001)
    threshold = slider->getDoubleClickReturnValue();

  slider->setValue(threshold);
  return true;
}

void OpenGLModulationManager::setDestinationSliderBounds(const std::string& name, Slider* slider) {
  if (slider_model_lookup_.count(name)) {
    SynthSlider* model = slider_model_lookup_[name];
    Point<float> local_top_left = getLocalPoint(model, Point<float>(0.0f, 0.0f));
    slider->setVisible(model->isVisible());
    slider->setBounds(local_top_left.x, local_top_left.y, model->getWidth(), model->getHeight());
    return;
  }

  if (button_model_lookup_.count(name)) {
    SynthButton* model = button_model_lookup_[name];
    Point<float> local_top_left = getLocalPoint(model, Point<float>(0.0f, 0.0f));
    int slider_width = jmax(8, model->getWidth() / 3);
    int slider_x = int(local_top_left.x) + (model->getWidth() - slider_width) / 2;
    slider->setVisible(model->isVisible());
    slider->setBounds(slider_x, int(local_top_left.y), slider_width, model->getHeight());
  }
}
