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

#include "open_gl_envelope.h"

#include "colors.h"
#include "envelope.h"
#include "synth_base.h"
#include "synth_gui_interface.h"
#include "utils.h"

using namespace juce::gl;

namespace {
  const float ATTACK_RANGE_PERCENT = 0.33f;
  const float DECAY_RANGE_PERCENT = 0.33f;
  const float DELAY_RANGE_PERCENT = 0.12f;
  const float HOLD_RANGE_PERCENT = 0.12f;
  const float HOVER_DISTANCE = 20.0f;
  const int GRID_CELL_WIDTH = 8;
  const float MARKER_WIDTH = 6.0f;
  const int IMAGE_HEIGHT = 256;

} // namespace


OpenGLEnvelope::OpenGLEnvelope() {
  delay_hover_ = false;
  attack_hover_ = false;
  hold_hover_ = false;
  decay_hover_ = false;
  sustain_hover_ = false;
  release_hover_ = false;
  mouse_down_ = false;

  delay_slider_ = nullptr;
  attack_slider_ = nullptr;
  hold_slider_ = nullptr;
  decay_slider_ = nullptr;
  sustain_slider_ = nullptr;
  release_slider_ = nullptr;
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

OpenGLEnvelope::~OpenGLEnvelope() {
  delete[] position_vertices_;
  delete[] position_triangles_;
}

void OpenGLEnvelope::paintBackground() {
  static const DropShadow shadow(Colour(Colors::Color_bb000000), 5, Point<int>(0, 0));

  if (getWidth() <= 0 || getHeight() <= 0)
    return;

  float ratio = getHeight() / 100.0f;

  auto *display = Desktop::getInstance().getDisplays().getPrimaryDisplay();
  jassert(display != nullptr);

  float scale = display->scale;
  background_image_ = Image(Image::ARGB, scale * getWidth(), scale * getHeight(), true);
  {
    Graphics g(background_image_);
    g.addTransform(AffineTransform::scale(scale, scale));

    g.fillAll(Colour(Colors::Color_ff424242));

    g.setColour(Colour(Colors::Color_ff4a4a4a));
    for (int x = 0; x < getWidth(); x += GRID_CELL_WIDTH)
      g.drawLine(x, 0, x, getHeight());
    for (int y = 0; y < getHeight(); y += GRID_CELL_WIDTH)
      g.drawLine(0, y, getWidth(), y);

    shadow.drawForPath(g, envelope_line_);
    g.setColour(Colors::graph_fill);
    g.fillPath(envelope_line_);

    g.setColour(Colour(Colors::Color_ff505050));
    if (delay_slider_)
      g.drawLine(getDelayX(), 0.0f, getDelayX(), getHeight());
    g.drawLine(getAttackX(), 0.0f, getAttackX(), getHeight());
    g.drawLine(getHoldX(), 0.0f, getHoldX(), getHeight());
    g.drawLine(getDecayX(), getSustainY(), getDecayX(), getHeight());

    g.setColour(Colors::modulation);
    float line_width = 1.0f * getHeight() / 150.0f;
    PathStrokeType stroke(line_width, PathStrokeType::beveled, PathStrokeType::rounded);
    g.strokePath(envelope_line_, stroke);

    float hover_line_x = -20;
    if (delay_hover_)
      hover_line_x = getDelayX();
    else if (attack_hover_)
      hover_line_x = getAttackX();
    else if (hold_hover_)
      hover_line_x = getHoldX();
    else if (decay_hover_)
      hover_line_x = getDecayX();
    else if (release_hover_)
      hover_line_x = getReleaseX();

    g.setColour(Colour(Colors::Color_bbffffff));
    g.fillRect(hover_line_x - 0.5f, 0.0f, 1.0f, 1.0f * getHeight());

    float grab_radius = 20.0f * ratio;
    float hover_radius = 7.0f * ratio;
    if (sustain_hover_) {
      if (mouse_down_) {
        g.setColour(Colour(Colors::Color_11ffffff));
        g.fillEllipse(getDecayX() - grab_radius, getSustainY() - grab_radius,
                      2.0f * grab_radius, 2.0f * grab_radius);
      }

      g.setColour(Colour(Colors::Color_bbffffff));
      g.drawEllipse(getDecayX() - hover_radius, getSustainY() - hover_radius,
                    2.0f * hover_radius, 2.0f * hover_radius, 1.0);
    }
    else if (mouse_down_) {
      g.setColour(Colour(Colors::Color_11ffffff));
      g.fillRect(hover_line_x - 10.0f, 0.0f, 20.0f, 1.0f * getHeight());
    }

  }

  background_.updateBackgroundImage(background_image_);
}

void OpenGLEnvelope::paintPositionImage() {
  int min_image_width = roundToInt(4 * MARKER_WIDTH);
  int image_width = mopo::utils::nextPowerOfTwo(min_image_width);
  int image_height = roundToInt(2 * IMAGE_HEIGHT);
  position_image_ = Image(Image::ARGB, image_width, image_height, true);
  Graphics g(position_image_);

  g.setColour(Colour(Colors::Color_77ffffff));
  g.fillRect(image_width / 2.0f - 0.5f, 0.0f, 1.0f, 1.0f * image_height);
}

void OpenGLEnvelope::resized() {
  resetEnvelopeLine();

  SynthGuiInterface* parent = findParentComponentOfClass<SynthGuiInterface>();
  if (parent) {
    synth_ = parent->getSynth();
    telemetry_indices_[0] = synth_->getVisualTelemetryIndex(
        synth_->getModSource(getName().toStdString() + "_phase"));
    telemetry_indices_[1] = synth_->getVisualTelemetryIndex(
        synth_->getModSource(getName().toStdString() + "_amp"));
    telemetry_indices_[2] = synth_->getVisualTelemetryIndex(
      synth_->getModSource(getName().toStdString() + "_progress"));
  }
}

void OpenGLEnvelope::mouseMove(const MouseEvent& e) {
  float x = e.getPosition().x;
  float y = e.getPosition().y;
  float delay_delta = fabs(x - getDelayX());
  float attack_delta = fabs(x - getAttackX());
  float hold_delta = fabs(x - getHoldX());
  float decay_delta = fabs(x - getDecayX());
  float release_delta = fabs(x - getReleaseX());
  float sustain_delta = fabs(y - getSustainY());

  bool delay_is_hover = delay_slider_ && delay_delta < HOVER_DISTANCE &&
                        y > getHeight() - HOVER_DISTANCE;
  bool hold_is_hover = hold_slider_ && hold_delta < HOVER_DISTANCE &&
                       y < HOVER_DISTANCE;
  bool a_hover = !delay_is_hover && !hold_is_hover &&
                 attack_delta < decay_delta && attack_delta < HOVER_DISTANCE;
  bool d_hover = !delay_is_hover && !hold_is_hover && !a_hover &&
                 decay_delta < release_delta && decay_delta < HOVER_DISTANCE;
  bool r_hover = !delay_is_hover && !hold_is_hover && !a_hover && !d_hover &&
                 release_delta < HOVER_DISTANCE;
  bool s_hover = !delay_is_hover && !hold_is_hover && !a_hover && !r_hover &&
                 x > getDecayX() - HOVER_DISTANCE &&
  x < getDecayX() + HOVER_DISTANCE && sustain_delta < HOVER_DISTANCE;

  if (delay_is_hover != delay_hover_ || a_hover != attack_hover_ ||
      hold_is_hover != hold_hover_ || d_hover != decay_hover_ ||
      s_hover != sustain_hover_ || r_hover != release_hover_) {
    delay_hover_ = delay_is_hover;
    attack_hover_ = a_hover;
    hold_hover_ = hold_is_hover;
    decay_hover_ = d_hover;
    sustain_hover_ = s_hover;
    release_hover_ = r_hover;
    paintBackground();
  }
}

void OpenGLEnvelope::mouseExit(const MouseEvent& e) {
  delay_hover_ = false;
  attack_hover_ = false;
  hold_hover_ = false;
  decay_hover_ = false;
  sustain_hover_ = false;
  release_hover_ = false;
  paintBackground();
}

void OpenGLEnvelope::mouseDown(const MouseEvent& e) {
  mouse_down_ = true;
  paintBackground();
}

void OpenGLEnvelope::mouseDrag(const MouseEvent& e) {
  if (delay_hover_)
    setDelayX(e.getPosition().x);
  else if (attack_hover_)
    setAttackX(e.getPosition().x);
  else if (hold_hover_)
    setHoldX(e.getPosition().x);
  else if (decay_hover_)
    setDecayX(e.getPosition().x);
  else if (release_hover_)
    setReleaseX(e.getPosition().x);

  if (sustain_hover_)
    setSustainY(e.getPosition().y);

  if (delay_hover_ || attack_hover_ || hold_hover_ || decay_hover_ ||
      sustain_hover_ || release_hover_) {
    resetEnvelopeLine();
    paintBackground();
  }
}

void OpenGLEnvelope::mouseUp(const MouseEvent& e) {
  mouse_down_ = false;
  paintBackground();
}

void OpenGLEnvelope::guiChanged(SynthSlider* slider) {
  resetEnvelopeLine();
  paintBackground();
}

float OpenGLEnvelope::getDelayX() {
  if (!delay_slider_)
    return 1.0f;

  double percent = delay_slider_->valueToProportionOfLength(delay_slider_->getValue());
  return 1.0f + (getWidth() - 1.0f) * percent * DELAY_RANGE_PERCENT;
}

float OpenGLEnvelope::getAttackX() {
  if (!attack_slider_)
    return 0.0;

  double percent = attack_slider_->valueToProportionOfLength(attack_slider_->getValue());
  return getDelayX() + (getWidth() - 1) * percent * ATTACK_RANGE_PERCENT;
}

float OpenGLEnvelope::getHoldX() {
  if (!hold_slider_)
    return getAttackX();

  double percent = hold_slider_->valueToProportionOfLength(hold_slider_->getValue());
  return getAttackX() + (getWidth() - 1) * percent * HOLD_RANGE_PERCENT;
}

float OpenGLEnvelope::getDecayX() {
  if (!decay_slider_)
    return 0.0;

  double percent = decay_slider_->valueToProportionOfLength(decay_slider_->getValue());
  return getHoldX() + getWidth() * percent * DECAY_RANGE_PERCENT;
}

float OpenGLEnvelope::getSustainY() {
  if (!sustain_slider_)
    return 0.0;

  double percent = sustain_slider_->valueToProportionOfLength(sustain_slider_->getValue());
  return getHeight() * (1.0 - percent);
}

float OpenGLEnvelope::getReleaseX() {
  if (!release_slider_)
    return 0.0;

  double percent = release_slider_->valueToProportionOfLength(release_slider_->getValue());
  return getDecayX() + getWidth() * percent * getReleaseRange();
}

float OpenGLEnvelope::getReleaseRange() {
  double delay_percent = delay_slider_ ?
      delay_slider_->valueToProportionOfLength(delay_slider_->getValue()) : 0.0;
  double hold_percent = hold_slider_ ?
      hold_slider_->valueToProportionOfLength(hold_slider_->getValue()) : 0.0;
  return 1.0f - ATTACK_RANGE_PERCENT - DECAY_RANGE_PERCENT -
         delay_percent * DELAY_RANGE_PERCENT - hold_percent * HOLD_RANGE_PERCENT;
}

void OpenGLEnvelope::setDelayX(double x) {
  if (!delay_slider_)
    return;

  double percent = (x - 1.0) / ((getWidth() - 1.0) * DELAY_RANGE_PERCENT);
  delay_slider_->setValue(delay_slider_->proportionOfLengthToValue(percent));
}

void OpenGLEnvelope::setAttackX(double x) {
  if (!attack_slider_)
    return;

  double percent = (x - getDelayX()) / ((getWidth() - 1.0) * ATTACK_RANGE_PERCENT);
  attack_slider_->setValue(attack_slider_->proportionOfLengthToValue(percent));
}

void OpenGLEnvelope::setHoldX(double x) {
  if (!hold_slider_)
    return;

  double percent = (x - getAttackX()) / ((getWidth() - 1.0) * HOLD_RANGE_PERCENT);
  hold_slider_->setValue(hold_slider_->proportionOfLengthToValue(percent));
}

void OpenGLEnvelope::setDecayX(double x) {
  if (!decay_slider_)
    return;

  double percent = (x - getHoldX()) / (getWidth() * DECAY_RANGE_PERCENT);
  decay_slider_->setValue(decay_slider_->proportionOfLengthToValue(percent));
}

void OpenGLEnvelope::setSustainY(double y) {
  if (!sustain_slider_)
    return;

  sustain_slider_->setValue(sustain_slider_->proportionOfLengthToValue(1.0 - y / getHeight()));
}

void OpenGLEnvelope::setReleaseX(double x) {
  if (!release_slider_)
    return;

  double percent = (x - getDecayX()) / (getWidth() * getReleaseRange());
  release_slider_->setValue(release_slider_->proportionOfLengthToValue(percent));
}

void OpenGLEnvelope::setDelaySlider(SynthSlider* delay_slider) {
  delay_slider_ = delay_slider;
  if (delay_slider_)
    delay_slider_->addSliderListener(this);
  resetEnvelopeLine();
}

void OpenGLEnvelope::setAttackSlider(SynthSlider* attack_slider) {
  attack_slider_ = attack_slider;
  attack_slider_->addSliderListener(this);
  resetEnvelopeLine();
}

void OpenGLEnvelope::setHoldSlider(SynthSlider* hold_slider) {
  hold_slider_ = hold_slider;
  if (hold_slider_)
    hold_slider_->addSliderListener(this);
  resetEnvelopeLine();
}

void OpenGLEnvelope::setDecaySlider(SynthSlider* decay_slider) {
  decay_slider_ = decay_slider;
  decay_slider_->addSliderListener(this);
  resetEnvelopeLine();
}

void OpenGLEnvelope::setSustainSlider(SynthSlider* sustain_slider) {
  sustain_slider_ = sustain_slider;
  sustain_slider_->addSliderListener(this);
  resetEnvelopeLine();
}

void OpenGLEnvelope::setReleaseSlider(SynthSlider* release_slider) {
  release_slider_ = release_slider;
  release_slider_->addSliderListener(this);
  resetEnvelopeLine();
}

void OpenGLEnvelope::resetEnvelopeLine() {
  envelope_line_.clear();
  envelope_line_.startNewSubPath(1, getHeight());
  envelope_line_.lineTo(getDelayX(), getHeight());
  envelope_line_.lineTo(getAttackX(), 0.0f);
  envelope_line_.lineTo(getHoldX(), 0.0f);
  envelope_line_.quadraticTo(0.5f * (getHoldX() + getDecayX()), getSustainY(),
                             getDecayX(), getSustainY());

  envelope_line_.quadraticTo(0.5f * (getReleaseX() + getDecayX()), getHeight(),
                             getReleaseX(), getHeight());

  auto geometry = std::make_shared<GeometrySnapshot>();
  geometry->line = envelope_line_;
  geometry->width = static_cast<float>(getWidth());
  geometry->height = static_cast<float>(getHeight());
  geometry->delay_x = getDelayX();
  geometry->attack_x = getAttackX();
  geometry->hold_x = getHoldX();
  geometry->decay_x = getDecayX();
  geometry->release_x = getReleaseX();
  geometry->sustain = sustain_slider_ != nullptr ? static_cast<float>(sustain_slider_->getValue()) : 0.0f;
  geometry_snapshot_.store(std::move(geometry), std::memory_order_release);

  paintBackground();
}

Point<float> OpenGLEnvelope::valuesToPosition(const GeometrySnapshot& geometry,
                                              float phase, float amp,
                                              float progress) {
  float y = (1.0f - amp) * geometry.height;
  float x = 0.0;

  if (phase < 0.0 || phase > static_cast<float>(mopo::Envelope::State::Holding))
    return Point<float>(-2.0, -2.0);

  float delay_x = geometry.delay_x;
  float attack_x = geometry.attack_x;
  float hold_x = geometry.hold_x;
  float decay_x = geometry.decay_x;
  float release_x = geometry.release_x;
  float sustain = geometry.sustain;

  const int stage = static_cast<int>(phase);
  if (stage == static_cast<int>(mopo::Envelope::State::Delaying)) {
    x = 1.0f + progress * (delay_x - 1.0f);
  }
  else if (stage == static_cast<int>(mopo::Envelope::State::Attacking)) {
    x = delay_x + amp * (attack_x - delay_x);
  }
  else if (stage == static_cast<int>(mopo::Envelope::State::Holding)) {
    x = attack_x + progress * (hold_x - attack_x);
  }
  else if (stage == static_cast<int>(mopo::Envelope::State::Decaying)) {
    float amp_diff = 1.0f - sustain;
    if (amp_diff == 0.0f)
      x = decay_x;
    else {
      float percent = (1.0f - amp) / amp_diff;
      x = hold_x + percent * (decay_x - hold_x);
    }
  }
  else {
    float delta = release_x - decay_x;
    x = decay_x + delta - delta * (amp / sustain);
  }

  Point<float> closest;
  geometry.line.getNearestPoint(Point<float>(x, y), closest);
  if (phase > 1.5f && phase < 2.5f && closest.x < decay_x) {
    closest.x = decay_x;
    closest.y = (1.0f - amp) * geometry.height;
  }
  if (phase > 0.5f && phase < 1.5f && closest.x > decay_x) {
    closest.x = decay_x;
    closest.y = (1.0f - amp) * geometry.height;
  }
  return Point<float>(2.0f * closest.x / geometry.width - 1.0f,
                      1.0f - 2.0f * closest.y / geometry.height);
}

void OpenGLEnvelope::init(OpenGLContext& open_gl_context) {
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

void OpenGLEnvelope::drawPosition(OpenGLContext& open_gl_context) {
  open_gl_context.extensions.glActiveTexture(GL_TEXTURE0);
  if (position_texture_.getWidth() != position_image_.getWidth())
    position_texture_.loadImage(position_image_);

  auto geometry = geometry_snapshot_.load(std::memory_order_acquire);
  if (synth_ == nullptr || !geometry || geometry->width <= 0.0f || geometry->height <= 0.0f ||
      telemetry_indices_[0] < 0 || telemetry_indices_[1] < 0 || telemetry_indices_[2] < 0)
    return;

  (void)synth_->copyVisualTelemetry(telemetry_indices_, telemetry_values_);
  if (telemetry_values_[1] <= 0.0f &&
      static_cast<int>(telemetry_values_[0]) !=
          static_cast<int>(mopo::Envelope::State::Delaying))
    return;

  Point<float> point = valuesToPosition(*geometry, telemetry_values_[0],
                                        telemetry_values_[1], telemetry_values_[2]);
  float x = point.x;
  float y = point.y;

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  int draw_width = static_cast<int>(geometry->width);
  int draw_height = static_cast<int>(geometry->height);

  float ratio = geometry->height / 100.0f;

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

void OpenGLEnvelope::render(OpenGLContext& open_gl_context, bool animate) {
  MOPO_ASSERT(glGetError() == GL_NO_ERROR);

  setViewPort(open_gl_context);

  background_.render(open_gl_context);

  if (animate)
    drawPosition(open_gl_context);

  MOPO_ASSERT(glGetError() == GL_NO_ERROR);
}

void OpenGLEnvelope::destroy(OpenGLContext& open_gl_context) {
  position_texture_.release();

  texture_ = nullptr;
  open_gl_context.extensions.glDeleteBuffers(1, &vertex_buffer_);
  open_gl_context.extensions.glDeleteBuffers(1, &triangle_buffer_);
  background_.destroy(open_gl_context);
}
