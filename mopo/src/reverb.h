/* Copyright 2025 Marc Scheffer
 *
 * mopo is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This work is based on bepzi's Helm project, <https://github.com/bepzi/helm>,
 * itself based on Matt Tytel's Helm <https://tytel.org/helm/>
 *
 * mopo is distributed in the hope that it will be useful,

 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with mopo.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#/**
 * @file reverb.h
 * @brief Processeur de réverbération stéréo (comb + all-pass filters).
 */
#ifndef REVERB_H
#define REVERB_H

#include <xsimd/xsimd.hpp>
#include <vector>
#include <memory>
#include "processor_router.h"

#if defined(HELMBOY_ENABLE_REVERB_JUCE_PATH)
#include <juce_audio_basics/juce_audio_basics.h>
#endif

namespace mopo {

  /**
   * @brief Processeur de r�verb�ration st�r�o avec filtrage comb et all-pass.
   */
  class Reverb : public ProcessorRouter {
    public:
      /**
       * @brief Enum�ration des entr�es du processeur de r�verb�ration.
       */
      enum class Inputs : int {
        Audio,
        Feedback,
        Damping,
        StereoWidth,
        Wet,
        FreezeMode,  ///< 0 = normal mode, >=0.5 = infinite decay (freeze). Available for future UI.
        AudioRight,
        NumInputs
      };

      /**
       * @brief Constructeur.
       */
      Reverb();

      /**
       * @brief Constructeur de copie.
       * Copies all state except juce_reverb_ (which is re-initialized lazily on first use).
       */
      Reverb(const Reverb& other);

      /**
       * @brief Destructeur.
       */
      ~Reverb() override = default;

      /**
       * @brief Traite le buffer d'entr�e et applique la r�verb�ration.
       */
      void process() override;

      /**
       * @brief Clone le processeur.
       * @return Un pointeur vers une nouvelle instance clon�e.
       */
      [[nodiscard]] Processor* clone() const override { return new Reverb(*this); }

    protected:
      [[nodiscard]] bool shouldUseJucePath() const;
      void processLegacyPath(const mopo_float* audio,
                             const mopo_float* audio_right,
                             const mopo_float* left_wet_audio,
                             const mopo_float* right_wet_audio,
                             mopo_float* dest_left,
                             mopo_float* dest_right,
                             mopo_float wet_inc,
                             mopo_float dry_inc) const;
      void processJucePath(const mopo_float* audio,
                           const mopo_float* audio_right,
                           mopo_float* dest_left,
                           mopo_float* dest_right,
                           mopo_float wet_in);

      Processor* reverb_wet_left_ = nullptr;
      Processor* reverb_wet_right_ = nullptr;

      mopo_float current_dry_ = 0.0f;
      mopo_float current_wet_ = 0.0f;

#if defined(HELMBOY_ENABLE_REVERB_JUCE_PATH)
      std::unique_ptr<juce::Reverb> juce_reverb_;  ///< JUCE Freeverb engine for the optional JUCE path.
      mopo_float juce_last_sample_rate_ = 0.0;    ///< Last sample rate passed to juce_reverb_->setSampleRate().
      std::vector<float> juce_left_buf_;           ///< double→float conversion buffer, left channel.
      std::vector<float> juce_right_buf_;          ///< double→float conversion buffer, right channel.
#endif
  };
} // namespace mopo

#endif // REVERB_H
