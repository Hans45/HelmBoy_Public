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

#include "delay_section.h"

#include "colors.h"
#include "fonts.h"
#include "synth_button.h"
#include "tempo_selector.h"
#include "text_look_and_feel.h"

#define KNOB_WIDTH 40
#define TEXT_WIDTH 42
#define TEXT_HEIGHT 16

/**
 * @file delay_section.cpp
 * @brief Implementation of the DelaySection UI.
 */

DelaySection::DelaySection(String name) : SynthSection(name) {
  static const int TEMPO_DRAG_SENSITIVITY = 150;
  addSlider((frequency_ = std::make_unique<SynthSlider>("delay_frequency")).get());
  frequency_->setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
  frequency_->setLookAndFeel(TextLookAndFeel::instance());

  addSlider((tempo_ = std::make_unique<SynthSlider>("delay_tempo")).get());
  tempo_->setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
  tempo_->setStringLookup(mopo::strings::synced_frequencies);
  tempo_->setLookAndFeel(TextLookAndFeel::instance());
  tempo_->setMouseDragSensitivity(TEMPO_DRAG_SENSITIVITY);

  addSlider((sync_ = std::make_unique<TempoSelector>("delay_sync")).get());
  sync_->setSliderStyle(Slider::LinearBar);
  sync_->setTempoSlider(tempo_.get());
  sync_->setFreeSlider(frequency_.get());
  sync_->setStringLookup(mopo::strings::freq_sync_styles);

  addSlider((feedback_ = std::make_unique<SynthSlider>("delay_feedback")).get());
  feedback_->setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
  feedback_->setBipolar();

  addSlider((dry_wet_ = std::make_unique<SynthSlider>("delay_dry_wet")).get());
  dry_wet_->setSliderStyle(Slider::RotaryHorizontalVerticalDrag);

  addButton((on_ = std::make_unique<SynthButton>("delay_on")).get());
  setActivator(on_.get());

  addButton((ping_pong_ = std::make_unique<SynthButton>("delay_ping_pong")).get());
  ping_pong_->setLookAndFeel(TextLookAndFeel::instance());
  ping_pong_->setButtonText("");
  ping_pong_->setTooltip(TRANS("Ping-pong delay feedback"));
}

DelaySection::~DelaySection() {
  on_ = nullptr;
  ping_pong_ = nullptr;
  frequency_ = nullptr;
  tempo_ = nullptr;
  sync_ = nullptr;
  feedback_ = nullptr;
  dry_wet_ = nullptr;
}

void DelaySection::paintBackground(Graphics& g) {
  SynthSection::paintBackground(g);

  int text_height = size_ratio_ * TEXT_HEIGHT;

  g.setColour(Colors::control_label_text);
  g.setFont(Fonts::instance()->proportional_regular().withPointHeight(size_ratio_ * 8.0f));

  g.drawFittedText(TRANS("FEEDB"), feedback_->getX(), feedback_->getBottom() + size_ratio_ * 2.0f,
                   feedback_->getWidth(), size_ratio_ * 10.0f, Justification::centred, 1, 0.65f);
  g.drawFittedText(TRANS("MIX"), dry_wet_->getX(), dry_wet_->getBottom() + size_ratio_ * 2.0f,
                   dry_wet_->getWidth(), size_ratio_ * 10.0f, Justification::centred, 1, 0.65f);
  const auto ping_pong_bounds = ping_pong_->getBounds();
  g.drawFittedText(TRANS("PING PONG"),
                   ping_pong_bounds.getRight() + size_ratio_ * 3.0f,
                   ping_pong_bounds.getY(),
                   getWidth() - ping_pong_bounds.getRight() - size_ratio_ * 3.0f,
                   ping_pong_bounds.getHeight(), Justification::centredLeft, 1, 0.65f);
  g.drawText(TRANS("FREQUENCY"),
             frequency_->getBounds().getX() - size_ratio_ * 5.0f,
             size_ratio_ * 30 + getStandardKnobSize() + size_ratio_ * 4,
             frequency_->getBounds().getWidth() + text_height + size_ratio_ * 10,
             size_ratio_ * 10.0f, Justification::centred, false);
}

void DelaySection::resized() {
  int title_width = getTitleWidth();
  on_->setBounds(size_ratio_ * 2.0f, 0, title_width, title_width);
  int standard_knob_width = getStandardKnobSize();
  int knob_width = getSmallKnobSize();
  int knob_offset = (standard_knob_width - knob_width) / 2;
  int text_width = size_ratio_ * TEXT_WIDTH;
  int text_height = size_ratio_ * TEXT_HEIGHT;

  float space = (getWidth() - (2.0f * standard_knob_width) - text_width - text_height) / 4.0f;
  int label_height = size_ratio_ * 10.0f;
  int knob_y = getHeight() - knob_width - label_height - size_ratio_ * 3.0f;
  int text_y = size_ratio_ * 44;

  ping_pong_->setBounds(space,
                        title_width, title_width, title_width);
  frequency_->setBounds(space, text_y, text_width, text_height);
  sync_->setBounds(space + text_width, text_y, text_height, text_height);
  tempo_->setBounds(frequency_->getBounds());
  feedback_->setBounds(text_width + text_height + 2 * space + knob_offset, knob_y, knob_width, knob_width);
  dry_wet_->setBounds(text_width + text_height + standard_knob_width + 3 * space + knob_offset, knob_y,
                      knob_width, knob_width);

  SynthSection::resized();

  frequency_->setPopupDisplayEnabled(false, false, nullptr);
  tempo_->setPopupDisplayEnabled(false, false, nullptr);
}
