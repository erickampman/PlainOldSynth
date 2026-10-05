// pos_module_specs.cpp — PlainOldSynth's module registry.
//
// getModuleInitSpecs() is the whole "factory": Voice::initializeModules() walks it and instantiates
// one Module per row, per voice. ModuleInitSpec carries the function pointers (init/deinit/process/
// midi/param/reset/sampleRate) that make up a module — there is no per-module class.
//
// Rows are KINDS with an instance-pool size; the loop at the bottom expands each into concrete
// addresses (kind<<16 | instance<<8). PlainOldSynth needs only the modules its fixed voice graph
// uses — see PlainOldSynthParams.h for the graph. Vendored from PolygraphModularUnified's
// src/Voice/pm_module_specs.cpp and trimmed to just these kinds; every kind value and address
// encoding is unchanged, so presets/patches stay address-compatible with the larger engine.

#include "ModuleInitSpecs.hpp"

#include "ADSRModule.hpp"
#include "CCConverterModule.hpp"
#include "ComparatorModule.hpp"
#include "CombineModule.hpp"
#include "CVMixerModule.hpp"
#include "FinalModule.hpp"
#include "LFOModule.hpp"
#include "LPFilterModule.hpp"
#include "LevelModule.hpp"
#include "PanModule.hpp"
#include "PitchConverterModule.hpp"
#include "SlewModule.hpp"
#include "TriSawModule.hpp"
#include "pm_param_data.h"

// One row per KIND: the instance-1 address (which encodes the kind), the param group + procs, and
// how many instances to pre-instantiate in every voice's pool. Pools are created up front, at init,
// and never allocated while audio is running.
struct KindPool {
    AUParameterAddress               base;   // instance-1 address (kind in bits 16..23, instance in 8..15)
    const ModuleParamConstantsGroup* group;
    ModuleInitProc       init;
    ModuleDeinitProc     deinit;
    ModuleProcessProc    process;
    ModuleMIDIProc       midi;
    ModuleParamProc      param;
    ModuleResetProc      reset;
    ModuleSampleRateProc sampleRate;
    uint8_t              instances;   // pool size for this kind
    ModuleSharedStateCreateProc sharedStateCreateProc = nullptr;   // unused here (no shared-state modules)
};

const std::vector<ModuleInitSpec>& getModuleInitSpecs() {
    static const KindPool pools[] = {
        // MIDI note → pitch CV. This is what makes MIDI 2.0 7.9-pitch NoteOns microtonal: its MIDI
        // proc switches on message.attributeType and routes kAttrPitch7_9 through
        // PitchConverter::Pitch79ToOctaveOffset(). The oscillators below read this CV instead of
        // tracking MIDI themselves, because their PitchInput is patched (see TriSawModule.cpp:
        // MIDITriSawOscillator returns early when PitchInput is connected).
        {MODULE_PARAM_ADDRESS(ModuleParamKindPitchConverter, ModuleInstance1, 0),
         &pitchConverterModuleConstantsGroup, InitPitchConverter, DeinitPitchConverter,
         ProcessPitchConverter, MIDIPitchConverter, nullptr, nullptr, nullptr, 1},

        // Portamento / glide.
        {MODULE_PARAM_ADDRESS(ModuleParamKindSlew, ModuleInstance1, 0),
         &slewModuleConstantsGroup, InitSlewModule, DeinitSlewModule, ProcessSlewModule,
         nullptr, nullptr, nullptr, nullptr, 1},

        // The oscillator: two instances (left / right) so their fine tune can differ.
        {MODULE_PARAM_ADDRESS(ModuleParamKindTriSawOsc, ModuleInstance1, 0),
         &triSawConstantsGroup, InitTriSawOscillator, DeinitTriSawOscillator,
         ProcessTriSawOscillator, MIDITriSawOscillator, nullptr, ResetTriSawOscillator, nullptr, 2},

        // Pulse shaper: comparator on each oscillator (tri/saw in, variable-width pulse out).
        {MODULE_PARAM_ADDRESS(ModuleParamKindComparator, ModuleInstance1, 0),
         &comparatorConstantsGroup, InitComparatorModule, DeinitComparatorModule,
         ProcessComparatorModule, nullptr, nullptr, ResetComparatorModule, nullptr, 2},

        // Raw / pulse path levels (4 = 2 paths × 2 channels); these are the Shape crossfade.
        {MODULE_PARAM_ADDRESS(ModuleParamKindLevel, ModuleInstance1, 0),
         &levelConstantsGroup, nullptr, nullptr, ProcessLevelModule,
         nullptr, nullptr, nullptr, nullptr, 4},

        // 1, 2: raw+pulse per channel (Add). 3: the L/R osc mix. 4: the VCA (Multiply).
        {MODULE_PARAM_ADDRESS(ModuleParamKindCombine, ModuleInstance1, 0),
         &combineConstantsGroup, nullptr, nullptr, ProcessCombineModule,
         nullptr, nullptr, nullptr, nullptr, 4},

        {MODULE_PARAM_ADDRESS(ModuleParamKindPan, ModuleInstance1, 0),
         &panConstantsGroup, nullptr, nullptr, ProcessPanModule,
         nullptr, nullptr, nullptr, nullptr, 2},

        // 1: filter envelope. 2: amplifier envelope (the VCA's second input).
        {MODULE_PARAM_ADDRESS(ModuleParamKindADSR, ModuleInstance1, 0),
         &adsrConstantsGroup, InitADSRModule, DeinitADSRModule, ProcessADSRModule,
         MIDIProcADSRModule, ParamADSR, ADSRModuleReset, SampleRateADSRModule, 2},

        // 1: vibrato (→ oscillator pitch). 2: PWM (→ comparator threshold).
        {MODULE_PARAM_ADDRESS(ModuleParamKindLFO, ModuleInstance1, 0),
         &lfoConstantsGroup, InitLFOModule, DeinitLFOModule, ProcessLFOModule,
         nullptr, nullptr, ResetLFOModule, nullptr, 2},

        // 24 dB/oct Butterworth low-pass.
        {MODULE_PARAM_ADDRESS(ModuleParamKindButterworthLowPassFilter, ModuleInstance1, 0),
         &lpfConstantsGroup, InitLPFilterModule, DeinitLPFilterModule, ProcessLPFilterModule,
         nullptr, nullptr, nullptr, SampleRateLPFilterModule, 1},

        // Output stage: DC block, M/S spread, level, clip.
        {MODULE_PARAM_ADDRESS(ModuleParamKindFinalOutput, ModuleInstance1, 0),
         &finalConstantsGroup, InitFinalModule, DeinitFinalModule, ProcessFinalModule,
         MIDIFinalModule, nullptr, nullptr, SampleRateFinalModule, 1},

        // Modulation sections: one CVMixer per section (3). Each has two patchable CV inputs with
        // per-input levels and an Add/Multiply mode, and its output is routed to a selectable
        // destination — see PlainOldSynthParams.h's kParamMod* and plugin.cpp's applyModRouting.
        {MODULE_PARAM_ADDRESS(ModuleParamKindCVMixer, ModuleInstance1, 0),
         &cvMixerConstantsGroup, InitCVMixerModule, DeinitCVMixerModule, ProcessCVMixerModule,
         MIDICVMixerModule, nullptr, nullptr, nullptr, 3},

        // MIDI CC → CV, so "CC #1" (mod wheel) is an ordinary connectable modulation source. One
        // instance serves every section: its output fans out. It must be fed MIDI even when nothing
        // is patched to it, so buildDefaultPatch bumps its activeCount (see there).
        {MODULE_PARAM_ADDRESS(ModuleParamKindControlChangeConverter, ModuleInstance1, 0),
         &ccConverterConstantsGroup, InitCCConverter, DeinitCCConverter, ProcessCCConverter,
         MIDICCConverter, nullptr, nullptr, nullptr, 1},
    };

    static const std::vector<ModuleInitSpec> specs = [] {
        std::vector<ModuleInitSpec> v;
        for (const KindPool& p : pools) {
            const AUParameterAddress kind = (p.base >> 16) & 0xff;
            const AUParameterAddress base = (p.base >> 8)  & 0xff;   // = ModuleInstance1
            for (uint8_t i = 0; i < p.instances; ++i) {
                const AUParameterAddress addr = (kind << 16) | ((base + i) << 8);
                v.push_back({addr, p.group, p.init, p.deinit, p.process, p.midi,
                             p.param, p.reset, p.sampleRate, p.sharedStateCreateProc});
            }
        }
        return v;
    }();
    return specs;
}
