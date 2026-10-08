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

#include "open_gl_modulation_meter.h"

#include "mopo.h"
#include "synth_base.h"
#include "synth_gui_interface.h"
#include "shaders.h"
#include "text_look_and_feel.h"

using namespace juce::gl;

OpenGLModulationMeter::OpenGLModulationMeter(SynthBase* synth,
       const mopo::Output* mono_total,
               const mopo::Output* poly_total,
               const SynthSlider* slider,
               std::span<float> vertices) :
  synth_(synth), destination_(slider), vertices_(vertices),
        current_value_(0.0), knob_percent_(0.0), mod_percent_(0.0),
    rotary_(false) {
  static const float initial_rotary_vertices[24] {
    0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f
  };

  static const float initial_horizontal_vertices[24] {
    0.0f, 0.0f, 0.0f, 0.0f, -10.0f, 10.0f,
    0.0f, 0.0f, 0.0f, 0.0f, -10.0f, 10.0f,
    0.0f, 0.0f, 0.0f, 0.0f, -10.0f, 10.0f,
    0.0f, 0.0f, 0.0f, 0.0f, -10.0f, 10.0f
  };

  static const float initial_vertical_vertices[24] {
    0.0f, 0.0f, 0.0f, 0.0f, -10.0f, 10.0f,
    0.0f, 0.0f, 0.0f, 0.0f, -10.0f, 10.0f,
    0.0f, 0.0f, 0.0f, 0.0f, -10.0f, 10.0f,
    0.0f, 0.0f, 0.0f, 0.0f, -10.0f, 10.0f
  };

  rotary_ = destination_->isRotary() &&
            &destination_->getLookAndFeel() != TextLookAndFeel::instance();
  telemetry_indices_[0] = synth_->getVisualTelemetryIndex(mono_total);
  telemetry_indices_[1] = synth_->getVisualTelemetryIndex(poly_total);
  telemetry_count_ = telemetry_indices_[1] >= 0 ? 2 : telemetry_indices_[0] >= 0 ? 1 : 0;

  if (rotary_)
    memcpy(vertices_.data(), initial_rotary_vertices, sizeof(initial_rotary_vertices));
  else if (destination_->isHorizontal())
    memcpy(vertices_.data(), initial_horizontal_vertices, sizeof(initial_horizontal_vertices));
  else
    memcpy(vertices_.data(), initial_vertical_vertices, sizeof(initial_vertical_vertices));

  setInterceptsMouseClicks(false, false);
  updateDrawing();
}

OpenGLModulationMeter::~OpenGLModulationMeter() { }

bool OpenGLModulationMeter::copyModulatedValue(float& value) const {
  std::array<float, 2> values {};
  if (!isModulated() || telemetry_count_ == 0 ||
      !synth_->copyVisualTelemetry(
          std::span<const int>(telemetry_indices_.data(), telemetry_count_),
          std::span<float>(values.data(), telemetry_count_)))
    return false;
  value = values[0] + (telemetry_count_ > 1 ? values[1] : 0.0f);
  return true;
}

void OpenGLModulationMeter::paint(Graphics& g) { }

void OpenGLModulationMeter::resized() {
  SynthGuiInterface* parent = findParentComponentOfClass<SynthGuiInterface>();
  if (parent) {
    std::vector<mopo::ModulationConnection*> connections;
    connections = parent->getSynth()->getSourceConnections(getName().toStdString());
    setModulated(connections.size());
  }

  if (isVisible())
    setVertices();
  else
    collapseVertices();
}

void OpenGLModulationMeter::setVisible(bool should_be_visible) {
  if (should_be_visible)
    setVertices();
  else
    collapseVertices();

  Component::setVisible(should_be_visible);
  render_visible_.store(should_be_visible && destination_->isVisible() &&
                            modulated_.load(std::memory_order_acquire),
                        std::memory_order_release);
}

void OpenGLModulationMeter::updateGeometry() {
  if (isVisible())
    setVertices();
  else
    collapseVertices();
  render_visible_.store(isVisible() && destination_->isVisible() &&
                            modulated_.load(std::memory_order_acquire),
                        std::memory_order_release);
}

void OpenGLModulationMeter::setVertices() {
  Component* parent = getParentComponent();
  if (parent == nullptr) {
    collapseVertices();
    return;
  }
  Rectangle<int> parent_bounds = parent->getBounds();
  if (parent_bounds.getWidth() <= 0 || parent_bounds.getHeight() <= 0) {
    collapseVertices();
    return;
  }
  Rectangle<int> bounds = getBounds();
  float left = bounds.getX();
  float right = bounds.getRight();
  float top = parent_bounds.getHeight() - bounds.getY();
  float bottom = parent_bounds.getHeight() - bounds.getBottom();

  bool horizontal = destination_->isHorizontal();
  if (!destination_->isRotary()) {
    if (horizontal) {
      bottom += getHeight() / 2.0f - SynthSlider::linear_rail_width;
      top -= getHeight() / 2.0f - SynthSlider::linear_rail_width;
    }
    else {
      left += getWidth() / 2.0f - SynthSlider::linear_rail_width;
      right -= getWidth() / 2.0f - SynthSlider::linear_rail_width;
    }
  }

  auto snapshot = std::make_shared<GeometrySnapshot>();
  snapshot->minimum = static_cast<float>(destination_->getMinimum());
  snapshot->maximum = static_cast<float>(destination_->getMaximum());
  snapshot->knob_percent = snapshot->maximum != snapshot->minimum
                               ? static_cast<float>((destination_->getValue() - snapshot->minimum) /
                                                    (snapshot->maximum - snapshot->minimum))
                               : 0.0f;
  snapshot->left = 2.0f * left / parent_bounds.getWidth() - 1.0f;
  snapshot->right = 2.0f * right / parent_bounds.getWidth() - 1.0f;
  snapshot->top = 2.0f * top / parent_bounds.getHeight() - 1.0f;
  snapshot->bottom = 2.0f * bottom / parent_bounds.getHeight() - 1.0f;
  snapshot->visible = true;
  snapshot->rotary = rotary_;
  snapshot->horizontal = horizontal;
  snapshot->text_style = &destination_->getLookAndFeel() == TextLookAndFeel::instance();
  geometry_.store(std::move(snapshot), std::memory_order_release);
}

void OpenGLModulationMeter::collapseVertices() {
  auto snapshot = std::make_shared<GeometrySnapshot>();
  snapshot->minimum = static_cast<float>(destination_->getMinimum());
  snapshot->maximum = static_cast<float>(destination_->getMaximum());
  snapshot->knob_percent = snapshot->maximum != snapshot->minimum
                               ? static_cast<float>((destination_->getValue() - snapshot->minimum) /
                                                    (snapshot->maximum - snapshot->minimum))
                               : 0.0f;
  snapshot->rotary = rotary_;
  snapshot->horizontal = destination_->isHorizontal();
  snapshot->text_style = &destination_->getLookAndFeel() == TextLookAndFeel::instance();
  geometry_.store(std::move(snapshot), std::memory_order_release);
}

void OpenGLModulationMeter::collapseRenderVertices() {
  vertices_[0] = vertices_[6] = vertices_[12] = vertices_[18] = 0.0f;
  vertices_[1] = vertices_[7] = vertices_[13] = vertices_[19] = 0.0f;
}

void OpenGLModulationMeter::updateDrawing() {
  auto geometry = geometry_.load(std::memory_order_acquire);
  if (!geometry || !geometry->visible || !render_visible_.load(std::memory_order_acquire) ||
      telemetry_count_ == 0) {
    collapseRenderVertices();
    return;
  }

  if (synth_->copyVisualTelemetry(std::span<const int>(telemetry_indices_.data(), telemetry_count_),
                                  std::span<float>(telemetry_values_.data(), telemetry_count_))) {
    current_value_ = telemetry_values_[0];
    if (telemetry_count_ > 1)
      current_value_ += telemetry_values_[1];
  }

  const double range = geometry->maximum - geometry->minimum;
  if (range == 0.0)
    return;
  const double value = (current_value_ - geometry->minimum) / range;
  const double new_mod_percent = mopo::utils::clamp(value, 0.0, 1.0);
  const double new_knob_percent = geometry->knob_percent;
  const bool geometry_changed = geometry != last_geometry_;
  if (geometry_changed || new_mod_percent != mod_percent_ || new_knob_percent != knob_percent_) {
    last_geometry_ = geometry;
    mod_percent_ = new_mod_percent;
    knob_percent_ = new_knob_percent;

    float min_percent = std::min(mod_percent_, knob_percent_);
    float max_percent = std::max(mod_percent_, knob_percent_);

    if (geometry->rotary) {
      float angle = SynthSlider::rotary_angle;

      float min_radians = mopo::utils::interpolate(-angle, angle, min_percent);
      float max_radians = mopo::utils::interpolate(-angle, angle, max_percent);

      vertices_[0] = vertices_[6] = geometry->left;
      vertices_[12] = vertices_[18] = geometry->right;
      vertices_[1] = vertices_[19] = geometry->top;
      vertices_[7] = vertices_[13] = geometry->bottom;

      for (int i = 0; i < 4; ++i) {
        vertices_[4 + 6 * i] = min_radians;
        vertices_[5 + 6 * i] = max_radians;
      }
    }
    else if (geometry->horizontal) {
      float start = mopo::utils::interpolate(geometry->left, geometry->right, min_percent);
      vertices_[0] = vertices_[6] = start;

      float end = mopo::utils::interpolate(geometry->left, geometry->right, max_percent);
      vertices_[12] = vertices_[18] = end;

      vertices_[1] = vertices_[19] = geometry->top;
      vertices_[7] = vertices_[13] = geometry->bottom;
    }
    else if (geometry->text_style) {
      float start = geometry->bottom;
      float end = geometry->top;
      float diff_percent = mod_percent_ - knob_percent_;

      if (diff_percent > 0.0)
        end = mopo::utils::interpolate(geometry->bottom, geometry->top, diff_percent);
      else
        start = mopo::utils::interpolate(geometry->top, geometry->bottom, -diff_percent);

      vertices_[7] = vertices_[13] = start;
      vertices_[1] = vertices_[19] = end;

      vertices_[0] = vertices_[6] = geometry->left;
      vertices_[12] = vertices_[18] = geometry->right;
    }
    else {
      float start = mopo::utils::interpolate(geometry->bottom, geometry->top, min_percent);
      vertices_[7] = vertices_[13] = start;

      float end = mopo::utils::interpolate(geometry->bottom, geometry->top, max_percent);
      vertices_[1] = vertices_[19] = end;

      vertices_[0] = vertices_[6] = geometry->left;
      vertices_[12] = vertices_[18] = geometry->right;
    }
  }
}
