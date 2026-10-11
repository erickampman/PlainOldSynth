//
//  VelocityConverterModule.cpp
//
//  MIDI note velocity → a held control value, with a floor (Min, percent) and a response curve
//  (Curve 0..1 → exponent 0.3..3.0; 0.259 is linear):  out = min + (1 - min) · velocity^exponent.
//  Vendored from PolygraphModularUnified's VelocityConverterModule. One instance per voice latches
//  that voice's note-on velocity. PlainOldSynth uses two (see PlainOldSynthParams.h):
//    1  the amplifier: multiplied into the VCA, shaped by the "Velocity" dropdown (Off = Min 100%)
//    2  the "Velocity" modulation source: linear, 0..1
//
//  PlainOldSynth's addition: de-zippering (ControlRamp). A mono legato note-on changes the level of a
//  note that is already sounding, and a VCA gain that steps clicks; a fresh note snaps instead.
//

#include "VelocityConverterModule.hpp"
#include "ControlRamp.hpp"
#include "pm_midi.h"

#include <algorithm>
#include <cmath>

namespace {

float exponentForT(float t) {
    constexpr float p_min = 0.3f;
    constexpr float p_max = 3.0f;
    return p_min + t * (p_max - p_min);
}

struct VelocityConverterHelperStruct {
    float curValue = 0.0f;
    ControlRamp ramp;
};

} // namespace

const void InitVelocityConverter(uint32_t /*count*/, Module* module)
{
    module->state = new VelocityConverterHelperStruct();
}

const void DeinitVelocityConverter(Module* module)
{
    delete static_cast<VelocityConverterHelperStruct*>(module->state);
    module->state = nullptr;
}

const void ResetVelocityConverter(Module* module)
{
    *static_cast<VelocityConverterHelperStruct*>(module->state) = VelocityConverterHelperStruct();
}

const KPBuffPair* ProcessVelocityConverter(uint64_t /*processCallID*/, uint32_t count, Module* module)
{
    auto* state = static_cast<VelocityConverterHelperStruct*>(module->state);
    float* outL = module->output.first->data;
    float* outR = module->output.second->data;

    state->ramp.fill(state->curValue, module->mSampleRate, outL, count);
    std::copy_n(outL, count, outR);

    return &module->output;
}

void MIDIVelocityConverter(const pm::UmpMessage& message, Module* module)
{
    if (!pm::isNoteOnOrOff(message)) return;

    const bool ignoreNoteOff = module->paramByEnum(VelocityParamItemIgnoreNoteOff) >= 0.5f;
    if (ignoreNoteOff && pm::isNoteOff(message)) return;

    auto* state = static_cast<VelocityConverterHelperStruct*>(module->state);
    const float min      = module->paramByEnum(VelocityParamItemMin) / 100.0f;
    const float exponent = exponentForT(module->paramByEnum(VelocityParamItemCurve));
    const float x        = float(message.velocity) / 65535.0f;
    state->curValue = min + (1.0f - min) * std::pow(x, exponent);

    // A fresh note starts at its own level; only a legato continuation (already sounding) ramps.
    if (pm::isNoteOn(message) && !module->legatoContinuation) state->ramp.snapNext();
}
