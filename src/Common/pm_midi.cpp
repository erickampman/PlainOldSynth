// pm_midi.cpp — non-inline helpers for the spec-derived UMP message type.

#include "pm_midi.h"

#include <cstdio>

namespace pm {

UmpMessage makeNoteOn(uint8_t note, uint16_t velocity, uint8_t channel, uint8_t group) {
   UmpMessage m;
   m.messageType = kUmpTypeChannelVoice2;
   m.group       = group;
   m.status      = kStatusNoteOn;
   m.channel     = channel;
   m.note        = note;
   m.velocity    = velocity ? velocity : 1; // a real note-on is non-zero velocity
   m.attributeType = kAttrNone;
   return m;
}

UmpMessage makeNoteOff(uint8_t note, uint8_t channel, uint8_t group) {
   UmpMessage m;
   m.messageType = kUmpTypeChannelVoice2;
   m.group       = group;
   m.status      = kStatusNoteOff;
   m.channel     = channel;
   m.note        = note;
   m.velocity    = 0;
   m.attributeType = kAttrNone;
   return m;
}

bool containsNoteNumber(const UmpMessage& m, uint8_t& note) {
   if (isNoteOnOrOff(m) || isPerNoteMessage(m)) {
      note = m.note;
      return true;
   }
   return false;
}

// UMP MIDI 2.0 channel-voice layout (2 words):
//   word0: [mt:4][group:4][status:4][channel:4][note:8][attributeType:8]
//   word1: [velocity:16][attribute:16]
// The low byte of word0 is the attribute type for note on/off, the controller index for per-note
// controllers (0x0, 0x1) and the option flags for per-note management (0xF). For control change the
// controller index is in the note byte instead.
UmpMessage decodeUmp(const uint32_t words[2]) {
   const uint32_t w0 = words[0];
   const uint32_t w1 = words[1];
   UmpMessage m;
   m.messageType   = (w0 >> 28) & 0x0F;
   m.group         = (w0 >> 24) & 0x0F;
   m.status        = (w0 >> 20) & 0x0F;
   m.channel       = (w0 >> 16) & 0x0F;
   m.note          = (w0 >> 8) & 0xFF;
   m.attributeType = w0 & 0xFF;
   m.velocity      = (w1 >> 16) & 0xFFFF;
   m.attribute     = w1 & 0xFFFF;
   const bool perNoteController = m.status == kStatusRegPerNoteController
                               || m.status == kStatusAssignPerNoteController;
   m.ccIndex       = perNoteController ? (w0 & 0xFF)          // per-note controller: low byte
                                       : ((w0 >> 8) & 0xFF);  // control change: the note byte
   m.ccData        = w1;               // word1 is the full 32-bit value
   m.data32        = w1;               // pressure / pitch bends: word1 is the 32-bit value
   m.optionFlags   = (m.status == kStatusPerNoteManagement) ? (w0 & 0xFF) : 0;
   return m;
}

std::optional<UmpMessage> makeNoteOnMsgFromOnMsg(const UmpMessage& m) {
   if (!isChannelVoice2(m) || m.status != kStatusNoteOn)
      return std::nullopt;
   return m;
}
std::optional<UmpMessage> makeNoteOffMsgFromOnMsg(const UmpMessage& m) {
   if (!isChannelVoice2(m) || m.status != kStatusNoteOn)
      return std::nullopt;
   UmpMessage r = m;
   r.status = kStatusNoteOff;
   return r;
}
std::optional<UmpMessage> makeNoteOffMsgFromOffMsg(const UmpMessage& m) {
   if (!isChannelVoice2(m) || m.status != kStatusNoteOff)
      return std::nullopt;
   return m;
}

std::string toString(const UmpMessage& m) {
   char buf[96];
   std::snprintf(buf, sizeof(buf), "UMP mt=%u grp=%u st=0x%X ch=%u note=%u vel=%u attr(%u)=0x%X",
                 m.messageType, m.group, m.status, m.channel, m.note, m.velocity, m.attributeType,
                 m.attribute);
   return std::string(buf);
}

} // namespace pm
