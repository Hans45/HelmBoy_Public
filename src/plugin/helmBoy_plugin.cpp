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

#include "helmBoy_plugin.h"
#include "helmBoy_common.h"
#include "helmBoy_editor.h"
#include "load_save.h"
#include "startup_trace.h"
#include <cmath>
#include <cstdio>
#include <optional>

#define appendPluginHostTrace(message) HELMBOY_STARTUP_TRACE("plugin_host_trace.log", message)

#define PITCH_WHEEL_RESOLUTION 0x3fff
#define MAX_BUFFER_PROCESS 256
#define SET_PROGRAM_WAIT_MILLISECONDS 500

HelmPlugin::HelmPlugin() :
    AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)) {
  appendPluginHostTrace("HelmPlugin::HelmPlugin begin");
  set_state_time_ = 0;

  current_program_ = 0;

  appendPluginHostTrace("HelmPlugin::HelmPlugin loadPatches start");
  loadPatches();
  appendPluginHostTrace("HelmPlugin::HelmPlugin loadPatches done");

  appendPluginHostTrace("HelmPlugin::HelmPlugin parameters init start");
  const auto add_parameter_bridge = [this](const std::string& name, mopo::Value* control) {
    ValueBridge* bridge = new ValueBridge(name, control);
    bridge->setListener(this);
    bridge_lookup_[name] = bridge;
    addParameter(bridge);
  };

  for (const auto& [name, control] : controls_) {
    if (name == "limiter_ceiling" || name == "limiter_release" || name == "pan" ||
      name == "delay_ping_pong")
      continue;
    add_parameter_bridge(name, control);
  }
  add_parameter_bridge("limiter_ceiling", controls_.at("limiter_ceiling"));
  add_parameter_bridge("limiter_release", controls_.at("limiter_release"));
  add_parameter_bridge("pan", controls_.at("pan"));
  add_parameter_bridge("delay_ping_pong", controls_.at("delay_ping_pong"));
  appendPluginHostTrace("HelmPlugin::HelmPlugin parameters init done");

  // Initialize MIDI 2.0 manager with high-resolution callbacks
  appendPluginHostTrace("HelmPlugin::HelmPlugin midi2 manager start");
  midi2_manager_ = std::make_unique<helmboy::Midi2Manager>();

  // Charger la configuration MIDI 2.0 sauvegardée
  bool enable_midi2 = LoadSave::shouldEnableMidi2();
  midi2_manager_->setMidiVersion(
    enable_midi2 ? helmboy::Midi2Manager::MidiVersion::MIDI_2_0
                 : helmboy::Midi2Manager::MidiVersion::MIDI_1_0
  );

  // Connect MIDI 2.0 callbacks to synthesis engine
  midi2_manager_->onNoteOn = [this](int channel, int note, float velocity) {
    engine_.noteOn(note, velocity, 0, channel);
  };

  midi2_manager_->onNoteOff = [this](int channel, int note, float velocity) {
    engine_.noteOff(note, 0);
  };

  midi2_manager_->onControlChange = [this](int channel, int cc, float value) {
    // Route CC to MIDI manager (converts back to 7-bit for existing system)
    juce::MidiMessage msg = juce::MidiMessage::controllerEvent(channel + 1, cc, static_cast<int>(value * 127.0f));
    midi_manager_->processMidiMessage(msg, 0);
  };

  midi2_manager_->onPitchBend = [this](int channel, float value) {
    // Convert -1.0 to +1.0 range to 0-16383 MIDI pitch bend range
    int midi_value = static_cast<int>((value + 1.0f) * 8192.0f);
    juce::MidiMessage msg = juce::MidiMessage::pitchWheel(channel + 1, midi_value);
    midi_manager_->processMidiMessage(msg, 0);
  };

  appendPluginHostTrace("HelmPlugin::HelmPlugin end");
}

HelmPlugin::~HelmPlugin() {
  stopGuiUpdates();
  appendPluginHostTrace("HelmPlugin::~HelmPlugin begin");
  midi_manager_ = nullptr;
  keyboard_state_ = nullptr;
  midi2_manager_ = nullptr;
  appendPluginHostTrace("HelmPlugin::~HelmPlugin end");
}

std::optional<SynthGuiInterface*> HelmPlugin::getGuiInterface() {
  AudioProcessorEditor* editor = getActiveEditor();
  if (editor) [[likely]] {
    if (auto* gui_interface = dynamic_cast<SynthGuiInterface*>(editor)) [[likely]]
      return gui_interface;
  }
  return std::nullopt;
}

void HelmPlugin::beginChangeGesture(std::string_view name) {
  std::string name_str{name}; // Convert to string for map key
  bridge_lookup_[name_str]->beginChangeGesture();
}

void HelmPlugin::endChangeGesture(std::string_view name) {
  std::string name_str{name}; // Convert to string for map key
  bridge_lookup_[name_str]->endChangeGesture();
}

void HelmPlugin::setValueNotifyHost(std::string_view name, mopo::mopo_float value) {
  std::string name_str{name}; // Convert to string for map key
  mopo::mopo_float plugin_value =  bridge_lookup_[name_str]->convertToPluginValue(value);
  bridge_lookup_[name_str]->setValueNotifyHost(plugin_value);
}

const CriticalSection& HelmPlugin::getCriticalSection() {
  return getCallbackLock();
}

const String HelmPlugin::getName() const {
  return JucePlugin_Name;
}

const String HelmPlugin::getInputChannelName(int channel_index) const {
  return String(channel_index + 1);
}

const String HelmPlugin::getOutputChannelName(int channel_index) const {
  return String(channel_index + 1);
}

bool HelmPlugin::isInputChannelStereoPair(int index) const {
  return true;
}

bool HelmPlugin::isOutputChannelStereoPair(int index) const {
  return true;
}

bool HelmPlugin::acceptsMidi() const {
#if JucePlugin_WantsMidiInput
  return true;
#else
  return false;
#endif
}

bool HelmPlugin::producesMidi() const {
#if JucePlugin_ProducesMidiOutput
  return true;
#else
  return false;
#endif
}

bool HelmPlugin::silenceInProducesSilenceOut() const {
  return false;
}

double HelmPlugin::getTailLengthSeconds() const {
  return 0.0;
}

int HelmPlugin::getNumPrograms() {
  const ScopedLock lock(getCallbackLock());
  return std::max(1, all_patches_.size());
}

int HelmPlugin::getCurrentProgram() {
  const ScopedLock lock(getCallbackLock());
  return current_program_;
}

void HelmPlugin::setCurrentProgram(int index) {
  // Hack for some DAWs that set program on load for VSTs.
  if (Time::getMillisecondCounter() - set_state_time_.load(std::memory_order_acquire) < SET_PROGRAM_WAIT_MILLISECONDS) [[unlikely]]
    return;

  File patch;
  {
    const ScopedLock lock(getCallbackLock());
    if (index < 0 || index >= all_patches_.size())
      return;
    patch = all_patches_[index];
  }

  var state;
  if (JSON::parse(patch.loadFileAsString(), state).failed())
    return;
  LoadSave::PreparedSynthState prepared = LoadSave::prepareState(std::move(state));
  if (!prepared.valid)
    return;

  {
    const ScopedLock lock(getCallbackLock());
    if (index >= all_patches_.size() || all_patches_[index] != patch)
      return;
    processControlChanges();
    processModulationChanges();
    current_program_ = index;
    LoadSave::applyPreparedState(this, save_info_, prepared);
  }
  requestGuiRefresh(true);
}

const String HelmPlugin::getProgramName(int index) {
  const ScopedLock lock(getCallbackLock());
  if (index < 0 || index >= all_patches_.size())
    return "";

  return all_patches_[index].getFileNameWithoutExtension();
}

void HelmPlugin::changeProgramName(int index, const String& new_name) {
  File patch;
  {
    const ScopedLock lock(getCallbackLock());
    if (index < 0 || index >= all_patches_.size())
      return;
    patch = all_patches_[index];
  }
  File new_patch_location = patch.getParentDirectory().getChildFile(new_name + "." + mopo::PATCH_EXTENSION);
  if (patch.moveFileTo(new_patch_location)) {
    const ScopedLock lock(getCallbackLock());
    if (index < all_patches_.size() && all_patches_[index] == patch)
      all_patches_.set(index, new_patch_location);
  }
}

void HelmPlugin::prepareToPlay(double sample_rate, int buffer_size) {
  engine_.setSampleRate(sample_rate);
  engine_.setBufferSize(std::min<int>(buffer_size, MAX_BUFFER_PROCESS));
  midi_manager_->setSampleRate(sample_rate);
}

void HelmPlugin::releaseResources() {
  // When playback stops, you can use this as an opportunity to free up any
  // spare memory, etc.
}

void HelmPlugin::processBlock(AudioSampleBuffer& buffer, MidiBuffer& midi_messages) {
  int total_samples = buffer.getNumSamples();
  int num_channels = getTotalNumOutputChannels();
  position_info_.resetToDefault();
  auto* play_head = getPlayHead();
  const bool has_position = play_head != nullptr && play_head->getCurrentPosition(position_info_);
  if (has_position && std::isfinite(position_info_.bpm) && position_info_.bpm > 0.0)
    engine_.setBpm(position_info_.bpm);

  if (has_position && (position_info_.isPlaying || position_info_.isLooping || position_info_.isRecording))
    engine_.correctToTime(position_info_.timeInSamples);

  processControlChanges();
  processModulationChanges();

  MidiBuffer keyboard_messages = midi_messages;
  processKeyboardEvents(keyboard_messages, total_samples);

  for (auto sample_offset = 0; sample_offset < total_samples;) {
    auto num_samples = std::min<int>(total_samples - sample_offset, MAX_BUFFER_PROCESS);

    processMidi(midi_messages, sample_offset, sample_offset + num_samples);

    processAudioSafe(&buffer, num_channels, num_samples, sample_offset);

    sample_offset += num_samples;
  }

#if HELMBOY_DEBUG_BUFFER_INTEGRITY
  float peak = 0.0f;
  for (int channel = 0; channel < num_channels; ++channel) {
    const float* channel_data = buffer.getReadPointer(channel);
    for (int sample = 0; sample < total_samples; ++sample) {
      float value = channel_data[sample];
      if (!std::isfinite(value)) {
        std::fprintf(stderr,
                     "[HELMBOY_DEBUG_BUFFER_INTEGRITY] Non-finite sample ch=%d idx=%d value=%f\n",
                     channel, sample, value);
      }
      peak = std::max(peak, std::fabs(value));
    }
  }

  static int block_counter = 0;
  if ((++block_counter % 512) == 0) {
    std::fprintf(stderr,
                 "[HELMBOY_DEBUG_BUFFER_INTEGRITY] block=%d peak=%f channels=%d samples=%d\n",
                 block_counter, peak, num_channels, total_samples);
  }
#endif
}

bool HelmPlugin::hasEditor() const {
  return true;
}

AudioProcessorEditor* HelmPlugin::createEditor() {
  appendPluginHostTrace("HelmPlugin::createEditor begin");

  try {
    auto* editor = new HelmEditor(*this);
    appendPluginHostTrace("HelmPlugin::createEditor end");
    return editor;
  }
  catch (const std::exception& ex) {
    Logger::writeToLog("[Plugin Editor] Initialization failed: " + String(ex.what()));
  }
  catch (...) {
    Logger::writeToLog("[Plugin Editor] Initialization failed with an unknown exception.");
  }

  return nullptr;
}

void HelmPlugin::parameterChanged(std::string_view name, mopo::mopo_float value) {
  std::string name_str{name}; // Convert to string for parameter lookup
  valueChangedExternal(name_str, value);
}

void HelmPlugin::loadPatches() {
  auto patches = LoadSave::getAllPatches();
  const ScopedLock lock(getCallbackLock());
  all_patches_ = std::move(patches);
}

bool HelmPlugin::isBusesLayoutSupported(const BusesLayout &layouts) const {
  if (layouts.getMainOutputChannelSet() == juce::AudioChannelSet::disabled())
      return false;
  return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void HelmPlugin::getStateInformation(MemoryBlock& dest_data) {
  var state = LoadSave::stateToVar(this, save_info_, getCallbackLock());
  String data_string = JSON::toString(state);
  MemoryOutputStream stream;
  stream.writeString(data_string);
  dest_data.append(stream.getData(), stream.getDataSize());
}

void HelmPlugin::setStateInformation(const void* data, int size_in_bytes) {
  if (data == nullptr || size_in_bytes <= 0)
    return;

  MemoryInputStream stream(data, size_in_bytes, false);
  String data_string = stream.readEntireStreamAsString();
  var state;
  if (JSON::parse(data_string, state).failed() || !state.isObject())
    return;
  LoadSave::PreparedSynthState prepared = LoadSave::prepareState(std::move(state));
  if (!prepared.valid)
    return;

  {
    const ScopedLock lock(getCallbackLock());
    set_state_time_.store(Time::getMillisecondCounter(), std::memory_order_release);
    processControlChanges();
    processModulationChanges();
    LoadSave::applyPreparedState(this, save_info_, prepared);
  }

  requestGuiRefresh(true);
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  appendPluginHostTrace("createPluginFilter begin");

  try {
    AudioProcessor* plugin = new HelmPlugin();
    appendPluginHostTrace("createPluginFilter success");
    return plugin;
  }
  catch (const std::exception& ex) {
    Logger::writeToLog("[Plugin Startup] Initialization failed: " + String(ex.what()));
    appendPluginHostTrace("createPluginFilter exception: " + juce::String(ex.what()));
  }
  catch (...) {
    Logger::writeToLog("[Plugin Startup] Initialization failed with an unknown exception.");
    appendPluginHostTrace("createPluginFilter unknown exception");
  }

  appendPluginHostTrace("createPluginFilter returning nullptr");
  return nullptr;
}
