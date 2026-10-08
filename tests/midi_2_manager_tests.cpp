#include "midi_2_manager.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <span>

namespace {

int failures = 0;

void expect(bool condition, const char* description) {
  if (!condition) {
    std::cerr << "FAIL: " << description << '\n';
    ++failures;
  }
}

void testValidPackets() {
  using Manager = helmboy::Midi2Manager;
  using Event = Manager::UMPEvent;

  Event event;
  constexpr std::array<uint8_t, 8> note_on{
      0x43, 0x99, 60, 0, 0x80, 0, 0, 0};
  expect(Manager::decodeUMP(note_on, 23, event) == Manager::UMPDecodeResult::decoded,
         "decode MIDI 2.0 Note On");
  expect(event.type == Event::Type::noteOn && event.group == 3
             && event.channel == 9 && event.index == 60
             && event.value == 0x8000 && event.sampleOffset == 23,
         "preserve Note On fields, velocity, group and sample offset");

  constexpr std::array<uint8_t, 8> note_off{
         0x41, 0x82, 64, 0, 0x40, 0, 0, 0};
  expect(Manager::decodeUMP(note_off, 7, event) == Manager::UMPDecodeResult::decoded,
               "decode MIDI 2.0 Note Off");
  expect(event.type == Event::Type::noteOff && event.group == 1
                      && event.channel == 2 && event.index == 64
                      && event.value == 0x4000 && event.sampleOffset == 7,
               "preserve Note Off release velocity and timing");

  constexpr std::array<uint8_t, 8> control_change{
      0x4f, 0xb1, 74, 0, 0x12, 0x34, 0x56, 0x78};
  expect(Manager::decodeUMP(control_change, 0, event) == Manager::UMPDecodeResult::decoded,
         "decode MIDI 2.0 Control Change");
  expect(event.type == Event::Type::controlChange && event.group == 15
             && event.channel == 1 && event.index == 74
             && event.value == 0x12345678,
         "decode big-endian 32-bit CC value and retain group");

  constexpr std::array<uint8_t, 8> pitch_bend{
      0x40, 0xe3, 0, 0, 0x80, 0, 0, 1};
  expect(Manager::decodeUMP(pitch_bend, 127, event) == Manager::UMPDecodeResult::decoded,
         "decode MIDI 2.0 Pitch Bend");
  expect(event.type == Event::Type::pitchBend && event.channel == 3
             && event.value == 0x80000001 && event.sampleOffset == 127,
         "preserve full-resolution pitch bend and offset");

  constexpr std::array<uint8_t, 8> note_on_zero{
      0x40, 0x90, 60, 0, 0, 0, 0, 0};
  expect(Manager::decodeUMP(note_on_zero, 0, event) == Manager::UMPDecodeResult::decoded
             && event.type == Event::Type::noteOff,
         "interpret zero-velocity Note On as Note Off");
}

void testRejectedPackets() {
  using Manager = helmboy::Midi2Manager;
  using Result = Manager::UMPDecodeResult;
  using Event = Manager::UMPEvent;

  Event event;
  constexpr std::array<uint8_t, 8> valid{
      0x40, 0x90, 60, 0, 0x7f, 0, 0, 0};
  expect(Manager::decodeUMP(std::span(valid).first(7), 0, event) == Result::invalidLength,
         "reject truncated packet");
  constexpr std::array<uint8_t, 9> overlong{
      0x40, 0x90, 60, 0, 0x7f, 0, 0, 0, 0};
  expect(Manager::decodeUMP(overlong, 0, event) == Result::invalidLength,
         "reject overlong single packet");

  auto invalid = valid;
  invalid[0] = 0x20;
  expect(Manager::decodeUMP(invalid, 0, event) == Result::unsupportedMessageType,
         "reject MIDI 1.0 UMP type in MIDI 2.0 decoder");
  invalid = valid;
  invalid[1] = 0xa0;
  expect(Manager::decodeUMP(invalid, 0, event) == Result::unsupportedStatus,
         "reject unsupported Channel Voice status");
  invalid = valid;
  invalid[2] = 128;
  expect(Manager::decodeUMP(invalid, 0, event) == Result::invalidData,
         "reject out-of-range note number");
  invalid = valid;
  invalid[3] = 1;
  expect(Manager::decodeUMP(invalid, 0, event) == Result::unsupportedAttribute,
         "reject unsupported note attribute");
  expect(Manager::decodeUMP(valid, -1, event) == Result::invalidArgument,
         "reject negative sample offset");

  invalid = {0x40, 0xb0, 128, 0, 0, 0, 0, 1};
  expect(Manager::decodeUMP(invalid, 0, event) == Result::invalidData,
         "reject out-of-range controller number");
  invalid = {0x40, 0xe0, 1, 0, 0, 0, 0, 1};
  expect(Manager::decodeUMP(invalid, 0, event) == Result::invalidData,
         "reject nonzero reserved pitch-bend fields");
}

void testModeAndMidi1Callbacks() {
  using Manager = helmboy::Midi2Manager;
  using Result = Manager::UMPDecodeResult;

  Manager manager;
  constexpr std::array<uint8_t, 8> packet{
      0x40, 0x90, 60, 0, 0x7f, 0, 0, 0};
  int ump_events = 0;
       int legacy_note_events = 0;
  manager.onUMPEvent = [&] (const Manager::UMPEvent& event) {
    ++ump_events;
    expect(event.sampleOffset == 31, "publish UMP event with original sample offset");
  };
       manager.onNoteOn = [&] (int, int, float) { ++legacy_note_events; };

  manager.setMidiVersion(Manager::MidiVersion::MIDI_1_0);
  expect(manager.processUMP(packet, 31) == Result::disabled,
         "do not process UMP when MIDI 2.0 mode is disabled");
  expect(ump_events == 0, "disabled UMP does not publish callbacks");

  manager.setMidiVersion(Manager::MidiVersion::MIDI_2_0);
  expect(manager.processUMP(packet, 31) == Result::decoded,
         "process UMP when MIDI 2.0 mode is enabled");
  expect(ump_events == 1 && legacy_note_events == 0,
         "publish full UMP event without using callbacks that lose its offset");

  Manager grouped_manager;
  grouped_manager.setMidiVersion(Manager::MidiVersion::MIDI_2_0);
  int grouped_events = 0;
  grouped_manager.onUMPEvent = [&] (const Manager::UMPEvent& event) {
    expect(event.group == 3, "preserve nonzero group in full UMP event");
    ++grouped_events;
  };
  constexpr std::array<uint8_t, 8> grouped_note{
      0x43, 0x90, 60, 0, 0x7f, 0, 0, 0};
  expect(grouped_manager.processUMP(grouped_note, 0) == Result::decoded,
         "decode valid nonzero-group packet");
  expect(grouped_events == 1,
         "publish nonzero group only through the full event callback");

  int midi1_notes = 0;
  int midi1_channels = 0;
  manager.onNoteOn = [&] (int channel, int note, float velocity) {
    expect(channel == 1 && note == 64, "MIDI 1.0 callback channel and note");
    expect(std::abs(velocity - 96.0f / 127.0f) < 0.00001f,
           "MIDI 1.0 velocity remains 7-bit normalized");
    ++midi1_notes;
  };
  manager.onControlChange = [&] (int channel, int controller, float value) {
    expect(channel == 1 && controller == 74, "MIDI 1.0 CC callback fields");
    expect(std::abs(value - 91.0f / 127.0f) < 0.00001f,
           "MIDI 1.0 CC remains 7-bit normalized");
    ++midi1_channels;
  };
  for (const auto version : {Manager::MidiVersion::MIDI_1_0,
                             Manager::MidiVersion::MIDI_2_0}) {
    manager.setMidiVersion(version);
    manager.processMidiMessage(juce::MidiMessage::noteOn(2, 64, uint8_t{96}));
    manager.processMidiMessage(juce::MidiMessage::controllerEvent(2, 74, 91));
  }
  expect(midi1_notes == 2 && midi1_channels == 2,
         "MIDI 1.0 messages remain active in both version settings");
}

} // namespace

int main() {
  testValidPackets();
  testRejectedPackets();
  testModeAndMidi1Callbacks();
  if (failures != 0)
    std::cerr << failures << " MIDI test(s) failed\n";
  return failures == 0 ? 0 : 1;
}