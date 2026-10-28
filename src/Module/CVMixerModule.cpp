//
//  CVMixerModule.cpp — see header. Combine two patchable CV inputs and two virtual MIDI-controller
//  inputs (CC / Aftertouch / Pitch Bend / Pitch / Velocity) into one CV output, add or multiply.
//
//  Add:      every input contributes Level·input (identity 0).
//  Multiply: the two PATCHED inputs are plain gains, Level·input, so the product of a zero-mean
//            source (an LFO) and anything else stays zero-mean — the destination swings around
//            where its knob sets it and is never pushed off centre. The two virtual MIDI inputs are
//            DEPTH factors, 1 + Level·(input−1), so their Level 0 is "off" (identity 1) with no
//            discontinuity, and they too leave a zero-mean product zero-mean.
//  A detached physical input is skipped (identity), never a 0 that would collapse a multiply. An
//  idle Multiply (nothing connected/raised) sits at the multiplicative identity, 1 — not 0. Levels
//  are one-pole smoothed (Smoothing seconds) to de-zip.
//

#include "CVMixerModule.hpp"

#include <cmath>
#include <cstdint>

#include "pm_midi.h"

struct CVMixerState {
    // Latched MIDI-derived values (updated by the MIDI callback, read at process time).
    float cc[128]     = {0};    // per-controller value, normalised 0..1
    float aftertouch  = 0.0f;   // channel pressure, 0..1
    float pitchBend   = 0.0f;   // −1..+1 (centre = 0)
    float velocity    = 0.0f;   // last note-on velocity, 0..1
    float note        = 60.0f;  // last note-on pitch as a (possibly fractional) MIDI note number
    bool  hasNote     = false;  // no note yet → Pitch source reads 0 (sits on the reference)

    // Smoothed per-input levels + the two MIDI inputs' L/R skews (toward the param targets).
    float sIn1 = 0.0f, sIn2 = 0.0f, sA = 0.0f, sB = 0.0f, sSkewA = 0.0f, sSkewB = 0.0f;
    bool  primed = false;       // first process snaps the smoothers to their targets (no ramp-in)
};

const void InitCVMixerModule(uint32_t /*count*/, Module* module)
{
    module->state = new CVMixerState();
}

const void DeinitCVMixerModule(Module* module)
{
    delete static_cast<CVMixerState*>(module->state);
    module->state = nullptr;
}

void MIDICVMixerModule(const pm::UmpMessage& message, Module* module)
{
    auto* st = static_cast<CVMixerState*>(module->state);
    if (!st || message.messageType != pm::kUmpTypeChannelVoice2) return;

    switch (message.status) {
        case pm::kStatusNoteOn:
            {   // every note-on, velocity 0 included (MIDI 2.0: that is a note-on, not a note-off)
                st->velocity = static_cast<float>(message.velocity) / 65535.0f;
                // Prefer the MIDI 2.0 7.9 fractional pitch when the note-on carried one, else the
                // integer note number. attribute is 7.9 fixed point → /512 gives the note + fraction.
                st->note = (message.attributeType == pm::kAttrPitch7_9)
                             ? static_cast<float>(message.attribute) / 512.0f
                             : static_cast<float>(message.note);
                st->hasNote = true;
            }
            break;   // note-off (0x8) is ignored entirely
        case pm::kStatusChannelPressure:
            st->aftertouch = static_cast<float>(message.data32) / 4294967296.0f;   // 32-bit → 0..1
            break;
        case pm::kStatusPitchBend:
            // 32-bit, centre 0x80000000 → −1..+1.
            st->pitchBend = static_cast<float>(static_cast<double>(message.data32) / 4294967296.0 * 2.0 - 1.0);
            break;
        case pm::kStatusControlChange:
            st->cc[message.ccIndex & 0x7f] = static_cast<float>(message.ccData) / 4294967296.0f;
            break;
        default:
            break;
    }
}

// Current value of a virtual MIDI input, given its Source selector. CC uses the slot's own CC
// number; Pitch is bipolar per-octave, centred on the shared Reference Note; Velocity is 0..1.
static inline float cvMixerSourceValue(int source, const CVMixerState* st, int ccNumber, float refNote)
{
    switch (source) {
        case 0:  return st->cc[ccNumber & 0x7f];                             // CC
        case 1:  return st->aftertouch;                                      // Aftertouch (channel)
        case 2:  return st->pitchBend;                                       // Pitch Bend
        case 3:  return st->hasNote ? (st->note - refNote) / 12.0f : 0.0f;   // Pitch (octaves re refNote)
        case 4:  return st->velocity;                                        // Velocity
        default: return 0.0f;
    }
}

const KPBuffPair* ProcessCVMixerModule(uint64_t processCallID, uint32_t count, Module* module)
{
    auto* st = static_cast<CVMixerState*>(module->state);

    float* outL = module->output.first->data;
    float* outR = module->output.second->data;

    const float lvl1 = module->paramByEnum(CVMixerParamItemInput1Level);
    const float lvl2 = module->paramByEnum(CVMixerParamItemInput2Level);
    const int   srcA = static_cast<int>(module->paramByEnum(CVMixerParamItemMIDISourceA));
    const float lA   = module->paramByEnum(CVMixerParamItemMIDILevelA);
    const int   ccA  = static_cast<int>(module->paramByEnum(CVMixerParamItemCCNumberA));
    const float refNoteA = module->paramByEnum(CVMixerParamItemReferenceNoteA);
    const int   srcB = static_cast<int>(module->paramByEnum(CVMixerParamItemMIDISourceB));
    const float lB   = module->paramByEnum(CVMixerParamItemMIDILevelB);
    const int   ccB  = static_cast<int>(module->paramByEnum(CVMixerParamItemCCNumberB));
    const float refNoteB = module->paramByEnum(CVMixerParamItemReferenceNoteB);
    const float skewA = module->paramByEnum(CVMixerParamItemSkewA);
    const float skewB = module->paramByEnum(CVMixerParamItemSkewB);
    const bool  mult      = module->paramByEnum(CVMixerParamItemMode) > 0.5f;
    const float smoothing = module->paramByEnum(CVMixerParamItemSmoothing);

    const bool in1Conn = module->isInputConnected(CVMixerParamItemInput1);
    const bool in2Conn = module->isInputConnected(CVMixerParamItemInput2);
    // The two patched inputs are processed as STEREO (independent L/R). The MIDI sources are scalar
    // and apply identically to both channels.
    const KPBuffPair* in1Pair = module->retrieveInput(CVMixerParamItemInput1, processCallID, count);
    const KPBuffPair* in2Pair = module->retrieveInput(CVMixerParamItemInput2, processCallID, count);
    const float* in1L = in1Pair->first->data;
    const float* in1R = in1Pair->second->data;
    const float* in2L = in2Pair->first->data;
    const float* in2R = in2Pair->second->data;

    // Virtual sources are control-rate (latched by the MIDI callback), so evaluate once per block.
    const float vA = cvMixerSourceValue(srcA, st, ccA, refNoteA);
    const float vB = cvMixerSourceValue(srcB, st, ccB, refNoteB);

    // One-pole level smoothing. coeff 1 == instant (Smoothing 0). mFrequencyScale == 1/sampleRate.
    const float coeff = smoothing > 0.0f
                          ? 1.0f - std::exp(-static_cast<float>(module->mFrequencyScale) / smoothing)
                          : 1.0f;
    if (!st->primed) {
        st->sIn1 = lvl1; st->sIn2 = lvl2; st->sA = lA; st->sB = lB;
        st->sSkewA = skewA; st->sSkewB = skewB; st->primed = true;
    }

    // One channel of the mix. x1/x2 are that channel's patched-input samples; vAch/vBch are the
    // virtual sources' values FOR THIS CHANNEL (Skew pans them per channel — see the loop). The
    // patched inputs use the shared sIn1/sIn2 (their stereo is already in the signal).
    //   Add:      identity 0 — a skipped/detached input contributes nothing.
    //   Multiply: a patched input is the gain Level·x. It used to be the depth factor 1 + Level·(x−1)
    //             like the virtual inputs, but on a bipolar source that factor has a DC term of
    //             1−Level: LFO 1 at Level 0.58 times the mod wheel pushed Osc Pitch up by 0.42·wheel
    //             instead of only deepening the vibrato. A virtual input stays a depth factor (Level 0
    //             or idle = 1, continuous). A DETACHED patched input is skipped (identity 1), never
    //             fed its zero buffer, so a lone connected input passes through at its Level; an idle
    //             multiply sits at 1 (the user's call, 2026-09-23).
    auto mixChannel = [&](float x1, float x2, float vAch, float vBch) -> float {
        if (!mult) {
            float o = 0.0f;
            if (in1Conn) o += st->sIn1 * x1;
            if (in2Conn) o += st->sIn2 * x2;
            o += st->sA * vAch;
            o += st->sB * vBch;
            return o;
        }
        float prod = 1.0f;
        if (in1Conn) prod *= st->sIn1 * x1;
        if (in2Conn) prod *= st->sIn2 * x2;
        prod *= 1.0f + st->sA * (vAch - 1.0f);
        prod *= 1.0f + st->sB * (vBch - 1.0f);
        return prod;
    };

    for (uint32_t i = 0; i < count; ++i) {
        st->sIn1   += (lvl1  - st->sIn1)   * coeff;
        st->sIn2   += (lvl2  - st->sIn2)   * coeff;
        st->sA     += (lA    - st->sA)     * coeff;
        st->sB     += (lB    - st->sB)     * coeff;
        st->sSkewA += (skewA - st->sSkewA) * coeff;
        st->sSkewB += (skewB - st->sSkewB) * coeff;

        // Skew pans the (mono) MIDI source between L and R, scaled by the source's OWN value, so a
        // source value of 0 produces no L/R difference: value_L = v·(1+Skew), value_R = v·(1−Skew).
        outL[i] = mixChannel(in1L[i], in2L[i], vA * (1.0f + st->sSkewA), vB * (1.0f + st->sSkewB));
        outR[i] = mixChannel(in1R[i], in2R[i], vA * (1.0f - st->sSkewA), vB * (1.0f - st->sSkewB));
    }

    return &module->output;
}
