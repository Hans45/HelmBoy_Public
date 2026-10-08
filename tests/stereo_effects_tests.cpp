#include "chorus.h"
#include "delay.h"
#include "limiter.h"
#include "reverb.h"
#include "stereo_balance.h"
#include "bypass_router.h"
#include "value.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>

namespace {
  int failures = 0;

  void expect(bool condition, const char* description) {
    if (!condition) {
      std::cerr << "FAIL: " << description << '\n';
      ++failures;
    }
  }

  struct ChorusFixture {
    mopo::Chorus chorus;
    mopo::Output audio;
    mopo::cr::Value on { 0.0f };
    mopo::cr::Value rate { 0.8f };
    mopo::cr::Value depth { 0.25f };
    mopo::cr::Value mix { 0.5f };
    mopo::cr::Value feedback { 0.0f };
    mopo::cr::Value delay { 8.0f };
    mopo::cr::Value width { 1.0f };
    std::array<mopo::Value*, 7> controls { &on, &rate, &depth, &mix, &feedback, &delay, &width };

    ChorusFixture() {
      chorus.setBufferSize(64);
      chorus.plug(&audio, 0);
      for (size_t index = 0; index < controls.size(); ++index)
        chorus.plug(controls[index], static_cast<unsigned int>(index + 1));
    }

    ~ChorusFixture() {
      chorus.destroy();
      for (auto* control : controls)
        control->destroy();
    }

    void processBlocks(int count) {
      for (int block = 0; block < count; ++block) {
        for (int sample = 0; sample < chorus.getBufferSize(); ++sample)
          audio.buffer[sample] = 0.2f * std::sin((block * chorus.getBufferSize() + sample) * 0.07f);
        chorus.process();
      }
    }
  };

  bool channelsEqual(const mopo::Processor& processor) {
    for (int sample = 0; sample < processor.getBufferSize(); ++sample)
      if (processor.output(0)->buffer[sample] != processor.output(1)->buffer[sample])
        return false;
    return true;
  }

  void testChorus() {
    ChorusFixture fixture;
    fixture.processBlocks(1);
    expect(channelsEqual(fixture.chorus), "disabled chorus has identical channels");
    expect(std::equal(fixture.audio.buffer, fixture.audio.buffer + 64, fixture.chorus.output()->buffer),
           "disabled chorus is exact dry passthrough");
    fixture.on.set(1.0f);
    fixture.processBlocks(200);
    expect(!channelsEqual(fixture.chorus), "chorus produces stereo from mono input");
    fixture.width.set(0.0f);
    fixture.processBlocks(200);
    expect(channelsEqual(fixture.chorus), "zero chorus width is mono after smoothing");
    fixture.on.set(0.0f);
    fixture.processBlocks(200);
    expect(std::equal(fixture.audio.buffer, fixture.audio.buffer + 64, fixture.chorus.output()->buffer),
           "chorus returns to exact dry after bypass fade");
    fixture.on.set(1.0f);
    for (auto* control : fixture.controls)
      if (control != &fixture.on)
        control->set(std::numeric_limits<float>::quiet_NaN());
    fixture.audio.buffer[0] = std::numeric_limits<float>::infinity();
    fixture.chorus.process();
    bool finite = true;
    for (int channel = 0; channel < 2; ++channel)
      for (int sample = 0; sample < 64; ++sample)
        finite = finite && std::isfinite(fixture.chorus.output(channel)->buffer[sample]);
    expect(finite, "chorus handles non-finite controls and audio");
    for (const int sample_rate : { 44100, 48000, 96000, 192000 }) {
      fixture.chorus.setSampleRate(sample_rate);
      fixture.delay.set(100.0f);
      fixture.depth.set(1.0f);
      fixture.feedback.set(0.95f);
      fixture.processBlocks(100);
      expect(std::isfinite(fixture.chorus.output(0)->buffer[63]) &&
             std::isfinite(fixture.chorus.output(1)->buffer[63]), "chorus supports sample-rate changes and limits");
    }
  }

  void testStereoBypass() {
    mopo::Output left;
    mopo::Output right;
    mopo::cr::Value off(0.0f);
    mopo::BypassRouter bypass;
    bypass.setBufferSize(64);
    bypass.plug(&left, static_cast<int>(mopo::BypassRouter::Inputs::Audio));
    bypass.plug(&right, static_cast<int>(mopo::BypassRouter::Inputs::AudioRight));
    bypass.plug(&off, static_cast<int>(mopo::BypassRouter::Inputs::On));
    auto* outputs = new mopo::Delay(1024);
    bypass.addProcessor(outputs);
    bypass.registerOutput(outputs->output(0));
    bypass.registerOutput(outputs->output(1));
    std::fill_n(left.buffer, 64, 0.25f);
    std::fill_n(right.buffer, 64, -0.5f);
    bypass.process();
    expect(bypass.output(0)->buffer[63] == 0.25f && bypass.output(1)->buffer[63] == -0.5f,
           "stereo bypass preserves each input separately");
    bypass.destroy();
    off.destroy();
  }

  void testStereoDelay() {
    mopo::Delay delay(4096);
    mopo::Output left;
    mopo::Output right;
    mopo::cr::Value wet(1.0f);
    mopo::cr::Value period(16.0f);
    mopo::cr::Value feedback(0.5f);
    delay.setBufferSize(64);
    delay.plug(&left, static_cast<int>(mopo::Delay::Inputs::Audio));
    delay.plug(&right, static_cast<int>(mopo::Delay::Inputs::AudioRight));
    delay.plug(&wet, static_cast<int>(mopo::Delay::Inputs::Wet));
    delay.plug(&period, static_cast<int>(mopo::Delay::Inputs::SampleDelay));
    delay.plug(&feedback, static_cast<int>(mopo::Delay::Inputs::Feedback));
    delay.process();
    left.buffer[0] = 1.0f;
    delay.process();
    float left_energy = 0.0f;
    float right_energy = 0.0f;
    for (int sample = 0; sample < 64; ++sample) {
      left_energy += std::abs(delay.output(0)->buffer[sample]);
      right_energy += std::abs(delay.output(1)->buffer[sample]);
    }
    expect(left_energy > 0.0f && right_energy == 0.0f, "stereo delay and feedback do not leak left into right");
    delay.destroy();
    wet.destroy();
    period.destroy();
    feedback.destroy();
  }

  void testDelayPingPongBouncesBetweenChannels() {
    mopo::Delay delay(4096);
    mopo::Output left;
    mopo::Output right;
    mopo::cr::Value wet(1.0f);
    mopo::cr::Value period(16.0f);
    mopo::cr::Value feedback(0.5f);
    mopo::cr::Value ping_pong(1.0f);
    delay.setSampleRate(48000);
    delay.setBufferSize(64);
    delay.plug(&left, static_cast<int>(mopo::Delay::Inputs::Audio));
    delay.plug(&right, static_cast<int>(mopo::Delay::Inputs::AudioRight));
    delay.plug(&wet, static_cast<int>(mopo::Delay::Inputs::Wet));
    delay.plug(&period, static_cast<int>(mopo::Delay::Inputs::SampleDelay));
    delay.plug(&feedback, static_cast<int>(mopo::Delay::Inputs::Feedback));
    delay.plug(&ping_pong, static_cast<int>(mopo::Delay::Inputs::PingPong));

    for (int block = 0; block < 4; ++block) {
      std::fill_n(left.buffer, 64, 0.0);
      std::fill_n(right.buffer, 64, 0.0);
      delay.process();
    }

    std::fill_n(left.buffer, 64, 0.0);
    std::fill_n(right.buffer, 64, 0.0);
    left.buffer[0] = 1.0;
    delay.process();

    int first_left_echo = -1;
    int first_right_echo = -1;
    float left_energy = 0.0f;
    float right_energy = 0.0f;
    for (int sample = 0; sample < 64; ++sample) {
      const float left_sample = static_cast<float>(delay.output(0)->buffer[sample]);
      const float right_sample = static_cast<float>(delay.output(1)->buffer[sample]);
      left_energy += std::abs(left_sample);
      right_energy += std::abs(right_sample);
      if (first_left_echo < 0 && std::abs(left_sample) > 0.01f)
        first_left_echo = sample;
      if (first_right_echo < 0 && std::abs(right_sample) > 0.01f)
        first_right_echo = sample;
    }

    expect(first_right_echo >= 0 && first_left_echo > first_right_echo,
           "ping-pong sends a left impulse right before it bounces left");
    expect(left_energy > 0.0f && right_energy > 0.0f,
           "ping-pong feedback produces echoes on both channels");

    delay.destroy();
    wet.destroy();
    period.destroy();
    feedback.destroy();
    ping_pong.destroy();
  }

  void testStereoReverbDry() {
    mopo::Reverb reverb;
    mopo::Output left;
    mopo::Output right;
    mopo::cr::Value feedback(0.7f);
    mopo::cr::Value damping(0.5f);
    mopo::cr::Value width(1.0f);
    mopo::cr::Value wet(0.0f);
    mopo::cr::Value freeze(0.0f);
    reverb.setBufferSize(64);
    reverb.plug(&left, static_cast<int>(mopo::Reverb::Inputs::Audio));
    reverb.plug(&right, static_cast<int>(mopo::Reverb::Inputs::AudioRight));
    reverb.plug(&feedback, static_cast<int>(mopo::Reverb::Inputs::Feedback));
    reverb.plug(&damping, static_cast<int>(mopo::Reverb::Inputs::Damping));
    reverb.plug(&width, static_cast<int>(mopo::Reverb::Inputs::StereoWidth));
    reverb.plug(&wet, static_cast<int>(mopo::Reverb::Inputs::Wet));
    reverb.plug(&freeze, static_cast<int>(mopo::Reverb::Inputs::FreezeMode));
    std::fill_n(left.buffer, 64, 0.25f);
    std::fill_n(right.buffer, 64, -0.5f);
    for (int block = 0; block < 100; ++block)
      reverb.process();
    expect(std::abs(reverb.output(0)->buffer[63] - 0.25f) < 1.0e-5f &&
           std::abs(reverb.output(1)->buffer[63] + 0.5f) < 1.0e-5f, "reverb dry mix retains stereo input");
    reverb.destroy();
    for (auto* control : std::array<mopo::Value*, 5> { &feedback, &damping, &width, &wet, &freeze })
      control->destroy();
  }

#if !defined(HELMBOY_ENABLE_REVERB_JUCE_PATH)
  void testLegacyReverbKeepsDifferentialStereoExcitation() {
    mopo::Reverb reverb;
    mopo::Output left;
    mopo::Output right;
    mopo::cr::Value feedback(0.7f);
    mopo::cr::Value damping(0.5f);
    mopo::cr::Value width(1.0f);
    mopo::cr::Value wet(1.0f);
    mopo::cr::Value freeze(0.0f);
    reverb.setSampleRate(44100);
    reverb.setBufferSize(64);
    reverb.plug(&left, static_cast<int>(mopo::Reverb::Inputs::Audio));
    reverb.plug(&right, static_cast<int>(mopo::Reverb::Inputs::AudioRight));
    reverb.plug(&feedback, static_cast<int>(mopo::Reverb::Inputs::Feedback));
    reverb.plug(&damping, static_cast<int>(mopo::Reverb::Inputs::Damping));
    reverb.plug(&width, static_cast<int>(mopo::Reverb::Inputs::StereoWidth));
    reverb.plug(&wet, static_cast<int>(mopo::Reverb::Inputs::Wet));
    reverb.plug(&freeze, static_cast<int>(mopo::Reverb::Inputs::FreezeMode));

    float left_energy = 0.0f;
    float right_energy = 0.0f;
    for (int block = 0; block < 200; ++block) {
      std::fill_n(left.buffer, 64, 0.25f);
      std::fill_n(right.buffer, 64, -0.25f);
      reverb.process();
      for (int sample = 0; sample < 64; ++sample) {
        left_energy += std::abs(static_cast<float>(reverb.output(0)->buffer[sample]));
        right_energy += std::abs(static_cast<float>(reverb.output(1)->buffer[sample]));
      }
    }

    expect(left_energy > 0.01f && right_energy > 0.01f,
           "legacy reverb retains wet energy from opposite-phase stereo inputs");
    reverb.destroy();
    for (auto* control : std::array<mopo::Value*, 5> { &feedback, &damping, &width, &wet, &freeze })
      control->destroy();
  }
#endif

  void testLimiterCeilingReleaseAndBypass() {
    mopo::Limiter limiter;
    mopo::Output left;
    mopo::Output right;
    mopo::cr::Value on(1.0f);
    mopo::cr::Value ceiling(-6.0f);
    mopo::cr::Value release(100.0f);
    limiter.setSampleRate(48000);
    limiter.setBufferSize(64);
    limiter.plug(&left, static_cast<int>(mopo::Limiter::Inputs::AudioLeft));
    limiter.plug(&right, static_cast<int>(mopo::Limiter::Inputs::AudioRight));
    limiter.plug(&on, static_cast<int>(mopo::Limiter::Inputs::On));
    limiter.plug(&ceiling, static_cast<int>(mopo::Limiter::Inputs::Ceiling));
    limiter.plug(&release, static_cast<int>(mopo::Limiter::Inputs::Release));

    std::fill_n(left.buffer, 64, 4.0f);
    std::fill_n(right.buffer, 64, -2.0f);
    limiter.process();
    const float ceiling_linear = std::pow(10.0f, -6.0f / 20.0f);
    float first_block_peak = 0.0f;
    for (int sample = 0; sample < 64; ++sample) {
      first_block_peak = std::max(first_block_peak, std::abs(limiter.output(0)->buffer[sample]));
      first_block_peak = std::max(first_block_peak, std::abs(limiter.output(1)->buffer[sample]));
    }
    expect(first_block_peak <= ceiling_linear, "linked limiter output stays below configured ceiling");

    std::fill_n(left.buffer, 64, 0.5f);
    std::fill_n(right.buffer, 64, -0.25f);
    limiter.process();
    expect(limiter.output(0)->buffer[63] > limiter.output(0)->buffer[0],
           "limiter gain recovers progressively over the release interval");
    expect(limiter.output(0)->buffer[63] < ceiling_linear,
           "release recovery remains below the configured ceiling");

    on.set(0.0f);
    std::fill_n(left.buffer, 64, 1.25f);
    std::fill_n(right.buffer, 64, -0.75f);
    limiter.process();
    expect(limiter.output(0)->buffer[0] == left.buffer[0] &&
           limiter.output(1)->buffer[0] == right.buffer[0],
           "limiter bypass preserves both channels exactly");

    limiter.destroy();
    on.destroy();
    ceiling.destroy();
    release.destroy();
  }

  void testStereoBalanceCenterAndExtremes() {
    mopo::StereoBalance balance;
    mopo::Output left;
    mopo::Output right;
    mopo::Output pan;
    balance.setBufferSize(64);
    balance.plug(&left, static_cast<int>(mopo::StereoBalance::Inputs::AudioLeft));
    balance.plug(&right, static_cast<int>(mopo::StereoBalance::Inputs::AudioRight));
    balance.plug(&pan, static_cast<int>(mopo::StereoBalance::Inputs::Pan));
    std::fill_n(left.buffer, 64, 0.4);
    std::fill_n(right.buffer, 64, -0.6);

    std::fill_n(pan.buffer, 64, 0.0);
    balance.process();
    expect(balance.output(static_cast<int>(mopo::StereoBalance::Outputs::Left))->buffer[63] == 0.4 &&
           balance.output(static_cast<int>(mopo::StereoBalance::Outputs::Right))->buffer[63] == -0.6,
           "center pan preserves both stereo channels");

    std::fill_n(pan.buffer, 64, -1.0);
    balance.process();
    expect(balance.output(static_cast<int>(mopo::StereoBalance::Outputs::Left))->buffer[63] == 0.4 &&
           balance.output(static_cast<int>(mopo::StereoBalance::Outputs::Right))->buffer[63] == 0.0,
           "hard-left pan attenuates only the right channel");

    std::fill_n(pan.buffer, 64, 1.0);
    balance.process();
    expect(balance.output(static_cast<int>(mopo::StereoBalance::Outputs::Left))->buffer[63] == 0.0 &&
           balance.output(static_cast<int>(mopo::StereoBalance::Outputs::Right))->buffer[63] == -0.6,
           "hard-right pan attenuates only the left channel");

    std::fill_n(pan.buffer, 64, 0.0);
    balance.process();
    for (auto sample = 0; sample < 64; ++sample) {
      left.buffer[sample] = 0.4;
      right.buffer[sample] = -0.6;
      pan.buffer[sample] = sample < 32 ? -0.5 : 0.5;
    }
    balance.process();
    expect(std::abs(balance.output(static_cast<int>(mopo::StereoBalance::Outputs::Right))->buffer[0] + 0.3) < 1.0e-9 &&
           std::abs(balance.output(static_cast<int>(mopo::StereoBalance::Outputs::Left))->buffer[63] - 0.2) < 1.0e-9,
           "intermediate pan applies a linear opposing-channel balance");

    balance.destroy();
  }
}

int main() {
  testChorus();
  testStereoBypass();
  testStereoDelay();
  testDelayPingPongBouncesBetweenChannels();
  testStereoReverbDry();
#if !defined(HELMBOY_ENABLE_REVERB_JUCE_PATH)
  testLegacyReverbKeepsDifferentialStereoExcitation();
#endif
  testLimiterCeilingReleaseAndBypass();
  testStereoBalanceCenterAndExtremes();
  if (failures == 0)
    std::cout << "All stereo effects tests passed\n";
  return failures == 0 ? 0 : 1;
}