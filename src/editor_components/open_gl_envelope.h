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
#ifndef OPEN_GL_ENVELOPE_H
#define OPEN_GL_ENVELOPE_H

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>
#include "helmBoy_common.h"
#include "open_gl_background.h"
#include "open_gl_component.h"
#include "synth_slider.h"

class SynthBase;

/**
 * @file open_gl_envelope.h
 * @brief OpenGL-accelerated envelope editor for interactive ADSR visualization.
 */

class OpenGLEnvelope : public OpenGLComponent, public SynthSlider::SliderListener {
  public:
    OpenGLEnvelope();
    ~OpenGLEnvelope();

    /** @brief Réinitialise la ligne d'enveloppe interne. */
    void resetEnvelopeLine();
    void guiChanged(SynthSlider* slider) override;

    void setDelaySlider(SynthSlider* delay_slider);
    void setAttackSlider(SynthSlider* attack_slider);
    void setHoldSlider(SynthSlider* hold_slider);
    void setDecaySlider(SynthSlider* decay_slider);
    void setSustainSlider(SynthSlider* sustain_slider);
    void setReleaseSlider(SynthSlider* release_slider);

    void resized() override;
    void mouseMove(const MouseEvent& e) override;
    void mouseExit(const MouseEvent& e) override;
    void mouseDown(const MouseEvent& e) override;
    void mouseDrag(const MouseEvent& e) override;
    void mouseUp(const MouseEvent& e) override;

    void init(OpenGLContext& open_gl_context) override;
    void render(OpenGLContext& open_gl_context, bool animate = true) override;
    void destroy(OpenGLContext& open_gl_context) override;
    void paintBackground(Graphics& g) override {
      if (background_image_.isValid())
        g.drawImage(background_image_, getLocalBounds().toFloat());
    }

  private:
    void drawPosition(OpenGLContext& open_gl_context);
    void paintPositionImage();
    void paintBackground();
    struct GeometrySnapshot {
      Path line;
      float width = 0.0f;
      float height = 0.0f;
      float delay_x = 0.0f;
      float attack_x = 0.0f;
      float hold_x = 0.0f;
      float decay_x = 0.0f;
      float release_x = 0.0f;
      float sustain = 0.0f;
    };

    Point<float> valuesToPosition(const GeometrySnapshot& geometry,
                    float phase, float amp, float progress);

    float getDelayX();
    float getAttackX();
    float getHoldX();
    float getDecayX();
    float getSustainY();
    float getReleaseX();
    float getReleaseRange();

    void setDelayX(double x);
    void setAttackX(double x);
    void setHoldX(double x);
    void setDecayX(double x);
    void setSustainY(double y);
    void setReleaseX(double x);

    bool delay_hover_;
    bool attack_hover_;
    bool hold_hover_;
    bool decay_hover_;
    bool sustain_hover_;
    bool release_hover_;
    bool mouse_down_;
    Path envelope_line_;

    SynthBase* synth_ = nullptr;
    std::array<int, 3> telemetry_indices_ { -1, -1, -1 };
    std::array<float, 3> telemetry_values_ { 0.0f, 0.0f, 0.0f };
    std::atomic<std::shared_ptr<const GeometrySnapshot>> geometry_snapshot_;

    SynthSlider* delay_slider_;
    SynthSlider* attack_slider_;
    SynthSlider* hold_slider_;
    SynthSlider* decay_slider_;
    SynthSlider* sustain_slider_;
    SynthSlider* release_slider_;

    OpenGLBackground background_;

    Image position_image_;
    Image background_image_;
    OpenGLTexture position_texture_;
    std::unique_ptr<OpenGLShaderProgram::Uniform> texture_;

    float* position_vertices_;
    int* position_triangles_;
    GLuint vertex_buffer_;
    GLuint triangle_buffer_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpenGLEnvelope)
};

#endif // OPEN_GL_ENVELOPE_H
