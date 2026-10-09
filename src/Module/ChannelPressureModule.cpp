//
//  ChannelPressureModule.cpp
//
//  Pressure (aftertouch) → a held 0..1 control value. Vendored from PolygraphModularUnified's
//  ChannelPressureModule; PlainOldSynth extends it so it is the per-voice "Pressure" modulation
//  source:
//
//  - Polyphonic key pressure (UMP status 0xA, MIDI 1.0 0xA0, CLAP's Pressure note expression). One
//    instance lives in every voice and the voice only passes it the pressure for its own note
//    (VoiceStateManager::handleMIDIMessage), so each note is pressed independently.
//  - Channel pressure tracked PER CHANNEL, like PitchBendConverter's channel bend: every voice
//    remembers all sixteen channels (× groups) and follows the one its note was played on. One
//    channel = ordinary aftertouch on every note; a note per channel (MPE) = per-note pressure.
//  - Once a note has received its own poly pressure, that wins over the channel's. Both reset on
//    note-on (and per-note management Reset drops back to the channel value).
//  - De-zippered (ControlRamp); a fresh note snaps to its starting pressure.
//  - MPE (the MPE param; lower zone): a note on a member channel takes the HIGHER of its own pressure
//    and the master channel's, so zone-wide aftertouch reaches every note without cancelling a
//    harder-pressed one. (MPE leaves the combining rule to the synth; max is the common choice.)
//

#include "ChannelPressureModule.hpp"
#include "ControlRamp.hpp"
#include "pm_midi.h"
#include <algorithm>

namespace {

constexpr int kGroups   = 16;
constexpr int kChannels = 16;

struct ChannelPressureHelperStruct {
    float   channelPressure[kGroups][kChannels] = {};   // 0..1, last channel pressure per group/channel
    float   notePressure = 0.0f;                        // 0..1, poly pressure for this voice's note
    bool    hasNotePressure = false;                    // has this note had any poly pressure yet?
    uint8_t group = 0, channel = 0;                     // where this voice's note was played
    ControlRamp ramp;
};

float unitFromWord(uint32_t word) {
    return float(std::clamp(double(word) / 4294967295.0, 0.0, 1.0));
}

} // namespace

const void InitChannelPressure(uint32_t /*count*/, Module* module)
{
    module->state = new ChannelPressureHelperStruct();
}

const void DeinitChannelPressure(Module* module)
{
    delete static_cast<ChannelPressureHelperStruct*>(module->state);
    module->state = nullptr;
}

const void ResetChannelPressure(Module* module)
{
    *static_cast<ChannelPressureHelperStruct*>(module->state) = ChannelPressureHelperStruct();
}

const KPBuffPair* ProcessChannelPressure(uint64_t /*processCallID*/, uint32_t count, Module* module)
{
    auto* st = static_cast<ChannelPressureHelperStruct*>(module->state);
    float* outL = module->output.first->data;
    float* outR = module->output.second->data;

    float target = st->hasNotePressure ? st->notePressure
                                       : st->channelPressure[st->group][st->channel];
    if (pm::isMpeMemberChannel(module->paramByEnum(ChannelPressureParamItemMPE) >= 0.5f, st->channel))
        target = std::max(target, st->channelPressure[st->group][pm::kMpeMasterChannel]);
    st->ramp.fill(target, module->mSampleRate, outL, count);
    std::copy_n(outL, count, outR);

    return &module->output;
}

void MIDIChannelPressure(const pm::UmpMessage& message, Module* module)
{
    if (!pm::isChannelVoice2(message)) return;
    auto* st = static_cast<ChannelPressureHelperStruct*>(module->state);

    switch (message.status) {
    case pm::kStatusNoteOn:
        if (!pm::isNoteOn(message)) break;   // velocity 0 = note-off
        st->group = message.group & 0x0F;
        st->channel = message.channel & 0x0F;
        st->hasNotePressure = false;
        if (!module->legatoContinuation) st->ramp.snapNext();
        break;
    case pm::kStatusChannelPressure:
        st->channelPressure[message.group & 0x0F][message.channel & 0x0F] = unitFromWord(message.data32);
        break;
    case pm::kStatusPolyPressure:
        st->notePressure = unitFromWord(message.data32);
        st->hasNotePressure = true;
        break;
    case pm::kStatusPerNoteManagement:
        if (message.optionFlags & pm::kPerNoteMgmtReset) st->hasNotePressure = false;
        break;
    default:
        break;
    }
}
