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

#include "volume_section.h"
#include "colors.h"
#include "fonts.h"
#include "synth_slider.h"
#include <algorithm>

/**
 * @file volume_section.cpp
 * @brief Implementation of VolumeSection UI (volume slider + peak meters).
 */

VolumeSection::VolumeSection(String name) : SynthSection(name) {
  addSlider((volume_ = std::make_unique<SynthSlider>("volume")).get());
  addSlider((pan_ = std::make_unique<SynthSlider>("pan")).get());
  addOpenGLComponent((peak_meter_left_ = std::make_unique<OpenGLPeakMeter>(true)).get());
  addOpenGLComponent((peak_meter_right_ = std::make_unique<OpenGLPeakMeter>(false)).get());
  volume_->setSliderStyle(Slider::LinearBar);
  volume_->setPopupPlacement(BubbleComponent::below, 0);
  pan_->setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
}

VolumeSection::~VolumeSection() {
  volume_ = nullptr;
  pan_ = nullptr;
}

void VolumeSection::paintBackground(Graphics& g) {
  SynthSection::paintBackground(g);

  const int title_width = getTitleWidth();
  const int meter_area_height = (getHeight() - title_width) / 2;
  const int meter_gap = size_ratio_ * 2.0f;
  const int meter_height = (meter_area_height - meter_gap) / 2;
  const int label_width = size_ratio_ * 9.0f;
  const int controls_y = title_width + meter_area_height;
  const int controls_height = getHeight() - controls_y;
  const int pan_size = std::min(static_cast<int>(getSmallKnobSize()), controls_height);
  const int controls_gap = size_ratio_ * 4.0f;
  const int pan_label_width = size_ratio_ * 30.0f;
  const int volume_width = getWidth() - pan_size - pan_label_width - 2 * controls_gap;
  const int pan_x = volume_width + controls_gap;

  g.setColour(Colors::control_label_text);
  g.setFont(Fonts::instance()->proportional_regular().withPointHeight(size_ratio_ * 8.0f));
  g.drawText("L", 0, title_width, label_width, meter_height, Justification::centred, false);
  g.drawText("R", 0, title_width + meter_height + meter_gap,
             label_width, meter_height, Justification::centred, false);
  g.drawText("PAN", pan_x + pan_size + controls_gap, controls_y,
             pan_label_width, controls_height, Justification::centred, false);
}

void VolumeSection::resized() {
  int title_width = getTitleWidth();
  int height = getHeight() - title_width;
  int meter_area_height = height / 2;
  int meter_gap = size_ratio_ * 2.0f;
  int meter_height = (meter_area_height - meter_gap) / 2;
  int controls_y = title_width + meter_area_height;
  int controls_height = height - meter_area_height;
  int label_width = size_ratio_ * 9.0f;
  int pan_size = std::min(static_cast<int>(getSmallKnobSize()), controls_height);
  int controls_gap = size_ratio_ * 4.0f;
  int pan_label_width = size_ratio_ * 30.0f;
  int volume_width = getWidth() - pan_size - pan_label_width - 2 * controls_gap;
  int pan_x = volume_width + controls_gap;

  peak_meter_left_->setBounds(label_width, title_width,
                              getWidth() - label_width, meter_height);
  peak_meter_right_->setBounds(label_width, title_width + meter_height + meter_gap,
                               getWidth() - label_width, meter_height);

  volume_->setBounds(0, controls_y, volume_width, controls_height);
  pan_->setBounds(pan_x, controls_y + (controls_height - pan_size) / 2,
                  pan_size, pan_size);

  SynthSection::resized();
}
