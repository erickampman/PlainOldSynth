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
//   pitchBend1 ──────────────► triSaw1/2.modPitch2             (channel + MIDI 2.0 per-note bend)
//
//   triSaw1 ─┬─► levelRaw1 ────► combineOsc1.in1 ──► pan1 (−1) ─┐
//            └─► comparator1 ─► levelPulse1 ──► combineOsc1.in2 │
//   triSaw2 ─┬─► levelRaw2 ────► combineOsc2.in1 ──► pan2 (+1) ─┴─► combineMix ─► lpf1 ─► combineVca ─► final1
//            └─► comparator2 ─► levelPulse2 ──► combineOsc2.in2                        ▲
//   adsr1 ──► lpf1.freqModInput2 (filter envelope)              adsr2 ────────────────┘ (VCA, Multiply)
//   lfo1 ──► triSaw1/2.modPitch (vibrato)      lfo2 ──► comparator1/2.threshCV (PWM)
//
// "One Tri/Saw oscillator with detune" is two TriSaw instances — left and right — hard-panned by
// pan1/pan2. The single Detune parameter is written to both with opposite sign (right +N cents,
// left -N cents), so it lands as stereo width rather than as a single monophonic beat. The
// Comparator turns each tri/saw into a variable-width pulse; the raw and
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
inline constexpr AUParameterAddress posPitchBend1      = POS_MODULE(PitchBendConverter, 1);   // → osc modPitch2
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

// Modulation sections. Each is one CVMixer: two patchable CV inputs with per-input levels and an
// Add/Multiply mode, whose output is routed to a selectable destination. The second input can also
// select a MIDI source: posCcConverter1 ("CC #1" = mod wheel), posChannelPressure1 ("Pressure":
// channel or polyphonic, per note) and posCcConverter2 ("Timbre": CC74 in Per Note mode, or the MIDI
// 2.0 per-note controller #74 / CLAP Brightness).
inline constexpr AUParameterAddress posCvMixer1        = POS_MODULE(CVMixer, 1);
inline constexpr AUParameterAddress posCvMixer2        = POS_MODULE(CVMixer, 2);
inline constexpr AUParameterAddress posCvMixer3        = POS_MODULE(CVMixer, 3);
inline constexpr AUParameterAddress posCcConverter1    = POS_MODULE(ControlChangeConverter, 1);
inline constexpr AUParameterAddress posCcConverter2    = POS_MODULE(ControlChangeConverter, 2);   // timbre
inline constexpr AUParameterAddress posChannelPressure1 = POS_MODULE(ChannelPressure, 1);
inline constexpr uint8_t kTimbreCC = 74;   // MPE's "slide"; MIDI 2.0 registered per-note controller #74

// What the MPE switch (kParamMpe) sets: every per-voice module that reads the MPE zone.
inline constexpr AUParameterAddress kMpeEngineSwitches[] = {
    POS_ITEM(posPitchBend1,       PitchBendConverterParamItemMPE),
    POS_ITEM(posChannelPressure1, ChannelPressureParamItemMPE),
    POS_ITEM(posCcConverter2,     CCParamItemMPE),
};

// VoiceManager's own (non-per-voice) parameters.
inline constexpr AUParameterAddress posMidiVoiceCount = POS_ITEM(POS_MODULE(MIDI, 1), MIDIParamItemVoiceCount);
inline constexpr AUParameterAddress posMidiLegato     = POS_ITEM(POS_MODULE(MIDI, 1), MIDIParamItemLegato);

// ================================================================================================
// Host parameters
// ================================================================================================

enum ParamIndex : uint32_t {
    kParamShape = 0,        // Tri/Saw or Pulse: WHICH signal path is audible, not the waveform shape
    kParamPulseWidth,       // Comparator threshold — sets the pulse duty cycle
    kParamPeak,             // 0.5 = symmetric triangle, ~1.0 = saw (the tri<->saw morph)
    kParamPitch,            // semitones, both oscillators. CONTINUOUS, not integer/stepped: a host
                            // that sees CLAP_PARAM_IS_STEPPED will snap automation to whole values.
    kParamDetune,           // cents of spread: right oscillator up, left down by the same amount
    kParamSelfMod,          // DX7-style phase self-modulation depth (the oscillator's own feedback)
    // (no Osc Level: the oscillator section's output level is now fixed, see initPatchConstants)

    kParamCutoff,           // low-pass cutoff in octaves (0 = A440)
    kParamResonance,        // low-pass Q
    kParamKeyTrack,         // 0..2; 1.0 = the cutoff tracks the keyboard 1:1
    // (no Env Amount: how much adsr1 moves the cutoff is a modulation section's job now)

    kParamFilterAttack, kParamFilterDecay, kParamFilterSustain, kParamFilterRelease,
    kParamAmpAttack,    kParamAmpDecay,    kParamAmpSustain,    kParamAmpRelease,

    // (no LFO Depth knobs: how much each LFO moves its destination is a modulation section's job)
    kParamLfo1Rate, kParamLfo1Wave,
    kParamLfo2Rate, kParamLfo2Wave,

    kParamVoiceMode,        // Mono / Poly
    kParamMonoLegato,       // mono note-steal behaviour: Legato / Retrigger
    kParamGlide,            // portamento time (mono)
    kParamMasterLevel,
    kParamSpread,

    // --- Modulation sections (3 x 6; see kModSectionCount / ModField). There were four; the fourth
    //     gave its screen slot to the Effect panel. -------------------------------------------
    kParamMod1SourceA, kParamMod1SourceB, kParamMod1LevelA, kParamMod1LevelB,
    kParamMod1Mode, kParamMod1Dest,
    kParamMod2SourceA, kParamMod2SourceB, kParamMod2LevelA, kParamMod2LevelB,
    kParamMod2Mode, kParamMod2Dest,
    kParamMod3SourceA, kParamMod3SourceB, kParamMod3LevelA, kParamMod3LevelB,
    kParamMod3Mode, kParamMod3Dest,

    // --- Effect (post voice sum; see src/Effects/ModDelay.hpp). Appended after the mod sections so
    //     every earlier parameter keeps its CLAP id. -------------------------------------------
    kParamFxMode,           // Off / Flanger / Chorus / Delay / Sync Delay
    kParamFxTime,           // 0..1, read through the mode (ms range, or a note division)
    kParamFxFeedback,       // -0.95..0.95; negative is the hollow flange
    kParamFxTone,           // 1 = undamped; lower darkens the repeats
    kParamFxRate,           // LFO rate, Hz
    kParamFxDepth,          // LFO depth, 0..1 of the mode's maximum swing
    kParamFxSpread,         // stereo width: LFO phase split (Flanger/Chorus), ping-pong (Delay modes)
    kParamFxMix,            // 0.5 = dry and wet both at unity

    // --- Appended so every earlier parameter keeps its CLAP id. ---------------------------------
    kParamMpe,              // MPE (lower zone): member-channel bend ±48 + master channel to all notes

    kParamIndexCount   // sentinel only — the number of parameters is kParamCount, below
};

enum PanelId : uint8_t {
    kPanelOsc = 0, kPanelFilter, kPanelEnv, kPanelLfo, kPanelGlobal,
    kPanelMod1, kPanelMod2, kPanelMod3,
    kPanelFx,
    kPanelCount
};

inline constexpr const char* kPanelNames[kPanelCount] = {
    "Oscillator", "Filter", "Envelopes", "LFO", "Global",
    "Mod 1", "Mod 2", "Mod 3", "Effect"
};

// The panel grid. Each panel is a rows x cols grid and every parameter declares the cell it sits in
// (ParamSpec::cell). That is what lets a column hold two stacked controls: the Global panel puts its
// two dropdowns one above the other in column 0, and each modulation section does the same in all
// three of its columns.
inline constexpr int kPanelRows[kPanelCount] = {
    2,  // Oscillator
    1,  // Filter
    2,  // Envelopes: filter ADSR on row 0, amp ADSR on row 1
    2,  // LFO: LFO 1 on row 0, LFO 2 on row 1
    2,  // Global
    2, 2, 2,      // Mod 1..3: sources / levels / mode+destination, two per column
    3,  // Effect: a 3 x 3 grid down the right-hand column (see kPanelBand)
};
inline constexpr int kPanelCols[kPanelCount] = {
    3,  // Oscillator: Shape / Pulse Width / Peak, then Coarse / Detune / Osc Level
    3,  // Filter: cutoff, resonance, key track
    4,  // Envelopes
    2,  // LFO: rate + wave per LFO (the depth knobs are now owned by the modulation sections)
    5,  // Global: Voices / Mono Mode, MPE, then Glide / Level / Spread
    3, 3, 3,      // Mod 1..3: input selections | input levels | mode + destination
    3,  // Effect
};

// How many KNOB-ROW units tall each panel needs on screen. Normally this equals kPanelRows, but
// Global is the exception: its second grid row holds only dropdowns, and a dropdown needs ~42px
// against a knob's ~104px, so both stacked dropdowns still fit inside a single knob row.
inline constexpr int kPanelRowUnits[kPanelCount] = { 2, 1, 2, 2, 1, 2, 2, 2, 0 };   // Effect: see kPanelBand

// Panels are drawn in three bands, top to bottom (see PlainOldSynthUI::layout):
//   0: Oscillator, Envelopes, LFO      1: Filter, Global      2: the three Mod sections
// Band heights are derived from the row units, so giving a panel an extra row re-flows the window.
// The Effect panel is the exception: it is a column down the right-hand side spanning bands 1 AND 2
// (kPanelBandSpanning), as wide as one Mod section. It contributes no row units of its own — its
// height is whatever bands 1 and 2 add up to — and layout() places it before dividing what is left
// of those two bands among their other panels.
inline constexpr int kBandCount = 3;
inline constexpr int kPanelBand[kPanelCount] = {
    0,  // Oscillator
    1,  // Filter
    0,  // Envelopes
    0,  // LFO
    1,  // Global
    2, 2, 2,      // Mod 1..3
    -1, // Effect: spans bands 1 and 2 (kPanelBandSpanning)
};
inline constexpr int kPanelBandSpanning = -1;

constexpr int rowUnitsInBand(int band)
{
    int units = 1;
    for (int p = 0; p < kPanelCount; ++p)
        if (kPanelBand[p] == band && kPanelRowUnits[p] > units)
            units = kPanelRowUnits[p];
    return units;
}
constexpr int totalRowUnits()
{
    int units = 0;
    for (int b = 0; b < kBandCount; ++b)
        units += rowUnitsInBand(b);
    return units;
}

// Triangle and saw are ONE selection. They are the same oscillator read at different Peak values
// (0.5 = symmetric triangle, ~1.0 = saw), so the tri<->saw morph belongs to the Peak knob, not to a
// menu entry: a separate "Saw" choice would just be Peak pinned to a constant.
inline constexpr const char* kShapeChoices[]  = { "Tri/Saw", "Pulse", nullptr };
// Guards the rule above: exactly two entries, so a "Saw" choice cannot quietly creep back in.
static_assert(kShapeChoices[0] != nullptr && kShapeChoices[1] != nullptr && kShapeChoices[2] == nullptr,
              "Shape must offer exactly Tri/Saw and Pulse");
// The engine's LFO can also produce an Impulse; this synth deliberately does not offer it.
inline constexpr const char* kWaveChoices[]   = { "Sine", "Tri", "Square", nullptr };
inline constexpr const char* kVoiceChoices[]  = { "Mono", "Poly", nullptr };
inline constexpr const char* kLegatoChoices[] = { "Legato", "Retrigger", nullptr };
// Same order as FxMode (src/Effects/ModDelay.hpp). "Off" is the default and is a true bypass.
inline constexpr const char* kMpeChoices[]    = { "Off", "On", nullptr };
inline constexpr const char* kFxModeChoices[] = { "Off", "Flanger", "Chorus", "Delay", "Sync Delay",
                                                  nullptr };

// Shape indices (kParamShape): this picks which signal path is audible, NOT the waveform's shape.
enum Shape : int { kShapeTriSaw = 0, kShapePulse };
// LFO wave indices — same order as the engine's LFOParamWave, minus the Impulse tail we do not offer.
enum LfoWave : int { kLfoSine = 0, kLfoTri, kLfoSquare };
// Voice mode / mono-legato indices.
enum VoiceMode : int { kVoiceMono = 0, kVoicePoly };
enum MonoLegato : int { kMonoLegatoTrue = 0, kMonoLegatoRetrigger };

// --- Modulation sections -------------------------------------------------------------------------
// Each section is one CVMixer: two inputs, a per-input level, an Add/Multiply mode, and one output
// routed to a destination. The first input's menu is deliberately the pitch-ish pair (LFO 1 / ADSR 1)
// and the second's is the other pair plus the performance controls (mod wheel, pressure, timbre), so
// a section reads as "primary + secondary". Like ModDest, sources are appended, never inserted: the
// index is what presets store.
inline constexpr const char* kModSourceAChoices[] = { "None", "LFO 1", "ADSR 1", nullptr };
inline constexpr const char* kModSourceBChoices[] = { "None", "LFO 2", "ADSR 2", "CC #1", "Pressure",
                                                      "Timbre", nullptr };
inline constexpr const char* kModModeChoices[]    = { "Add", "Multiply", nullptr };
// "Off" is what makes the destination column safe by default: without it every section would have to
// point somewhere, and several sections would fight over one input the moment the patch loads.
inline constexpr const char* kModDestChoices[]    = { "Off", "Osc Pitch", "Pulse Width", "Peak",
                                                      "Cutoff", "Self Mod", nullptr };
enum ModSourceA : int { kModSourceANone = 0, kModSourceALfo1, kModSourceAAdsr1 };
enum ModSourceB : int { kModSourceBNone = 0, kModSourceBLfo2, kModSourceBAdsr2, kModSourceBCC1,
                       kModSourceBPressure, kModSourceBTimbre };
enum ModMode : int { kModModeAdd = 0, kModModeMultiply };
// Appended, never inserted: these values are the Dest dropdown's stored indices, so adding one in the
// middle would silently repoint every existing patch.
enum ModDest : int { kModDestOff = 0, kModDestOscPitch, kModDestPulseWidth, kModDestPeak,
                     kModDestCutoff, kModDestSelfMod };

inline constexpr int kModSectionCount = 3;

// Which of a section's six parameters a given control is. The six are contiguous in ParamIndex and
// in the table, in exactly this order, so one lookup serves both the routing code and the UI.
enum ModField : int {
    kModFieldSourceA = 0, kModFieldSourceB, kModFieldLevelA, kModFieldLevelB,
    kModFieldMode, kModFieldDest, kModFieldCount
};
inline constexpr uint32_t modParamIndex(int section, ModField field)
{
    return uint32_t(kParamMod1SourceA) + uint32_t(section) * uint32_t(kModFieldCount)
         + uint32_t(field);
}

// Where one control sits in its panel's grid. `span` is how many grid rows the cell covers, which is
// how Glide/Level/Spread stay full height while Voices and Mono Mode split column 0 between them.
struct PanelCell { uint8_t row, col, span; };

struct ParamSpec {
    const char*        name;
    const char*        symbol;    // stable key: Presets/*.json and CLAP state files use this
    const char*        unit;      // display suffix; "" for none
    float              min, max, def;
    bool               logarithmic;
    bool               integer;   // stepped: integer readout, dropdown when choices != nullptr
    PanelId            panel;
    const char* const* choices;   // null unless this is a dropdown
    PanelCell          cell;      // where it sits inside that panel's grid
};

inline constexpr ParamSpec kParams[] = {
    // --- Oscillator: 2 rows x 3 cols ---------------------------------------------------------
    { "Shape",       "shape",       "",   0.0f,     1.0f,   0.0f,  false, true,  kPanelOsc, kShapeChoices, {0, 0, 1} },
    { "Pulse Width", "pulseWidth",  "",  -0.9f,     0.9f,   0.0f,  false, false, kPanelOsc, nullptr,       {0, 1, 1} },
    { "Peak",        "peak",        "",   0.01f,    0.99f,  0.99f, false, false, kPanelOsc, nullptr,       {0, 2, 1} },
    { "Pitch",       "pitch",       "st", -24.0f,   24.0f,   0.0f,  false, false, kPanelOsc, nullptr,       {1, 0, 1} },
    { "Detune",      "detune",      "ct",   0.0f,   25.0f,   7.0f,  false, false, kPanelOsc, nullptr,       {1, 1, 1} },
    { "Self Mod",    "selfMod",     "",   0.0f,     0.9f,   0.0f,  false, false, kPanelOsc, nullptr,       {1, 2, 1} },
    // --- Filter: 1 row x 3 cols (how far adsr1 moves the cutoff is a modulation section's job) --
    { "Cutoff",      "cutoff",      "oct", -6.0f,    5.4f,   2.0f,  false, false, kPanelFilter, nullptr,    {0, 0, 1} },
    { "Resonance",   "resonance",   "",   1.0f,    20.0f,    1.0f,  false, false, kPanelFilter, nullptr,    {0, 1, 1} },
    { "Key Track",   "keyTrack",    "",   0.0f,     2.0f,   0.0f,  false, false, kPanelFilter, nullptr,    {0, 2, 1} },
    // --- Envelopes: 2 rows x 4 cols, one envelope per row -------------------------------------
    // Row 0 — filter envelope (adsr1).
    { "Attack",      "fAttack",     "s",  0.001f,  10.0f,   0.010f, true, false, kPanelEnv, nullptr,       {0, 0, 1} },
    { "Decay",       "fDecay",      "s",  0.001f,  10.0f,   0.400f, true, false, kPanelEnv, nullptr,       {0, 1, 1} },
    { "Sustain",     "fSustain",    "",   0.0f,     1.0f,   0.300f, false, false, kPanelEnv, nullptr,      {0, 2, 1} },
    { "Release",     "fRelease",    "s",  0.001f,  10.0f,   0.400f, true, false, kPanelEnv, nullptr,       {0, 3, 1} },
    // Row 1 — amplifier envelope (adsr2).
    { "Attack",      "aAttack",     "s",  0.001f,  10.0f,   0.005f, true, false, kPanelEnv, nullptr,       {1, 0, 1} },
    { "Decay",       "aDecay",      "s",  0.001f,  10.0f,   1.000f, true, false, kPanelEnv, nullptr,       {1, 1, 1} },
    { "Sustain",     "aSustain",    "",   0.0f,     1.0f,   0.700f, false, false, kPanelEnv, nullptr,      {1, 2, 1} },
    { "Release",     "aRelease",    "s",  0.001f,  10.0f,   0.300f, true, false, kPanelEnv, nullptr,       {1, 3, 1} },
    // --- LFOs: 2 rows x 2 cols, rate + wave per LFO (depths live in the mod sections now) ------
    { "LFO 1 Rate",  "lfo1Rate",    "oct", -7.0f,   7.0f,   0.0f,  false, false, kPanelLfo, nullptr,       {0, 0, 1} },
    { "LFO 1 Wave",  "lfo1Wave",    "",   0.0f,     2.0f,   0.0f,  false, true,  kPanelLfo, kWaveChoices, {0, 1, 1} },
    { "LFO 2 Rate",  "lfo2Rate",    "oct", -7.0f,   7.0f,   0.0f,  false, false, kPanelLfo, nullptr,       {1, 0, 1} },
    { "LFO 2 Wave",  "lfo2Wave",    "",   0.0f,     2.0f,   1.0f,  false, true,  kPanelLfo, kWaveChoices, {1, 1, 1} },
    // --- Global: 2 rows x 4 cols. The two dropdowns stack in column 0; the three knobs span both
    //     rows so they keep their full height beside them. ------------------------------------
    { "Voices",      "voiceMode",   "",   0.0f,     1.0f,   1.0f,  false, true,  kPanelGlobal, kVoiceChoices,  {0, 0, 1} },
    { "Mono Mode",   "monoLegato",  "",   0.0f,     1.0f,   0.0f,  false, true,  kPanelGlobal, kLegatoChoices, {1, 0, 1} },
    // Glide is LINEAR, not logarithmic, and deliberately so: its minimum is 0 ("no glide"), and a log
    // law through zero is undefined — it produced the "nan s" readout. Use the fine-drag modifier for
    // the short end of the range instead. Knob::configure also refuses a log law with min <= 0, so a
    // future parameter cannot reintroduce the same NaN.
    { "Glide",       "glide",       "s",  0.0f,     5.0f,   0.0f,  false, false, kPanelGlobal, nullptr,        {0, 2, 2} },
    { "Level",       "masterLevel", "",   0.0f,     1.0f,   0.7f,  false, false, kPanelGlobal, nullptr,        {0, 3, 2} },
    { "Spread",      "spread",      "",   0.0f,     1.0f,   0.5f,  false, false, kPanelGlobal, nullptr,        {0, 4, 2} },

    // --- Modulation sections: 3 panels, each 2 rows x 3 cols ------------------------------------
    // Column 0 = the two input selections, column 1 = their levels, column 2 = Add/Multiply above
    // the destination. Level defaults are 0, so a section is silent until dialled in; section 1
    // ships pointed at the filter envelope's one-time job (adsr1 -> cutoff) so the default patch
    // still has its filter sweep.
    { "In A",    "mod1InA",    "",   0.0f,   2.0f,   2.0f,  false, true,  kPanelMod1, kModSourceAChoices, {0, 0, 1} },
    { "In B",    "mod1InB",    "",   0.0f,   5.0f,   0.0f,  false, true,  kPanelMod1, kModSourceBChoices, {1, 0, 1} },
    { "Level A", "mod1LevelA", "",  -1.0f,   1.0f,   0.25f, false, false, kPanelMod1, nullptr,            {0, 1, 1} },
    { "Level B", "mod1LevelB", "",  -1.0f,   1.0f,   0.0f,  false, false, kPanelMod1, nullptr,            {1, 1, 1} },
    { "Mode",    "mod1Mode",   "",   0.0f,   1.0f,   0.0f,  false, true,  kPanelMod1, kModModeChoices,    {0, 2, 1} },
    { "Dest",    "mod1Dest",   "",   0.0f,   5.0f,   4.0f,  false, true,  kPanelMod1, kModDestChoices,    {1, 2, 1} },

    { "In A",    "mod2InA",    "",   0.0f,   2.0f,   0.0f,  false, true,  kPanelMod2, kModSourceAChoices, {0, 0, 1} },
    { "In B",    "mod2InB",    "",   0.0f,   5.0f,   0.0f,  false, true,  kPanelMod2, kModSourceBChoices, {1, 0, 1} },
    { "Level A", "mod2LevelA", "",  -1.0f,   1.0f,   0.0f,  false, false, kPanelMod2, nullptr,            {0, 1, 1} },
    { "Level B", "mod2LevelB", "",  -1.0f,   1.0f,   0.0f,  false, false, kPanelMod2, nullptr,            {1, 1, 1} },
    { "Mode",    "mod2Mode",   "",   0.0f,   1.0f,   0.0f,  false, true,  kPanelMod2, kModModeChoices,    {0, 2, 1} },
    { "Dest",    "mod2Dest",   "",   0.0f,   5.0f,   0.0f,  false, true,  kPanelMod2, kModDestChoices,    {1, 2, 1} },

    { "In A",    "mod3InA",    "",   0.0f,   2.0f,   0.0f,  false, true,  kPanelMod3, kModSourceAChoices, {0, 0, 1} },
    { "In B",    "mod3InB",    "",   0.0f,   5.0f,   0.0f,  false, true,  kPanelMod3, kModSourceBChoices, {1, 0, 1} },
    { "Level A", "mod3LevelA", "",  -1.0f,   1.0f,   0.0f,  false, false, kPanelMod3, nullptr,            {0, 1, 1} },
    { "Level B", "mod3LevelB", "",  -1.0f,   1.0f,   0.0f,  false, false, kPanelMod3, nullptr,            {1, 1, 1} },
    { "Mode",    "mod3Mode",   "",   0.0f,   1.0f,   0.0f,  false, true,  kPanelMod3, kModModeChoices,    {0, 2, 1} },
    { "Dest",    "mod3Dest",   "",   0.0f,   5.0f,   0.0f,  false, true,  kPanelMod3, kModDestChoices,    {1, 2, 1} },


    // --- Effect: 3 rows x 3 cols, the right-hand column. Mode and the delay itself on top, the LFO
    //     below, the stereo/output pair at the bottom (one empty cell, under Mode). Off by default, so presets that predate the effect
    //     (and name none of these symbols) load exactly as before. Time is 0..1 because its meaning
    //     depends on Mode; its readouts come from formatFxTime(). ------------------------------
    { "Mode",     "fxMode",     "",    0.0f,   4.0f,  0.0f,  false, true,  kPanelFx, kFxModeChoices, {0, 0, 1} },
    { "Time",     "fxTime",     "",    0.0f,   1.0f,  0.5f,  false, false, kPanelFx, nullptr,        {0, 1, 1} },
    { "Feedback", "fxFeedback", "",   -0.95f,  0.95f, 0.0f,  false, false, kPanelFx, nullptr,        {0, 2, 1} },
    { "Tone",     "fxTone",     "",    0.0f,   1.0f,  1.0f,  false, false, kPanelFx, nullptr,        {1, 0, 1} },
    { "Rate",     "fxRate",     "Hz",  0.05f, 10.0f,  0.5f,  true,  false, kPanelFx, nullptr,        {1, 1, 1} },
    { "Depth",    "fxDepth",    "",    0.0f,   1.0f,  0.5f,  false, false, kPanelFx, nullptr,        {1, 2, 1} },
    { "Spread",   "fxSpread",   "",    0.0f,   1.0f,  1.0f,  false, false, kPanelFx, nullptr,        {2, 1, 1} },
    { "Mix",      "fxMix",      "",    0.0f,   1.0f,  0.5f,  false, false, kPanelFx, nullptr,        {2, 2, 1} },

    // --- Appended: MPE sits in the Global panel, beside Voices / Mono Mode. Off by default, so presets
    //     that predate it load exactly as before. -------------------------------------------------
    { "MPE",      "mpe",        "",    0.0f,   1.0f,  0.0f,  false, true,  kPanelGlobal, kMpeChoices, {0, 1, 1} },
};

inline constexpr uint32_t kParamCount = sizeof(kParams) / sizeof(kParams[0]);

// The enum sentinel and the table must describe the same parameter set.
static_assert(int(kParamCount) == int(kParamIndexCount),
              "ParamIndex and kParams have drifted apart");

// ...in the same ORDER: kParams is indexed by ParamIndex, so a swapped pair compiles fine and silently
// cross-wires two controls (Effect Mix and Spread once did exactly that). Spot-check the entries most
// at risk — each block's first and last, where rows get added.
constexpr bool symbolIs(ParamIndex index, const char* symbol)
{
    const char* a = kParams[index].symbol;
    while (*a != '\0' && *a == *symbol) { ++a; ++symbol; }
    return *a == *symbol;
}
static_assert(symbolIs(kParamShape, "shape") && symbolIs(kParamSelfMod, "selfMod")
              && symbolIs(kParamVoiceMode, "voiceMode") && symbolIs(kParamSpread, "spread")
              && symbolIs(kParamMod1SourceA, "mod1InA") && symbolIs(kParamMod3Dest, "mod3Dest")
              && symbolIs(kParamFxMode, "fxMode") && symbolIs(kParamFxSpread, "fxSpread")
              && symbolIs(kParamFxMix, "fxMix") && symbolIs(kParamMpe, "mpe"),
              "kParams rows are out of ParamIndex order");

// Every control must land inside its panel's grid, and cover at least one row of it.
constexpr bool cellsFitTheirPanels()
{
    for (uint32_t i = 0; i < kParamCount; ++i) {
        const ParamSpec& spec = kParams[i];
        if (spec.cell.row >= kPanelRows[spec.panel] || spec.cell.col >= kPanelCols[spec.panel])
            return false;
        if (spec.cell.span == 0 || spec.cell.row + spec.cell.span > kPanelRows[spec.panel])
            return false;
    }
    return true;
}
static_assert(cellsFitTheirPanels(), "a parameter's grid cell falls outside its panel");

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
