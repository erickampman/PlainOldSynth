// PlainOldSynthParams.h — the host-facing parameter table: the single source of truth for the CLAP
// `params` extension, the GUI, CLAP state save/load, and the factory presets baked in by
// tools/gen_presets.py.
//
// Every host parameter is either
//   (a) a plain engine module param, written to one or two module addresses, or
//   (b) a derived/global control (Shape, VoiceMode, MonoLegato, Glide) that fans out over several
//       modules.
// Either way the actual engine writes live in one place: applyParam() in src/plugin.cpp.
//
// The module addresses are declared HERE rather than pulled from the vendored pm_addresses.h, so
// the exact voice graph this plugin wires is readable in one place.

#pragma once

#include <cstdint>
#include <cstring>

#include "CPPTypes.hpp"
#include "ParamAddressMacros.h"
#include "pm_compat.h"
#include "pm_params.h"

namespace plainoldsynth {

// ================================================================================================
// THE VOICE GRAPH (one copy per voice; wired by VoiceManager::buildDefaultPatch)
// ================================================================================================
//
//   pitchConverter1 ─┬─► slew1 ─┬─► triSaw1.pitchInput         (left oscillator)
//                    │          └─► triSaw2.pitchInput         (right oscillator)
//                    └─► lpf1.freqModInput                     (key tracking)
//
//   triSaw1 ─┬─► levelRaw1 ────► combineOsc1.in1 ──► pan1 (−1) ─┐
//            └─► comparator1 ─► levelPulse1 ──► combineOsc1.in2 │
//   triSaw2 ─┬─► levelRaw2 ────► combineOsc2.in1 ──► pan2 (+1) ─┴─► combineMix ─► lpf1 ─► combineVca ─► final1
//            └─► comparator2 ─► levelPulse2 ──► combineOsc2.in2                        ▲
//   adsr1 ──► lpf1.freqModInput2 (filter envelope)              adsr2 ────────────────┘ (VCA, Multiply)
//   lfo1 ──► triSaw1/2.modPitch (vibrato)      lfo2 ──► comparator1/2.threshCV (PWM)
//
// "One Tri/Saw oscillator with L/R detune" is two TriSaw instances — left and right — each with its
// own fine tune, hard-panned by pan1/pan2, so the detune lands as stereo width rather than as a
// single monophonic beat. The Comparator turns each tri/saw into a variable-width pulse; the raw and
// pulsed paths are then summed through two Level modules, which lets the Shape selector crossfade
// between Tri/Saw and Pulse WITHOUT re-patching the graph while audio is running (a topology change
// would have to travel through VoiceManager's lock-free connection command queue).
//
// There is no separate VCA module: Combine in Multiply mode is the VCA (osc × amp envelope), which
// is the same idiom PolygraphModularUnified's own default patch uses.

#define POS_MODULE(kind, instance) MODULE_PARAM_ADDRESS(ModuleParamKind##kind, ModuleInstance##instance, 0)
#define POS_ITEM(base, item)       MODULE_PARAM_FROM_BASE_AND_ITEM(base, item)

inline constexpr AUParameterAddress posPitchConverter1 = POS_MODULE(PitchConverter, 1);
inline constexpr AUParameterAddress posSlew1           = POS_MODULE(Slew, 1);
inline constexpr AUParameterAddress posTriSaw1         = POS_MODULE(TriSawOsc, 1);   // left oscillator
inline constexpr AUParameterAddress posTriSaw2         = POS_MODULE(TriSawOsc, 2);   // right oscillator
inline constexpr AUParameterAddress posComparator1     = POS_MODULE(Comparator, 1);  // left pulse
inline constexpr AUParameterAddress posComparator2     = POS_MODULE(Comparator, 2);  // right pulse
inline constexpr AUParameterAddress posLevelRaw1       = POS_MODULE(Level, 1);
inline constexpr AUParameterAddress posLevelPulse1     = POS_MODULE(Level, 2);
inline constexpr AUParameterAddress posLevelRaw2       = POS_MODULE(Level, 3);
inline constexpr AUParameterAddress posLevelPulse2     = POS_MODULE(Level, 4);
inline constexpr AUParameterAddress posCombineOsc1     = POS_MODULE(Combine, 1);     // raw + pulse, left
inline constexpr AUParameterAddress posCombineOsc2     = POS_MODULE(Combine, 2);     // raw + pulse, right
inline constexpr AUParameterAddress posCombineMix      = POS_MODULE(Combine, 3);     // L + R osc mix
inline constexpr AUParameterAddress posCombineVca      = POS_MODULE(Combine, 4);     // VCA (Multiply)
inline constexpr AUParameterAddress posPan1            = POS_MODULE(Pan, 1);
inline constexpr AUParameterAddress posPan2            = POS_MODULE(Pan, 2);
inline constexpr AUParameterAddress posAdsr1           = POS_MODULE(ADSR, 1);        // filter envelope
inline constexpr AUParameterAddress posAdsr2           = POS_MODULE(ADSR, 2);        // amplifier envelope
inline constexpr AUParameterAddress posLfo1            = POS_MODULE(LFO, 1);         // vibrato
inline constexpr AUParameterAddress posLfo2            = POS_MODULE(LFO, 2);         // pulse width
inline constexpr AUParameterAddress posLpf1            = POS_MODULE(ButterworthLowPassFilter, 1);
inline constexpr AUParameterAddress posFinal1          = POS_MODULE(FinalOutput, 1);

// VoiceManager's own (non-per-voice) parameters.
inline constexpr AUParameterAddress posMidiVoiceCount = POS_ITEM(POS_MODULE(MIDI, 1), MIDIParamItemVoiceCount);
inline constexpr AUParameterAddress posMidiLegato     = POS_ITEM(POS_MODULE(MIDI, 1), MIDIParamItemLegato);

// ================================================================================================
// Host parameters
// ================================================================================================

enum ParamIndex : uint32_t {
    kParamShape = 0,        // Tri / Saw / Pulse  (derived: osc Peak + raw/pulse path levels)
    kParamPulseWidth,       // Comparator threshold — sets the pulse duty cycle
    kParamCoarse,           // semitones, both oscillators
    kParamDetuneL,          // cents, left oscillator
    kParamDetuneR,          // cents, right oscillator
    kParamOscLevel,         // oscillator section output level

    kParamCutoff,           // low-pass cutoff in octaves (0 = A440)
    kParamResonance,        // low-pass Q
    kParamKeyTrack,         // 0..2; 1.0 = the cutoff tracks the keyboard 1:1
    kParamFilterEnvAmount,  // filter-envelope depth into the cutoff

    kParamFilterAttack, kParamFilterDecay, kParamFilterSustain, kParamFilterRelease,
    kParamAmpAttack,    kParamAmpDecay,    kParamAmpSustain,    kParamAmpRelease,

    kParamLfo1Rate, kParamLfo1Wave, kParamLfo1Depth,   // LFO 1 → oscillator pitch (vibrato)
    kParamLfo2Rate, kParamLfo2Wave, kParamLfo2Depth,   // LFO 2 → pulse width (PWM)

    kParamVoiceMode,        // Mono / Poly
    kParamMonoLegato,       // mono note-steal behaviour: Legato / Retrigger
    kParamGlide,            // portamento time (mono)
    kParamMasterLevel,
    kParamSpread,

    kParamIndexCount   // sentinel only — the number of parameters is kParamCount, below
};

enum PanelId : uint8_t {
    kPanelOsc = 0, kPanelFilter, kPanelFilterEnv, kPanelAmpEnv, kPanelLfo, kPanelGlobal, kPanelCount
};

inline constexpr const char* kPanelNames[kPanelCount] = {
    "Oscillator", "Filter", "Filter Env", "Amp Env", "LFO", "Global"
};

inline constexpr const char* kShapeChoices[]  = { "Tri", "Saw", "Pulse", nullptr };
inline constexpr const char* kWaveChoices[]   = { "Sine", "Tri", "Square", "Impulse", nullptr };
inline constexpr const char* kVoiceChoices[]  = { "Mono", "Poly", nullptr };
inline constexpr const char* kLegatoChoices[] = { "Legato", "Retrigger", nullptr };

// Shape indices (kParamShape).
enum Shape : int { kShapeTri = 0, kShapeSaw, kShapePulse };
// LFO wave indices — same order as the engine's LFOParamWave.
enum LfoWave : int { kLfoSine = 0, kLfoTri, kLfoSquare, kLfoImpulse };
// Voice mode / mono-legato indices.
enum VoiceMode : int { kVoiceMono = 0, kVoicePoly };
enum MonoLegato : int { kMonoLegatoTrue = 0, kMonoLegatoRetrigger };

struct ParamSpec {
    const char*        name;
    const char*        symbol;    // stable key: Presets/*.json and CLAP state files use this
    const char*        unit;      // display suffix; "" for none
    float              min, max, def;
    bool               logarithmic;
    bool               integer;   // stepped: integer readout, dropdown when choices != nullptr
    PanelId            panel;
    const char* const* choices;   // null unless this is a dropdown
};

inline constexpr ParamSpec kParams[] = {
    // --- Oscillator -------------------------------------------------------------------------
    { "Shape",       "shape",       "",   0.0f,     2.0f,   1.0f,  false, true,  kPanelOsc, kShapeChoices },
    { "Pulse Width", "pulseWidth",  "",  -0.9f,     0.9f,   0.0f,  false, false, kPanelOsc, nullptr },
    { "Coarse",      "coarse",      "st", -24.0f,   24.0f,   0.0f,  false, true,  kPanelOsc, nullptr },
    { "Detune L",    "detuneL",     "ct", -25.0f,   25.0f,  -7.0f,  false, false, kPanelOsc, nullptr },
    { "Detune R",    "detuneR",     "ct", -25.0f,   25.0f,   7.0f,  false, false, kPanelOsc, nullptr },
    { "Osc Level",   "oscLevel",    "",   0.0f,     1.0f,   0.8f,  false, false, kPanelOsc, nullptr },
    // --- Filter ----------------------------------------------------------------------------
    { "Cutoff",      "cutoff",      "oct", -6.0f,    5.4f,   2.0f,  false, false, kPanelFilter, nullptr },
    { "Resonance",   "resonance",   "",   1.0f,    20.0f,    1.0f,  false, false, kPanelFilter, nullptr },
    { "Key Track",   "keyTrack",    "",   0.0f,     2.0f,   0.0f,  false, false, kPanelFilter, nullptr },
    { "Env Amount",  "envAmount",   "",  -8.0f,     8.0f,   2.0f,  false, false, kPanelFilter, nullptr },
    // --- Filter envelope (adsr1) ------------------------------------------------------------
    { "Attack",      "fAttack",     "s",  0.001f,  10.0f,   0.010f, true, false, kPanelFilterEnv, nullptr },
    { "Decay",       "fDecay",      "s",  0.001f,  10.0f,   0.400f, true, false, kPanelFilterEnv, nullptr },
    { "Sustain",     "fSustain",    "",   0.0f,     1.0f,   0.300f, false, false, kPanelFilterEnv, nullptr },
    { "Release",     "fRelease",    "s",  0.001f,  10.0f,   0.400f, true, false, kPanelFilterEnv, nullptr },
    // --- Amplifier envelope (adsr2) ---------------------------------------------------------
    { "Attack",      "aAttack",     "s",  0.001f,  10.0f,   0.005f, true, false, kPanelAmpEnv, nullptr },
    { "Decay",       "aDecay",      "s",  0.001f,  10.0f,   1.000f, true, false, kPanelAmpEnv, nullptr },
    { "Sustain",     "aSustain",    "",   0.0f,     1.0f,   0.700f, false, false, kPanelAmpEnv, nullptr },
    { "Release",     "aRelease",    "s",  0.001f,  10.0f,   0.300f, true, false, kPanelAmpEnv, nullptr },
    // --- LFOs ------------------------------------------------------------------------------
    { "LFO 1 Rate",  "lfo1Rate",    "oct", -7.0f,   7.0f,   0.0f,  false, false, kPanelLfo, nullptr },
    { "LFO 1 Wave",  "lfo1Wave",    "",   0.0f,     3.0f,   0.0f,  false, true,  kPanelLfo, kWaveChoices },
    { "LFO 1 Depth", "lfo1Depth",   "",   0.0f,     2.0f,   0.0f,  false, false, kPanelLfo, nullptr },
    { "LFO 2 Rate",  "lfo2Rate",    "oct", -7.0f,   7.0f,   0.0f,  false, false, kPanelLfo, nullptr },
    { "LFO 2 Wave",  "lfo2Wave",    "",   0.0f,     3.0f,   1.0f,  false, true,  kPanelLfo, kWaveChoices },
    { "LFO 2 Depth", "lfo2Depth",   "",   0.0f,     0.99f,  0.0f,  false, false, kPanelLfo, nullptr },
    // --- Global ----------------------------------------------------------------------------
    { "Voices",      "voiceMode",   "",   0.0f,     1.0f,   1.0f,  false, true,  kPanelGlobal, kVoiceChoices },
    { "Mono Mode",   "monoLegato",  "",   0.0f,     1.0f,   0.0f,  false, true,  kPanelGlobal, kLegatoChoices },
    { "Glide",       "glide",       "s",  0.0f,     5.0f,   0.0f,  true,  false, kPanelGlobal, nullptr },
    { "Level",       "masterLevel", "",   0.0f,     1.0f,   0.7f,  false, false, kPanelGlobal, nullptr },
    { "Spread",      "spread",      "",   0.0f,     1.0f,   0.5f,  false, false, kPanelGlobal, nullptr },
};

inline constexpr uint32_t kParamCount = sizeof(kParams) / sizeof(kParams[0]);

// The enum sentinel and the table must describe the same parameter set.
static_assert(int(kParamCount) == int(kParamIndexCount),
              "ParamIndex and kParams have drifted apart");

// CLAP parameter ids are 1-based (0 is CLAP_INVALID_ID's neighbour and some hosts dislike it);
// the id of param index i is i + 1. Saved state and presets use the SYMBOL, not the id, so the
// ids themselves can be reordered freely.
inline constexpr uint32_t paramIdForIndex(uint32_t index) { return index + 1; }

inline int paramIndexForId(uint32_t id) {
    return (id >= 1 && id <= kParamCount) ? int(id - 1) : -1;
}

inline int paramIndexForSymbol(const char* symbol) {
    if (symbol == nullptr) return -1;
    for (uint32_t i = 0; i < kParamCount; ++i) {
        if (std::strcmp(kParams[i].symbol, symbol) == 0) return int(i);
    }
    return -1;
}

inline AUParameterAddress paramAddressForId(uint32_t id) {
    const int i = paramIndexForId(id);
    return i < 0 ? 0 : 0;
}

// Factory presets, generated at build time from Presets/*.json by tools/gen_presets.py.
struct PresetValue { const char* symbol; float value; };
struct PresetEntry { const char* name; int count; const PresetValue* values; };

} // namespace plainoldsynth
