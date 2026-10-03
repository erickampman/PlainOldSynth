//
//  ADSRModule.mm
//
//  Created by Eric Kampman on 5/31/25.
//

#include "ADSRHelper.hpp"
#include "pm_midi.h"

#include "ADSRModule.hpp"

struct ADSRHelperStruct {
    ADSRHelper env;
    float prevGateValue = 0.0f;
    uint8_t lastNoteNumber = 255;   // 255 = no previous note
    bool isSamePitch = false;

    explicit ADSRHelperStruct(double sampleRate)
        : env(sampleRate) {}
};

const void InitADSRModule(uint32_t count, Module* module)
{
    auto* state = new ADSRHelperStruct(1.0 / module->mFrequencyScale);
    module->state = state;
}
const void DeinitADSRModule(Module* module)
{
    delete static_cast<ADSRHelperStruct*>(module->state);
    module->state = nullptr;
}

const void SampleRateADSRModule(Module* module, double newSampleRate)
{
    if (auto* st = static_cast<ADSRHelperStruct*>(module->state)) {
        st->env.onSampleRateChanged(static_cast<float>(newSampleRate));
    }
}

const KPBuffPair* ProcessADSRModule(uint64_t processCallID, uint32_t count, Module* module)
{
    auto* state = static_cast<ADSRHelperStruct*>(module->state);
    float* outL = module->output.first->data;
    float* outR = module->output.second->data;

    if (module->hasInput(ADSRParamItemGateInput)) {
        auto gateInputPair = module->retrieveInput(ADSRParamItemGateInput, processCallID, count);
        float* gate = gateInputPair->first->data;

        for (uint32_t i = 0; i < count; ++i) {
            float currentGate = gate[i];
            float prevGate = state->prevGateValue;

            if (prevGate < 0.5f && currentGate >= 0.5f) {
                // Rising edge → note on
                state->env.noteOn();
            } else if (prevGate >= 0.5f && currentGate < 0.5f) {
                // Falling edge → note off
                state->env.noteOff();
            }

            // Save current gate for next sample
            state->prevGateValue = currentGate;

            // Compute envelope
            float envVal = state->env.nextSample();
            outL[i] = envVal;
            outR[i] = envVal;
        }

    } else {
        // No gate input — driven by MIDI
        state->env.nextValueSet(count, outL);
        std::memcpy(outR, outL, sizeof(float) * count);
    }

    return &module->output;
}

void MIDIProcADSRModule(const pm::UmpMessage& message, Module* module)
{
    auto* state = static_cast<ADSRHelperStruct*>(module->state);

    if (module->hasInput(ADSRParamItemGateInput)) {
        LOG_MODULE("MIDIProcADSRModule has gate input", module->id);
        return; // ignore MIDI if we have a gate input.
    }

    LOG_MODULE("MIDIProcADSRModule entrypoint state(%s)", module->id, state->env.getStateString());

    if (pm::isNoteOn(message)) {
        // Track pitch so repeated notes on the same voice can be treated as legato.
        uint8_t incomingNote = message.note;
        state->isSamePitch = (state->lastNoteNumber != 255 &&
                              state->lastNoteNumber == incomingNote);
        state->lastNoteNumber = incomingNote;

        // ADSRParamItemLegato index: item value minus 1 (params are 1-based).
        uint8_t legatoMode = static_cast<uint8_t>(module->params[ADSRParamItemLegato - 1]);
        if (legatoMode == 1) {
            // LegatoRetrigger: keep current envelope value, restart attack.
            state->env.noteOnLegato();
        } else if (legatoMode == 2) {
            // LegatoTrue: suppress retrigger only while the envelope is in a
            // sustaining phase (Attack / Decay / Sustain).  If it's Idle or
            // still in Release, start (or restart) the attack from the current
            // level so there is no click and no silence on note changes.
            auto envState = state->env.getState();
            bool sustaining = (envState == ADSRHelper::State::Attack  ||
                               envState == ADSRHelper::State::Decay   ||
                               envState == ADSRHelper::State::Sustain);
            if (!sustaining) {
                state->env.noteOnLegato();
            }
        } else {
            // Steal (default): hard retrigger from zero — but if the same pitch
            // is re-triggered on this voice, use a legato retrigger instead to
            // avoid a click from snapping the envelope back to zero.
            if (state->isSamePitch) {
                state->env.noteOnLegato();
            } else {
                state->env.noteOn();
            }
        }
    } else {
        state->env.processMIDI(message);
    }

    LOG_MODULE("MIDIProcADSRModule exit state(%s)", module->id, state->env.getStateString());
}

const void ParamADSR(Module *module, AUParameterAddress address, AUValue value)
{
    auto state = static_cast<ADSRHelperStruct*>(module->state);
        
    switch (MODULE_ITEM_FROM_ADDRESS(address)) {
    case ADSRParamItemAttack:
        state->env.setAttackTime(value);
        break;
    case ADSRParamItemDecay:
        state->env.setDecayTime(value);
        break;
    case ADSRParamItemSustain:
        state->env.setSustainLevel(value);
        break;
    case ADSRParamItemRelease:
        state->env.setReleaseTime(value);
        break;
    case ADSRParamItemLegato:
        // Stored in module->params[] by the Voice::setParameter() caller;
        // MIDIProcADSRModule reads it directly from there.
        break;
    case ADSRParamItemLinear:
        state->env.setLinearMode(value != 0.0f);
        break;
    }
}

const void ADSRModuleReset(Module* module) {
    auto helper = static_cast<ADSRHelperStruct*>(module->state);
    helper->env.reset();
    helper->prevGateValue = 0.0f;
    helper->lastNoteNumber = 255;
    helper->isSamePitch = false;
}
