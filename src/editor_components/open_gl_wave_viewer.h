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

#pragma once
#ifndef OPEN_GL_WAVE_VIEWER_H
#define OPEN_GL_WAVE_VIEWER_H

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>

#include "helmBoy_common.h"
#include "open_gl_background.h"
#include "open_gl_component.h"
#include "synth_slider.h"

class SynthBase;

class OpenGLWaveViewer : public OpenGLComponent, public SynthSlider::SliderListener,
                         private Timer {
  public:
    OpenGLWaveViewer(int resolution);
    virtual ~OpenGLWaveViewer();

    void setWaveSlider(SynthSlider* slider);
    void setAmplitudeSlider(SynthSlider* slider);
    void setPhaseStretchSlider(SynthSlider* slider);
    void drawRandom();
    void drawSmoothRandom();
    void resetWavePath();
    void guiChanged(SynthSlider* slider) override;

    void mouseDown(const MouseEvent& e) override;
    void resized() override;

    void init(OpenGLContext& open_gl_context) override;
    void render(OpenGLContext& open_gl_context, bool animate = true) override;
    void destroy(OpenGLContext& open_gl_context) override;
    void paintBackground(Graphics& g) override {
      if (background_image_.isValid())
        g.drawImage(background_image_, getLocalBounds().toFloat());
    }

  private:
    struct RenderGeometry {
      float width = 0.0f;
      float height = 0.0f;
      float padding = 0.0f;
    };

    void drawPosition(OpenGLContext& open_gl_context);
    void paintPositionImage();
    void paintBackground();
    void publishRenderGeometry();
    float phaseToX(float phase);
    float getRatio();
    void timerCallback() override;
    float getPhaseStretch() const;
    float displayed_phase_stretch_ = 0.5f;
    float modulated_phase_stretch_ = 0.5f;
    bool has_modulated_phase_stretch_ = false;

    SynthSlider* wave_slider_;
    SynthSlider* amplitude_slider_;
    SynthSlider* phase_stretch_slider_;
    SynthBase* synth_ = nullptr;
    std::array<int, 2> telemetry_indices_ { -1, -1 };
    std::array<float, 2> telemetry_values_ { 0.0f, 0.0f };
    std::atomic<std::shared_ptr<const RenderGeometry>> render_geometry_;
    Path wave_path_;
    int resolution_;

    OpenGLBackground background_;

    Image position_image_;
    Image background_image_;
    OpenGLTexture position_texture_;
    std::unique_ptr<OpenGLShaderProgram::Uniform> texture_;

    float* position_vertices_;
    int* position_triangles_;
    GLuint vertex_buffer_;
    GLuint triangle_buffer_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGLWaveViewer)
};

#endif // OPEN_GL_WAVE_VIEWER_H
