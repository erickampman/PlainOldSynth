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
   kStatusRegPerNoteController    = 0x0, // registered per-note controller (index in word0 low byte)
   kStatusAssignPerNoteController = 0x1, // assignable per-note controller (index in word0 low byte)
   kStatusRegController           = 0x2, // registered controller = RPN (bank in the note byte, index low)
   kStatusAssignController        = 0x3, // assignable controller = NRPN (same layout)
   kStatusPerNotePitchBend        = 0x6, // per-note pitch bend (32-bit, centre 0x80000000)
   kStatusNoteOff         = 0x8,
   kStatusNoteOn          = 0x9,
   kStatusPolyPressure    = 0xA,
   kStatusControlChange   = 0xB,
   kStatusProgramChange   = 0xC,
   kStatusChannelPressure = 0xD,
   kStatusPitchBend       = 0xE,
   kStatusPerNoteManagement = 0xF,      // per-note management (option flags in word0 low byte)
};

// Per-note management option flags, per spec.
enum : uint8_t {
   kPerNoteMgmtReset  = 0x01, // S: reset this note's per-note controllers to their defaults
   kPerNoteMgmtDetach = 0x02, // D: detach per-note controllers from previously received notes
};

// Note-on/off attribute types, per spec.
enum : uint8_t {
   kAttrNone     = 0,
   kAttrManuf    = 1,
   kAttrProfile  = 2,
   kAttrPitch7_9 = 3, // 7-bit note + 9-bit fraction of a semitone
};

// A decoded MIDI 2.0 channel-voice message. Flat by design (not Apple's nested union).
struct UmpMessage {
   uint8_t  messageType   = 0;
   uint8_t  group         = 0;
   uint8_t  status        = 0;
   uint8_t  channel       = 0;
   uint8_t  note          = 0;
   uint16_t velocity      = 0; // 16-bit
   uint8_t  attributeType = 0;
   uint16_t attribute     = 0;
   uint8_t  ccIndex       = 0; // control change (0xB), per-note controller (0x0, 0x1), RPN/NRPN (0x2,
                               // 0x3): index. For RPN/NRPN the BANK is in `note`.
   uint32_t ccData        = 0; // control change / controllers: 32-bit value
   uint32_t data32        = 0; // poly pressure (0xA), channel pressure (0xD), pitch bends (0x6, 0xE)
   uint8_t  optionFlags   = 0; // per-note management (0xF): kPerNoteMgmt* flags
   int32_t  noteId        = -1; // host note id (CLAP note_id) when the host supplies one; not UMP
};

// Registered per-note controller indexes, per spec.
enum : uint8_t {
   kRegPerNotePitch7_25   = 3,  // absolute pitch, Q7.25 semitones; both pitch bends offset from it
   kRegPerNoteBrightness  = 74, // the MIDI 2.0 counterpart of MPE's CC74 "timbre"
};

// --- predicates ---------------------------------------------------------------------------------
// Note-on vs note-off is the status alone. A MIDI 2.0 note-on with velocity 0 is still a note-on
// (M2-104 §4.2.8: "Unlike the MIDI 1.0 Note On message, a velocity value of zero does not function as
// a Note Off"). MIDI 1.0's velocity-0 note-off is translated to a real note-off where MIDI 1.0 is
// decoded (fromMidi1Bytes), as the spec's translation rules require, so nothing here needs it.
inline bool isChannelVoice2(const UmpMessage& m) { return m.messageType == kUmpTypeChannelVoice2; }

inline bool isNoteOn(const UmpMessage& m) {
   return isChannelVoice2(m) && m.status == kStatusNoteOn;
}
inline bool isNoteOff(const UmpMessage& m) {
   return isChannelVoice2(m) && m.status == kStatusNoteOff;
}
inline bool isNoteOnOrOff(const UmpMessage& m) {
   return isChannelVoice2(m) && (m.status == kStatusNoteOn || m.status == kStatusNoteOff);
}
// A message addressed to one note that is not itself a note-on/off: poly pressure, per-note pitch
// bend, per-note controllers and per-note management. These follow the voice playing that note.
inline bool isPerNoteMessage(const UmpMessage& m) {
   if (!isChannelVoice2(m)) return false;
   switch (m.status) {
   case kStatusRegPerNoteController:
   case kStatusAssignPerNoteController:
   case kStatusPerNotePitchBend:
   case kStatusPolyPressure:
   case kStatusPerNoteManagement:
      return true;
   default:
      return false;
   }
}
// Same note: when both messages carry a host note id (CLAP), the ids decide — the same key can be
// re-struck as a new note, and a CLAP note expression may name only the id. Otherwise it is the
// MIDI 2.0 sense: note number AND channel AND group, so two notes on the same key but different
// channels (MPE, or a MIDI 2.0 controller) are different notes with independent per-note state.
inline bool isSameNote(const UmpMessage& a, const UmpMessage& b) {
   if (a.noteId >= 0 && b.noteId >= 0) return a.noteId == b.noteId;
   return a.note == b.note && a.channel == b.channel && a.group == b.group;
}
inline bool isChannelMessage(const UmpMessage& m) {
   return m.messageType == kUmpTypeChannelVoice2 || m.messageType == kUmpTypeChannelVoice1;
}

// --- MIDI 1.0 → UMP -----------------------------------------------------------------------
// The MIDI 2.0 spec's Min-Center-Max upscaling (M2-104, "Translation"): values up to the centre are a
// plain shift, so 64 (7-bit) / 8192 (14-bit) land exactly on the 2.0 centre; above it the spare low
// bits repeat the source's, so the maximum (127 / 16383) lands exactly on full scale. A plain shift
// would leave velocity 127 at 0.992 and CC 127 at 0.992.
inline uint32_t scaleUp(uint32_t value, uint8_t srcBits, uint8_t dstBits) {
   const uint8_t  scaleBits = dstBits - srcBits;
   uint32_t       result = value << scaleBits;
   if (value <= (1u << (srcBits - 1))) return result;
   const uint8_t  repeatBits = srcBits - 1;
   uint32_t       repeat = value & ((1u << repeatBits) - 1);
   repeat = (scaleBits > repeatBits) ? repeat << (scaleBits - repeatBits) : repeat >> (repeatBits - scaleBits);
   while (repeat != 0) {
      result |= repeat;
      repeat >>= repeatBits;
   }
   return result;
}

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
      m.velocity = static_cast<uint16_t>(scaleUp(d2, 7, 16));
   } else if (statusNibble == 0x80 || (statusNibble == 0x90 && d2 == 0)) {
      m.messageType = kUmpTypeChannelVoice2;
      m.status = kStatusNoteOff;
   } else if (statusNibble == 0xA0) {
      m.messageType = kUmpTypeChannelVoice2;
      m.status = kStatusPolyPressure;
      m.data32 = scaleUp(d2, 7, 32); // the note is data1, set above
   } else if (statusNibble == 0xB0) {
      m.messageType = kUmpTypeChannelVoice2;
      m.status = kStatusControlChange;
      m.ccIndex = data1 & 0x7F;
      m.ccData = scaleUp(d2, 7, 32);
   } else if (statusNibble == 0xD0) {
      m.messageType = kUmpTypeChannelVoice2;
      m.status = kStatusChannelPressure;
      m.data32 = scaleUp(data1 & 0x7F, 7, 32);
   } else if (statusNibble == 0xE0) {
      m.messageType = kUmpTypeChannelVoice2;
      m.status = kStatusPitchBend;
      const uint32_t bend14 = (static_cast<uint32_t>(d2) << 7) | (data1 & 0x7F); // 14-bit, centre 0x2000
      m.data32 = scaleUp(bend14, 14, 32);
   }
   return m;
}

// --- MPE -------------------------------------------------------------------------------------
// MPE (MIDI Polyphonic Expression, MIDI 1.0) gives each note its own MEMBER channel and carries
// zone-wide controls on the zone's MASTER channel. PlainOldSynth's MPE switch assumes the standard
// lower zone: master = channel 1 (index 0), members = channels 2..16. Member-channel bend range
// defaults to ±48 semitones, master-channel bend to ±2.
constexpr uint8_t kMpeMasterChannel            = 0;
constexpr float   kMpeMemberBendRangeSemitones = 48.0f;

inline bool isMpeMemberChannel(bool mpe, uint8_t channel) { return mpe && channel != kMpeMasterChannel; }

// --- pitch bend ----------------------------------------------------------------------------
// A 32-bit MIDI 2.0 bend value (channel or per-note) → -1..+1, with 0x80000000 mapping to exactly 0.
// The top of the range is 1 - 2^-31, so up-bends fall a hair short of full scale, as the spec intends.
inline double pitchBendToUnit(uint32_t data32) {
   return (static_cast<double>(data32) - 2147483648.0) / 2147483648.0;
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
