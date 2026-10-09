// clap_ump.h — the CLAP boundary: decode CLAP note / MIDI2 events into pm::UmpMessage.
// This is the single seam where host events become the framework's internal MIDI type
// (and where a future netmidi2 *network* UMP stream would decode in the same way).

#ifndef PM_CLAP_UMP_H
#define PM_CLAP_UMP_H

#include <clap/clap.h>

#include <cmath>
#include <optional>

#include "pm_midi.h"

namespace pm {

// clap_event_note_t → UMP (integer-key path; no microtonal attribute).
inline UmpMessage fromClapNote(const clap_event_note_t& ev, bool on) {
   UmpMessage m;
   m.messageType   = kUmpTypeChannelVoice2;
   m.status        = on ? kStatusNoteOn : kStatusNoteOff;
   m.channel       = ev.channel < 0 ? 0 : static_cast<uint8_t>(ev.channel & 0x0F);
   m.note          = ev.key < 0 ? 0 : static_cast<uint8_t>(ev.key & 0x7F);
   double v        = ev.velocity < 0.0 ? 0.0 : (ev.velocity > 1.0 ? 1.0 : ev.velocity);
   m.velocity      = on ? static_cast<uint16_t>(v * 65535.0 + 0.5) : 0;
   m.attributeType = kAttrNone;
   m.noteId        = ev.note_id;
   return m;
}

// Registered per-note controller #74 — the MIDI 2.0 counterpart of MPE's CC74 "timbre".
constexpr uint8_t kRegPerNoteBrightness = 74;

// clap_event_note_expression_t → the equivalent MIDI 2.0 per-note message, so CLAP-dialect hosts
// (Bitwig; VST3 note expression via clap-wrapper) reach the same engine path as UMP:
//   TUNING     → per-note pitch bend. CLAP gives semitones, UMP a fraction of the bend range, so the
//                caller passes the engine's CURRENT per-note range and the tuning comes out exact.
//                Tuning beyond ±range is clamped to it.
//   PRESSURE   → polyphonic key pressure
//   BRIGHTNESS → registered per-note controller #74
// VOLUME, PAN, VIBRATO and EXPRESSION have no destination yet (nullopt). So does an event that
// names neither a note id nor a key: a wildcard over every note is not supported.
inline std::optional<UmpMessage> fromClapNoteExpression(const clap_event_note_expression_t& ev,
                                                        double perNoteBendRangeSemitones) {
   if (ev.note_id < 0 && ev.key < 0) return std::nullopt;

   auto unitToWord = [](double unit) {   // 0..1 → 0..0xFFFFFFFF
      unit = unit < 0.0 ? 0.0 : (unit > 1.0 ? 1.0 : unit);
      return static_cast<uint32_t>(std::llround(unit * 4294967295.0));
   };

   UmpMessage m;
   m.messageType = kUmpTypeChannelVoice2;
   m.channel     = ev.channel < 0 ? 0 : static_cast<uint8_t>(ev.channel & 0x0F);
   m.note        = ev.key < 0 ? 0 : static_cast<uint8_t>(ev.key & 0x7F);
   m.noteId      = ev.note_id;

   switch (ev.expression_id) {
   case CLAP_NOTE_EXPRESSION_TUNING: {
      if (!(perNoteBendRangeSemitones > 0.0)) return std::nullopt;
      double unit = ev.value / perNoteBendRangeSemitones;            // -1..+1 of the range
      unit = unit < -1.0 ? -1.0 : (unit > 1.0 ? 1.0 : unit);
      m.status = kStatusPerNotePitchBend;
      const double word = std::round(2147483648.0 + unit * 2147483648.0);
      m.data32 = word >= 4294967295.0 ? 0xFFFFFFFFu : static_cast<uint32_t>(word);   // +1 → the top
      return m;
   }
   case CLAP_NOTE_EXPRESSION_PRESSURE:
      m.status = kStatusPolyPressure;
      m.data32 = unitToWord(ev.value);
      return m;
   case CLAP_NOTE_EXPRESSION_BRIGHTNESS:
      m.status  = kStatusRegPerNoteController;
      m.ccIndex = kRegPerNoteBrightness;
      m.ccData  = unitToWord(ev.value);
      return m;
   default:
      return std::nullopt;
   }
}

// clap_event_midi2_t → UMP (raw words; carries the 7.9 pitch attribute → microtonal).
inline UmpMessage fromClapMidi2(const clap_event_midi2_t& ev) {
   return decodeUmp(ev.data); // ev.data is uint32_t[4]; the channel-voice message is words 0..1
}

// clap_event_midi_t (MIDI 1.0, 3 bytes) → UMP note/CC. messageType left 0 (ignored) if unhandled.
inline UmpMessage fromClapMidi1(const clap_event_midi_t& ev) {
   return fromMidi1Bytes(ev.data[0], ev.data[1], ev.data[2]);
}

} // namespace pm

#endif // PM_CLAP_UMP_H
