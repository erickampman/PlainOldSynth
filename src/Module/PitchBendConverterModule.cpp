//
//  PitchBendConverterModule.cpp
//
//  MIDI 2.0 pitch bend → a bipolar octaves signal (patch into an oscillator's pitch-mod input).
//  Vendored from PolygraphModularUnified's PitchBendConverterModule; PlainOldSynth extends it with:
//
//  - Per-note pitch bend (UMP status 0x6). One instance lives in every voice, and the voice only
//    passes it per-note messages for the note it is sounding (VoiceStateManager::handleMIDIMessage),
//    so this module simply applies whatever per-note bend reaches it. Range: PerNoteSemitones
//    (default ±48, the MPE convention).
//  - Channel bend tracked PER CHANNEL. Channel bends reach every voice, including idle ones; each
//    voice remembers all sixteen channels (× groups) and applies the one its note was played on. With
//    everything on one channel that is the ordinary behaviour; with a note per channel (MPE, or a
//    MIDI 2.0 controller) each note follows its own channel's bend. Range: QuarterTones / 2 semitones
//    (default ±2).
//  - De-zippering (ControlRamp). A fresh note snaps instead of ramping, so it starts at its bent
//    pitch.
//  - MPE (the MPE param; lower zone, see pm::kMpeMasterChannel). A note on a member channel uses the
//    member bend range (±48 by default) and ADDS the master channel's bend (±2 by default): the
//    controller's own pitch wheel moves every note in the zone on top of each note's own slide.
//  - Pitch bend sensitivity (RPN 0), as MIDI 1.0 CC 101/100 + 6/38 or a MIDI 2.0 registered
//    controller message, overrides a channel's range. With MPE on, RPN 0 on any member channel sets
//    every member channel, as MPE specifies.
//  - Those overrides go back to the defaults, as MPE specifies for a zone (re)configuration, when an
//    MPE Configuration Message arrives (RPN 6 on channel 1 or 16; only its reset is honoured, the
//    zone layout is the switch's) and when the MPE switch itself is flipped. A range a controller set
//    for one mode must not leak into the other.
//
//  Per-note bend resets to centre on every note-on (a new note, a steal or a mono retrigger) and on a
//  per-note management message with the Reset flag. Per-note bend sent BEFORE its note-on is
//  dropped: no voice owns that note yet.
//

#include "PitchBendConverterModule.hpp"
#include "ControlRamp.hpp"
#include "pm_midi.h"
#include <algorithm>

namespace {

constexpr int kGroups   = 16;
constexpr int kChannels = 16;

// MIDI 1.0 RPN selection state for one channel. 127/127 is the RPN "null" (nothing selected).
struct RpnState {
    uint8_t msb = 127, lsb = 127;
    uint8_t semitones = 0;   // the last data-entry MSB for RPN 0, so a later LSB (cents) can add to it
};

struct PitchBendConverterHelperStruct {
    float   channelBend[kGroups][kChannels] = {};   // -1..+1, last channel bend per group/channel
    float   rangeOverride[kGroups][kChannels];      // semitones from RPN 0; < 0 = not set (default)
    RpnState rpn[kGroups][kChannels];
    float   perNoteBend = 0.0f;                     // -1..+1, for the note this voice is playing
    uint8_t group = 0, channel = 0;                 // where this voice's note was played
    ControlRamp ramp;                               // octaves, as output
    bool    lastMpe = false;                        // the MPE switch as last seen (syncMpeSwitch)

    PitchBendConverterHelperStruct() { std::fill_n(&rangeOverride[0][0], kGroups * kChannels, -1.0f); }
};

bool mpeOn(Module* module) { return module->paramByEnum(PitchBendConverterParamItemMPE) >= 0.5f; }

// A channel's bend range in semitones: RPN 0 if a controller set one, else the member default under
// MPE, else the ordinary channel range (the QuarterTones param is in quarter tones, so / 2).
float channelRange(const PitchBendConverterHelperStruct& st, Module* module, uint8_t group, uint8_t channel) {
    if (st.rangeOverride[group][channel] >= 0.0f) return st.rangeOverride[group][channel];
    if (pm::isMpeMemberChannel(mpeOn(module), channel)) return pm::kMpeMemberBendRangeSemitones;
    return module->paramByEnum(PitchBendConverterParamItemQuarterTones) * 0.5f;
}

float targetOctaves(const PitchBendConverterHelperStruct& st, Module* module) {
    const uint8_t g = st.group, c = st.channel;
    float semis = st.channelBend[g][c] * channelRange(st, module, g, c)
                + st.perNoteBend * module->paramByEnum(PitchBendConverterParamItemPerNoteSemitones);
    if (pm::isMpeMemberChannel(mpeOn(module), c)) {
        const uint8_t m = pm::kMpeMasterChannel;
        semis += st.channelBend[g][m] * channelRange(st, module, g, m);   // the zone's pitch wheel
    }
    return semis / 12.0f;
}

void clearBendRanges(PitchBendConverterHelperStruct& st, uint8_t group) {
    std::fill_n(&st.rangeOverride[group][0], kChannels, -1.0f);
}

// Flipping the MPE switch reconfigures the zone, so every RPN 0 override goes back to the defaults.
void syncMpeSwitch(PitchBendConverterHelperStruct& st, Module* module) {
    const bool on = mpeOn(module);
    if (on == st.lastMpe) return;
    st.lastMpe = on;
    std::fill_n(&st.rangeOverride[0][0], kGroups * kChannels, -1.0f);
}

// The MPE Configuration Message (RPN 6) is sent on a zone's master channel: 1 (lower) or 16 (upper).
bool isMpeConfigChannel(uint8_t channel) { return channel == 0 || channel == 15; }

// RPN 0 arrived for (group, channel): set its range, or every member channel's under MPE.
void setBendRange(PitchBendConverterHelperStruct& st, Module* module, uint8_t group, uint8_t channel,
                  float semitones) {
    if (pm::isMpeMemberChannel(mpeOn(module), channel)) {
        for (uint8_t c = 0; c < kChannels; ++c)
            if (c != pm::kMpeMasterChannel) st.rangeOverride[group][c] = semitones;
    } else {
        st.rangeOverride[group][channel] = semitones;
    }
}

// MIDI 1.0 RPN: CC 101/100 select, CC 6 = data entry MSB (semitones for RPN 0), CC 38 = LSB (cents).
void handleRpnCC(PitchBendConverterHelperStruct& st, Module* module, uint8_t group, uint8_t channel,
                 uint8_t cc, uint8_t value) {
    RpnState& rpn = st.rpn[group][channel];
    switch (cc) {
    case 101: rpn.msb = value; break;
    case 100: rpn.lsb = value; break;
    case 6:
        if (rpn.msb == 0 && rpn.lsb == 6 && isMpeConfigChannel(channel)) {   // MPE Configuration
            clearBendRanges(st, group);
            break;
        }
        if (rpn.msb != 0 || rpn.lsb != 0) break;
        rpn.semitones = value;
        setBendRange(st, module, group, channel, float(value));
        break;
    case 38:
        if (rpn.msb != 0 || rpn.lsb != 0) break;
        setBendRange(st, module, group, channel, float(rpn.semitones) + float(value) / 100.0f);
        break;
    default:
        break;
    }
}

} // namespace

const void InitPitchBendConverter(uint32_t /*count*/, Module* module)
{
    module->state = new PitchBendConverterHelperStruct();
}

const void DeinitPitchBendConverter(Module* module)
{
    delete static_cast<PitchBendConverterHelperStruct*>(module->state);
    module->state = nullptr;
}

const void ResetPitchBendConverter(Module* module)
{
    *static_cast<PitchBendConverterHelperStruct*>(module->state) = PitchBendConverterHelperStruct();
}

const KPBuffPair* ProcessPitchBendConverter(uint64_t /*processCallID*/, uint32_t count, Module* module)
{
    auto* st = static_cast<PitchBendConverterHelperStruct*>(module->state);
    float* outL = module->output.first->data;
    float* outR = module->output.second->data;

    syncMpeSwitch(*st, module);
    // Recomputed every block, so a range change applies (ramped) without waiting for the next bend.
    st->ramp.fill(targetOctaves(*st, module), module->mSampleRate, outL, count);
    std::copy_n(outL, count, outR);

    return &module->output;
}

void MIDIPitchBendConverter(const pm::UmpMessage& message, Module* module)
{
    if (!pm::isChannelVoice2(message)) return;
    auto* st = static_cast<PitchBendConverterHelperStruct*>(module->state);
    const uint8_t group   = message.group & 0x0F;
    const uint8_t channel = message.channel & 0x0F;
    syncMpeSwitch(*st, module);   // before any RPN below lands under the old mode

    switch (message.status) {
    case pm::kStatusNoteOn:
        st->group = group;
        st->channel = channel;
        st->perNoteBend = 0.0f;
        // A legato continuation is already sounding, so it ramps; anything else starts in tune.
        if (!module->legatoContinuation) st->ramp.snapNext();
        break;
    case pm::kStatusPitchBend:
        st->channelBend[group][channel] = float(pm::pitchBendToUnit(message.data32));
        break;
    case pm::kStatusControlChange:
        handleRpnCC(*st, module, group, channel, message.ccIndex, uint8_t(message.ccData >> 25));
        break;
    case pm::kStatusRegController:
        // MIDI 2.0 RPN 0 (bank 0, index 0). Translated from MIDI 1.0, the 14-bit semitones/cents pair
        // is the top 14 bits of the 32-bit value: semitones in the upper 7, cents in the lower 7.
        // RPN 6 (bank 0, index 6) is the MPE Configuration Message.
        if (message.note == 0 && message.ccIndex == 0) {
            const uint32_t data14 = message.ccData >> 18;
            setBendRange(*st, module, group, channel, float(data14 >> 7) + float(data14 & 0x7F) / 100.0f);
        } else if (message.note == 0 && message.ccIndex == 6 && isMpeConfigChannel(channel)) {
            clearBendRanges(*st, group);   // MPE Configuration Message
        }
        break;
    case pm::kStatusPerNotePitchBend:
        st->perNoteBend = float(pm::pitchBendToUnit(message.data32));
        break;
    case pm::kStatusPerNoteManagement:
        if (message.optionFlags & pm::kPerNoteMgmtReset) st->perNoteBend = 0.0f;
        break;
    default:
        break;
    }
}
