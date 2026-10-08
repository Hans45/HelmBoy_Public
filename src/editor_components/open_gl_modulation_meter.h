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

#ifndef OPEN_GL_MODULATION_METER_H
#define OPEN_GL_MODULATION_METER_H

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>
#include "open_gl_component.h"
#include "processor.h"
#include "synth_slider.h"

class SynthBase;

/**
 * @file open_gl_modulation_meter.h
 * @brief OpenGL-accelerated modulation meter for visual feedback.
 */

class OpenGLModulationMeter : public Component {
  public:
  /**
   * @brief Constructeur.
    * @param synth Synthétiseur qui publie la télémétrie de modulation.
   * @param mono_total Source mono de modulation.
   * @param poly_total Source poly de modulation.
   * @param slider Slider lié.
   * @param vertices Vue sur le buffer de vertices OpenGL.
   */
  OpenGLModulationMeter(SynthBase* synth,
              const mopo::Output* mono_total,
              const mopo::Output* poly_total,
              const SynthSlider* slider,
              std::span<float> vertices);
    virtual ~OpenGLModulationMeter();

    void paint(Graphics& g) override;
    void resized() override;
    void setVisible(bool should_be_visible) override;

    /** @brief Met à jour le dessin GPU/vertex data. */
    void updateDrawing();
    void updateGeometry();

    bool isRenderVisible() const { return render_visible_.load(std::memory_order_acquire); }
    void setModulated(bool modulated) { modulated_.store(modulated, std::memory_order_release); }
    bool isModulated() const { return modulated_.load(std::memory_order_acquire); }
    bool copyModulatedValue(float& value) const;

  private:
    struct GeometrySnapshot {
      float minimum = 0.0f;
      float maximum = 1.0f;
      float knob_percent = 0.0f;
      float left = 0.0f;
      float right = 0.0f;
      float top = 0.0f;
      float bottom = 0.0f;
      bool visible = false;
      bool rotary = false;
      bool horizontal = false;
      bool text_style = false;
    };

    void setVertices();
    void collapseVertices();
    void collapseRenderVertices();

    SynthBase* synth_;
    const SynthSlider* destination_;
    std::span<float> vertices_;
    std::array<int, 2> telemetry_indices_ { -1, -1 };
    std::array<float, 2> telemetry_values_ { 0.0f, 0.0f };
    int telemetry_count_ = 0;
    std::atomic<std::shared_ptr<const GeometrySnapshot>> geometry_;
    std::shared_ptr<const GeometrySnapshot> last_geometry_;
    std::atomic<bool> render_visible_ { false };
    std::atomic<bool> modulated_ { false };

    double current_value_;
    double knob_percent_;
    double mod_percent_;
    bool rotary_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGLModulationMeter)
};

#endif // OPEN_GL_MODULATION_METER_H
