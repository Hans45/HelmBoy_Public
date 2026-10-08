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

#include "open_gl_wave_viewer.h"

#include "colors.h"
#include "synth_base.h"
#include "synth_gui_interface.h"
#include "utils.h"
#include <cmath>

using namespace juce::gl;

#define GRID_CELL_WIDTH 8
#define PADDING 5.0f
#define MARKER_WIDTH 12.0f
#define NOISE_RESOLUTION 6
#define IMAGE_HEIGHT 256

namespace {
  static const float random_values[NOISE_RESOLUTION] = {0.3f, 0.9f, -0.9f, -0.2f, -0.5f, 0.7f };
} // namespace

OpenGLWaveViewer::OpenGLWaveViewer(int resolution) {
  wave_slider_ = nullptr;
  amplitude_slider_ = nullptr;
  phase_stretch_slider_ = nullptr;
  resolution_ = resolution;

  position_vertices_ = new float[16] {
    0.0f, 1.0f, 0.0f, 1.0f,
    0.0f, -1.0f, 0.0f, 0.0f,
    0.1f, -1.0f, 1.0f, 0.0f,
    0.1f, 1.0f, 1.0f, 1.0f
  };

  position_triangles_ = new int[6] {
    0, 1, 2,
    2, 3, 0
  };
}

OpenGLWaveViewer::~OpenGLWaveViewer() {
  stopTimer();
  for (auto* slider : { wave_slider_, amplitude_slider_, phase_stretch_slider_ }) {
    if (slider)
      slider->removeSliderListener(this);
  }
  delete[] position_vertices_;
  delete[] position_triangles_;
}

void OpenGLWaveViewer::paintBackground() {
  static const DropShadow shadow(Colour(Colors::Color_bb000000), 5, Point<int>(0, 0));

  if (getWidth() <= 0 || getHeight() <= 0)
    return;

  auto *display = Desktop::getInstance().getDisplays().getPrimaryDisplay();
  jassert(display != nullptr);

  float scale = display->scale;
  background_image_ = Image(Image::ARGB, scale * getWidth(), scale * getHeight(), true);
  {
    Graphics g(background_image_);
    g.addTransform(AffineTransform::scale(scale, scale));
    // Fond de la grille d'affichage de la courbe d'onde
    g.fillAll(Colour(Colors::Color_ff424242));
    // Quadrillage de la grille d'affichage de la courbe d'onde
    g.setColour(Colour(Colors::Color_ff4a4a4a));
    for (int x = 0; x < getWidth(); x += GRID_CELL_WIDTH)
      g.drawLine(x, 0, x, getHeight());
    for (int y = 0; y < getHeight(); y += GRID_CELL_WIDTH)
      g.drawLine(0, y, getWidth(), y);

    shadow.drawForPath(g, wave_path_);

    g.setColour(Colors::graph_fill);
    g.fillPath(wave_path_);

    g.setColour(Colors::modulation);
    float line_width = 1.0f * getHeight() / 75.0f;
    PathStrokeType stroke(line_width, PathStrokeType::beveled, PathStrokeType::rounded);
    g.strokePath(wave_path_, stroke);
  }

  background_.updateBackgroundImage(background_image_);
}

void OpenGLWaveViewer::paintPositionImage() {
  int min_image_width = roundToInt(2 * MARKER_WIDTH);
  int image_width = mopo::utils::nextPowerOfTwo(min_image_width);
  int marker_width = MARKER_WIDTH;
  int image_height = roundToInt(2 * IMAGE_HEIGHT);
  position_image_ = Image(Image::ARGB, image_width, image_height, true);
  Graphics g(position_image_);

  g.setColour(Colour(Colors::Color_77ffffff));
  g.fillRect(image_width / 2.0f - 0.5f, 0.0f, 1.0f, 1.0f * image_height);

  g.setColour(Colors::modulation);
  g.fillEllipse(image_width / 2 - marker_width / 2, image_height / 2 - marker_width / 2,
                marker_width, marker_width);
  g.setColour(Colour(Colors::Color_ff000000));
  g.fillEllipse(image_width / 2 - marker_width / 4, image_height / 2 - marker_width / 4,
                marker_width / 2, marker_width / 2);
}

void OpenGLWaveViewer::mouseDown(const MouseEvent& e) {
  if (wave_slider_) {
    int current_value = wave_slider_->getValue();
    if (e.mods.isRightButtonDown())
      current_value = current_value + wave_slider_->getMaximum();
    else
      current_value = current_value + 1;
    wave_slider_->setValue(current_value % static_cast<int>(wave_slider_->getMaximum() + 1));

    resetWavePath();
  }
}

void OpenGLWaveViewer::resized() {
  resetWavePath();

  SynthGuiInterface* parent = findParentComponentOfClass<SynthGuiInterface>();
  if (parent) {
    synth_ = parent->getSynth();
    mopo::Output* amplitude = synth_->getModSource(getName().toStdString() + "_amp");
    if (amplitude == nullptr)
      amplitude = synth_->getModSource(getName().toStdString());
    mopo::Output* phase = synth_->getModSource(getName().toStdString() + "_phase");
    telemetry_indices_[0] = synth_->getVisualTelemetryIndex(phase);
    telemetry_indices_[1] = synth_->getVisualTelemetryIndex(amplitude);
  }
  publishRenderGeometry();
}

void OpenGLWaveViewer::setWaveSlider(SynthSlider* slider) {
  wave_slider_ = slider;
  wave_slider_->addSliderListener(this);
  resetWavePath();
}

void OpenGLWaveViewer::setAmplitudeSlider(SynthSlider* slider) {
  amplitude_slider_ = slider;
  amplitude_slider_->addSliderListener(this);
  resetWavePath();
}

void OpenGLWaveViewer::setPhaseStretchSlider(SynthSlider* slider) {
  phase_stretch_slider_ = slider;
  phase_stretch_slider_->addSliderListener(this);
  has_modulated_phase_stretch_ = false;
  startTimerHz(30);
  resetWavePath();
}

float OpenGLWaveViewer::getPhaseStretch() const {
  float value = has_modulated_phase_stretch_ ? modulated_phase_stretch_
      : phase_stretch_slider_ ? static_cast<float>(phase_stretch_slider_->getValue()) : 0.5f;
  return std::isfinite(value) ? mopo::utils::clamp(value, 0.01f, 0.99f) : 0.5f;
}

void OpenGLWaveViewer::timerCallback() {
  if (!phase_stretch_slider_ || !isShowing())
    return;
  auto* interface = findParentComponentOfClass<FullInterface>();
  if (!interface || !interface->isAnimating())
    return;

  const std::string name = phase_stretch_slider_->getName().toStdString();
  if (interface->isDestinationModulated(name)) {
    float value = 0.5f;
    if (interface->copyModulatedValue(name, value) && std::isfinite(value)) {
      modulated_phase_stretch_ = mopo::utils::clamp(value, 0.01f, 0.99f);
      has_modulated_phase_stretch_ = true;
    }
  }
  else
    has_modulated_phase_stretch_ = false;

  if (std::abs(getPhaseStretch() - displayed_phase_stretch_) >= 0.0001f)
    resetWavePath();
}

void OpenGLWaveViewer::drawRandom() {
  float amplitude = amplitude_slider_ ? amplitude_slider_->getValue() : 1.0f;
  float draw_width = getWidth();
  float padding = getRatio() * PADDING;
  float draw_height = getHeight() - 2.0f * padding;

  wave_path_.startNewSubPath(0, getHeight() / 2.0f);
  for (int i = 0; i < NOISE_RESOLUTION; ++i) {
    float t1 = (1.0f * i) / NOISE_RESOLUTION;
    float t2 = (1.0f + i) / NOISE_RESOLUTION;
    float val = amplitude * random_values[i];
    wave_path_.lineTo(t1 * draw_width, padding + draw_height * ((1.0f - val) / 2.0f));
    wave_path_.lineTo(t2 * draw_width, padding + draw_height * ((1.0f - val) / 2.0f));
  }

  wave_path_.lineTo(getWidth(), getHeight() / 2.0f);
}

void OpenGLWaveViewer::drawSmoothRandom() {
  float amplitude = amplitude_slider_ ? amplitude_slider_->getValue() : 1.0f;
  float phase_stretch = getPhaseStretch();
  float draw_width = getWidth();
  float padding = getRatio() * PADDING;
  float draw_height = getHeight() - 2.0f * padding;

  float start_val = amplitude * random_values[0];
  wave_path_.startNewSubPath(-50, getHeight() / 2.0f);
  wave_path_.lineTo(0, PADDING + draw_height * ((1.0f - start_val) / 2.0f));
  for (int i = 1; i < resolution_ - 1; ++i) {
    float t = (1.0f * i) / resolution_;
    float phase = mopo::utils::remapPhase(t, phase_stretch) * (NOISE_RESOLUTION - 1);
    int index = (int)phase;
    phase = mopo::PI * (phase - index);
    float val = amplitude * mopo::utils::interpolate(random_values[index],
                                                     random_values[index + 1],
                                                     0.5f - cosf(phase) / 2.0f);
    wave_path_.lineTo(t * draw_width, padding + draw_height * ((1.0f - val) / 2.0f));
  }

  float end_val = amplitude * random_values[NOISE_RESOLUTION - 1];
  wave_path_.lineTo(getWidth(), padding + draw_height * ((1.0f - end_val) / 2.0f));
  wave_path_.lineTo(getWidth() + 50, getHeight() / 2.0f);

}

void OpenGLWaveViewer::resetWavePath() {
  wave_path_.clear();

  if (wave_slider_ == nullptr) {
    publishRenderGeometry();
    return;
  }

  float amplitude = amplitude_slider_ ? amplitude_slider_->getValue() : 1.0f;
  float phase_stretch = getPhaseStretch();
  displayed_phase_stretch_ = phase_stretch;
  float draw_width = getWidth();
  float padding = getRatio() * PADDING;
  float draw_height = getHeight() - 2.0f * padding;

  mopo::Wave::Type type = static_cast<mopo::Wave::Type>(static_cast<int>(wave_slider_->getValue()));

  if (type < mopo::Wave::Type::WhiteNoise) {
    wave_path_.startNewSubPath(0, getHeight() / 2.0f);
    for (int i = 1; i < resolution_ - 1; ++i) {
      float t = (1.0f * i) / resolution_;
      float val = amplitude * mopo::Wave::wave(type, mopo::utils::remapPhase(t, phase_stretch));
      wave_path_.lineTo(t * draw_width, padding + draw_height * ((1.0f - val) / 2.0f));
    }

    wave_path_.lineTo(getWidth(), getHeight() / 2.0f);
  }
  else if (type == mopo::Wave::Type::WhiteNoise)
    drawRandom();
  else
    drawSmoothRandom();

  paintBackground();
}

void OpenGLWaveViewer::publishRenderGeometry() {
  auto geometry = std::make_shared<RenderGeometry>();
  geometry->width = static_cast<float>(getWidth());
  geometry->height = static_cast<float>(getHeight());
  geometry->padding = getRatio() * PADDING;
  render_geometry_.store(std::move(geometry), std::memory_order_release);
}

void OpenGLWaveViewer::guiChanged(SynthSlider* slider) {
  resetWavePath();
}

float OpenGLWaveViewer::phaseToX(float phase) {
  return phase * getWidth();
}

void OpenGLWaveViewer::init(OpenGLContext& open_gl_context) {
  paintPositionImage();

  open_gl_context.extensions.glGenBuffers(1, &vertex_buffer_);
  open_gl_context.extensions.glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);

  GLsizeiptr vert_size = static_cast<GLsizeiptr>(static_cast<size_t>(16 * sizeof(float)));
  open_gl_context.extensions.glBufferData(GL_ARRAY_BUFFER, vert_size,
                                          position_vertices_, GL_STATIC_DRAW);

  open_gl_context.extensions.glGenBuffers(1, &triangle_buffer_);
  open_gl_context.extensions.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangle_buffer_);

  GLsizeiptr tri_size = static_cast<GLsizeiptr>(static_cast<size_t>(6 * sizeof(float)));
  open_gl_context.extensions.glBufferData(GL_ELEMENT_ARRAY_BUFFER, tri_size,
                                          position_triangles_, GL_STATIC_DRAW);

  background_.init(open_gl_context);
}

void OpenGLWaveViewer::drawPosition(OpenGLContext& open_gl_context) {
  open_gl_context.extensions.glActiveTexture(GL_TEXTURE0);
  if (position_texture_.getWidth() != position_image_.getWidth())
    position_texture_.loadImage(position_image_);

  auto geometry = render_geometry_.load(std::memory_order_acquire);
  if (synth_ == nullptr || !geometry || geometry->width <= 0.0f || geometry->height <= 0.0f ||
      telemetry_indices_[0] < 0 || telemetry_indices_[1] < 0)
    return;

  (void)synth_->copyVisualTelemetry(telemetry_indices_, telemetry_values_);
  if (telemetry_values_[0] <= 0.0f)
    return;

  float x = 2.0f * telemetry_values_[0] - 1.0f;
  float y = (geometry->height - 2 * geometry->padding) * telemetry_values_[1] / geometry->height;

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  int draw_width = static_cast<int>(geometry->width);
  int draw_height = static_cast<int>(geometry->height);

  float ratio = geometry->height / 75.0f;

  float position_height = ratio * (0.5f * position_texture_.getHeight()) / draw_height;
  float position_width = ratio * (0.5f * position_texture_.getWidth()) / draw_width;
  position_vertices_[0] = x - position_width;
  position_vertices_[1] = y + position_height;
  position_vertices_[4] = x - position_width;
  position_vertices_[5] = y - position_height;
  position_vertices_[8] = x + position_width;
  position_vertices_[9] = y - position_height;
  position_vertices_[12] = x + position_width;
  position_vertices_[13] = y + position_height;

  open_gl_context.extensions.glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  GLsizeiptr vert_size = static_cast<GLsizeiptr>(static_cast<size_t>(16 * sizeof(float)));
  open_gl_context.extensions.glBufferData(GL_ARRAY_BUFFER, vert_size,
                                          position_vertices_, GL_STATIC_DRAW);

  open_gl_context.extensions.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangle_buffer_);
  position_texture_.bind();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  background_.shader()->use();

  if (background_.texture_uniform() != nullptr)
    background_.texture_uniform()->set(0);

  background_.enableAttributes(open_gl_context);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
  background_.disableAttributes(open_gl_context);

  position_texture_.unbind();

  open_gl_context.extensions.glBindBuffer(GL_ARRAY_BUFFER, 0);
  open_gl_context.extensions.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void OpenGLWaveViewer::render(OpenGLContext& open_gl_context, bool animate) {
  MOPO_ASSERT(glGetError() == GL_NO_ERROR);

  setViewPort(open_gl_context);

  background_.render(open_gl_context);

  if (animate)
    drawPosition(open_gl_context);

  MOPO_ASSERT(glGetError() == GL_NO_ERROR);
}

void OpenGLWaveViewer::destroy(OpenGLContext& open_gl_context) {
  position_texture_.release();

  texture_ = nullptr;
  open_gl_context.extensions.glDeleteBuffers(1, &vertex_buffer_);
  open_gl_context.extensions.glDeleteBuffers(1, &triangle_buffer_);
  background_.destroy(open_gl_context);
}

float OpenGLWaveViewer::getRatio() {
  return getHeight() / 80.0f;
}
