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

#include "synth_base.h"

#include <cmath>
#include <algorithm>
#include <cstdio>
#include <memory>
#include <optional>
#include "load_save.h"
#include "startup.h"
#include "startup_trace.h"
#include "synth_gui_interface.h"
#include "utils.h"

#define OUTPUT_WINDOW_MIN_NOTE 16.0

namespace {
constexpr int kGuiNotificationIntervalMs = 30;

#if HELMBOY_DEBUG_AUDIO_LEVELS
float computeStereoPeak(std::span<const mopo::mopo_float> left,
                        std::span<const mopo::mopo_float> right) {
  float peak = 0.0f;
  int samples = static_cast<int>(left.size());
  for (int i = 0; i < samples; ++i) {
    peak = std::max(peak, std::fabs(static_cast<float>(left[i])));
    peak = std::max(peak, std::fabs(static_cast<float>(right[i])));
  }
  return peak;
}
#endif

}

#define appendStartupTraceBase(message) HELMBOY_STARTUP_TRACE("startup_trace.log", message)

SynthBase::SynthBase() try {
  gui_lifetime_->synth = this;
  controls_ = engine_.getControls();
  rebuildGuiControlTable();
  rebuildVisualTelemetryTable();
  visual_peak_source_ = engine_.getModulationSource("peak_meter");
  keyboard_state_ = std::make_unique<MidiKeyboardState>();
  midi_manager_ = std::make_unique<MidiManager>(this, keyboard_state_.get(), &save_info_, this);

  last_played_note_ = 0.0;
  last_num_pressed_.store(0, std::memory_order_release);
  memset(output_memory_, 0, 2 * mopo::MEMORY_RESOLUTION * sizeof(float));
  memset(output_memory_write_, 0, 2 * mopo::MEMORY_RESOLUTION * sizeof(float));
  memory_reset_period_ = mopo::MEMORY_RESOLUTION;
  memory_input_offset_ = 0;
  memory_index_ = 0;
} catch (const std::exception& ex) {
  Logger::writeToLog("[Startup] SynthBase ctor EXCEPTION (base/member init or body): " + String(ex.what()));
  throw;
} catch (...) {
  Logger::writeToLog("[Startup] SynthBase ctor UNKNOWN EXCEPTION (base/member init or body)");
  throw;
}

bool SynthBase::performStartupChecks() {
  appendStartupTraceBase("SynthBase::performStartupChecks start");
  const std::lock_guard<std::mutex> startupLock(startup_checks_mutex_);

  if (startup_checks_done_) {
    appendStartupTraceBase("SynthBase::performStartupChecks already done");
    return true;
  }

  if (!ensureGuiPreflight()) {
    appendStartupTraceBase("SynthBase::performStartupChecks ensureGuiPreflight failed");
    return false;
  }

  try {
    appendStartupTraceBase("SynthBase::performStartupChecks Startup::doStartupChecks start");
    Startup::doStartupChecks(midi_manager_.get());
    appendStartupTraceBase("SynthBase::performStartupChecks Startup::doStartupChecks done");
  }
  catch (const std::exception& ex) {
    appendStartupTraceBase("SynthBase::performStartupChecks exception: " + String(ex.what()));
    return false;
  }
  catch (...) {
    appendStartupTraceBase("SynthBase::performStartupChecks unknown exception");
    return false;
  }

  startup_checks_done_ = true;
  appendStartupTraceBase("SynthBase::performStartupChecks done");
  return true;
}

bool SynthBase::ensureGuiPreflight() {
  if (!JUCEApplicationBase::isStandaloneApp())
    return true;

  appendStartupTraceBase("SynthBase::ensureGuiPreflight start");
  const std::lock_guard<std::mutex> preflightLock(gui_preflight_mutex_);

  if (gui_preflight_done_) {
    appendStartupTraceBase("SynthBase::ensureGuiPreflight already done");
    return true;
  }

  try {
    appendStartupTraceBase("SynthBase::ensureGuiPreflight ensureDeferredModuleInit start");
    if (!engine_.ensureDeferredModuleInit()) {
      appendStartupTraceBase("SynthBase::ensureGuiPreflight ensureDeferredModuleInit failed");
      return false;
    }
    appendStartupTraceBase("SynthBase::ensureGuiPreflight ensureDeferredModuleInit done");

    // Standalone runtime can defer submodule initialization until after window creation.
    // Refresh control cache now that all deferred controls are guaranteed to exist.
    appendStartupTraceBase("SynthBase::ensureGuiPreflight refresh controls start");
    controls_ = engine_.getControls();
    rebuildGuiControlTable();
    rebuildVisualTelemetryTable();
    appendStartupTraceBase("SynthBase::ensureGuiPreflight refresh controls done");
  }
  catch (const std::exception& ex) {
    appendStartupTraceBase("SynthBase::ensureGuiPreflight exception: " + String(ex.what()));
    return false;
  }
  catch (...) {
    appendStartupTraceBase("SynthBase::ensureGuiPreflight unknown exception");
    return false;
  }

  gui_preflight_done_ = true;
  appendStartupTraceBase("SynthBase::ensureGuiPreflight done");
  return true;
}

void SynthBase::valueChanged(const std::string& name, mopo::mopo_float value) {
  auto control_it = controls_.find(name);
  if (control_it == controls_.end() || control_it->second == nullptr)
    return;

  value_change_queue_.enqueue(mopo::control_change(control_it->second, value));
}

void SynthBase::valueChangedInternal(const std::string& name, mopo::mopo_float value) {
  valueChanged(name, value);
  setValueNotifyHost(name, value);
}

void SynthBase::valueChangedThroughMidi(const std::string& name, mopo::mopo_float value) {
  auto control_it = controls_.find(name);
  if (control_it == controls_.end() || control_it->second == nullptr)
    return;

  control_it->second->set(value);
  setValueNotifyHost(name, value);
  queueGuiControlChange(name, value);
}

void SynthBase::patchChangedThroughMidi(File patch) {
  requestGuiRefresh(true);
}

void SynthBase::valueChangedExternal(const std::string& name, mopo::mopo_float value) {
  valueChanged(name, value);
  queueGuiControlChange(name, value);
}

void SynthBase::changeModulationAmount(const std::string& source,
                                       const std::string& destination,
                                       mopo::mopo_float amount) {
  auto connection_opt = getConnection(source, destination);
  mopo::ModulationConnection* connection = connection_opt.value_or(nullptr);

  if (!connection && amount != 0.0)
    connection = modulation_bank_.get(source, destination);

  if (connection)
    setModulationAmount(connection, amount);
}

std::optional<mopo::ModulationConnection*> SynthBase::getConnection(const std::string& source,
                                                     const std::string& destination) {
  for (auto* connection : mod_connections_) {
    if (connection->source == source && connection->destination == destination)
      return connection;
  }
  return std::nullopt;
}

void SynthBase::setModulationAmount(mopo::ModulationConnection* connection,
                                    mopo::mopo_float amount) {
  if (connection == nullptr) {
    Logger::writeToLog("[Preset Load] Ignored modulation with null connection.");
    return;
  }

  if (!std::isfinite(static_cast<double>(amount))) {
    Logger::writeToLog("[Preset Load] Modulation '" + String(connection->source) + " -> " +
                       String(connection->destination) +
                       "' has non-finite amount. Replaced by 0.");
    amount = 0.0;
  }

  if (amount == 0.0) {
    modulation_bank_.recycle(connection);
    mod_connections_.erase(connection);
  }
  else if (mod_connections_.count(connection) == 0)
    mod_connections_.insert(connection);
  modulation_change_queue_.enqueue(mopo::modulation_change(connection, amount));
}

void SynthBase::disconnectModulation(mopo::ModulationConnection* connection) {
  setModulationAmount(connection, 0.0);
}

void SynthBase::clearModulations() {
  while (mod_connections_.size())
    disconnectModulation(*mod_connections_.begin());
}

int SynthBase::getNumModulations(const std::string& destination) {
  int connections = 0;
  for (const auto* connection : mod_connections_) {
    if (connection->destination == destination)
      connections++;
  }
  return connections;
}

std::vector<mopo::ModulationConnection*>
SynthBase::getSourceConnections(const std::string& source) {
  std::vector<mopo::ModulationConnection*> connections;
  for (auto* connection : mod_connections_) {
    if (connection->source == source)
      connections.push_back(connection);
  }
  return connections;
}

std::vector<mopo::ModulationConnection*>
SynthBase::getDestinationConnections(const std::string& destination) {
  std::vector<mopo::ModulationConnection*> connections;
  for (auto* connection : mod_connections_) {
    if (connection->destination == destination)
      connections.push_back(connection);
  }
  return connections;
}

mopo::Output* SynthBase::getModSource(const std::string& name) {
  ScopedLock lock(getCriticalSection());
  return engine_.getModulationSource(name);
}

var SynthBase::saveToVar(String author) {
  save_info_["author"] = author;
  return LoadSave::stateToVar(this, save_info_, getCriticalSection());
}

void SynthBase::loadInitPatch() {
  const ScopedLock lock(getCriticalSection());
  LoadSave::initSynth(this, save_info_);
}

void SynthBase::loadFromVar(juce::var state) {
  LoadSave::PreparedSynthState prepared = LoadSave::prepareState(std::move(state));
  if (!prepared.valid)
    return;

  const ScopedLock lock(getCriticalSection());
  LoadSave::applyPreparedState(this, save_info_, prepared);
}

bool SynthBase::loadFromFile(File patch) {
  var parsed_json_state;
  if (patch.exists() && JSON::parse(patch.loadFileAsString(), parsed_json_state).wasOk()) {
    active_file_ = patch;
    File parent = patch.getParentDirectory();
    loadFromVar(parsed_json_state);
    setFolderName(parent.getFileNameWithoutExtension());
    setPatchName(patch.getFileNameWithoutExtension());

    auto gui_interface = getGuiInterface();
    if (gui_interface.has_value()) {
      gui_interface.value()->updateFullGui();
      gui_interface.value()->notifyFresh();
    }

    return true;
  }
  return false;
}

bool SynthBase::exportToFile() {
  auto gui_interface = getGuiInterface();
  return gui_interface.has_value() && gui_interface.value()->showExportDialog();
}

bool SynthBase::saveToFile(File patch) {
  if (patch.getFileExtension() != String(mopo::PATCH_EXTENSION))
    patch = patch.withFileExtension(String(mopo::PATCH_EXTENSION));

  File parent = patch.getParentDirectory();
  setFolderName(parent.getFileNameWithoutExtension());
  setPatchName(patch.getFileNameWithoutExtension());

  auto gui_interface = getGuiInterface();
  if (gui_interface.has_value()) {
    gui_interface.value()->updateFullGui();
    gui_interface.value()->notifyFresh();
  }

  if (patch.replaceWithText(JSON::toString(saveToVar(save_info_["author"])))) {
    active_file_ = patch;
    return true;
  }
  return false;
}

bool SynthBase::saveToActiveFile() {
  if (!active_file_.exists() || !active_file_.hasWriteAccess())
    return false;

  return saveToFile(active_file_);
}

void SynthBase::processAudio(AudioSampleBuffer* buffer, int channels, int samples, int offset) {
  mopo::utils::enableDenormalFlushing(true);

  if (engine_.getBufferSize() != samples)
    engine_.setBufferSize(samples);

  engine_.process();

  auto engine_output_left = std::span<const mopo::mopo_float>(engine_.output(0)->buffer, samples);
  auto engine_output_right = std::span<const mopo::mopo_float>(engine_.output(1)->buffer, samples);
  #include <xsimd/xsimd.hpp>
  using batch = xsimd::batch<float>;
  constexpr std::size_t simd_size = batch::size;
  for (int channel = 0; channel < channels; ++channel) {
    float* channelData = buffer->getWritePointer(channel, offset);
  auto synth_output = (channel % 2) ? engine_output_right : engine_output_left;
    int simd_end = samples - (samples % simd_size);
    int i = 0;
    alignas(alignof(batch)) float tmp[simd_size];
    for (; i < simd_end; i += simd_size) {
  for (std::size_t j = 0; j < simd_size; ++j) tmp[j] = static_cast<float>(synth_output[i + j]);
      batch s = batch::load_aligned(tmp);
      s.store_unaligned(&channelData[i]);
      auto mask = xsimd::isfinite(s);
      for (std::size_t j = 0; j < simd_size; ++j) {
        MOPO_ASSERT(mask.get(j));
      }
    }
    for (; i < samples; ++i) {
  channelData[i] = synth_output[i];
  MOPO_ASSERT(std::isfinite(synth_output[i]));
    }
  }

  updateMemoryOutput(samples, engine_output_left, engine_output_right);
}

void SynthBase::processAudioSafe(AudioSampleBuffer* buffer, int channels, int samples, int offset) {
  mopo::utils::enableDenormalFlushing(true);

  if (engine_.getBufferSize() != samples)
    engine_.setBufferSize(samples);

  engine_.process();

  // Create safe buffer wrapper
  helmboy::SafeAudioBuffer safe_buffer(*buffer);

  // Get engine output with bounds checking
  auto engine_output_left = std::span<const mopo::mopo_float>(engine_.output(0)->buffer, samples);
  auto engine_output_right = std::span<const mopo::mopo_float>(engine_.output(1)->buffer, samples);

#if HELMBOY_DEBUG_AUDIO_LEVELS
  static int debug_block_count = 0;
  if ((++debug_block_count % 512) == 0) {
    float peak = computeStereoPeak(engine_output_left, engine_output_right);
    std::fprintf(stderr,
                 "[HELMBOY_DEBUG_AUDIO_LEVELS] block=%d samples=%d peak=%f voices=%d\n",
                 debug_block_count, samples, peak, engine_.getNumActiveVoices());
  }
#endif

  // Process each channel safely using std::span
  #include <xsimd/xsimd.hpp>
  using batch = xsimd::batch<float>;
  constexpr std::size_t simd_size = batch::size;
  for (int channel = 0; channel < channels; ++channel) {
    auto channel_span = safe_buffer.getChannelSpan(channel);
  auto synth_output = (channel % 2) ? engine_output_right : engine_output_left;
    auto processing_span = channel_span.subspan(offset, samples);
    int simd_end = samples - (samples % simd_size);
    int i = 0;
    alignas(alignof(batch)) float tmp[simd_size];
    for (; i < simd_end; i += simd_size) {
  for (std::size_t j = 0; j < simd_size; ++j) tmp[j] = static_cast<float>(synth_output[i + j]);
      batch s = batch::load_aligned(tmp);
      s.store_unaligned(&processing_span[i]);
      auto mask = xsimd::isfinite(s);
      for (std::size_t j = 0; j < simd_size; ++j) {
        MOPO_ASSERT(mask.get(j));
      }
    }
    for (; i < samples; ++i) {
  processing_span[i] = synth_output[i];
  MOPO_ASSERT(std::isfinite(synth_output[i]));
    }
  }

  updateMemoryOutput(samples, engine_output_left, engine_output_right);
}

void SynthBase::processMidi(MidiBuffer& midi_messages, int start_sample, int end_sample) {
  if (midi_manager_ == nullptr)
    return;

  bool process_all = end_sample == 0;

  for (auto it = midi_messages.cbegin(); it != midi_messages.cend(); ++it) {
    int midi_sample = (*it).samplePosition;
    if (process_all || (midi_sample >= start_sample && midi_sample < end_sample))
      midi_manager_->processMidiMessage((*it).getMessage(), midi_sample - start_sample);
  }
}

void SynthBase::processKeyboardEvents(MidiBuffer& buffer, int num_samples) {
  MidiBuffer keyboard_messages;
  midi_manager_->replaceKeyboardMessages(keyboard_messages, num_samples);
  midi_manager_->replaceKeyboardMessages(buffer, num_samples);

  processMidi(keyboard_messages);
}

void SynthBase::processControlChanges() {
  mopo::control_change change;
  while (getNextControlChange(change))
    change.first->set(change.second);
}

void SynthBase::processModulationChanges() {
  mopo::modulation_change change;
  while (getNextModulationChange(change)) {
    mopo::ModulationConnection* connection = change.first;
    mopo::mopo_float amount = change.second;
    connection->amount.set(amount);

    bool active = engine_.isModulationActive(connection);
    if (active && amount == 0.0)
      engine_.disconnectModulation(connection);
    else if (!active && amount)
      engine_.connectModulation(connection);
  }
}

void SynthBase::updateMemoryOutput(int samples, std::span<const mopo::mopo_float> left,
                                                std::span<const mopo::mopo_float> right) {
  if (samples <= 0)
    return;

  publishVisualTelemetry();

  if (visual_peak_source_ != nullptr) {
    visual_peaks_[0].store(static_cast<float>(visual_peak_source_->buffer[0]), std::memory_order_relaxed);
    visual_peaks_[1].store(static_cast<float>(visual_peak_source_->buffer[1]), std::memory_order_relaxed);
  }

  mopo::mopo_float last_played = std::max(engine_.getLastActiveNote(), OUTPUT_WINDOW_MIN_NOTE);
  int num_pressed = engine_.getPressedNotes().size();
  int output_inc = std::max<int>(1, engine_.getSampleRate() / mopo::MEMORY_SAMPLE_RATE);

  mopo::mopo_float current_last_played_note = last_played_note_.load(std::memory_order_acquire);
  if (last_played && (current_last_played_note != last_played || num_pressed > last_num_pressed_.load(std::memory_order_acquire))) {
    last_played_note_.store(last_played, std::memory_order_release);

    mopo::mopo_float frequency = mopo::utils::midiNoteToFrequency(last_played);
    mopo::mopo_float period = engine_.getSampleRate() / frequency;
    int window_length = output_inc * mopo::MEMORY_RESOLUTION;

    memory_reset_period_ = period;
    while (memory_reset_period_ < window_length)
      memory_reset_period_ += memory_reset_period_;

    memory_reset_period_ = std::min(memory_reset_period_, 2.0 * window_length);
        memory_index_ = 0;
        publishOutputMemory();
  }
  last_num_pressed_.store(num_pressed, std::memory_order_release);

  // Load atomics into local temporaries for arithmetic in the audio loop
  mopo::mopo_float local_input_offset = memory_input_offset_.load(std::memory_order_acquire);
  int local_memory_index = memory_index_.load(std::memory_order_acquire);

  for (; local_input_offset < samples; local_input_offset += output_inc) {
    int input_index = mopo::utils::iclamp(static_cast<int>(local_input_offset), 0, samples - 1);
    local_memory_index = mopo::utils::iclamp(local_memory_index, 0, 2 * mopo::MEMORY_RESOLUTION - 1);
    MOPO_ASSERT(input_index >= 0);
    MOPO_ASSERT(input_index < samples);
    MOPO_ASSERT(local_memory_index >= 0);
    MOPO_ASSERT(local_memory_index < 2 * mopo::MEMORY_RESOLUTION);
    output_memory_write_[local_memory_index++] = (left[input_index] + right[input_index]) / 2.0;

    if (local_memory_index * output_inc >= memory_reset_period_) {
      local_input_offset += memory_reset_period_ - local_memory_index * output_inc;
      local_memory_index = 0;
      publishOutputMemory();
    }
  }

  // Store temporaries back to atomics
  memory_input_offset_.store(local_input_offset - samples, std::memory_order_release);
  memory_index_.store(local_memory_index, std::memory_order_release);
}

void SynthBase::armMidiLearn(const std::string& name) {
  midi_manager_->armMidiLearn(name);
}

void SynthBase::cancelMidiLearn() {
  midi_manager_->cancelMidiLearn();
}

void SynthBase::clearMidiLearn(const std::string& name) {
  midi_manager_->clearMidiLearn(name);
}

bool SynthBase::isMidiMapped(const std::string& name) {
  return midi_manager_->isMidiMapped(name);
}

void SynthBase::setAuthor(String author) {
  const ScopedLock lock(getCriticalSection());
  save_info_["author"] = author;
}

void SynthBase::setPatchName(String patch_name) {
  const ScopedLock lock(getCriticalSection());
  save_info_["patch_name"] = patch_name;
}

void SynthBase::setFolderName(String folder_name) {
  const ScopedLock lock(getCriticalSection());
  save_info_["folder_name"] = folder_name;
}

String SynthBase::getAuthor() {
  const ScopedLock lock(getCriticalSection());
  return save_info_["author"];
}

String SynthBase::getPatchName() {
  const ScopedLock lock(getCriticalSection());
  return save_info_["patch_name"];
}

String SynthBase::getFolderName() {
  const ScopedLock lock(getCriticalSection());
  return save_info_["folder_name"];
}

SynthGuiStateSnapshot SynthBase::captureGuiStateSnapshot() {
  SynthGuiStateSnapshot snapshot;
  snapshot.controls.reserve(controls_.size());
  {
    const ScopedLock lock(getCriticalSection());
    for (const auto& [name, control] : controls_) {
      if (control != nullptr)
        snapshot.controls.emplace_back(name, control->value());
    }
    snapshot.modulations.reserve(mod_connections_.size());
    for (const mopo::ModulationConnection* connection : mod_connections_) {
      if (connection != nullptr)
        snapshot.modulations.push_back({connection->source, connection->destination,
                                         connection->amount.value()});
    }
  }
  return snapshot;
}

SynthBase::~SynthBase() {
  stopGuiUpdates();
}

void SynthBase::stopGuiUpdates() {
  {
    const std::lock_guard<std::mutex> lock(gui_lifetime_->mutex);
    gui_lifetime_->synth = nullptr;
  }
  gui_updates_enabled_.store(false, std::memory_order_release);
  gui_notification_timer_.stopTimer();
}

void SynthBase::startGuiUpdates() {
  if (gui_updates_enabled_.load(std::memory_order_acquire) && !gui_notification_timer_.isTimerRunning())
    gui_notification_timer_.startTimer(kGuiNotificationIntervalMs);
}

SynthBase::GuiControlTable::GuiControlTable(const mopo::control_map& controls) {
  names.reserve(controls.size());
  for (const auto& control : controls)
    names.push_back(control.first);

  const size_t count = names.size();
  values = std::make_unique<std::atomic<mopo::mopo_float>[]>(count);
  dirty = std::make_unique<std::atomic<bool>[]>(count);
  indices.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    indices.emplace(names[i], static_cast<int>(i));
    values[i].store(0.0, std::memory_order_relaxed);
    dirty[i].store(false, std::memory_order_relaxed);
  }
}

void SynthBase::rebuildGuiControlTable() {
  auto table = std::make_unique<GuiControlTable>(controls_);
  gui_control_table_.store(table.get(), std::memory_order_release);
  gui_control_tables_.push_back(std::move(table));
}

void SynthBase::rebuildVisualTelemetryTable() {
  auto table = std::make_unique<VisualTelemetryTable>();
  auto addSources = [&table](mopo::output_map& outputs) {
    for (const auto& entry : outputs) {
      const mopo::Output* output = entry.second;
      if (output == nullptr || table->indices.count(output) != 0)
        continue;
      int index = static_cast<int>(table->sources.size());
      table->indices.emplace(output, index);
      table->sources.push_back(output);
    }
  };

  addSources(engine_.getModulationSources());
  addSources(engine_.getMonoModulations());
  addSources(engine_.getPolyModulations());
  table->values.resize(table->sources.size(), 0.0f);
  visual_telemetry_table_.store(table.get(), std::memory_order_release);
  visual_telemetry_tables_.push_back(std::move(table));
}

int SynthBase::getVisualTelemetryIndex(const mopo::Output* output) const {
  VisualTelemetryTable* table = visual_telemetry_table_.load(std::memory_order_acquire);
  if (table == nullptr || output == nullptr)
    return -1;

  auto index = table->indices.find(output);
  return index == table->indices.end() ? -1 : index->second;
}

bool SynthBase::copyVisualTelemetry(std::span<const int> indices,
                                    std::span<float> destination) const {
  if (destination.size() < indices.size())
    return false;

  VisualTelemetryTable* table = visual_telemetry_table_.load(std::memory_order_acquire);
  if (table == nullptr || table->busy.test_and_set(std::memory_order_acquire))
    return false;

  for (size_t i = 0; i < indices.size(); ++i) {
    if (indices[i] < 0 || static_cast<size_t>(indices[i]) >= table->values.size()) {
      table->busy.clear(std::memory_order_release);
      return false;
    }
    destination[i] = table->values[indices[i]];
  }
  table->busy.clear(std::memory_order_release);
  return true;
}

void SynthBase::publishVisualTelemetry() {
  VisualTelemetryTable* table = visual_telemetry_table_.load(std::memory_order_acquire);
  if (table == nullptr || table->busy.test_and_set(std::memory_order_acquire))
    return;

  for (size_t i = 0; i < table->sources.size(); ++i) {
    const mopo::Output* source = table->sources[i];
    table->values[i] = source != nullptr && source->buffer_size > 0
                           ? static_cast<float>(source->buffer[0])
                           : 0.0f;
  }
  table->busy.clear(std::memory_order_release);
}

// Real-time safe: no allocation, no lock, no message posting.
void SynthBase::queueGuiControlChange(const std::string& name, mopo::mopo_float value) {
  if (!gui_updates_enabled_.load(std::memory_order_acquire))
    return;

  GuiControlTable* table = gui_control_table_.load(std::memory_order_acquire);
  if (table == nullptr)
    return;

  auto index_it = table->indices.find(name);
  if (index_it == table->indices.end())
    return;

  table->values[index_it->second].store(value, std::memory_order_relaxed);
  table->dirty[index_it->second].store(true, std::memory_order_release);
  gui_changes_pending_.store(true, std::memory_order_release);
}

void SynthBase::requestGuiRefresh(bool fresh) {
  if (!gui_updates_enabled_.load(std::memory_order_acquire))
    return;
  if (fresh)
    gui_fresh_pending_.store(true, std::memory_order_release);
  gui_refresh_pending_.store(true, std::memory_order_release);
}

void SynthBase::deliverGuiNotifications() {
  if (!gui_updates_enabled_.load(std::memory_order_acquire))
    return;

  std::vector<std::pair<const std::string*, mopo::mopo_float>> changes;
  if (gui_changes_pending_.exchange(false, std::memory_order_acq_rel)) {
    GuiControlTable* table = gui_control_table_.load(std::memory_order_acquire);
    if (table != nullptr) {
      for (size_t i = 0; i < table->names.size(); ++i) {
        if (table->dirty[i].exchange(false, std::memory_order_acquire))
          changes.emplace_back(&table->names[i], table->values[i].load(std::memory_order_relaxed));
      }
    }
  }

  const bool refresh = gui_refresh_pending_.exchange(false, std::memory_order_acq_rel);
  const bool fresh = gui_fresh_pending_.exchange(false, std::memory_order_acq_rel);
  if (!refresh && !fresh && changes.empty())
    return;

  auto gui_interface = getGuiInterface();
  if (!gui_interface.has_value())
    return;

  if (refresh)
    gui_interface.value()->updateFullGui();
  else {
    for (const auto& [name, value] : changes)
      gui_interface.value()->updateGuiControl(*name, value);
  }
  if (fresh)
    gui_interface.value()->notifyFresh();
  else if (!changes.empty())
    gui_interface.value()->notifyChange();
}

bool SynthBase::copyOutputMemory(std::span<float> destination) {
  if (destination.size() < 2 * mopo::MEMORY_RESOLUTION ||
      output_memory_busy_.test_and_set(std::memory_order_acquire))
    return false;
  std::copy_n(output_memory_, 2 * mopo::MEMORY_RESOLUTION, destination.data());
  output_memory_busy_.clear(std::memory_order_release);
  return true;
}

void SynthBase::publishOutputMemory() {
  if (!output_memory_busy_.test_and_set(std::memory_order_acquire)) {
    std::copy_n(output_memory_write_, 2 * mopo::MEMORY_RESOLUTION, output_memory_);
    output_memory_busy_.clear(std::memory_order_release);
  }
}
