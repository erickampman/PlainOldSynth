// pm_midi.h — a small MIDI 2.0 (UMP channel-voice) message type + helpers, derived from the
// public MIDI 2.0 / UMP specification (M2-104). This is the PM framework's internal MIDI
// representation for the CLAP port: it replaces Apple's CoreMIDI MIDIUniversalMessage (which
// is not ours to use) with a clean, spec-based struct of our own. The CLAP boundary decodes
// clap events into this (see clap_ump.h); the DSP modules consume it.

#ifndef PM_MIDI_H
#define PM_MIDI_H

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>

namespace pm {

// UMP Message Type (the top nibble of word 0), per spec.
enum : uint8_t {
   kUmpTypeUtility       = 0x0,
   kUmpTypeSystem        = 0x1,
   kUmpTypeChannelVoice1 = 0x2,
   kUmpTypeSysEx         = 0x3,
   kUmpTypeChannelVoice2 = 0x4,
   kUmpTypeData128       = 0x5,
   kUmpTypeFlexData      = 0xD,
   kUmpTypeStream        = 0xF,
};

// MIDI 2.0 channel-voice status nibbles, per spec.
enum : uint8_t {
   kStatusNoteOff         = 0x8,
   kStatusNoteOn          = 0x9,
   kStatusPolyPressure    = 0xA,
   kStatusControlChange   = 0xB,
   kStatusProgramChange   = 0xC,
   kStatusChannelPressure = 0xD,
   kStatusPitchBend       = 0xE,
};

// Note-on/off attribute types, per spec.
enum : uint8_t {
   kAttrNone     = 0,
   kAttrManuf    = 1,
   kAttrProfile  = 2,
   kAttrPitch7_9 = 3, // 7-bit note + 9-bit fraction of a semitone
};

// A decoded MIDI 2.0 channel-voice message. Flat by design (not Apple's nested union).
// CC / pitch-bend / per-note fields will be added as modulation modules are ported.
struct UmpMessage {
   uint8_t  messageType   = 0;
   uint8_t  group         = 0;
   uint8_t  status        = 0;
   uint8_t  channel       = 0;
   uint8_t  note          = 0;
   uint16_t velocity      = 0; // 16-bit
   uint8_t  attributeType = 0;
   uint16_t attribute     = 0;
   uint8_t  ccIndex       = 0; // control change (status 0xB): controller index
   uint32_t ccData        = 0; // control change: 32-bit value
   uint32_t data32        = 0; // channel pressure (0xD) / pitch bend (0xE): 32-bit value
};

// --- predicates (spec logic; note-on with velocity 0 counts as note-off) -----------------
inline bool isChannelVoice2(const UmpMessage& m) { return m.messageType == kUmpTypeChannelVoice2; }

inline bool isNoteOn(const UmpMessage& m) {
   return isChannelVoice2(m) && m.status == kStatusNoteOn && m.velocity != 0;
}
inline bool isNoteOff(const UmpMessage& m) {
   return isChannelVoice2(m) &&
          (m.status == kStatusNoteOff || (m.status == kStatusNoteOn && m.velocity == 0));
}
inline bool isNoteOnOrOff(const UmpMessage& m) {
   return isChannelVoice2(m) && (m.status == kStatusNoteOn || m.status == kStatusNoteOff);
}
inline bool isChannelMessage(const UmpMessage& m) {
   return m.messageType == kUmpTypeChannelVoice2 || m.messageType == kUmpTypeChannelVoice1;
}

// --- MIDI 1.0 → UMP -----------------------------------------------------------------------
// Decode a 3-byte MIDI 1.0 channel message (status, data1, data2) into a channel-voice-2
// UmpMessage. Note-on with velocity 0 becomes a note-off. Unhandled statuses leave
// messageType == 0 (so isChannelMessage() is false). Host-agnostic: used both by the CLAP
// boundary (clap_ump.h) and by the DPF wrapper, which receive raw MIDI 1.0 bytes.
inline UmpMessage fromMidi1Bytes(uint8_t status, uint8_t data1, uint8_t data2) {
   UmpMessage m;
   const uint8_t statusNibble = status & 0xF0;
   m.channel = status & 0x0F;
   m.note = data1 & 0x7F;
   const uint8_t d2 = data2 & 0x7F;
   if (statusNibble == 0x90 && d2 > 0) {
      m.messageType = kUmpTypeChannelVoice2;
      m.status = kStatusNoteOn;
      m.velocity = static_cast<uint16_t>(d2 << 9); // 7-bit → 16-bit
   } else if (statusNibble == 0x80 || (statusNibble == 0x90 && d2 == 0)) {
      m.messageType = kUmpTypeChannelVoice2;
      m.status = kStatusNoteOff;
   } else if (statusNibble == 0xB0) {
      m.messageType = kUmpTypeChannelVoice2;
      m.status = kStatusControlChange;
      m.ccIndex = data1 & 0x7F;
      m.ccData = static_cast<uint32_t>(d2) << 25; // 7-bit → 32-bit
   } else if (statusNibble == 0xD0) {
      m.messageType = kUmpTypeChannelVoice2;
      m.status = kStatusChannelPressure;
      m.data32 = static_cast<uint32_t>(data1 & 0x7F) << 25; // 7-bit → 32-bit
   } else if (statusNibble == 0xE0) {
      m.messageType = kUmpTypeChannelVoice2;
      m.status = kStatusPitchBend;
      const uint32_t bend14 = (static_cast<uint32_t>(d2) << 7) | (data1 & 0x7F); // 14-bit, centre 0x2000
      m.data32 = bend14 << 18; // 14-bit → 32-bit
   }
   return m;
}

// --- pitch (A440; matches PolygraphModular's MIDINoteToFrequency) -------------------------
inline double noteToFrequency(int note) {
   return (440.0 / 32.0) * std::pow(2.0, (note - 9) / 12.0);
}
// Ratio for the 9-bit fraction of a semitone (equivalent to PM's gRoots512[fraction]).
inline double pitchFractionRatio(uint16_t fraction /*0..511*/) {
   return std::pow(2.0, static_cast<double>(fraction) / 6144.0); // 6144 = 512 * 12
}
// MIDI 2.0 attribute-type-3 (7.9) → Hz.
inline double pitchAttributeToFrequency(uint16_t attribute) {
   const uint8_t  note     = (attribute & 0xFE00) >> 9;
   const uint16_t fraction = (attribute & 0x01FF);
   return noteToFrequency(note) * pitchFractionRatio(fraction);
}

// --- constructors / decode / debug (defined in pm_midi.cpp) ------------------------------
UmpMessage  makeNoteOn(uint8_t note, uint16_t velocity, uint8_t channel = 0, uint8_t group = 0);
UmpMessage  makeNoteOff(uint8_t note, uint8_t channel = 0, uint8_t group = 0);
bool        containsNoteNumber(const UmpMessage& m, uint8_t& note);
// Decode a raw MIDI 2.0 channel-voice UMP (2 x 32-bit words, native order) into UmpMessage.
UmpMessage  decodeUmp(const uint32_t words[2]);
std::string toString(const UmpMessage& m);

// Derive one note message from another (used by the voice state machine for retrigger/steal).
std::optional<UmpMessage> makeNoteOnMsgFromOnMsg(const UmpMessage& m);   // copy if it's a note-on
std::optional<UmpMessage> makeNoteOffMsgFromOnMsg(const UmpMessage& m);  // note-on → matching off
std::optional<UmpMessage> makeNoteOffMsgFromOffMsg(const UmpMessage& m); // copy if it's a note-off

} // namespace pm

#endif // PM_MIDI_H
