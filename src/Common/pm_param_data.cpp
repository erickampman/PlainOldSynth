#include "pm_param_data.h"

// Per-parameter `unit` → the UI's control type (mirrors how PolygraphModular's ParamGroupData.m used
// it): U_IDX = dropdown (Indexed), U_BOOL = toggle (Boolean), everything else = slider (the unit is a
// display hint). Signal INPUTS are marked with ParamMetaDataInput and drawn as ports — their unit is
// U_GEN and unused (PM uses CustomUnit there; the port marker is what matters). Units are copied from
// PolygraphModular's ParamGroupData.m: the base modules from main, the IFS/Strike/ModalBank modules
// from the next-version branch (where PM integrated them). cvMixer is IFSynth-only (not in PM) — by
// inspection.
namespace {
constexpr AudioUnitParameterUnit U_GEN  = kAudioUnitParameterUnit_Generic;
constexpr AudioUnitParameterUnit U_IDX  = kAudioUnitParameterUnit_Indexed;            // dropdown
constexpr AudioUnitParameterUnit U_BOOL = kAudioUnitParameterUnit_Boolean;            // toggle
constexpr AudioUnitParameterUnit U_SEC  = kAudioUnitParameterUnit_Seconds;
constexpr AudioUnitParameterUnit U_CENT = kAudioUnitParameterUnit_Cents;
constexpr AudioUnitParameterUnit U_SEMI = kAudioUnitParameterUnit_RelativeSemiTones;
constexpr AudioUnitParameterUnit U_GAIN = kAudioUnitParameterUnit_LinearGain;
constexpr AudioUnitParameterUnit U_OCT  = kAudioUnitParameterUnit_Octaves;
constexpr AudioUnitParameterUnit U_CUST = kAudioUnitParameterUnit_CustomUnit;
[[maybe_unused]] constexpr AudioUnitParameterUnit U_CC = kAudioUnitParameterUnit_MIDIController;
constexpr AudioUnitParameterUnit U_FADER = kAudioUnitParameterUnit_MixerFaderCurve;
constexpr AudioUnitParameterUnit U_DB   = kAudioUnitParameterUnit_Decibels;
constexpr AudioUnitParameterUnit U_HZ   = kAudioUnitParameterUnit_Hertz;
constexpr AudioUnitParameterUnit U_PCT  = kAudioUnitParameterUnit_Percent;
constexpr AudioUnitParameterUnit U_MS   = kAudioUnitParameterUnit_Milliseconds;
constexpr AudioUnitParameterUnit U_NOTE = kAudioUnitParameterUnit_MIDINoteNumber;      // knob shows note name, snaps to semitone
}

// --- Indexed-param value labels: what each choice means (shown by the picker / menu instead of a
// bare 0,1,2…). Sourced from PolygraphModular's per-module SwiftUI views + our DSP enums; the order
// MUST match the enum values (index = value - min). Null-terminated. ---
static const char* const kAddMultiply[]  = {"Add", "Multiply", nullptr};                       // Combine type · CVMixer mode
static const char* const kCVMixerSource[]= {"CC", "Aftertouch", "Pitch Bend", "Pitch", "Velocity", nullptr}; // CVMixer virtual MIDI input source
static const char* const kOscSyncType[]  = {"Down", "Up", "Both", nullptr};                     // SinOsc modSyncType (OscSyncType)
static const char* const kAdsrLegato[]   = {"Steal", "Legato Retrigger", "Legato True", nullptr}; // ADSR + ADR
static const char* const kComparatorType[]= {"Full Clip", "Top", "Bottom", nullptr};
static const char* const kFormantGain[]  = {"0 dB","6 dB","12 dB","18 dB","24 dB","32 dB","36 dB","40 dB","44 dB","48 dB", nullptr};
static const char* const kRezNotchMode[]  = {"Bump", "Notch", "All Pass", nullptr}; // Rez dropped (folded into Bump)
static const char* const kFilterMode[]    = {"Passthrough", "Low Pass", "High Pass", "Band Pass", nullptr};
static const char* const kDoubleRezSlope[] = {"12 dB/oct", "24 dB/oct", nullptr};                // DoubleRezBandpass slope
static const char* const kStrikeType[]   = {"Mallet", "Noise", nullptr};
static const char* const kStrikeContact[]= {"Note-Relative", "Wall Clock", nullptr};
static const char* const kIfsOscSync[]   = {"Off", "Up", "Down", "Both", nullptr};              // IFSOsc syncMode
static const char* const kIfsIter34[]    = {"3", "4", nullptr};                                 // IFSOsc iterations
static const char* const kMidiRangeType[] = {"Bipolar", "Monopolar", nullptr};                  // MIDINoteNumberConverter
static const char* const kXFadeCurve[]    = {"Linear", "Equal Power", nullptr};                 // XFade curve
static const char* const kXFadePanType[]  = {"Mix", "Select", nullptr};                         // XFade panType
static const char* const kSelectorCurve[] = {"Hard", "Linear", "Equal Power", nullptr};          // Selector curve
static const char* const kImpulseTrigger[] = {"Rising", "Falling", "Both", nullptr};             // DeltaImpulse trigger
static const char* const kClockDivision[] = {"1/1","1/2","1/4","1/8","1/8T","1/16","1/16T","1/32", nullptr}; // Clock division (pulses per beat)
static const char* const kClockSync[] = {"Internal","DAW", nullptr}; // Clock tempo source
static const char* const kLogicOperator[] = {"AND", "OR", "XOR", nullptr};                       // LogicGate operation
static const char* const kLogicOutputMode[] = {"Gate", "Impulse", nullptr};                      // LogicGate outputMode
static const char* const kLogicGateEdgeMode[] = {"Rising", "Falling", "Both", nullptr};           // LogicGate edge
static const char* const kDiscretizerKind[] = {"Step Size", "Musical", nullptr};                 // Discretizer kind
static const char* const kDiscretizerNoteInterval[] =                                            // Discretizer noteInterval
   {"5", "7", "12", "19", "22", "31", "53", nullptr};
static const char* const kChannelChoice[] = {"Left", "Right", "Sum", nullptr};                   // Unsplit channel choice
static const char* const kIfsIter8[]     = {"1","2","3","4","5","6","7","8", nullptr};          // IFSEnv/IFSLFO iterations
static const char* const kIfsHoldMode[]  = {"None", "Blue", "Green", "Loop", nullptr};
static const char* const kIfsEnvLegato[] = {"Steal", "Match Y", nullptr};
static const char* const kModalMaterial[]= {"String","Bar","Tuned Bar","Membrane","Open Tube","Closed Tube",
   "Tine","Bell","Guitar Body","Violin Body","Soundboard","Generic Body","Harmonic (n)","Octave (2^n)",
   "Fibonacci","Prime","Triangular","Tetrahedral","Partition","Golden (phi^n)","Silver","Piano (stiff)",
   "Power (n^k)","Sqrt (sqrt n)","Box 3D","Harmonic Number", nullptr};
static const char* const kModalStretch[] = {"-0.50","-0.20","-0.10","-0.05","-0.02","-0.01","-0.005","-0.002",
   "-0.001","0","+0.001","+0.002","+0.005","+0.01","+0.02","+0.05","+0.10","+0.20","+0.50","+1.00", nullptr};
static const char* const kLfoWave[]      = {"Sine", "Triangle", "Square", "Impulse", nullptr};   // LFO wave
// LFO/SharedLFO clock ratio (LFO cycles per clock pulse; UTF-8 ×). Order matches kLFOClockRatios
// in LFOClockSync.hpp.
static const char* const kLfoClockRatio[] = {"Auto", "1/16", "1/8", "1/4", "1/3", "1/2", "2/3", "3/4",
                                             "\xC3\x97" "1", "\xC3\x97" "2", "\xC3\x97" "3", "\xC3\x97" "4", "\xC3\x97" "8", "\xC3\x97" "16", nullptr};
static const char* const kLfoPhase[]     = {"0\xC2\xB0", "60\xC2\xB0", "90\xC2\xB0", "120\xC2\xB0", "180\xC2\xB0", nullptr}; // LFO L/R phase (UTF-8 °)
// MIDI group/channel filter: 0 = no filtering, 1..16 = that group/channel (index = value).
static const char* const kMidiNoneOr16[] = {"None","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16", nullptr};
static const char* const kVoiceSteal[]   = {"Oldest", "Lowest Pitch", "Highest Pitch", "Closest Pitch", "Furthest Pitch", nullptr}; // VoiceStealAlgorithm
// ClockGran indexed labels (order matches ClockGranModule.cpp's kTicksPerSlot / kMeterTable).
static const char* const kClockGranRate[]  = {"1/16", "1/8", "1/4", "1/2", "1/1", nullptr};
static const char* const kClockGranMeter[] = {"Straight","5 (3+2)","5 (2+3)","7 (2+2+3)","7 (3+2+2)","7 (2+3+2)","9 (3+3+3)", nullptr};
static const char* const kGateResetMode[]  = {"Partial", "Full", nullptr};
static const char* const kQuantiseMode[]   = {"Snap (nearest mode)", "Scatter (centre \xC2\xB1 N)", nullptr}; // UTF-8 ±
static const char* const kNoiseType[]      = {"White", "Pink", "Brown", nullptr}; // Noise type (order = NoiseType)
// QuantiseMaterial: value 0 = Off, value N = material N-1 (kModalMaterial order).
static const char* const kClockGranQuantMaterial[] = {"Off",
   "String","Bar","Tuned Bar","Membrane","Open Tube","Closed Tube","Tine","Bell","Guitar Body",
   "Violin Body","Soundboard","Generic Body","Harmonic (n)","Octave (2^n)","Fibonacci","Prime",
   "Triangular","Tetrahedral","Partition","Golden (phi^n)","Silver","Piano (stiff)","Power (n^k)",
   "Sqrt (sqrt n)","Box 3D","Harmonic Number", nullptr};

// SinOsc — sine oscillator (matches PM; tri/saw live in TriSawOsc). Defaults 0 → A440 with no pitch
// input connected. Adds stereo detune + DX7-style phase self-modulation. Item 13 (Waveform) is
// reserved/unused (see pm_params.h) so the self-mod items keep their numbers.
// name / item / unit / flags / valueStrings / min / max / def / info
static const ModuleParamConstants sinOscConstants[] = {
   {"pitch",         SinOscParamItemPitch,         U_SEMI, 0, nullptr, -48.0f, 48.0f, 0.0f, 0},
   {"cents",         SinOscParamItemPitchFine,     U_CENT, 0, nullptr, -100.0f, 100.0f, 0.0f, 0},
   {"pitchInput",    SinOscParamItemPitchInput,    U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"modPitch",      SinOscParamItemModPitch,      U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"modPitch2",     SinOscParamItemModPitch2,     U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"modPhase",      SinOscParamItemModPhase,      U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"freqModLevel",  SinOscParamItemFreqModLevel,  U_CUST, 0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"phaseModLevel", SinOscParamItemPhaseModLevel, U_CUST, 0, nullptr, 0.0f, 1.0f, 0.0f, 0},
   {"freqModLevel2", SinOscParamItemFreqModLevel2, U_CUST, 0, nullptr, 0.0f, 1.0f, 0.0f, 0},
   {"modSync",       SinOscParamItemModSync,       U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"modSyncType",   SinOscParamItemModSyncType,   U_IDX,  0, kOscSyncType, 0.0f, 2.0f, 0.0f, 0},
   {"detune",        SinOscParamItemDetune,        U_CENT, 0, nullptr, 0.0f, 25.0f, 0.0f, 0},
   {"selfModIn",     SinOscParamItemSelfModInput,      U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"selfModLvl",    SinOscParamItemSelfModLevel,      U_CUST, 0, nullptr, -1.2f, 1.2f, 0.0f, 0},
   {"selfModInLvl",  SinOscParamItemSelfModInputLevel, U_CUST, 0, nullptr, -2.0f, 2.0f, 0.0f, 0},
};

// Level — gain stage (output = input × level). Default unity; the plugin sets the sine mix level.
// Matches PM's Level ("attenuator"): a bipolar level plus per-channel invert toggles.
static const ModuleParamConstants levelConstants[] = {
   {"input",   LevelParamItemInput,       U_GEN,  0, nullptr,  0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"level",   LevelParamItemLevel,       U_GEN,  0, nullptr, -4.0f, 4.0f, 0.7f, 0},
   {"invertL", LevelParamItemInvertLeft,  U_BOOL, 0, nullptr,  0.0f, 1.0f, 0.0f, 0},   // 0 = normal, 1 = inverted
   {"invertR", LevelParamItemInvertRight, U_BOOL, 0, nullptr,  0.0f, 1.0f, 0.0f, 0},
};
const ModuleParamConstantsGroup levelConstantsGroup = {
   const_cast<char*>("level"), sizeof(levelConstants) / sizeof(levelConstants[0]), levelConstants};

// Bus labels A..H for the tunnel Bus selector (value 1..8 → index value-1). Null-terminated.
static const char* const tunnelBusLabels[] = {"A", "B", "C", "D", "E", "F", "G", "H", nullptr};

// Tunnel In — accepts a signal (item 1 is a visible input port) and a Bus selector. The plugin pairs
// it, by bus, with the Tunnel Outs that re-emit the signal on other planes.
static const ModuleParamConstants tunnelInConstants[] = {
   {"in",  TunnelInParamItemInput, U_GEN, 0, nullptr,         0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"bus", TunnelInParamItemBus,   U_IDX, 0, tunnelBusLabels, 1.0f, 8.0f, 1.0f, 0},
};
const ModuleParamConstantsGroup tunnelInConstantsGroup = {
   const_cast<char*>("Tunnel In"), sizeof(tunnelInConstants) / sizeof(tunnelInConstants[0]), tunnelInConstants};

// Tunnel Out — re-emits (from its "out" port) the signal fed to the Tunnel In on its Bus. Item 1
// (source) is a HIDDEN internal slot the plugin wires to the matching Tunnel In: min==max==0 and NOT
// ParamMetaDataInput, so it is neither a visible port nor a knob.
static const ModuleParamConstants tunnelOutConstants[] = {
   {"source", TunnelOutParamItemSource, U_GEN, 0, nullptr,         0.0f, 0.0f, 0.0f, 0}, // hidden (no port, no knob)
   {"bus",    TunnelOutParamItemBus,    U_IDX, 0, tunnelBusLabels, 1.0f, 8.0f, 1.0f, 0},
};
const ModuleParamConstantsGroup tunnelOutConstantsGroup = {
   const_cast<char*>("Tunnel Out"), sizeof(tunnelOutConstants) / sizeof(tunnelOutConstants[0]), tunnelOutConstants};

const ModuleParamConstantsGroup sinOscConstantsGroup = {
   const_cast<char*>("sinOsc"),
   sizeof(sinOscConstants) / sizeof(sinOscConstants[0]),
   sinOscConstants,
};

// IFS oscillator — trimmed v1. Defaults match IFSShapeParams{} in IFSCore.hpp so the initial
// wavetable and the parameter tree agree.
static const ModuleParamConstants ifsOscConstants[] = {
   {"pitchInput", IFSOscParamItemPitchInput, U_GEN,  0, nullptr,   0.0f,   0.0f,  0.0f, ParamMetaDataInput},
   {"pitch",      IFSOscParamItemPitch,      U_SEMI, 0, nullptr, -24.0f,  24.0f,  0.0f, 0},
   {"cents",      IFSOscParamItemPitchFine,  U_CENT, 0, nullptr, -100.0f, 100.0f, 0.0f, 0},
   {"ifs1Squash", IFSOscParamItemIFS1Squash, U_GEN,  0, nullptr, -0.99f,  0.99f,  0.5f, 0},
   {"ifs2Squash", IFSOscParamItemIFS2Squash, U_GEN,  0, nullptr, -0.99f,  0.99f, -0.5f, 0},
   {"bx",         IFSOscParamItemBx,         U_GEN,  0, nullptr,  -1.0f,   1.0f,  0.0f, 0},
   {"by",         IFSOscParamItemBy,         U_GEN,  0, nullptr,  -1.0f,   1.0f,  0.5f, 0},
   {"startEndY",  IFSOscParamItemStartEndY,  U_GEN,  0, nullptr,  -1.0f,   1.0f, -1.0f, 0},
   {"smoothing",  IFSOscParamItemSmoothing,  U_GEN,  0, nullptr,   0.0f,   1.0f,  0.5f, 0},
   {"iterations", IFSOscParamItemIterations, U_IDX,  0, kIfsIter34, 0.0f,   1.0f,  1.0f, 0},
   {"pitchModIn",  IFSOscParamItemPitchModInput,   U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"pitchModLvl", IFSOscParamItemPitchModLevel,   U_SEMI, 0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"bxModIn",     IFSOscParamItemBxModInput,      U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"bxModLvl",    IFSOscParamItemBxModLevel,      U_GEN, 0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"byModIn",     IFSOscParamItemByModInput,      U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"byModLvl",    IFSOscParamItemByModLevel,      U_GEN, 0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"sq1ModIn",    IFSOscParamItemSquash1ModInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"sq1ModLvl",   IFSOscParamItemSquash1ModLevel, U_GEN, 0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"sq2ModIn",    IFSOscParamItemSquash2ModInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"sq2ModLvl",   IFSOscParamItemSquash2ModLevel, U_GEN, 0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"detune",      IFSOscParamItemDetune,          U_CENT, 0, nullptr,  0.0f, 25.0f, 0.0f, 0},
   {"syncIn",      IFSOscParamItemSyncInput,       U_GEN, 0, nullptr,  0.0f,  0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"syncMode",    IFSOscParamItemSyncMode,        U_IDX, 0, kIfsOscSync,  0.0f,  3.0f, 0.0f, 0}, // 0=Off
   {"level",       IFSOscParamItemLevel,           U_GAIN, 0, nullptr,  0.0f,  1.0f, 1.0f, 0}, // unity; plugin sets mix
};
const ModuleParamConstantsGroup ifsOscConstantsGroup = {
   const_cast<char*>("ifsOsc"),
   sizeof(ifsOscConstants) / sizeof(ifsOscConstants[0]),
   ifsOscConstants,
};

// IFS envelope. Defaults picked for a musical amplitude envelope: startEndY=0 (silence at the
// ends), blue peak → green sustain, holdMode=Green so it holds at green while the note is held
// and releases to 0 on note-off (ADSR-like). NOTE these differ from IFSEnvShapeParams{}; the
// module renders struct defaults at init then re-renders from these on the first process.
static const ModuleParamConstants ifsEnvConstants[] = {
   {"gateInput",  IFSEnvParamItemGateInput,  U_GEN,  0, nullptr,  0.0f,   0.0f,  0.0f, ParamMetaDataInput},
   {"duration",   IFSEnvParamItemDuration,   U_SEC,  0, nullptr,  0.01f,  10.0f, 1.0f,  0},
   {"startEndY",  IFSEnvParamItemStartEndY,  U_GEN,  0, nullptr, -1.0f,   1.0f,  0.0f,  0},
   {"blueX",      IFSEnvParamItemBlueX,      U_GEN,  0, nullptr,  0.0f,   1.0f,  0.15f, 0},
   {"blueY",      IFSEnvParamItemBlueY,      U_GEN,  0, nullptr, -1.0f,   1.0f,  1.0f,  0},
   {"greenX",     IFSEnvParamItemGreenX,     U_GEN,  0, nullptr,  0.0f,   1.0f,  0.45f, 0},
   {"greenY",     IFSEnvParamItemGreenY,     U_GEN,  0, nullptr, -1.0f,   1.0f,  0.5f,  0},
   {"ifs1Squash", IFSEnvParamItemIFS1Squash, U_GEN,  0, nullptr, -0.99f,  0.99f, 0.2f,  0},
   {"ifs2Squash", IFSEnvParamItemIFS2Squash, U_GEN,  0, nullptr, -0.99f,  0.99f, 0.2f,  0},
   {"ifs3Squash", IFSEnvParamItemIFS3Squash, U_GEN,  0, nullptr, -0.99f,  0.99f, 0.2f,  0},
   {"smoothing",  IFSEnvParamItemSmoothing,  U_GEN,  0, nullptr,  0.0f,   1.0f,  0.5f,  0},
   {"iterations", IFSEnvParamItemIterations, U_IDX,  0, kIfsIter8,    0.0f,   7.0f,  2.0f,  0},
   {"holdMode",   IFSEnvParamItemHoldMode,   U_IDX,  0, kIfsHoldMode, 0.0f,   3.0f,  2.0f,  0},
   {"legatoMode", IFSEnvParamItemLegatoMode, U_IDX,  0, kIfsEnvLegato,0.0f,   1.0f,  0.0f,  0},
   {"scaleRelease", IFSEnvParamItemScaleRelease, U_BOOL, 0, nullptr,  0.0f,   1.0f,  0.0f,  0},
};
const ModuleParamConstantsGroup ifsEnvConstantsGroup = {
   const_cast<char*>("ifsEnv"),
   sizeof(ifsEnvConstants) / sizeof(ifsEnvConstants[0]),
   ifsEnvConstants,
};

// IFS LFO — 2-transform shape, defaults matching IFSShapeParams{} (iterations param 3 → 4 iters).
static const ModuleParamConstants ifsLFOConstants[] = {
   {"pitch",      IFSLFOParamItemPitch,      U_OCT, 0, nullptr, -7.0f,  7.0f,  0.0f, 0},
   {"bx",         IFSLFOParamItemBx,         U_GEN, 0, nullptr, -1.0f,  1.0f,  0.0f, 0},
   {"by",         IFSLFOParamItemBy,         U_GEN, 0, nullptr, -1.0f,  1.0f,  0.5f, 0},
   {"ifs1Squash", IFSLFOParamItemIFS1Squash, U_GEN, 0, nullptr, -0.99f, 0.99f, 0.5f, 0},
   {"ifs2Squash", IFSLFOParamItemIFS2Squash, U_GEN, 0, nullptr, -0.99f, 0.99f,-0.5f, 0},
   {"startEndY",  IFSLFOParamItemStartEndY,  U_GEN, 0, nullptr, -1.0f,  1.0f, -1.0f, 0},
   {"smoothing",  IFSLFOParamItemSmoothing,  U_GEN, 0, nullptr,  0.0f,  1.0f,  0.5f, 0},
   {"iterations", IFSLFOParamItemIterations, U_IDX, 0, kIfsIter8,  0.0f,  7.0f,  3.0f, 0},
};
const ModuleParamConstantsGroup ifsLFOConstantsGroup = {
   const_cast<char*>("ifsLFO"),
   sizeof(ifsLFOConstants) / sizeof(ifsLFOConstants[0]),
   ifsLFOConstants,
};

// CV mixer. All four inputs default off (Level 0, bipolar -1..1 — negative inverts); source = CC#1;
// Pitch pivot at middle C (60); mode = add; a little Level smoothing to de-zipper knob moves. Levels
// are DEPTH controls, not gains — see pm_params.h's CVMixerParamItem comment for the identity-
// interpolation and the Pitch/Velocity latching + Reference-Note semantics.
static const ModuleParamConstants cvMixerConstants[] = {
   {"in1",        CVMixerParamItemInput1,        U_GEN,  0, nullptr,        0.0f,   0.0f,  0.0f, ParamMetaDataInput}, // connection
   {"in2",        CVMixerParamItemInput2,        U_GEN,  0, nullptr,        0.0f,   0.0f,  0.0f, ParamMetaDataInput}, // connection
   {"in1Level",   CVMixerParamItemInput1Level,   U_GEN,  0, nullptr,       -1.0f,   1.0f,  0.0f, 0},
   {"in2Level",   CVMixerParamItemInput2Level,   U_GEN,  0, nullptr,       -1.0f,   1.0f,  0.0f, 0},
   {"sourceA",    CVMixerParamItemMIDISourceA,    U_IDX,  0, kCVMixerSource, 0.0f,   4.0f,  0.0f, ParamMetaDataForceDropdown},
   {"levelA",     CVMixerParamItemMIDILevelA,     U_GEN,  0, nullptr,       -1.0f,   1.0f,  0.0f, 0},
   {"ccNumberA",  CVMixerParamItemCCNumberA,      U_IDX,  0, nullptr,        1.0f, 127.0f,  1.0f, 0}, // stepper (1..127)
   {"refNoteA",   CVMixerParamItemReferenceNoteA, U_NOTE, 0, nullptr,        0.0f, 127.0f, 60.0f, ParamMetaDataNoteNameOnly},
   {"sourceB",    CVMixerParamItemMIDISourceB,    U_IDX,  0, kCVMixerSource, 0.0f,   4.0f,  0.0f, ParamMetaDataForceDropdown},
   {"levelB",     CVMixerParamItemMIDILevelB,     U_GEN,  0, nullptr,       -1.0f,   1.0f,  0.0f, 0},
   {"ccNumberB",  CVMixerParamItemCCNumberB,      U_IDX,  0, nullptr,        1.0f, 127.0f,  1.0f, 0}, // stepper (1..127)
   {"refNoteB",   CVMixerParamItemReferenceNoteB, U_NOTE, 0, nullptr,        0.0f, 127.0f, 60.0f, ParamMetaDataNoteNameOnly},
   {"mode",       CVMixerParamItemMode,           U_IDX,  0, kAddMultiply,   0.0f,   1.0f,  0.0f, 0},
   {"smoothing",  CVMixerParamItemSmoothing,     U_SEC,  0, nullptr,        0.0f,   0.5f, 0.01f, 0},
   {"skewA",      CVMixerParamItemSkewA,          U_GEN,  0, nullptr,       -1.0f,   1.0f,  0.0f, 0},
   {"skewB",      CVMixerParamItemSkewB,          U_GEN,  0, nullptr,       -1.0f,   1.0f,  0.0f, 0},
};
const ModuleParamConstantsGroup cvMixerConstantsGroup = {
   const_cast<char*>("cvMixer"),
   sizeof(cvMixerConstants) / sizeof(cvMixerConstants[0]),
   cvMixerConstants,
};

// LPF — 24 dB/oct Butterworth. Cutoff default 5 octaves above A440 (~14 kHz, near-open); Q = 1
// (flat Butterworth). Input + mod inputs are connection ports (value 0); mod levels default 0.
static const ModuleParamConstants lpfConstants[] = {
   {"input",         LPFParamItemInput,          U_GEN,  0, nullptr,  0.0f,  0.0f,  0.0f, ParamMetaDataInput}, // connection
   {"frequency",     LPFParamItemPitch,          U_OCT,  0, nullptr, -6.0f,  5.4f,  5.0f, 0},
   {"freqModLevel",  LPFParamItemFreqModLevel,   U_CUST, 0, nullptr, -8.0f,  8.0f,  0.0f, 0},
   {"q",             LPFParamItemQ,              U_GEN,  0, nullptr,  1.0f, 20.0f,  1.0f, 0},
   {"freqModInput",  LPFParamItemModFreqInput,   U_GEN,  0, nullptr,  0.0f,  0.0f,  0.0f, ParamMetaDataInput}, // connection
   {"freqModInput2", LPFParamItemModFreqInput2,  U_GEN,  0, nullptr,  0.0f,  0.0f,  0.0f, ParamMetaDataInput}, // connection
   {"freqModLevel2", LPFParamItemFreqModLevel2,  U_CUST, 0, nullptr, -8.0f,  8.0f,  0.0f, 0},
   {"qModInput",     LPFParamItemModQInput,      U_GEN,  0, nullptr,  0.0f,  0.0f,  0.0f, ParamMetaDataInput}, // connection
   {"qModLevel",     LPFParamItemModQInputLevel, U_CUST, 0, nullptr,-19.0f, 19.0f,  0.0f, 0},
};
const ModuleParamConstantsGroup lpfConstantsGroup = {
   const_cast<char*>("lpf"),
   sizeof(lpfConstants) / sizeof(lpfConstants[0]),
   lpfConstants,
};

// ADSR — 7 params. Times in seconds; sustain is a level 0..1.
static const ModuleParamConstants adsrConstants[] = {
   {"attack",    ADSRParamItemAttack,    U_SEC,  0, nullptr, 0.001f, 10.0f, 0.01f, 0},
   {"decay",     ADSRParamItemDecay,     U_SEC,  0, nullptr, 0.001f, 10.0f, 1.00f, 0},
   {"sustain",   ADSRParamItemSustain,   U_GAIN, 0, nullptr, 0.0f, 1.0f, 0.5f, 0},
   {"release",   ADSRParamItemRelease,   U_SEC,  0, nullptr, 0.001f, 10.0f, 1.00f, 0},
   {"gateInput", ADSRParamItemGateInput, U_GEN,  0, nullptr, 0.0f, 3.4e38f, 0.0f, ParamMetaDataInput},
   {"legato",    ADSRParamItemLegato,    U_IDX,  0, kAdsrLegato, 0.0f, 2.0f, 0.0f, 0},
   {"linear",    ADSRParamItemLinear,    U_BOOL, 0, nullptr, 0.0f, 1.0f, 0.0f, 0},
};

const ModuleParamConstantsGroup adsrConstantsGroup = {
   const_cast<char*>("adsr"),
   sizeof(adsrConstants) / sizeof(adsrConstants[0]),
   adsrConstants,
};

// Combine — 4-quadrant multiplier (the VCA). Type defaults to Multiply, Level 1.0.
static const ModuleParamConstants combineConstants[] = {
   {"input1", CombineParamItemInput1, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"input2", CombineParamItemInput2, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"input3", CombineParamItemInput3, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"type", CombineParamItemType, U_IDX, 0, kAddMultiply, (float)ParamItemCombineType_Add,
    (float)ParamItemCombineType_Multiply, (float)ParamItemCombineType_Multiply, 0},
   {"level", CombineParamItemLevel, U_GAIN, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
};
const ModuleParamConstantsGroup combineConstantsGroup = {
   const_cast<char*>("combine"), sizeof(combineConstants) / sizeof(combineConstants[0]),
   combineConstants};

// Final — output stage (DC block / level / M-S spread / clip).
static const ModuleParamConstants finalConstants[] = {
   {"input", FinalOutputParamItemInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"spread", FinalOutputParamItemSpread, U_GEN, 0, nullptr, 0.0f, 1.0f, 0.7f, 0},
   {"level", FinalOutputParamItemLevel, U_CUST, 0, nullptr, 0.0f, 1.0f, 0.7f, 0},
   {"clipLevel", FinalOutputParamItemClipLevel, U_CUST, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"filterDC", FinalOutputParamItemFilterDC, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
};
const ModuleParamConstantsGroup finalConstantsGroup = {
   const_cast<char*>("final"), sizeof(finalConstants) / sizeof(finalConstants[0]), finalConstants};

// Slew — slew-rate limiter (smooths the pitch signal). Rise/Fall in seconds per unit.
static const ModuleParamConstants slewConstants[] = {
   {"input", SlewParamItemInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"rise", SlewParamItemRise, U_SEC, 0, nullptr, 1e-8f, 10.0f, 0.01f, 0},
   {"fall", SlewParamItemFall, U_SEC, 0, nullptr, 1e-8f, 10.0f, 0.01f, 0},
   {"curve", SlewParamItemCurve, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, 0},
};
const ModuleParamConstantsGroup slewModuleConstantsGroup = {
   const_cast<char*>("slew"), sizeof(slewConstants) / sizeof(slewConstants[0]), slewConstants};

// PitchConverter — MIDI note → pitch (1/octave). One placeholder param.
static const ModuleParamConstants pitchConverterConstants[] = {
   {"dummy", PitchConverterParamItemDummy, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, 0},
};
const ModuleParamConstantsGroup pitchConverterModuleConstantsGroup = {
   const_cast<char*>("pitchConverter"),
   sizeof(pitchConverterConstants) / sizeof(pitchConverterConstants[0]), pitchConverterConstants};

// Strike — note/gate → shaped excitation burst. Defaults from PM (ParamGroupData.m): Mallet,
// hardness 0.3, note-relative contact, level 1. Items 1-2 (gate/hardness) are connection ports.
static const ModuleParamConstants strikeConstants[] = {
   {"gateInput",        StrikeParamItemGateInput,        U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"hardnessInput",    StrikeParamItemHardnessInput,    U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"type",             StrikeParamItemType,             U_IDX,  0, kStrikeType, (float)StrikeTypeMallet, (float)StrikeTypeNoise, (float)StrikeTypeMallet, 0},
   {"hardness",         StrikeParamItemHardness,         U_CUST, 0, nullptr, 0.0f, 1.0f, 0.3f, 0},
   {"hardnessModLevel", StrikeParamItemHardnessModLevel, U_GAIN, 0, nullptr, -1.0f, 1.0f, 1.0f, 0},
   {"velToLevel",       StrikeParamItemVelToLevel,       U_CUST, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"velToHardness",    StrikeParamItemVelToHardness,    U_CUST, 0, nullptr, 0.0f, 1.0f, 0.5f, 0},
   {"level",            StrikeParamItemLevel,            U_GAIN, 0, nullptr, 0.0f, 2.0f, 1.0f, 0},
   {"contactMode",      StrikeParamItemContactMode,      U_IDX,  0, kStrikeContact, (float)StrikeContactNoteRelative, (float)StrikeContactWallClock, (float)StrikeContactNoteRelative, 0},
};
const ModuleParamConstantsGroup strikeConstantsGroup = {
   const_cast<char*>("strike"), sizeof(strikeConstants) / sizeof(strikeConstants[0]), strikeConstants};

// ModalBank — damped modal resonator bank. Defaults from PM (ParamGroupData.m). Items 1-5 are
// connection ports (0,0,0). Material default = Bar; Stretch is an INDEX into kModalStretchValues.
static const ModuleParamConstants modalBankConstants[] = {
   {"input",          ModalBankParamItemInput,          U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection (excitation)
   {"pitchInput",     ModalBankParamItemPitchInput,     U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"strikeInput",    ModalBankParamItemStrikeInput,    U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"decayModInput",  ModalBankParamItemDecayModInput,  U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"brightModInput", ModalBankParamItemBrightModInput, U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"pitch",          ModalBankParamItemPitch,          U_OCT,  0, nullptr, -4.0f, 4.0f, 0.0f, 0},
   {"pitchModLevel",  ModalBankParamItemPitchModLevel,  U_GAIN, 0, nullptr, -5.0f, 5.0f, 1.0f, 0},
   {"material",       ModalBankParamItemMaterial,       U_IDX,  0, kModalMaterial, (float)ModalBankMaterialString, (float)ModalBankMaterialGenHarmonicNumber, (float)ModalBankMaterialBar, 0},
   {"modeCount",      ModalBankParamItemModeCount,      U_CUST, 0, nullptr, 1.0f, 64.0f, 64.0f, 0},
   {"stretch",        ModalBankParamItemStretch,        U_IDX,  0, kModalStretch, 0.0f, 19.0f, 9.0f, 0}, // index into kModalStretchValues
   {"decayT60",       ModalBankParamItemDecayT60,       U_SEC,  0, nullptr, 0.02f, 60.0f, 2.0f, 0},
   {"decayModLevel",  ModalBankParamItemDecayModLevel,  U_GAIN, 0, nullptr, -5.0f, 5.0f, 0.0f, 0},
   {"dampTilt",       ModalBankParamItemDampTilt,       U_CUST, 0, nullptr, 0.0f, 3.0f, 0.9f, 0},
   {"dampBellAmount", ModalBankParamItemDampBellAmount, U_OCT,  0, nullptr, -4.0f, 4.0f, 0.0f, 0},
   {"dampBellCentre", ModalBankParamItemDampBellCentre, U_OCT,  0, nullptr, 0.0f, 7.0f, 2.0f, 0},
   {"dampBellWidth",  ModalBankParamItemDampBellWidth,  U_OCT,  0, nullptr, 0.1f, 4.0f, 0.4f, 0},
   {"strikePosition", ModalBankParamItemStrikePosition, U_CUST, 0, nullptr, 0.0f, 1.0f, 0.25f, 0},
   {"strikeModLevel", ModalBankParamItemStrikeModLevel, U_GAIN, 0, nullptr, -5.0f, 5.0f, 0.0f, 0},
   {"brightness",     ModalBankParamItemBrightness,     U_CUST, 0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"brightModLevel", ModalBankParamItemBrightModLevel, U_GAIN, 0, nullptr, -5.0f, 5.0f, 0.0f, 0},
   {"spread",         ModalBankParamItemSpread,         U_CUST, 0, nullptr, 0.0f, 1.0f, 0.3f, 0},
   {"mix",            ModalBankParamItemMix,            U_FADER,0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"level",          ModalBankParamItemLevel,          U_GAIN, 0, nullptr, 0.0f, 2.0f, 0.5f, 0},
};
const ModuleParamConstantsGroup modalBankConstantsGroup = {
   const_cast<char*>("modalBank"), sizeof(modalBankConstants) / sizeof(modalBankConstants[0]), modalBankConstants};

// TriSawOsc — variable tri↔saw oscillator (Peak morphs the apex) with hard sync + phase self-mod.
// Item order mirrors PM. Units from PM's ParamGroupData.m (mod inputs kept U_GEN per our convention).
static const ModuleParamConstants triSawConstants[] = {
   {"pitchInput",    TriSawParamItemPitchInput,        U_GEN,  0, nullptr,      0.0f,   0.0f,  0.0f, ParamMetaDataInput},
   {"pitch",         TriSawParamItemPitch,             U_SEMI, 0, nullptr,    -24.0f,  24.0f,  0.0f, 0},
   {"cents",         TriSawParamItemPitchFine,         U_CENT, 0, nullptr,    -25.0f,  25.0f,  0.0f, 0},
   {"modPitch",      TriSawParamItemModPitch,          U_GEN,  0, nullptr,      0.0f,   0.0f,  0.0f, ParamMetaDataInput},
   {"modPitch2",     TriSawParamItemModPitch2,         U_GEN,  0, nullptr,      0.0f,   0.0f,  0.0f, ParamMetaDataInput},
   {"modPhase",      TriSawParamItemModPhase,          U_GEN,  0, nullptr,      0.0f,   0.0f,  0.0f, ParamMetaDataInput},
   {"peak",          TriSawParamItemPeak,              U_CUST, 0, nullptr,      0.01f,  0.99f, 0.5f, 0},
   {"modPeak",       TriSawParamItemModPeak,           U_GEN,  0, nullptr,      0.0f,   0.0f,  0.0f, ParamMetaDataInput},
   {"freqModLevel",  TriSawParamItemFreqModLevel,      U_CUST, 0, nullptr,     -2.0f,   2.0f,  0.0f, 0},
   {"phaseModLevel", TriSawParamItemPhaseModLevel,     U_CUST, 0, nullptr,     -2.0f,   2.0f,  0.0f, 0},
   {"peakModLevel",  TriSawParamItemPeakModLevel,      U_CUST, 0, nullptr,      0.0f,   1.0f,  0.5f, 0},
   {"freqModLevel2", TriSawParamItemFreqModLevel2,     U_CUST, 0, nullptr,     -2.0f,   2.0f,  0.0f, 0},
   {"modSync",       TriSawParamItemModSync,           U_GEN,  0, nullptr,      0.0f,   0.0f,  0.0f, ParamMetaDataInput},
   {"modSyncType",   TriSawParamItemModSyncType,       U_IDX,  0, kOscSyncType, 0.0f,   2.0f,  0.0f, 0},
   {"selfModIn",     TriSawParamItemSelfModInput,      U_GEN,  0, nullptr,      0.0f,   0.0f,  0.0f, ParamMetaDataInput},
   {"selfModLvl",    TriSawParamItemSelfModLevel,      U_CUST, 0, nullptr,     -0.9f,   0.9f,  0.0f, 0},
   {"selfModInLvl",  TriSawParamItemSelfModInputLevel, U_CUST, 0, nullptr,     -2.0f,   2.0f,  0.0f, 0},
};
const ModuleParamConstantsGroup triSawConstantsGroup = {
   const_cast<char*>("Tri Saw Osc"), sizeof(triSawConstants) / sizeof(triSawConstants[0]), triSawConstants};

// LFO — Sine/Triangle/Square/Impulse control oscillator. Rate in octaves (0 = 1 Hz); L/R phase offset.
static const ModuleParamConstants lfoConstants[] = {
   {"frequency",  LFOParamItemPitch,      U_OCT, 0, nullptr,   -7.0f, 7.0f, 0.0f, 0},
   {"freqInput",  LFOParamItemPitchInput, U_GEN, 0, nullptr,    0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"wave",       LFOParamItemWave,       U_IDX, 0, kLfoWave,   0.0f, 3.0f, 1.0f, 0}, // Sine..Impulse; default Triangle
   {"lrPhase",    LFOParamItemPhase,      U_IDX, 0, kLfoPhase,  0.0f, 4.0f, 0.0f, 0},
   {"resetInput", LFOParamItemResetInput, U_GEN, 0, nullptr,    0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"clockInput", LFOParamItemClockInput, U_GEN, 0, nullptr,    0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"clockRatio", LFOParamItemClockRatio, U_IDX, 0, kLfoClockRatio, 0.0f, 13.0f, 0.0f, ParamMetaDataForceDropdown}, // default Auto
};
const ModuleParamConstantsGroup lfoConstantsGroup = {
   const_cast<char*>("LFO"), sizeof(lfoConstants) / sizeof(lfoConstants[0]), lfoConstants};

// Clock — free-running gate generator. Pulse rate = BPM/60 × the division's pulses-per-beat.
static const ModuleParamConstants clockConstants[] = {
   {"resetInput", ClockParamItemResetInput, U_GEN, 0, nullptr,        0.0f,   0.0f,   0.0f, ParamMetaDataInput},
   {"runInput",   ClockParamItemRunInput,   U_GEN, 0, nullptr,        0.0f,   0.0f,   0.0f, ParamMetaDataInput},
   {"sync",       ClockParamItemSync,       U_IDX, 0, kClockSync,     0.0f,   1.0f,   1.0f, 0}, // default DAW
   {"bpm",        ClockParamItemBPM,        U_GEN, 0, nullptr,       20.0f, 300.0f, 120.0f, 0},
   {"division",   ClockParamItemDivision,   U_IDX, 0, kClockDivision, 0.0f,   7.0f,   2.0f, 0}, // default 1/4
   {"width",      ClockParamItemWidth,      U_GEN, 0, nullptr,        0.05f,  0.95f,  0.5f, 0},
};
const ModuleParamConstantsGroup clockConstantsGroup = {
   const_cast<char*>("Clock"), sizeof(clockConstants) / sizeof(clockConstants[0]), clockConstants};

// ADR — Delay-Attack-Decay-Release envelope. Times in seconds; Sustains/Linear are toggles; Legato
// is a 3-way selector (shares ADSR's labels).
static const ModuleParamConstants adrConstants[] = {
   {"delay",     ADRParamItemDelay,     U_SEC,  0, nullptr,     0.0f,   10.0f, 0.0f,  0},
   {"attack",    ADRParamItemAttack,    U_SEC,  0, nullptr,     0.001f, 10.0f, 0.01f, 0},
   {"decay",     ADRParamItemDecay,     U_SEC,  0, nullptr,     0.001f, 10.0f, 1.0f,  0},
   {"release",   ADRParamItemRelease,   U_SEC,  0, nullptr,     0.001f, 10.0f, 1.0f,  0},
   {"sustains",  ADRParamItemSustains,  U_BOOL, 0, nullptr,     0.0f,   1.0f,  0.0f,  0},
   {"gateInput", ADRParamItemGateInput, U_GEN,  0, nullptr,     0.0f,   0.0f,  0.0f,  ParamMetaDataInput},
   {"legato",    ADRParamItemLegato,    U_IDX,  0, kAdsrLegato,  0.0f,   2.0f,  0.0f,  0},
   {"linear",    ADRParamItemLinear,    U_BOOL, 0, nullptr,     0.0f,   1.0f,  0.0f,  0},
};
const ModuleParamConstantsGroup adrConstantsGroup = {
   const_cast<char*>("ADR"), sizeof(adrConstants) / sizeof(adrConstants[0]), adrConstants};

// SineWarp — sine waveshaper. Scale sharpens the fold; Shift moves the sine phase. All Generic.
static const char* const kSineWarpOversample[] = { "Off", "2x", "4x", nullptr };
static const ModuleParamConstants sineWarpConstants[] = {
   {"input",        SineWarpParamItemInput,         U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"scale",        SineWarpParamItemScale,         U_GEN, 0, nullptr, 1.0f, 8.0f, 1.0f, 0},
   {"shift",        SineWarpParamItemShift,         U_GEN, 0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"scaleMod",     SineWarpParamItemModScale,      U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"shiftMod",     SineWarpParamItemModShift,      U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"scaleModLvl",  SineWarpParamItemModScaleLevel, U_GEN, 0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"shiftModLvl",  SineWarpParamItemModShiftLevel, U_GEN, 0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"oversample",   SineWarpParamItemOversample,    U_IDX, 0, kSineWarpOversample, 0.0f, 2.0f, 0.0f, 0},
};
const ModuleParamConstantsGroup sineWarpConstantsGroup = {
   const_cast<char*>("Sine Warp"), sizeof(sineWarpConstants) / sizeof(sineWarpConstants[0]), sineWarpConstants};

// Pan — true stereo panner. Position -1..+1; ModInput is summed with Position per-sample and
// clamped, same as the WebView/original — see PanModule.cpp for the actual pan law.
static const ModuleParamConstants panConstants[] = {
   {"input",    PanParamItemInput,    U_GEN, 0, nullptr, 0.0f,  0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"posMod",   PanParamItemModInput, U_GEN, 0, nullptr, 0.0f,  0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"position", PanParamItemPosition, U_GEN, 0, nullptr, -1.0f, 1.0f, 0.0f, 0},
};
const ModuleParamConstantsGroup panConstantsGroup = {
   const_cast<char*>("Pan"), sizeof(panConstants) / sizeof(panConstants[0]), panConstants};

// Comparator — Schmitt trigger. Type: Full Clip / Top / Bottom. Threshold, hysteresis, clip level,
// each with a CV input + level. Threshold ±0.99; clip 0..0.99.
static const ModuleParamConstants comparatorConstants[] = {
   {"input",         ComparatorParamItemInput,               U_GEN,  0, nullptr,         0.0f,   0.0f,  0.0f, ParamMetaDataInput}, // connection
   {"type",          ComparatorParamItemType,                U_IDX,  0, kComparatorType, 0.0f,   2.0f,  0.0f, 0},
   {"threshold",     ComparatorParamItemThreshold,           U_CUST, 0, nullptr,        -0.99f,  0.99f, 0.0f, 0},
   {"threshCV",      ComparatorParamItemThresholdInput,      U_GEN,  0, nullptr,         0.0f,   0.0f,  0.0f, ParamMetaDataInput}, // connection
   {"threshCVLvl",   ComparatorParamItemThresholdInputLevel, U_CUST, 0, nullptr,        -0.99f,  0.99f, 0.0f, 0},
   {"hysteresis",    ComparatorParamItemHysteresis,          U_CUST, 0, nullptr,         0.0f,   0.5f,  0.0f, 0},
   {"clipLevel",     ComparatorParamItemClipLevel,           U_CUST, 0, nullptr,         0.0f,   0.99f, 0.99f, 0},
   {"clipCV",        ComparatorParamItemClipLevelInput,      U_GEN,  0, nullptr,         0.0f,   0.0f,  0.0f, ParamMetaDataInput}, // connection
   {"clipCVLvl",     ComparatorParamItemClipLevelInputLevel, U_CUST, 0, nullptr,         0.0f,   0.99f, 0.0f, 0},
   {"bandlimit",     ComparatorParamItemBandlimit,           U_BOOL, 0, nullptr,         0.0f,   1.0f,  1.0f, 0}, // polyBLEP crossings (default on)
};
const ModuleParamConstantsGroup comparatorConstantsGroup = {
   const_cast<char*>("Comparator"), sizeof(comparatorConstants) / sizeof(comparatorConstants[0]), comparatorConstants};

// VariableSlope — tilt filter. Pivot in octaves, slope ±6 dB/oct, low cut in Hz; pivot/slope CV.
static const ModuleParamConstants variableSlopeConstants[] = {
   {"input",        VariableSlopeParamItemInput,           U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"pivotPitch",   VariableSlopeParamItemPivotPitch,      U_OCT,  0, nullptr, -2.0f, 4.0f, 0.0f, 0},
   {"slope",        VariableSlopeParamItemSlope,           U_DB,   0, nullptr, -6.0f, 6.0f, 3.0f, 0},
   {"lowCut",       VariableSlopeParamItemLowCutFrequency, U_HZ,   0, nullptr, 0.0f, 100.0f, 40.0f, 0},
   {"pivotModLvl",  VariableSlopeParamItemPivotInputLevel, U_GAIN, 0, nullptr, 0.0f, 1.0f, 0.0f, 0},
   {"slopeModLvl",  VariableSlopeParamItemSlopeInputLevel, U_GAIN, 0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"pivotMod",     VariableSlopeParamItemPivotModInput,   U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"slopeMod",     VariableSlopeParamItemSlopeModInput,   U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
};
const ModuleParamConstantsGroup variableSlopeConstantsGroup = {
   const_cast<char*>("Variable Slope"), sizeof(variableSlopeConstants) / sizeof(variableSlopeConstants[0]), variableSlopeConstants};

// Formant — 2D vowel morph. Gain (Indexed dB) applies in series mode; Parallel toggles stage topology.
static const ModuleParamConstants formantConstants[] = {
   {"input",        FormantFilterParamItemInput,         U_GEN,  0, nullptr,      0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"morphX",       FormantFilterParamItemMorphX,        U_CUST, 0, nullptr,      0.0f, 1.0f, 0.0f, 0},
   {"morphY",       FormantFilterParamItemMorphY,        U_CUST, 0, nullptr,      0.0f, 1.0f, 0.0f, 0},
   {"xMod",         FormantFilterParamItemXInput,        U_GEN,  0, nullptr,      0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"yMod",         FormantFilterParamItemYInput,        U_GEN,  0, nullptr,      0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"parallel",     FormantFilterParamItemParallel,      U_BOOL, 0, nullptr,      0.0f, 1.0f, 0.0f, 0},
   {"gain",         FormantFilterParamItemGain,          U_IDX,  0, kFormantGain, (float)FormantFilterGain0, (float)FormantFilterGain48, (float)FormantFilterGain32, 0},
   {"freqMod",      FormantFilterParamItemFreqModInput,  U_GEN,  0, nullptr,      0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"freqModLvl",   FormantFilterParamItemFreqModLevel,  U_GAIN, 0, nullptr,      0.0f, 2.0f, 0.0f, 0},
};
const ModuleParamConstantsGroup formantConstantsGroup = {
   const_cast<char*>("Formant"), sizeof(formantConstants) / sizeof(formantConstants[0]), formantConstants};

// RezNotch — pole/zero resonator/notch. R/Sep drive Notch & Rez modes; Rz/Rp are the Rez-mode radii.
static const ModuleParamConstants rezNotchConstants[] = {
   {"input",       RezNotchParamItemInput,         U_GEN,   0, nullptr,       0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"pitchMod",    RezNotchParamItemModPitchInput, U_GEN,   0, nullptr,       0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"pitch",       RezNotchParamItemPitch,         U_OCT,   0, nullptr,      -2.0f, 4.0f, 0.0f, 0},
   {"pitchModLvl", RezNotchParamItemPitchModLevel, U_GAIN,  0, nullptr,      -5.0f, 5.0f, 0.0f, 0},
   {"outerRadius", RezNotchParamItemR,             U_CUST,  0, nullptr,       0.8f, 0.999f, 0.9f, 0},
   {"separation",  RezNotchParamItemSep,           U_CUST,  0, nullptr,       0.001f, 0.1f, 0.05f, 0},
   {"zeroRadius",  RezNotchParamItemRz,            U_CUST,  0, nullptr,       0.0f, 1.0f, 0.5f, 0},
   {"poleRadius",  RezNotchParamItemRp,            U_CUST,  0, nullptr,       0.0f, 1.0f, 0.5f, 0},
   {"mode",        RezNotchParamItemMode,          U_IDX,   0, kRezNotchMode, (float)RezNotchModeBump, (float)RezNotchModeAllPass, (float)RezNotchModeBump, 0},
   {"mix",         RezNotchParamItemMix,           U_FADER, 0, nullptr,       0.0f, 1.0f, 1.0f, 0}, // default fully wet: a filter should do something on insert
   {"polarity",    RezNotchParamItemPolarity,      U_BOOL,  0, nullptr,       0.0f, 1.0f, 0.0f, 0},
   // Parametric peaking prototype (see pm_params.h): shared Q + a signed gain per mode.
   // Q: 0.25..40 (wide→surgical). notchDepth: 0..-60 dB (Notch cut). bumpGain: 0..+40 dB (Bump/Rez boost).
   {"q",           RezNotchParamItemQ,             U_GEN,   0, nullptr,       0.25f, 40.0f, 4.0f, 0},
   {"notchDepth",  RezNotchParamItemNotchDepth,    U_DB,    0, nullptr,      -60.0f,  0.0f, -18.0f, 0},
   {"bumpGain",    RezNotchParamItemBumpGain,      U_DB,    0, nullptr,        0.0f, 40.0f, 12.0f, 0},
};
const ModuleParamConstantsGroup rezNotchConstantsGroup = {
   const_cast<char*>("Rez Notch"), sizeof(rezNotchConstants) / sizeof(rezNotchConstants[0]), rezNotchConstants};

// Multimode — resonant biquad: Passthrough / LP / HP / BP. Frequency in octaves, two FM inputs + Q.
static const ModuleParamConstants multimodeConstants[] = {
   {"input",       MultimodeFilterParamItemInput,          U_GEN,  0, nullptr,     0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"type",        MultimodeFilterParamItemType,           U_IDX,  0, kFilterMode, (float)ParamItemFilterType_Passthrough, (float)ParamItemFilterType_Bandpass, (float)ParamItemFilterType_Lowpass, 0},
   {"frequency",   MultimodeFilterParamItemPitch,          U_OCT,  0, nullptr,    -4.0f, 4.0f, 0.0f, 0},
   {"freqModLvl",  MultimodeFilterParamItemFreqModLevel,   U_CUST, 0, nullptr,    -5.0f, 5.0f, 0.5f, 0},
   {"q",           MultimodeFilterParamItemQ,              U_GEN,  0, nullptr,     0.1f, 25.0f, 2.0f, 0},
   {"freqMod",     MultimodeFilterParamItemModPitchInput,  U_GEN,  0, nullptr,     0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"freqMod2",    MultimodeFilterParamItemModPitchInput2, U_GEN,  0, nullptr,     0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"freqModLvl2", MultimodeFilterParamItemFreqModLevel2,  U_CUST, 0, nullptr,    -5.0f, 5.0f, 0.0f, 0},
};
const ModuleParamConstantsGroup multimodeConstantsGroup = {
   const_cast<char*>("Multimode Filter"), sizeof(multimodeConstants) / sizeof(multimodeConstants[0]), multimodeConstants};

// Butterworth bandpass — 4th-order. Center pitch (octaves), bandwidth (-1..1), Q; pitch + bandwidth CV.
static const ModuleParamConstants butterBandpassConstants[] = {
   {"input",       BPParamItemInput,              U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"frequency",   BPParamItemPitch,              U_OCT,  0, nullptr, -2.0f, 5.0f, 0.0f, 0},
   {"freqModLvl",  BPParamItemFreqModLevel,       U_CUST, 0, nullptr, -5.0f, 5.0f, 0.5f, 0},
   {"bandwidth",   BPParamItemBandwidth,          U_GEN,  0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"q",           BPParamItemQ,                  U_GEN,  0, nullptr,  1.0f, 2.0f, 1.0f, 0},   // capped low: keep the flat-to-gentle band; DoubleRez owns the aggressive double-edge sound
   {"freqMod",     BPParamItemModPitchInput,      U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"bwMod",       BPParamItemModBandwidthInput,  U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"bwModLvl",    BPParamItemBandwidthModLevel,  U_CUST, 0, nullptr, 0.0f, 1.0f, 0.5f, 0},
};
const ModuleParamConstantsGroup butterBandpassConstantsGroup = {
   const_cast<char*>("Butterworth BP"), sizeof(butterBandpassConstants) / sizeof(butterBandpassConstants[0]), butterBandpassConstants};

// Biquad — RBJ biquad: Passthrough / LP / HP / BP. Frequency in octaves, two FM inputs + Q.
static const ModuleParamConstants biquadConstants[] = {
   {"input",       BiquadFilterParamItemInput,          U_GEN,  0, nullptr,     0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"type",        BiquadFilterParamItemType,           U_IDX,  0, kFilterMode, (float)ParamItemFilterType_Passthrough, (float)ParamItemFilterType_Bandpass, (float)ParamItemFilterType_Lowpass, 0},
   {"frequency",   BiquadFilterParamItemPitch,          U_OCT,  0, nullptr,    -4.0f, 4.0f, 0.0f, 0},
   {"q",           BiquadFilterParamItemQ,              U_GEN,  0, nullptr,     0.1f, 10.0f, 2.0f, 0},
   {"freqMod",     BiquadFilterParamItemModPitchInput,  U_GEN,  0, nullptr,     0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"freqModLvl",  BiquadFilterParamItemFreqModLevel,   U_CUST, 0, nullptr,    -5.0f, 5.0f, 0.5f, 0},
   {"freqMod2",    BiquadFilterParamItemModPitchInput2, U_GEN,  0, nullptr,     0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"freqModLvl2", BiquadFilterParamItemFreqModLevel2,  U_CUST, 0, nullptr,    -5.0f, 5.0f, 0.0f, 0},
};
const ModuleParamConstantsGroup biquadConstantsGroup = {
   const_cast<char*>("Biquad Filter"), sizeof(biquadConstants) / sizeof(biquadConstants[0]), biquadConstants};

// Double Rez Bandpass — resonant HP × resonant LP. Centre (octaves) + bandwidth (octave SEPARATION)
// + edge Q, each control-rate-mod'able; Slope 12/24 dB/oct, QLimit taper, equal-power dry/wet Mix.
static const ModuleParamConstants doubleRezBandpassConstants[] = {
   {"input",       DoubleRezBandpassParamItemInput,             U_GEN,   0, nullptr,         0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"centerMod",   DoubleRezBandpassParamItemCenterModInput,    U_GEN,   0, nullptr,         0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"bwMod",       DoubleRezBandpassParamItemBandwidthModInput, U_GEN,   0, nullptr,         0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"qMod",        DoubleRezBandpassParamItemQModInput,         U_GEN,   0, nullptr,         0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"center",      DoubleRezBandpassParamItemCenter,            U_OCT,   0, nullptr,        -3.0f, 5.0f, 1.0f, 0},
   {"centerModLvl",DoubleRezBandpassParamItemCenterModLevel,    U_GAIN,  0, nullptr,        -5.0f, 5.0f, 0.0f, 0},
   {"bandwidth",   DoubleRezBandpassParamItemBandwidth,         U_OCT,   0, nullptr,         0.0f, 6.0f, 2.0f, 0},
   {"bwModLvl",    DoubleRezBandpassParamItemBandwidthModLevel, U_GAIN,  0, nullptr,        -6.0f, 6.0f, 0.0f, 0},
   {"q",           DoubleRezBandpassParamItemQ,                 U_CUST,  0, nullptr,         0.5f, 40.0f, 4.0f, 0},
   {"qModLvl",     DoubleRezBandpassParamItemQModLevel,         U_CUST,  0, nullptr,       -20.0f, 20.0f, 0.0f, 0},
   {"slope",       DoubleRezBandpassParamItemSlope,             U_IDX,   0, kDoubleRezSlope, 0.0f, 1.0f, 0.0f, 0},
   {"mix",         DoubleRezBandpassParamItemMix,               U_FADER, 0, nullptr,         0.0f, 1.0f, 1.0f, 0},
   {"qLimit",      DoubleRezBandpassParamItemQLimit,            U_CUST,  0, nullptr,         0.0f, 1.0f, 0.0f, 0},
};
const ModuleParamConstantsGroup doubleRezBandpassConstantsGroup = {
   const_cast<char*>("Double Rez Bandpass"), sizeof(doubleRezBandpassConstants) / sizeof(doubleRezBandpassConstants[0]), doubleRezBandpassConstants};

// ClockGran — clock-driven granular synth. Ranges/defaults from PM's ParamGroupData.m (clockGranConstants).
// Pitch quantities use U_SEMI/U_CENT (display hint; control dispatch keys only on Indexed/Boolean).
// name / item / unit / flags / valueStrings / min / max / def / info
static const ModuleParamConstants clockGranConstants[] = {
   {"gateInput",   ClockGranParamItemGateInput,   U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"pitchInput",  ClockGranParamItemPitchInput,  U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"clockInput",  ClockGranParamItemClockInput,  U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"resetInput",  ClockGranParamItemResetInput,  U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"pitch",       ClockGranParamItemPitch,       U_SEMI, 0, nullptr, -48.0f, 48.0f, 0.0f, 0},
   {"pitchFine",   ClockGranParamItemPitchFine,   U_CENT, 0, nullptr, -100.0f, 100.0f, 0.0f, 0},
   {"rate",        ClockGranParamItemRate,        U_IDX,  0, kClockGranRate,  0.0f, 4.0f, 2.0f, 0},
   {"meter",       ClockGranParamItemMeter,       U_IDX,  0, kClockGranMeter, 0.0f, 6.0f, 0.0f, 0},
   {"density",     ClockGranParamItemDensity,     U_GEN,  0, nullptr, 1.0f, 16.0f, 1.0f, 0},
   {"duration",    ClockGranParamItemDuration,    U_GEN,  0, nullptr, 1.0f, 500.0f, 80.0f, 0},
   {"smearAmount", ClockGranParamItemSmearAmount, U_GEN,  0, nullptr, 0.0f, 1.0f, 0.3f, 0},
   {"smearPeak",   ClockGranParamItemSmearPeak,   U_GEN,  0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"scatterAmount", ClockGranParamItemPitchScatterAmount, U_GEN, 0, nullptr, 0.0f, 1.0f, 0.3f, 0},
   {"scatterSkew", ClockGranParamItemPitchScatterSkew,   U_GEN, 0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"edoSteps",    ClockGranParamItemEDOSteps,    U_IDX,  0, nullptr, 0.0f, 96.0f, 0.0f, 0},
   {"bx",          ClockGranParamItemBx,          U_GEN,  0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"by",          ClockGranParamItemBy,          U_GEN,  0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"ifs1Squash",  ClockGranParamItemIFS1Squash,  U_GEN,  0, nullptr, -0.99f, 0.99f, 0.5f, 0},
   {"ifs2Squash",  ClockGranParamItemIFS2Squash,  U_GEN,  0, nullptr, -0.99f, 0.99f, 0.5f, 0},
   {"startEndY",   ClockGranParamItemStartEndY,   U_GEN,  0, nullptr, -1.0f, 1.0f, -1.0f, 0},
   {"grainEnvSkew", ClockGranParamItemGrainEnvelopeSkew, U_GEN, 0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"panSpread",   ClockGranParamItemPanSpread,   U_GEN,  0, nullptr, 0.0f, 1.0f, 0.3f, 0},
   {"outputLevel", ClockGranParamItemOutputLevel, U_GAIN, 0, nullptr, 0.0f, 2.0f, 1.0f, 0},
   {"looping",     ClockGranParamItemLooping,     U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"gateResetMode", ClockGranParamItemGateResetMode, U_IDX, 0, kGateResetMode, 0.0f, 1.0f, 0.0f, 0},
   {"pitchOffset1", ClockGranParamItemPitchOffset1, U_SEMI, 0, nullptr, -24.0f, 24.0f, 0.0f, 0},
   {"pitchOffset2", ClockGranParamItemPitchOffset2, U_SEMI, 0, nullptr, -24.0f, 24.0f, 0.0f, 0},
   {"pitchOffset3", ClockGranParamItemPitchOffset3, U_SEMI, 0, nullptr, -24.0f, 24.0f, 0.0f, 0},
   {"pitchOffset4", ClockGranParamItemPitchOffset4, U_SEMI, 0, nullptr, -24.0f, 24.0f, 0.0f, 0},
   {"pitchOffset5", ClockGranParamItemPitchOffset5, U_SEMI, 0, nullptr, -24.0f, 24.0f, 0.0f, 0},
   {"chaosAmount1", ClockGranParamItemChaosAmount1, U_GEN, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"chaosAmount2", ClockGranParamItemChaosAmount2, U_GEN, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"chaosAmount3", ClockGranParamItemChaosAmount3, U_GEN, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"chaosAmount4", ClockGranParamItemChaosAmount4, U_GEN, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"chaosAmount5", ClockGranParamItemChaosAmount5, U_GEN, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"level1",      ClockGranParamItemLevel1,      U_GAIN, 0, nullptr, 0.0f, 1.0f, 0.7f, 0},
   {"level2",      ClockGranParamItemLevel2,      U_GAIN, 0, nullptr, 0.0f, 1.0f, 0.7f, 0},
   {"level3",      ClockGranParamItemLevel3,      U_GAIN, 0, nullptr, 0.0f, 1.0f, 0.7f, 0},
   {"level4",      ClockGranParamItemLevel4,      U_GAIN, 0, nullptr, 0.0f, 1.0f, 0.7f, 0},
   {"level5",      ClockGranParamItemLevel5,      U_GAIN, 0, nullptr, 0.0f, 1.0f, 0.7f, 0},
   {"pitchModInput",  ClockGranParamItemPitchModInput,  U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"pitchModLevel",  ClockGranParamItemPitchModLevel,  U_SEMI, 0, nullptr, -24.0f, 24.0f, 0.0f, 0},
   {"densityModInput", ClockGranParamItemDensityModInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"densityModLevel", ClockGranParamItemDensityModLevel, U_GEN, 0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"durationModInput", ClockGranParamItemDurationModInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"durationModLevel", ClockGranParamItemDurationModLevel, U_GEN, 0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"bxModInput",  ClockGranParamItemBxModInput,  U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"bxModLevel",  ClockGranParamItemBxModLevel,  U_CUST, 0, nullptr, -2.0f, 2.0f, 0.0f, 0},
   {"quantiseMaterial", ClockGranParamItemQuantiseMaterial, U_IDX, 0, kClockGranQuantMaterial, 0.0f, (float)ModalBankMaterialCount, 0.0f, 0},
   {"centreMode",  ClockGranParamItemCentreMode,  U_IDX,  0, nullptr, 0.0f, 63.0f, 0.0f, 0},
   {"modeScatter", ClockGranParamItemModeScatter, U_IDX,  0, nullptr, 0.0f, 32.0f, 2.0f, 0},
   {"modeSkew",    ClockGranParamItemModeSkew,    U_CUST, 0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"discretizerStretch", ClockGranParamItemDiscretizerStretch, U_IDX, 0, kModalStretch, 0.0f, 19.0f, 9.0f, 0},
   {"quantiseMode", ClockGranParamItemQuantiseMode, U_IDX, 0, kQuantiseMode, 0.0f, 1.0f, 1.0f, 0},
};
const ModuleParamConstantsGroup clockGranConstantsGroup = {
   const_cast<char*>("Clock Gran"), sizeof(clockGranConstants) / sizeof(clockGranConstants[0]), clockGranConstants};

// BufferGran — granular synth reading grains from a rolling capture of an audio input.
// Ranges/defaults from PM's ParamGroupData.m (bufferGranConstants). Shares the mode-ratio
// discretizer label arrays (kClockGranQuantMaterial / kQuantiseMode / kModalStretch) with ClockGran.
// name / item / unit / flags / valueStrings / min / max / def / info
static const char* const kBufferGranFreeze[] = {"Rolling", "Frozen", nullptr};
static const ModuleParamConstants bufferGranConstants[] = {
   {"input",            BufferGranParamItemInput,            U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"gateInput",        BufferGranParamItemGateInput,        U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"pitchInput",       BufferGranParamItemPitchInput,       U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"freezeInput",      BufferGranParamItemFreezeInput,      U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"positionModInput", BufferGranParamItemPositionModInput, U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"freeze",           BufferGranParamItemFreeze,           U_IDX,  0, kBufferGranFreeze, 0.0f, 1.0f, 0.0f, 0},
   {"bufferSeconds",    BufferGranParamItemBufferSeconds,    U_SEC,  0, nullptr, 0.05f, 4.0f, 1.0f, 0},
   {"position",         BufferGranParamItemPosition,         U_GEN,  0, nullptr, 0.0f, 1.0f, 0.5f, 0},
   {"positionModLevel", BufferGranParamItemPositionModLevel, U_GAIN, 0, nullptr, -1.0f, 1.0f, 1.0f, 0},
   {"scanRate",         BufferGranParamItemScanRate,         U_GEN,  0, nullptr, -2.0f, 2.0f, 0.0f, 0},
   {"spray",            BufferGranParamItemSpray,            U_GEN,  0, nullptr, 0.0f, 1.0f, 0.1f, 0},
   {"pitch",            BufferGranParamItemPitch,            U_SEMI, 0, nullptr, -24.0f, 24.0f, 0.0f, 0},
   {"pitchFine",        BufferGranParamItemPitchFine,        U_CENT, 0, nullptr, -100.0f, 100.0f, 0.0f, 0},
   {"pitchScatter",     BufferGranParamItemPitchScatter,     U_SEMI, 0, nullptr, 0.0f, 24.0f, 0.0f, 0},
   {"density",          BufferGranParamItemDensity,          U_HZ,   0, nullptr, 0.5f, 200.0f, 20.0f, 0},
   {"duration",         BufferGranParamItemDuration,         U_GEN,  0, nullptr, 1.0f, 500.0f, 100.0f, 0},
   {"panSpread",        BufferGranParamItemPanSpread,        U_GEN,  0, nullptr, 0.0f, 1.0f, 0.5f, 0},
   {"level",            BufferGranParamItemLevel,            U_GAIN, 0, nullptr, 0.0f, 2.0f, 1.0f, 0},
   {"velToLevel",       BufferGranParamItemVelToLevel,       U_GEN,  0, nullptr, 0.0f, 1.0f, 0.0f, 0},
   {"onsetRandom",      BufferGranParamItemOnsetRandom,      U_GEN,  0, nullptr, 0.0f, 1.0f, 0.0f, 0},
   {"edoSteps",         BufferGranParamItemEDOSteps,         U_IDX,  0, nullptr, 0.0f, 96.0f, 0.0f, 0},
   {"quantiseMaterial", BufferGranParamItemQuantiseMaterial, U_IDX,  0, kClockGranQuantMaterial, 0.0f, (float)ModalBankMaterialCount, 0.0f, 0},
   {"quantiseMode",     BufferGranParamItemQuantiseMode,     U_IDX,  0, kQuantiseMode, 0.0f, 1.0f, 0.0f, 0},
   {"centreMode",       BufferGranParamItemCentreMode,       U_IDX,  0, nullptr, 0.0f, 63.0f, 0.0f, 0},
   {"modeScatter",      BufferGranParamItemModeScatter,      U_IDX,  0, nullptr, 0.0f, 32.0f, 0.0f, 0},
   {"modeSkew",         BufferGranParamItemModeSkew,         U_CUST, 0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"discretizerStretch", BufferGranParamItemDiscretizerStretch, U_IDX, 0, kModalStretch, 0.0f, 19.0f, 9.0f, 0},
   // Control inputs + attenuators for Scatter/Density/Duration (mirror positionMod above).
   {"scatterModInput",  BufferGranParamItemScatterModInput,  U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"densityModInput",  BufferGranParamItemDensityModInput,  U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"durationModInput", BufferGranParamItemDurationModInput, U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"scatterModLevel",  BufferGranParamItemScatterModLevel,  U_GAIN, 0, nullptr, -1.0f, 1.0f, 1.0f, 0},
   {"densityModLevel",  BufferGranParamItemDensityModLevel,  U_GAIN, 0, nullptr, -1.0f, 1.0f, 1.0f, 0},
   {"durationModLevel", BufferGranParamItemDurationModLevel, U_GAIN, 0, nullptr, -1.0f, 1.0f, 1.0f, 0},
};
const ModuleParamConstantsGroup bufferGranConstantsGroup = {
   const_cast<char*>("Buffer Gran"), sizeof(bufferGranConstants) / sizeof(bufferGranConstants[0]), bufferGranConstants};

// SpectralGran — spectral granular synth reading analysis frames from a rolling audio capture.
// Ranges/defaults from PM's ParamGroupData.m (spectralGranConstants).
// name / item / unit / flags / valueStrings / min / max / def / info
static const char* const kSpectralGranFreeze[]  = {"Rolling", "Frozen", nullptr};
static const char* const kSpectralGranFFTSize[] = {"512", "1024", "2048", "4096", nullptr};
static const ModuleParamConstants spectralGranConstants[] = {
   {"input",            SpectralGranParamItemInput,            U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"gateInput",        SpectralGranParamItemGateInput,        U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"pitchInput",       SpectralGranParamItemPitchInput,       U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"freezeInput",      SpectralGranParamItemFreezeInput,      U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"positionModInput", SpectralGranParamItemPositionModInput, U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"freeze",           SpectralGranParamItemFreeze,           U_IDX,  0, kSpectralGranFreeze,  0.0f, 1.0f, 0.0f, 0},
   {"bufferSeconds",    SpectralGranParamItemBufferSeconds,    U_SEC,  0, nullptr, 0.05f, 4.0f, 1.0f, 0},
   {"fftSize",          SpectralGranParamItemFFTSize,          U_IDX,  0, kSpectralGranFFTSize, 0.0f, 3.0f, 2.0f, 0},
   {"position",         SpectralGranParamItemPosition,         U_GEN,  0, nullptr, 0.0f, 1.0f, 0.5f, 0},
   {"positionModLevel", SpectralGranParamItemPositionModLevel, U_GAIN, 0, nullptr, -1.0f, 1.0f, 1.0f, 0},
   {"scanRate",         SpectralGranParamItemScanRate,         U_GEN,  0, nullptr, -2.0f, 2.0f, 0.0f, 0},
   {"spray",            SpectralGranParamItemSpray,            U_GEN,  0, nullptr, 0.0f, 1.0f, 0.1f, 0},
   {"pitch",            SpectralGranParamItemPitch,            U_SEMI, 0, nullptr, -24.0f, 24.0f, 0.0f, 0},
   {"pitchFine",        SpectralGranParamItemPitchFine,        U_CENT, 0, nullptr, -100.0f, 100.0f, 0.0f, 0},
   {"formant",          SpectralGranParamItemFormant,          U_SEMI, 0, nullptr, -24.0f, 24.0f, 0.0f, 0},
   {"partialSpread",    SpectralGranParamItemPartialSpread,    U_GEN,  0, nullptr, 0.0f, 1.0f, 0.0f, 0},
   {"formantRes",       SpectralGranParamItemFormantRes,       U_GEN,  0, nullptr, 0.0f, 1.0f, 0.5f, 0},
   {"width",            SpectralGranParamItemWidth,            U_GEN,  0, nullptr, 0.0f, 1.0f, 0.0f, 0},
   {"level",            SpectralGranParamItemLevel,            U_GAIN, 0, nullptr, 0.0f, 2.0f, 1.0f, 0},
   {"velToLevel",       SpectralGranParamItemVelToLevel,       U_GEN,  0, nullptr, 0.0f, 1.0f, 0.0f, 0},
};
const ModuleParamConstantsGroup spectralGranConstantsGroup = {
   const_cast<char*>("Spectral Gran"), sizeof(spectralGranConstants) / sizeof(spectralGranConstants[0]), spectralGranConstants};

// IFSGran — IFS-oscillator grains scattered in time/pitch by a 2-transform IFS attractor.
// Ranges/defaults from PM's ParamGroupData.m (ifsGranConstants); rotation range -180..180°.
// Shares the discretizer label arrays (kClockGranQuantMaterial / kQuantiseMode / kModalStretch).
// name / item / unit / flags / valueStrings / min / max / def / info
static const ModuleParamConstants ifsGranConstants[] = {
   {"gateInput",     IFSGranParamItemGateInput,     U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"pitchInput",    IFSGranParamItemPitchInput,    U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"pitch",         IFSGranParamItemPitch,         U_SEMI, 0, nullptr, -48.0f, 48.0f, 0.0f, 0},
   {"pitchFine",     IFSGranParamItemPitchFine,     U_CENT, 0, nullptr, -100.0f, 100.0f, 0.0f, 0},
   {"bx",            IFSGranParamItemBx,            U_GEN,  0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"by",            IFSGranParamItemBy,            U_GEN,  0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"ifs1Squash",    IFSGranParamItemIFS1Squash,    U_GEN,  0, nullptr, -0.99f, 0.99f, 0.5f, 0},
   {"ifs2Squash",    IFSGranParamItemIFS2Squash,    U_GEN,  0, nullptr, -0.99f, 0.99f, 0.5f, 0},
   {"density",       IFSGranParamItemDensity,       U_GEN,  0, nullptr, 0.5f, 200.0f, 20.0f, 0},
   {"duration",      IFSGranParamItemDuration,      U_GEN,  0, nullptr, 1.0f, 500.0f, 50.0f, 0},
   {"t1FixedPointX", IFSGranParamItemT1FixedPointX, U_GEN,  0, nullptr, 0.0f, 1.0f, 0.5f, 0},
   {"t1Contraction", IFSGranParamItemT1Contraction, U_GEN,  0, nullptr, 0.01f, 0.99f, 0.5f, 0},
   {"t1Rotation",    IFSGranParamItemT1Rotation,    U_GEN,  0, nullptr, -180.0f, 180.0f, 0.0f, 0},
   {"t2FixedPointX", IFSGranParamItemT2FixedPointX, U_GEN,  0, nullptr, 0.0f, 1.0f, 0.25f, 0},
   {"t2Contraction", IFSGranParamItemT2Contraction, U_GEN,  0, nullptr, 0.01f, 0.99f, 0.5f, 0},
   {"t2Rotation",    IFSGranParamItemT2Rotation,    U_GEN,  0, nullptr, -180.0f, 180.0f, 0.0f, 0},
   {"probability",   IFSGranParamItemProbability,   U_GEN,  0, nullptr, 0.0f, 1.0f, 0.5f, 0},
   {"scatterAmount", IFSGranParamItemScatterAmount, U_GEN,  0, nullptr, 0.0f, 1.0f, 0.25f, 0},
   {"edoSteps",      IFSGranParamItemEDOSteps,      U_IDX,  0, nullptr, 0.0f, 96.0f, 0.0f, 0},
   {"panSpread",     IFSGranParamItemPanSpread,     U_GEN,  0, nullptr, 0.0f, 1.0f, 0.0f, 0},
   {"outputLevel",   IFSGranParamItemOutputLevel,   U_GAIN, 0, nullptr, 0.0f, 2.0f, 1.0f, 0},
   {"looping",       IFSGranParamItemLooping,       U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"pitchModInput", IFSGranParamItemPitchModInput, U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"pitchModLevel", IFSGranParamItemPitchModLevel, U_SEMI, 0, nullptr, -24.0f, 24.0f, 0.0f, 0},
   {"densityModInput",  IFSGranParamItemDensityModInput,  U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"densityModLevel",  IFSGranParamItemDensityModLevel,  U_CUST, 0, nullptr, -2.0f, 2.0f, 0.0f, 0},
   {"durationModInput", IFSGranParamItemDurationModInput, U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"durationModLevel", IFSGranParamItemDurationModLevel, U_CUST, 0, nullptr, -2.0f, 2.0f, 0.0f, 0},
   {"scatterAmountModInput", IFSGranParamItemScatterAmountModInput, U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"scatterAmountModLevel", IFSGranParamItemScatterAmountModLevel, U_CUST, 0, nullptr, -2.0f, 2.0f, 0.0f, 0},
   {"bxModInput",    IFSGranParamItemBxModInput,    U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"bxModLevel",    IFSGranParamItemBxModLevel,    U_CUST, 0, nullptr, -2.0f, 2.0f, 0.0f, 0},
   {"byModInput",    IFSGranParamItemByModInput,    U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"byModLevel",    IFSGranParamItemByModLevel,    U_CUST, 0, nullptr, -2.0f, 2.0f, 0.0f, 0},
   {"ifs1SquashModInput", IFSGranParamItemIFS1SquashModInput, U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"ifs1SquashModLevel", IFSGranParamItemIFS1SquashModLevel, U_CUST, 0, nullptr, -2.0f, 2.0f, 0.0f, 0},
   {"ifs2SquashModInput", IFSGranParamItemIFS2SquashModInput, U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"ifs2SquashModLevel", IFSGranParamItemIFS2SquashModLevel, U_CUST, 0, nullptr, -2.0f, 2.0f, 0.0f, 0},
   {"t1FixedPointY", IFSGranParamItemT1FixedPointY, U_GEN,  0, nullptr, 0.0f, 1.0f, 0.5f, 0},
   {"t2FixedPointY", IFSGranParamItemT2FixedPointY, U_GEN,  0, nullptr, 0.0f, 1.0f, 0.25f, 0},
   {"grainPeak",     IFSGranParamItemGrainPeak,     U_GEN,  0, nullptr, 0.0f, 1.0f, 0.5f, 0},
   {"quantiseMaterial", IFSGranParamItemQuantiseMaterial, U_IDX, 0, kClockGranQuantMaterial, 0.0f, (float)ModalBankMaterialCount, 0.0f, 0},
   {"quantiseMode",  IFSGranParamItemQuantiseMode,  U_IDX,  0, kQuantiseMode, 0.0f, 1.0f, 0.0f, 0},
   {"centreMode",    IFSGranParamItemCentreMode,    U_IDX,  0, nullptr, 0.0f, 63.0f, 0.0f, 0},
   {"modeScatter",   IFSGranParamItemModeScatter,   U_IDX,  0, nullptr, 0.0f, 32.0f, 2.0f, 0},
   {"modeSkew",      IFSGranParamItemModeSkew,      U_CUST, 0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"discretizerStretch", IFSGranParamItemDiscretizerStretch, U_IDX, 0, kModalStretch, 0.0f, 19.0f, 9.0f, 0},
   {"pitchRange",    IFSGranParamItemPitchRange,    U_GEN,  0, nullptr, 12.0f, 60.0f, 12.0f, 0},
};
const ModuleParamConstantsGroup ifsGranConstantsGroup = {
   const_cast<char*>("IFS Gran"), sizeof(ifsGranConstants) / sizeof(ifsGranConstants[0]), ifsGranConstants};

// FM4Op — DX7-style 4-operator phase-modulation voice. Ranges/defaults from PM's ParamGroupData.m
// (fm4OpConstants + the FM4OP_OP_CONSTANTS macro). Algorithm/wave labels from PM's FM4OpView; the
// retrigger picker reuses kAdsrLegato (Steal / Legato Retrigger / Legato True).
// "\xE2\x80\xBA" = › (routing arrow), "\xE2\x80\x93" = – (en dash).
static const char* const kFM4OpAlgo[] = {
   "4\xE2\x80\xBA""3\xE2\x80\xBA""2\xE2\x80\xBA""1", "(4,3)\xE2\x80\xBA""2\xE2\x80\xBA""1",
   "4\xE2\x80\xBA""2\xE2\x80\xBA""1, 3\xE2\x80\xBA""1", "(2,3,4)\xE2\x80\xBA""1",
   "2\xE2\x80\xBA""1, 4\xE2\x80\xBA""3", "4\xE2\x80\xBA""1,2,3",
   "3\xE2\x80\xBA""2\xE2\x80\xBA""1, +4", "parallel", nullptr };
static const char* const kFM4OpWave[] = { "Sine", "1+2", "1+3", "1+3+5", "1\xE2\x80\x93""4", nullptr };
static const char* const kFM4OpOversample[] = { "Off", "2x", "4x", nullptr };

// One operator's 13 contiguous rows (2 CV input ports + 11 params). LVL = default op level.
#define FM4OP_OP(N, LVL) \
   {"op" #N "AmpInput",  FM4OpParamItemOp##N##AmpInput,  U_GEN,  0, nullptr,    0.0f, 0.0f, 0.0f, ParamMetaDataInput}, \
   {"op" #N "FreqInput", FM4OpParamItemOp##N##FreqInput, U_GEN,  0, nullptr,    0.0f, 0.0f, 0.0f, ParamMetaDataInput}, \
   {"op" #N "Ratio",     FM4OpParamItemOp##N##Ratio,     U_GEN,  0, nullptr,    0.5f, 16.0f, 1.0f, 0}, \
   {"op" #N "Fine",      FM4OpParamItemOp##N##Fine,      U_CENT, 0, nullptr,    -100.0f, 100.0f, 0.0f, 0}, \
   {"op" #N "Wave",      FM4OpParamItemOp##N##Wave,      U_IDX,  0, kFM4OpWave, 0.0f, 4.0f, 0.0f, 0}, \
   {"op" #N "Level",     FM4OpParamItemOp##N##Level,     U_GAIN, 0, nullptr,    0.0f, 1.0f, LVL, 0}, \
   {"op" #N "AmpDepth",  FM4OpParamItemOp##N##AmpDepth,  U_GEN,  0, nullptr,    0.0f, 1.0f, 1.0f, 0}, \
   {"op" #N "FreqDepth", FM4OpParamItemOp##N##FreqDepth, U_GEN,  0, nullptr,    0.0f, 1.0f, 0.0f, 0}, \
   {"op" #N "Delay",     FM4OpParamItemOp##N##Delay,     U_SEC,  0, nullptr,    0.0f, 5.0f, 0.0f, 0}, \
   {"op" #N "Attack",    FM4OpParamItemOp##N##Attack,    U_SEC,  0, nullptr,    0.001f, 20.0f, 0.005f, 0}, \
   {"op" #N "Decay",     FM4OpParamItemOp##N##Decay,     U_SEC,  0, nullptr,    0.001f, 30.0f, 0.3f, 0}, \
   {"op" #N "Sustain",   FM4OpParamItemOp##N##Sustain,   U_GAIN, 0, nullptr,    0.0f, 1.0f, 0.70f, 0}, \
   {"op" #N "Release",   FM4OpParamItemOp##N##Release,   U_SEC,  0, nullptr,    0.001f, 30.0f, 0.3f, 0}

static const ModuleParamConstants fm4OpConstants[] = {
   {"algorithm",   FM4OpParamItemAlgorithm,   U_IDX,  0, kFM4OpAlgo,  0.0f, 7.0f, 0.0f, 0},
   {"feedback",    FM4OpParamItemFeedback,    U_GEN,  0, nullptr, 0.0f, 1.0f, 0.0f, 0},
   {"outputLevel", FM4OpParamItemOutputLevel, U_GAIN, 0, nullptr, 0.0f, 1.0f, 0.8f, 0},
   {"pitchInput",  FM4OpParamItemPitchInput,  U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"gateInput",   FM4OpParamItemGateInput,   U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"retrigger",   FM4OpParamItemRetrigger,   U_IDX,  0, kAdsrLegato, 0.0f, 2.0f, 0.0f, 0},
   {"detune",      FM4OpParamItemDetune,      U_GEN,  0, nullptr, 0.0f, 1.0f, 0.0f, 0},
   {"spread",      FM4OpParamItemSpread,      U_GEN,  0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   FM4OP_OP(1, 0.9f),
   FM4OP_OP(2, 0.6f),
   FM4OP_OP(3, 0.5f),
   FM4OP_OP(4, 0.4f),
   {"pitch",         FM4OpParamItemPitch,         U_SEMI, 0, nullptr, -48.0f, 48.0f, 0.0f, 0},
   {"fine",          FM4OpParamItemFine,          U_CENT, 0, nullptr, -100.0f, 100.0f, 0.0f, 0},
   {"fmInput",       FM4OpParamItemFMInput,       U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"fmLevel",       FM4OpParamItemFMLevel,       U_HZ,   0, nullptr, 0.0f, 2000.0f, 0.0f, 0},
   {"feedbackInput", FM4OpParamItemFeedbackInput, U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"feedbackDepth", FM4OpParamItemFeedbackDepth, U_GEN,  0, nullptr, 0.0f, 1.0f, 0.0f, 0},
   {"oversample",    FM4OpParamItemOversample,    U_IDX,  0, kFM4OpOversample, 0.0f, 2.0f, 0.0f, 0},
};
#undef FM4OP_OP
const ModuleParamConstantsGroup fm4OpConstantsGroup = {
   const_cast<char*>("FM 4-Op"), sizeof(fm4OpConstants) / sizeof(fm4OpConstants[0]), fm4OpConstants};

// DrumKit — master level + per-drum "finalized" user knobs (0..1). Defaults carried over from PM.
static const ModuleParamConstants drumKitConstants[] = {
   {"level",       DrumKitParamItemLevel,       U_GAIN, 0, nullptr, 0.0f, 2.0f, 1.0f,  0},
   // Crash
   {"crashSwell",  DrumKitParamItemCrashSwell,  U_CUST, 0, nullptr, 0.0f, 1.0f, 0.50f, 0},
   {"crashTone",   DrumKitParamItemCrashTone,   U_CUST, 0, nullptr, 0.0f, 1.0f, 0.60f, 0},
   {"crashDecay",  DrumKitParamItemCrashDecay,  U_CUST, 0, nullptr, 0.0f, 1.0f, 0.66f, 0},
   {"crashBright", DrumKitParamItemCrashBright, U_CUST, 0, nullptr, 0.0f, 1.0f, 0.86f, 0},
   // Snare
   {"snareTune",   DrumKitParamItemSnareTune,   U_CUST, 0, nullptr, 0.0f, 1.0f, 0.47f, 0},
   {"snareBody",   DrumKitParamItemSnareBody,   U_CUST, 0, nullptr, 0.0f, 1.0f, 0.35f, 0},
   {"snareBuzz",   DrumKitParamItemSnareBuzz,   U_CUST, 0, nullptr, 0.0f, 1.0f, 0.40f, 0},
   {"snareSnares", DrumKitParamItemSnareSnares, U_CUST, 0, nullptr, 0.0f, 1.0f, 0.55f, 0},
   // Toms
   {"tomTune",     DrumKitParamItemTomTune,     U_CUST, 0, nullptr, 0.0f, 1.0f, 0.49f, 0},
   {"tomDecay",    DrumKitParamItemTomDecay,    U_CUST, 0, nullptr, 0.0f, 1.0f, 0.50f, 0},
   {"tomBright",   DrumKitParamItemTomBright,   U_CUST, 0, nullptr, 0.0f, 1.0f, 0.50f, 0},
   {"tomThump",    DrumKitParamItemTomThump,    U_CUST, 0, nullptr, 0.0f, 1.0f, 0.50f, 0},
   // Hi-hats
   {"hatTune",     DrumKitParamItemHatTune,     U_CUST, 0, nullptr, 0.0f, 1.0f, 0.52f, 0},
   {"hatBalance",  DrumKitParamItemHatBalance,  U_CUST, 0, nullptr, 0.0f, 1.0f, 0.65f, 0},
   {"hatBright",   DrumKitParamItemHatBright,   U_CUST, 0, nullptr, 0.0f, 1.0f, 0.45f, 0},
};
const ModuleParamConstantsGroup drumKitConstantsGroup = {
   const_cast<char*>("DrumKit"), sizeof(drumKitConstants) / sizeof(drumKitConstants[0]), drumKitConstants};

// SharedLFO — plain-LFO params minus the inputs, plus Free Run. Wave/LrPhase reuse the LFO's own
// value-string tables (kLfoWave / kLfoPhase).
static const ModuleParamConstants sharedLFOConstants[] = {
   {"frequency", SharedLFOParamItemPitch,   U_OCT,  0, nullptr,   -7.0f, 7.0f, 0.0f, 0},
   {"wave",      SharedLFOParamItemWave,    U_IDX,  0, kLfoWave,   0.0f, 3.0f, 1.0f, 0}, // Sine..Impulse; default Triangle
   {"lrPhase",   SharedLFOParamItemPhase,   U_IDX,  0, kLfoPhase,  0.0f, 4.0f, 0.0f, 0},
   {"freeRun",   SharedLFOParamItemFreeRun, U_BOOL, 0, nullptr,    0.0f, 1.0f, 0.0f, 0}, // off = voices-active-only (reset on silence)
   {"clockInput", SharedLFOParamItemClockInput, U_GEN, 0, nullptr,   0.0f, 0.0f, 0.0f, ParamMetaDataInput},
   {"clockRatio", SharedLFOParamItemClockRatio, U_IDX, 0, kLfoClockRatio, 0.0f, 13.0f, 0.0f, ParamMetaDataForceDropdown}, // default Auto
};
const ModuleParamConstantsGroup sharedLFOConstantsGroup = {
   const_cast<char*>("Shared LFO"), sizeof(sharedLFOConstants) / sizeof(sharedLFOConstants[0]), sharedLFOConstants};

// Noise — white/pink/brown generator (no inputs). type default Pink (matches PM). Item 3
// (TypeMono) is reserved/unused (see pm_params.h) so no row here.
// name / item / unit / flags / valueStrings / min / max / def / info
static const ModuleParamConstants noiseConstants[] = {
   {"mono", NoiseParamItemMono, U_BOOL, 0, nullptr,    0.0f, 1.0f, 0.0f, 0},
   {"type", NoiseParamItemType, U_IDX,  0, kNoiseType, (float)NoiseTypeWhite, (float)NoiseTypeBrown, (float)NoiseTypePink, 0},
};
const ModuleParamConstantsGroup noiseConstantsGroup = {
   const_cast<char*>("Noise"), sizeof(noiseConstants) / sizeof(noiseConstants[0]), noiseConstants};

// --- MIDI converters. Ranges/defaults from PM's ParamGroupData.m. -------------------------

// Velocity converter — MIDI note velocity → a held control value, with a floor + response curve.
static const ModuleParamConstants velocityConverterConstants[] = {
   {"ignoreNoteOff", VelocityParamItemIgnoreNoteOff, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"velocityMin",   VelocityParamItemMin,           U_PCT,  0, nullptr, 0.0f, 100.0f, 30.0f, 0},
   {"curve",         VelocityParamItemCurve,         U_GEN,  0, nullptr, 0.0f, 1.0f, 0.5f, 0},
};
const ModuleParamConstantsGroup velocityConverterConstantsGroup = {
   const_cast<char*>("Velocity"), sizeof(velocityConverterConstants) / sizeof(velocityConverterConstants[0]), velocityConverterConstants};

// Channel pressure — no user params; one hidden dummy slot (min==max==0 → Catalog skips it, so it
// is neither a knob nor a port) reserves storage for UI tracking, matching PM's DUMMY_ITEM.
static const ModuleParamConstants channelPressureConstants[] = {
   {"dummy", ChannelPressureParamItemDummy, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, 0}, // hidden
};
const ModuleParamConstantsGroup channelPressureConstantsGroup = {
   const_cast<char*>("Channel Pressure"), sizeof(channelPressureConstants) / sizeof(channelPressureConstants[0]), channelPressureConstants};

// CC converter — a chosen controller number → a held value (Boolean gate or continuous), optionally
// scaling a patched input as a VCA (out = in × (offset + amount·cc)).
static const ModuleParamConstants ccConverterConstants[] = {
   {"ccNumber",    CCParamItemNumber,    U_IDX,  0, nullptr, 0.0f, 127.0f, 1.0f, 0},
   {"ccIsBoolean", CCParamItemIsBoolean, U_BOOL, 0, nullptr, 0.0f, 1.0f, 0.0f, 0},
   {"input",       CCParamItemInput,     U_GEN,  0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"ccAmount",    CCParamItemAmount,    U_GEN,  0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"ccOffset",    CCParamItemOffset,    U_GEN,  0, nullptr, 0.0f, 1.0f, 0.0f, 0},
   {"ccPerNote",   CCParamItemPerNote,   U_BOOL, 0, nullptr, 0.0f, 1.0f, 0.0f, 0},
};
const ModuleParamConstantsGroup ccConverterConstantsGroup = {
   const_cast<char*>("CC Converter"), sizeof(ccConverterConstants) / sizeof(ccConverterConstants[0]), ccConverterConstants};

// Pitch bend converter — MIDI 2.0 pitch bend → a bipolar octaves-re-A440 signal. Channel bend range
// in quarter-tones; per-note bend range in semitones (±48 by default, the MPE convention).
static const ModuleParamConstants pitchBendConverterConstants[] = {
   {"quarterTones", PitchBendConverterParamItemQuarterTones, U_IDX, 0, nullptr, 0.0f, 8.0f, 4.0f, 0},
   {"perNoteSemitones", PitchBendConverterParamItemPerNoteSemitones, U_SEMI, 0, nullptr, 0.0f, 96.0f, 48.0f, 0},
};
const ModuleParamConstantsGroup pitchBendConverterConstantsGroup = {
   const_cast<char*>("Pitch Bend"), sizeof(pitchBendConverterConstants) / sizeof(pitchBendConverterConstants[0]), pitchBendConverterConstants};

// Note gate — high while any note in [low, high] is held (tracks the note that raised the gate).
static const ModuleParamConstants noteGateConstants[] = {
   {"lowNote",  NoteGateParamItemMIDILowNote,  U_IDX, 0, nullptr, 0.0f, 127.0f, 0.0f, 0},
   {"highNote", NoteGateParamItemMIDIHighNote, U_IDX, 0, nullptr, 0.0f, 127.0f, 127.0f, 0},
   {"gateOffDelay", NoteGateParamItemGateOffDelay, U_SEC, 0, nullptr, 0.0f, 10.0f, 0.0f, 0}, // hold gate high after NoteOff
};
const ModuleParamConstantsGroup noteGateConstantsGroup = {
   const_cast<char*>("Note Gate"), sizeof(noteGateConstants) / sizeof(noteGateConstants[0]), noteGateConstants};

// MIDI Note Number Converter — latches each NoteOn's number, normalized over [low, high] to
// [0,1] (Monopolar) or [-1,+1] (Bipolar); holds between notes.
static const ModuleParamConstants midiNoteNumberConverterConstants[] = {
   {"rangeType", MIDINoteNumberConverterParamItemRangeType, U_IDX, 0, kMidiRangeType, 0.0f, 1.0f, 0.0f, 0},
   {"lowNote",   MIDINoteNumberConverterParamItemLowNote,   U_IDX, 0, nullptr,        0.0f, 127.0f, 0.0f, 0},
   {"highNote",  MIDINoteNumberConverterParamItemHighNote,  U_IDX, 0, nullptr,        0.0f, 127.0f, 127.0f, 0},
};
const ModuleParamConstantsGroup midiNoteNumberConverterConstantsGroup = {
   const_cast<char*>("MIDI Note Number"), sizeof(midiNoteNumberConverterConstants) / sizeof(midiNoteNumberConverterConstants[0]), midiNoteNumberConverterConstants};

// Value — a constant control signal (a knob, effectively).
static const ModuleParamConstants valueConstants[] = {
   {"value", ValueParamItemValue, U_GEN, 0, nullptr, -4.0f, 4.0f, 1.0f, 0},
};
const ModuleParamConstantsGroup valueConstantsGroup = {
   const_cast<char*>("Value"), sizeof(valueConstants) / sizeof(valueConstants[0]), valueConstants};

// Sequencer — 16-step control-value sequencer. First/Last span 0..7 by default (8 active steps);
// SequencerModule.cpp's to1Based() also accepts the wider 1..16 form for old patches.
static const ModuleParamConstants sequencerConstants[] = {
   {"level1",  SequencerParamItemLevel1,  U_GEN, 0, nullptr, -4.0f, 4.0f, 0.0f, 0},
   {"level2",  SequencerParamItemLevel2,  U_GEN, 0, nullptr, -4.0f, 4.0f, 0.0f, 0},
   {"level3",  SequencerParamItemLevel3,  U_GEN, 0, nullptr, -4.0f, 4.0f, 0.0f, 0},
   {"level4",  SequencerParamItemLevel4,  U_GEN, 0, nullptr, -4.0f, 4.0f, 0.0f, 0},
   {"level5",  SequencerParamItemLevel5,  U_GEN, 0, nullptr, -4.0f, 4.0f, 0.0f, 0},
   {"level6",  SequencerParamItemLevel6,  U_GEN, 0, nullptr, -4.0f, 4.0f, 0.0f, 0},
   {"level7",  SequencerParamItemLevel7,  U_GEN, 0, nullptr, -4.0f, 4.0f, 0.0f, 0},
   {"level8",  SequencerParamItemLevel8,  U_GEN, 0, nullptr, -4.0f, 4.0f, 0.0f, 0},
   {"level9",  SequencerParamItemLevel9,  U_GEN, 0, nullptr, -4.0f, 4.0f, 0.0f, 0},
   {"level10", SequencerParamItemLevel10, U_GEN, 0, nullptr, -4.0f, 4.0f, 0.0f, 0},
   {"level11", SequencerParamItemLevel11, U_GEN, 0, nullptr, -4.0f, 4.0f, 0.0f, 0},
   {"level12", SequencerParamItemLevel12, U_GEN, 0, nullptr, -4.0f, 4.0f, 0.0f, 0},
   {"level13", SequencerParamItemLevel13, U_GEN, 0, nullptr, -4.0f, 4.0f, 0.0f, 0},
   {"level14", SequencerParamItemLevel14, U_GEN, 0, nullptr, -4.0f, 4.0f, 0.0f, 0},
   {"level15", SequencerParamItemLevel15, U_GEN, 0, nullptr, -4.0f, 4.0f, 0.0f, 0},
   {"level16", SequencerParamItemLevel16, U_GEN, 0, nullptr, -4.0f, 4.0f, 0.0f, 0},
   {"trigger", SequencerParamItemTriggerInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"first",   SequencerParamItemFirst, U_IDX, 0, nullptr, 0.0f, 7.0f, 0.0f, 0},
   {"last",    SequencerParamItemLast,  U_IDX, 0, nullptr, 0.0f, 7.0f, 7.0f, 0},
   {"page",    SequencerParamItemPage,  U_IDX, 0, nullptr, 0.0f, 1.0f, 0.0f, 0},
   {"reset",   SequencerParamItemResetInput,  U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"runGate", SequencerParamItemRunGateInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
};
const ModuleParamConstantsGroup sequencerConstantsGroup = {
   const_cast<char*>("Sequencer"), sizeof(sequencerConstants) / sizeof(sequencerConstants[0]), sequencerConstants};

// Quad Combine — 4-input Add/Multiply, unconnected inputs use the operation's identity value.
static const ModuleParamConstants quadCombineConstants[] = {
   {"input1", QuadCombineParamItemInput1, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"input2", QuadCombineParamItemInput2, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"input3", QuadCombineParamItemInput3, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"input4", QuadCombineParamItemInput4, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"level",  QuadCombineParamItemOutputLevel, U_GAIN, 0, nullptr, 0.0f, 1.0f, 0.7f, 0},
   {"type",   QuadCombineParamItemType, U_IDX, 0, kAddMultiply, (float)ParamItemCombineType_Add,
    (float)ParamItemCombineType_Multiply, (float)ParamItemCombineType_Add, 0},
   {"clip",   QuadCombineParamItemClip, U_BOOL, 0, nullptr, 0.0f, 1.0f, 0.0f, 0},
};
const ModuleParamConstantsGroup quadCombineConstantsGroup = {
   const_cast<char*>("Quad Combine"), sizeof(quadCombineConstants) / sizeof(quadCombineConstants[0]), quadCombineConstants};

// Quad Stereo Mixer — 4 stereo inputs, each with its own Level and Pan. NOTE: PM's own
// ParamGroupData.m keys the level1..4 entries to QuadStereoMixerParamItemInput1..4 by mistake
// (copy-paste bug — they alias the very input-connection params right above them); this table
// uses the correct QuadStereoMixerParamItemLevel1..4 items instead.
static const ModuleParamConstants quadStereoMixerConstants[] = {
   {"input1", QuadStereoMixerParamItemInput1, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"input2", QuadStereoMixerParamItemInput2, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"input3", QuadStereoMixerParamItemInput3, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"input4", QuadStereoMixerParamItemInput4, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"level1", QuadStereoMixerParamItemLevel1, U_GAIN, 0, nullptr, 0.0f, 1.0f, 0.7f, 0},
   {"level2", QuadStereoMixerParamItemLevel2, U_GAIN, 0, nullptr, 0.0f, 1.0f, 0.7f, 0},
   {"level3", QuadStereoMixerParamItemLevel3, U_GAIN, 0, nullptr, 0.0f, 1.0f, 0.7f, 0},
   {"level4", QuadStereoMixerParamItemLevel4, U_GAIN, 0, nullptr, 0.0f, 1.0f, 0.7f, 0},
   {"pan1",   QuadStereoMixerParamItemPan1, U_GEN, 0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"pan2",   QuadStereoMixerParamItemPan2, U_GEN, 0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"pan3",   QuadStereoMixerParamItemPan3, U_GEN, 0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"pan4",   QuadStereoMixerParamItemPan4, U_GEN, 0, nullptr, -1.0f, 1.0f, 0.0f, 0},
};
const ModuleParamConstantsGroup quadStereoMixerConstantsGroup = {
   const_cast<char*>("Quad Stereo Mixer"), sizeof(quadStereoMixerConstants) / sizeof(quadStereoMixerConstants[0]), quadStereoMixerConstants};

// X-Fade — see pm_params.h's XFadeParamItem comment. NOTE: PM's own ParamGroupData.m keys the
// "panType" entry to XFadeParamItemCurve by mistake (copy-paste bug — it aliases Curve); this
// table uses the correct XFadeParamItemPanType item instead.
static const ModuleParamConstants xFadeConstants[] = {
   {"inputA",     XFadeParamItemInputA, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"inputB",     XFadeParamItemInputB, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"control",    XFadeParamItemControl, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"position",   XFadeParamItemPosition, U_GEN, 0, nullptr, 0.0f, 1.0f, 0.5f, 0},
   {"curve",      XFadeParamItemCurve, U_IDX, 0, kXFadeCurve, (float)XFadeTypeLinear,
    (float)XFadeTypeEqualPower, (float)XFadeTypeEqualPower, 0},
   {"outputLevel",XFadeParamItemOutputLevel, U_GAIN, 0, nullptr, 0.0f, 1.0f, 0.7f, 0},
   {"levelA",     XFadeParamItemLevelA, U_GAIN, 0, nullptr, 0.0f, 1.0f, 0.7f, 0},
   {"levelB",     XFadeParamItemLevelB, U_GAIN, 0, nullptr, 0.0f, 1.0f, 0.7f, 0},
   {"panA",       XFadeParamItemPanA, U_GEN, 0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"panB",       XFadeParamItemPanB, U_GEN, 0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"panType",    XFadeParamItemPanType, U_IDX, 0, kXFadePanType, (float)XFadePanTypeMix,
    (float)XFadePanTypeSelect, (float)XFadePanTypeSelect, 0},
};
const ModuleParamConstantsGroup xFadeConstantsGroup = {
   const_cast<char*>("X-Fade"), sizeof(xFadeConstants) / sizeof(xFadeConstants[0]), xFadeConstants};

// Selector — see pm_params.h's SelectorParamItem comment. 6 signal inputs + 1 Selector CV pick a
// zone via 5 boundaries. Boundaries are MIDI note numbers (0..127); the DSP converts each to the
// pitch CV domain via (note-69)/12 so it lines up with a Pitch Converter feeding the Selector input.
// The DGL UI renders these U_NOTE knobs as note names and snaps them to whole semitones. Defaults
// place the 5 splits at C2/C3/C4/C5/C6 (notes 36/48/60/72/84, one octave apart).
static const ModuleParamConstants selectorConstants[] = {
   {"input1",     SelectorParamItemInput1,    U_GEN,  0, nullptr,  0.0f,   0.0f,   0.0f, ParamMetaDataInput}, // connection
   {"input2",     SelectorParamItemInput2,    U_GEN,  0, nullptr,  0.0f,   0.0f,   0.0f, ParamMetaDataInput}, // connection
   {"input3",     SelectorParamItemInput3,    U_GEN,  0, nullptr,  0.0f,   0.0f,   0.0f, ParamMetaDataInput}, // connection
   {"input4",     SelectorParamItemInput4,    U_GEN,  0, nullptr,  0.0f,   0.0f,   0.0f, ParamMetaDataInput}, // connection
   {"input5",     SelectorParamItemInput5,    U_GEN,  0, nullptr,  0.0f,   0.0f,   0.0f, ParamMetaDataInput}, // connection
   {"input6",     SelectorParamItemInput6,    U_GEN,  0, nullptr,  0.0f,   0.0f,   0.0f, ParamMetaDataInput}, // connection
   {"selector",   SelectorParamItemSelector,  U_GEN,  0, nullptr,  0.0f,   0.0f,   0.0f, ParamMetaDataInput}, // connection (zone-select CV)
   {"boundary1",  SelectorParamItemBoundary1, U_NOTE, 0, nullptr,  0.0f, 127.0f,  36.0f, 0}, // C2
   {"boundary2",  SelectorParamItemBoundary2, U_NOTE, 0, nullptr,  0.0f, 127.0f,  48.0f, 0}, // C3
   {"boundary3",  SelectorParamItemBoundary3, U_NOTE, 0, nullptr,  0.0f, 127.0f,  60.0f, 0}, // C4
   {"boundary4",  SelectorParamItemBoundary4, U_NOTE, 0, nullptr,  0.0f, 127.0f,  72.0f, 0}, // C5
   {"boundary5",  SelectorParamItemBoundary5, U_NOTE, 0, nullptr,  0.0f, 127.0f,  84.0f, 0}, // C6
   {"curve",      SelectorParamItemCurve,     U_IDX,  0, kSelectorCurve, (float)SelectorCurveHard,
    (float)SelectorCurveEqualPower, (float)SelectorCurveHard, 0},
   {"spread",     SelectorParamItemSpread,    U_SEMI, 0, nullptr,  0.0f,  24.0f,   0.0f, 0}, // semitones (÷12 → CV)
   {"transition", SelectorParamItemTransition,U_MS,   0, nullptr,  0.0f, 500.0f,   0.0f, 0}, // glide on zone change only
   {"outputLevel",SelectorParamItemOutputLevel,U_GAIN,0, nullptr,  0.0f,   1.0f,   1.0f, 0},
};
const ModuleParamConstantsGroup selectorConstantsGroup = {
   const_cast<char*>("Selector"), sizeof(selectorConstants) / sizeof(selectorConstants[0]), selectorConstants};

// Chorus — modulated stereo delay with feedback.
static const ModuleParamConstants chorusConstants[] = {
   {"input",         ChorusParamItemInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"baseDelay",     ChorusParamItemBaseDelay, U_GEN, 0, nullptr, 5.0f, 30.0f, 10.0f, 0},
   {"mix",           ChorusParamItemMix, U_GEN, 0, nullptr, 0.0f, 1.0f, 0.5f, 0},
   {"feedback",      ChorusParamItemFeedback, U_GEN, 0, nullptr, 0.0f, 0.99f, 0.2f, 0},
   {"modInput",      ChorusParamItemModDelayInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"modDelayLevel", ChorusParamItemModDelayLevel, U_GAIN, 0, nullptr, 0.0f, 1.0f, 0.5f, 0},
};
const ModuleParamConstantsGroup chorusConstantsGroup = {
   const_cast<char*>("Chorus"), sizeof(chorusConstants) / sizeof(chorusConstants[0]), chorusConstants};

// Flanger — like Chorus, plus a second opposite-polarity dry tap.
static const ModuleParamConstants flangerConstants[] = {
   {"input",         FlangerParamItemInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"baseDelay",     FlangerParamItemBaseDelay, U_GEN, 0, nullptr, 0.5f, 5.0f, 2.5f, 0},
   {"feedback",      FlangerParamItemFeedback, U_GEN, 0, nullptr, 0.0f, 0.99f, 0.2f, 0},
   {"dryLevel",      FlangerParamItemDryLevel, U_GAIN, 0, nullptr, 0.0f, 1.0f, 0.7f, 0},
   {"tap1Level",     FlangerParamItemTap1Level, U_GAIN, 0, nullptr, 0.0f, 1.0f, 0.7f, 0},
   {"tap2Level",     FlangerParamItemTap2Level, U_GAIN, 0, nullptr, 0.0f, 1.0f, 0.3f, 0},
   {"modInput",      FlangerParamItemModDelayInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"modDelayLevel", FlangerParamItemModDelayLevel, U_GAIN, 0, nullptr, 0.0f, 1.0f, 0.5f, 0},
};
const ModuleParamConstantsGroup flangerConstantsGroup = {
   const_cast<char*>("Flanger"), sizeof(flangerConstants) / sizeof(flangerConstants[0]), flangerConstants};

// Phaser — 2..8 all-pass stages, center frequency + Q, modulated, with feedback and dry/wet mix.
static const ModuleParamConstants phaserConstants[] = {
   {"input",           PhaserParamItemInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"modInput",        PhaserParamItemModInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"stages",          PhaserParamItemStages, U_IDX, 0, nullptr, 2.0f, 8.0f, 6.0f, 0},
   {"feedback",        PhaserParamItemFeedback, U_GEN, 0, nullptr, 0.0f, 0.99f, 0.2f, 0},
   {"mix",             PhaserParamItemMix, U_GEN, 0, nullptr, 0.0f, 1.0f, 0.5f, 0},
   {"centerFrequency", PhaserParamItemCenterFrequency, U_HZ, 0, nullptr, 100.0f, 4000.0f, 1000.0f, 0},
   {"q",               PhaserParamItemQ, U_GEN, 0, nullptr, 0.3f, 5.0f, 0.707f, 0},
   {"modLevel",        PhaserParamItemModLevel, U_GAIN, 0, nullptr, 0.0f, 1.0f, 0.5f, 0},
};
const ModuleParamConstantsGroup phaserConstantsGroup = {
   const_cast<char*>("Phaser"), sizeof(phaserConstants) / sizeof(phaserConstants[0]), phaserConstants};

// Freq Shift — single-sideband (Hilbert) frequency shifter.
static const ModuleParamConstants freqShiftConstants[] = {
   {"input",         FreqShiftParamItemInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"shiftModInput", FreqShiftParamItemShiftModInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"shift",         FreqShiftParamItemShift, U_HZ, 0, nullptr, -2000.0f, 2000.0f, 0.0f, 0},
   {"shiftModLevel", FreqShiftParamItemShiftModLevel, U_HZ, 0, nullptr, -2000.0f, 2000.0f, 0.0f, 0},
   {"feedback",      FreqShiftParamItemFeedback, U_GEN, 0, nullptr, 0.0f, 0.95f, 0.0f, 0},
   {"mix",           FreqShiftParamItemMix, U_GEN, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
};
const ModuleParamConstantsGroup freqShiftConstantsGroup = {
   const_cast<char*>("Freq Shift"), sizeof(freqShiftConstants) / sizeof(freqShiftConstants[0]), freqShiftConstants};

// Clock Divider — divides an Input clock by Divisor, re-firing a fixed-width pulse.
static const ModuleParamConstants clockDividerConstants[] = {
   {"input",      ClockDividerParamItemInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"reset",      ClockDividerParamItemResetInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"runGate",    ClockDividerParamItemRunGate, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"divisor",    ClockDividerParamItemDivisor, U_IDX, 0, nullptr, 1.0f, 16.0f, 2.0f, 0},
   {"pulseWidth", ClockDividerParamItemPulseWidth, U_SEC, 0, nullptr, 0.001f, 0.5f, 0.01f, 0},
};
const ModuleParamConstantsGroup clockDividerConstantsGroup = {
   const_cast<char*>("Clock Divider"), sizeof(clockDividerConstants) / sizeof(clockDividerConstants[0]), clockDividerConstants};

// Delta Impulse — fires when the input changes by more than Delta since the previous sample.
static const ModuleParamConstants deltaImpulseConstants[] = {
   {"input",   DeltaImpulseParamItemInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"trigger", DeltaImpulseParamItemTrigger, U_IDX, 0, kImpulseTrigger, (float)ImpulseTriggerRising,
    (float)ImpulseTriggerBoth, (float)ImpulseTriggerRising, 0},
   {"delta",   DeltaImpulseParamItemDelta, U_GEN, 0, nullptr, -0.99f, 0.99f, 0.0f, 0},
};
const ModuleParamConstantsGroup deltaImpulseConstantsGroup = {
   const_cast<char*>("Delta Impulse"), sizeof(deltaImpulseConstants) / sizeof(deltaImpulseConstants[0]), deltaImpulseConstants};

// One Shot — a rising edge on Input fires a fixed-width Time pulse; Reset cancels it early.
static const ModuleParamConstants oneShotConstants[] = {
   {"input", OneShotParamItemInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"time",  OneShotParamItemTime, U_SEC, 0, nullptr, 0.001f, 5.0f, 0.1f, 0},
   {"reset", OneShotParamItemReset, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
};
const ModuleParamConstantsGroup oneShotConstantsGroup = {
   const_cast<char*>("One Shot"), sizeof(oneShotConstants) / sizeof(oneShotConstants[0]), oneShotConstants};

// Logic Gate — AND/OR/XOR up to 4 (optionally inverted) gate inputs; Gate or Impulse output.
static const ModuleParamConstants logicGateConstants[] = {
   {"input1",     LogicGateParamItemInput1, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"input2",     LogicGateParamItemInput2, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"input3",     LogicGateParamItemInput3, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"input4",     LogicGateParamItemInput4, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"invert1",    LogicGateParamItemInvert1, U_BOOL, 0, nullptr, 0.0f, 1.0f, 0.0f, 0},
   {"invert2",    LogicGateParamItemInvert2, U_BOOL, 0, nullptr, 0.0f, 1.0f, 0.0f, 0},
   {"invert3",    LogicGateParamItemInvert3, U_BOOL, 0, nullptr, 0.0f, 1.0f, 0.0f, 0},
   {"invert4",    LogicGateParamItemInvert4, U_BOOL, 0, nullptr, 0.0f, 1.0f, 0.0f, 0},
   {"operation",  LogicGateParamItemOperation, U_IDX, 0, kLogicOperator, (float)LogicOperatorAND,
    (float)LogicOperatorXOR, (float)LogicOperatorAND, 0},
   {"outputMode", LogicGateParamItemOutputMode, U_IDX, 0, kLogicOutputMode, (float)LogicOutputModeGate,
    (float)LogicOutputModeImpulse, (float)LogicOutputModeGate, 0},
   {"edge",       LogicGateParamItemEdge, U_IDX, 0, kLogicGateEdgeMode, (float)LogicGateEdgeModeRising,
    (float)LogicGateEdgeModeBoth, (float)LogicGateEdgeModeBoth, 0},
   {"pulseWidth", LogicGateParamItemPulseWidth, U_SEC, 0, nullptr, 0.001f, 0.5f, 0.01f, 0},
   {"hold",       LogicGateParamItemHold, U_SEC, 0, nullptr, 0.0f, 0.2f, 0.0f, 0},
};
const ModuleParamConstantsGroup logicGateConstantsGroup = {
   const_cast<char*>("Logic Gate"), sizeof(logicGateConstants) / sizeof(logicGateConstants[0]), logicGateConstants};

// Discretizer — quantizes Input to a raw step size or a musical (equal-division) grid.
static const ModuleParamConstants discretizerConstants[] = {
   {"input",         DiscretizerParamItemInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"kind",          DiscretizerParamItemKind, U_IDX, 0, kDiscretizerKind, (float)DiscretizerKindStepSize,
    (float)DiscretizerKindMusical, (float)DiscretizerKindStepSize, 0},
   {"stepSize",      DiscretizerParamItemStepSize, U_GEN, 0, nullptr, 0.001f, 0.999f, 0.1f, 0},
   {"noteInterval",  DiscretizerParamItemNoteInterval, U_IDX, 0, kDiscretizerNoteInterval,
    (float)DiscretizerNoteInterval5, (float)DiscretizerNoteInterval53, (float)DiscretizerNoteInterval12, 0},
   {"stepSizeInput", DiscretizerParamItemStepSizeInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
};
const ModuleParamConstantsGroup discretizerConstantsGroup = {
   const_cast<char*>("Discretizer"), sizeof(discretizerConstants) / sizeof(discretizerConstants[0]), discretizerConstants};

// EDO Discretizer — quantizes Input to a custom microtonal scale: StepsPerOctave equal
// divisions of the octave, individually enabled via NoteEnable0..30. See pm_params.h's
// EDODiscretizerParamItem for the full design note (Origin Note is display/fallback-only,
// not a transposition; the Piano/Alternating layout toggle was deliberately dropped).
static const char* const kOriginNoteNames[] =
   {"A", "A#", "B", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", nullptr};
static const ModuleParamConstants edoDiscretizerConstants[] = {
   {"input",          EDODiscretizerParamItemInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"stepsPerOctave", EDODiscretizerParamItemStepsPerOctave, U_IDX, 0, nullptr, 12.0f, 31.0f, 24.0f, 0},
   {"originNote",     EDODiscretizerParamItemOriginNote, U_IDX, 0, kOriginNoteNames, 0.0f, 11.0f, 0.0f, 0},
   {"enable0",    EDODiscretizerParamItemNoteEnable0, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable1",    EDODiscretizerParamItemNoteEnable1, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable2",    EDODiscretizerParamItemNoteEnable2, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable3",    EDODiscretizerParamItemNoteEnable3, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable4",    EDODiscretizerParamItemNoteEnable4, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable5",    EDODiscretizerParamItemNoteEnable5, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable6",    EDODiscretizerParamItemNoteEnable6, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable7",    EDODiscretizerParamItemNoteEnable7, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable8",    EDODiscretizerParamItemNoteEnable8, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable9",    EDODiscretizerParamItemNoteEnable9, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable10",   EDODiscretizerParamItemNoteEnable10, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable11",   EDODiscretizerParamItemNoteEnable11, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable12",   EDODiscretizerParamItemNoteEnable12, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable13",   EDODiscretizerParamItemNoteEnable13, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable14",   EDODiscretizerParamItemNoteEnable14, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable15",   EDODiscretizerParamItemNoteEnable15, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable16",   EDODiscretizerParamItemNoteEnable16, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable17",   EDODiscretizerParamItemNoteEnable17, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable18",   EDODiscretizerParamItemNoteEnable18, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable19",   EDODiscretizerParamItemNoteEnable19, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable20",   EDODiscretizerParamItemNoteEnable20, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable21",   EDODiscretizerParamItemNoteEnable21, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable22",   EDODiscretizerParamItemNoteEnable22, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable23",   EDODiscretizerParamItemNoteEnable23, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable24",   EDODiscretizerParamItemNoteEnable24, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable25",   EDODiscretizerParamItemNoteEnable25, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable26",   EDODiscretizerParamItemNoteEnable26, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable27",   EDODiscretizerParamItemNoteEnable27, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable28",   EDODiscretizerParamItemNoteEnable28, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable29",   EDODiscretizerParamItemNoteEnable29, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"enable30",   EDODiscretizerParamItemNoteEnable30, U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
};
const ModuleParamConstantsGroup edoDiscretizerConstantsGroup = {
   const_cast<char*>("EDO Discretizer"), sizeof(edoDiscretizerConstants) / sizeof(edoDiscretizerConstants[0]), edoDiscretizerConstants};

// Overdrive — cubic soft-clipper, Drive-blended, Bias-skewed (DC-corrected).
static const ModuleParamConstants overdriveConstants[] = {
   {"input", OverdriveParamItemInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"drive", OverdriveParamItemDrive, U_GEN, 0, nullptr, 0.0f, 1.0f, 0.5f, 0},
   {"bias",  OverdriveParamItemBias, U_GEN, 0, nullptr, 0.0f, 0.5f, 0.0f, 0},
};
const ModuleParamConstantsGroup overdriveConstantsGroup = {
   const_cast<char*>("Overdrive"), sizeof(overdriveConstants) / sizeof(overdriveConstants[0]), overdriveConstants};

// IFS Resonator — two coupled recursive delay lines per channel, pitch-tracked, fully CV-able.
static const ModuleParamConstants ifsResConstants[] = {
   {"audioInput",         IFSResParamItemAudioInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"pitch",              IFSResParamItemPitch, U_GEN, 0, nullptr, -48.0f, 48.0f, 0.0f, 0},
   {"bx",                 IFSResParamItemBx, U_GEN, 0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"by",                 IFSResParamItemBy, U_GEN, 0, nullptr, -1.0f, 1.0f, 0.0f, 0},
   {"ifs1Squash",         IFSResParamItemIFS1Squash, U_GEN, 0, nullptr, -0.99f, 0.99f, 0.5f, 0},
   {"ifs2Squash",         IFSResParamItemIFS2Squash, U_GEN, 0, nullptr, -0.99f, 0.99f, 0.5f, 0},
   {"outputLevel",        IFSResParamItemOutputLevel, U_GAIN, 0, nullptr, 0.0f, 2.0f, 1.0f, 0},
   {"dryWet",             IFSResParamItemDryWet, U_GEN, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"pitchModInput",      IFSResParamItemPitchModInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"pitchModLevel",      IFSResParamItemPitchModLevel, U_GEN, 0, nullptr, -24.0f, 24.0f, 0.0f, 0},
   {"bxModInput",         IFSResParamItemBxModInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"bxModLevel",         IFSResParamItemBxModLevel, U_GEN, 0, nullptr, -2.0f, 2.0f, 0.0f, 0},
   {"byModInput",         IFSResParamItemByModInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"byModLevel",         IFSResParamItemByModLevel, U_GEN, 0, nullptr, -2.0f, 2.0f, 0.0f, 0},
   {"ifs1SquashModInput", IFSResParamItemIFS1SquashModInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"ifs1SquashModLevel", IFSResParamItemIFS1SquashModLevel, U_GEN, 0, nullptr, -2.0f, 2.0f, 0.0f, 0},
   {"ifs2SquashModInput", IFSResParamItemIFS2SquashModInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"ifs2SquashModLevel", IFSResParamItemIFS2SquashModLevel, U_GEN, 0, nullptr, -2.0f, 2.0f, 0.0f, 0},
};
const ModuleParamConstantsGroup ifsResConstantsGroup = {
   const_cast<char*>("IFS Res"), sizeof(ifsResConstants) / sizeof(ifsResConstants[0]), ifsResConstants};

// Polynomial — out = clamp(SquareTerm*in^2 + LinearTerm*in + ConstantTerm, Minimum, Maximum).
static const ModuleParamConstants polynomialConstants[] = {
   {"input",        PolynomialParamItemInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"squareTerm",   PolynomialParamItemSquareTerm, U_GEN, 0, nullptr, -4.0f, 4.0f, 1.0f, 0},
   {"linearTerm",   PolynomialParamItemLinearTerm, U_GEN, 0, nullptr, -4.0f, 4.0f, 1.0f, 0},
   {"constantTerm", PolynomialParamItemConstantTerm, U_GEN, 0, nullptr, -4.0f, 4.0f, 1.0f, 0},
   {"minimum",      PolynomialParamItemMinimum, U_GEN, 0, nullptr, -4.0f, 4.0f, -4.0f, 0},
   {"maximum",      PolynomialParamItemMaximum, U_GEN, 0, nullptr, -4.0f, 4.0f, 4.0f, 0},
};
const ModuleParamConstantsGroup polynomialConstantsGroup = {
   const_cast<char*>("Polynomial"), sizeof(polynomialConstants) / sizeof(polynomialConstants[0]), polynomialConstants};

// Scale Offset — out = clamp(Scale*in + Offset, Minimum, Maximum); no clamp when Minimum>Maximum.
static const ModuleParamConstants scaleOffsetConstants[] = {
   {"input",   ScaleOffsetParamItemInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"scale",   ScaleOffsetParamItemScale, U_GEN, 0, nullptr, -2.0f, 2.0f, 1.0f, 0},
   {"offset",  ScaleOffsetParamItemOffset, U_GEN, 0, nullptr, -4.0f, 4.0f, 0.0f, 0},
   {"minimum", ScaleOffsetParamItemMinimum, U_GEN, 0, nullptr, -4.0f, 4.0f, -1.0f, 0},
   {"maximum", ScaleOffsetParamItemMaximum, U_GEN, 0, nullptr, -4.0f, 4.0f, 1.0f, 0},
};
const ModuleParamConstantsGroup scaleOffsetConstantsGroup = {
   const_cast<char*>("Scale Offset"), sizeof(scaleOffsetConstants) / sizeof(scaleOffsetConstants[0]), scaleOffsetConstants};

// Split Level — independent Level+Offset per channel of one stereo input.
static const ModuleParamConstants splitLevelConstants[] = {
   {"input",       SplitLevelParamItemInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"levelLeft",   SplitLevelParamItemLevelLeft, U_GEN, 0, nullptr, -4.0f, 4.0f, 1.0f, 0},
   {"levelRight",  SplitLevelParamItemLevelRight, U_GEN, 0, nullptr, -4.0f, 4.0f, 1.0f, 0},
   {"offsetLeft",  SplitLevelParamItemOffsetLeft, U_GEN, 0, nullptr, -4.0f, 4.0f, 0.0f, 0},
   {"offsetRight", SplitLevelParamItemOffsetRight, U_GEN, 0, nullptr, -4.0f, 4.0f, 0.0f, 0},
};
const ModuleParamConstantsGroup splitLevelConstantsGroup = {
   const_cast<char*>("Split Level"), sizeof(splitLevelConstants) / sizeof(splitLevelConstants[0]), splitLevelConstants};

// Sample Delay — independent exact-integer-sample delay (0..1s) per channel.
static const ModuleParamConstants sampleDelayConstants[] = {
   {"input",      SampleDelayParamItemInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"delayLeft",  SampleDelayParamItemDelayLeft, U_SEC, 0, nullptr, 0.0f, 1.0f, 0.01f, 0},
   {"delayRight", SampleDelayParamItemDelayRight, U_SEC, 0, nullptr, 0.0f, 1.0f, 0.01f, 0},
};
const ModuleParamConstantsGroup sampleDelayConstantsGroup = {
   const_cast<char*>("Sample Delay"), sizeof(sampleDelayConstants) / sizeof(sampleDelayConstants[0]), sampleDelayConstants};

// Unsplit — picks Left/Right/Sum independently per output channel from a Left/Right input pair.
static const ModuleParamConstants unsplitConstants[] = {
   {"inputLeft",   UnsplitParamItemInputLeft, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"inputRight",  UnsplitParamItemInputRight, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"chooseLeft",  UnsplitParamItemChooseLeft, U_IDX, 0, kChannelChoice, (float)ChannelChoiceLeft,
    (float)ChannelChoiceSum, (float)ChannelChoiceLeft, 0},
   {"chooseRight", UnsplitParamItemChooseRight, U_IDX, 0, kChannelChoice, (float)ChannelChoiceLeft,
    (float)ChannelChoiceSum, (float)ChannelChoiceRight, 0},
};
const ModuleParamConstantsGroup unsplitConstantsGroup = {
   const_cast<char*>("Unsplit"), sizeof(unsplitConstants) / sizeof(unsplitConstants[0]), unsplitConstants};

// Sample And Hold — latches Input at each Trigger rising edge, per channel.
static const ModuleParamConstants sampleAndHoldConstants[] = {
   {"input",   SampleAndHoldParamItemInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"trigger", SampleAndHoldParamItemTrigger, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
};
const ModuleParamConstantsGroup sampleAndHoldConstantsGroup = {
   const_cast<char*>("Sample And Hold"), sizeof(sampleAndHoldConstants) / sizeof(sampleAndHoldConstants[0]), sampleAndHoldConstants};

// Threshold Impulse — fires when Input crosses Threshold (+ CV), in the selected direction.
static const ModuleParamConstants thresholdImpulseConstants[] = {
   {"input",         ThresholdImpulseParamItemInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
   {"trigger",       ThresholdImpulseParamItemTrigger, U_IDX, 0, kImpulseTrigger, (float)ImpulseTriggerRising,
    (float)ImpulseTriggerBoth, (float)ImpulseTriggerRising, 0},
   {"threshold",     ThresholdImpulseParamItemThreshold, U_GEN, 0, nullptr, -0.99f, 0.99f, 0.0f, 0},
   {"deadband",      ThresholdImpulseParamItemDeadband, U_GEN, 0, nullptr, 0.001f, 0.1f, 0.05f, 0},
   {"thresholdInput",ThresholdImpulseParamItemThresholdInput, U_GEN, 0, nullptr, 0.0f, 0.0f, 0.0f, ParamMetaDataInput}, // connection
};
const ModuleParamConstantsGroup thresholdImpulseConstantsGroup = {
   const_cast<char*>("Threshold Impulse"), sizeof(thresholdImpulseConstants) / sizeof(thresholdImpulseConstants[0]), thresholdImpulseConstants};

// Global Delay — post-final stereo delay/echo, NOT a graph node (see pm_addresses.h's
// globalDelay1Address / GlobalModuleManager.hpp). Values copied from PolygraphModular's own
// globalDelayConstants (ParamGroupData.m).
static const ModuleParamConstants globalDelayConstants[] = {
   {"bypass",        GlobalDelayParamItemBypass,       U_BOOL, 0, nullptr, 0.0f, 1.0f, 0.0f, 0},
   {"secondsLeft",   GlobalDelayParamItemSecondsLeft,  U_SEC,  0, nullptr, 0.0f, 5.0f, 0.2f, 0},
   {"secondsRight",  GlobalDelayParamItemSecondsRight, U_SEC,  0, nullptr, 0.0f, 5.0f, 0.7f, 0},
   {"levelLeft",     GlobalDelayParamItemLevelLeft,    U_GAIN, 0, nullptr, 0.0f, 0.99f, 0.3f, 0},
   {"levelRight",    GlobalDelayParamItemLevelRight,   U_GAIN, 0, nullptr, 0.0f, 0.99f, 0.3f, 0},
   {"levelDry",      GlobalDelayParamItemLevelDry,     U_GAIN, 0, nullptr, 0.0f, 1.0f, 0.8f, 0},
   {"crossDelay",    GlobalDelayParamItemCrossDelay,   U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"crossFeedback", GlobalDelayParamItemCrossFeedback,U_BOOL, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"feedbackLevel", GlobalDelayParamItemFeedbackLevel,U_GAIN, 0, nullptr, 0.0f, 0.99f, 0.4f, 0},
};
// "Delay" (not "Global Delay") — this name also becomes the GlobalPanel card title verbatim
// (pm_global_snapshot_json()), matching PM's own GlobalView.swift card label exactly.
const ModuleParamConstantsGroup globalDelayConstantsGroup = {
   const_cast<char*>("Delay"), sizeof(globalDelayConstants) / sizeof(globalDelayConstants[0]), globalDelayConstants};

// Global Reverb — post-final Freeverb-style reverb, NOT a graph node (see pm_addresses.h's
// globalReverb1Address / GlobalModuleManager.hpp). Values copied from PolygraphModular's own
// globalReverbConstants (ParamGroupData.m). "Reverb" (not "Global Reverb") — this name also
// becomes the GlobalPanel card title verbatim, matching PM's own GlobalView.swift card label.
static const ModuleParamConstants globalReverbConstants[] = {
   {"bypass",       GlobalReverbParamItemBypass,       U_BOOL,  0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"mix",          GlobalReverbParamItemMix,          U_FADER, 0, nullptr, 0.0f, 1.0f, 0.5f, 0},
   {"predelayMS",   GlobalReverbParamItemPreDelayMS,   U_MS,    0, nullptr, 0.0f, 80.0f, 40.0f, 0},
   {"decaySeconds", GlobalReverbParamItemDecaySeconds, U_SEC,   0, nullptr, 0.2f, 12.0f, 4.0f, 0},
   {"damping",      GlobalReverbParamItemDamping,      U_CUST,  0, nullptr, 0.0f, 1.0f, 0.4f, 0},
   {"width",        GlobalReverbParamItemWidth,        U_CUST,  0, nullptr, 0.0f, 1.0f, 0.7f, 0},
};
const ModuleParamConstantsGroup globalReverbConstantsGroup = {
   const_cast<char*>("Reverb"), sizeof(globalReverbConstants) / sizeof(globalReverbConstants[0]), globalReverbConstants};

// Global Enhance — post-final "Enigma"-style enhancer, NOT a graph node (see pm_addresses.h's
// globalEnhance1Address / GlobalModuleManager.hpp). Values copied from PolygraphModular's own
// globalEnahanceConstants (ParamGroupData.m; note the source array itself has a typo in its C
// name, "globalEnahanceConstants" — not reproduced here). "Enhance" (not "Global Enhance") — this
// name also becomes the GlobalPanel card title verbatim, same convention as "Delay"/"Reverb".
// apLRPhase/delayLRPhase reuse kLfoPhase (defined above for LFOParamItemPhase) — PM's own comment
// on GlobalEnhanceParamItemApLRPhase confirms it's the same LFOParamLRPhase index encoding.
static const ModuleParamConstants globalEnhanceConstants[] = {
   {"bypass",      GlobalEnhanceParamItemBypass,      U_BOOL, 0, nullptr,   0.0f, 1.0f,  1.0f, 0},
   {"rate",        GlobalEnhanceParamItemRate,        U_HZ,   0, nullptr,   0.01f, 5.0f, 0.25f, 0},
   {"depth",       GlobalEnhanceParamItemDepth,       U_CUST, 0, nullptr,   0.0f, 1.0f,  0.35f, 0},
   {"stages",      GlobalEnhanceParamItemStages,      U_IDX,  0, nullptr,   2.0f, 10.0f, 6.0f, 0},
   {"feedback",    GlobalEnhanceParamItemFeedback,    U_CUST, 0, nullptr,   0.0f, 0.85f, 0.15f, 0},
   {"fixedDelay",  GlobalEnhanceParamItemFixedDelay,  U_MS,   0, nullptr,   0.0f, 25.0f, 10.0f, 0},
   {"mix",         GlobalEnhanceParamItemMix,         U_FADER,0, nullptr,   0.0f, 1.0f,  0.35f, 0},
   {"delayMod",    GlobalEnhanceParamItemDelayMod,    U_MS,   0, nullptr,   0.0f, 5.0f,  0.0f, 0},
   {"delayRate",   GlobalEnhanceParamItemDelayRate,   U_HZ,   0, nullptr,   0.01f, 5.0f, 0.25f, 0},
   {"apLRPhase",   GlobalEnhanceParamItemApLRPhase,   U_IDX,  0, kLfoPhase, 0.0f, 4.0f,  2.0f, 0},
   {"delayLRPhase",GlobalEnhanceParamItemDelayLRPhase,U_IDX,  0, kLfoPhase, 0.0f, 4.0f,  2.0f, 0},
};
const ModuleParamConstantsGroup globalEnhanceConstantsGroup = {
   const_cast<char*>("Enhance"), sizeof(globalEnhanceConstants) / sizeof(globalEnhanceConstants[0]), globalEnhanceConstants};

// MIDI/voice configuration — VoiceManager's OWN MIDIParamItem params (voice count, free-run,
// legato, group/channel filter, voice-steal algorithm). NOT a GlobalModule/graph node — see
// pm_midi_snapshot_json()'s own comment in plugin.cpp for why this is a third, separate seam.
// Values/ranges copied from PolygraphModular's own midiConstants (ParamGroupData.m), EXCEPT:
//   - voiceCount's default here is 8, not PM's stale "10" — matches what plug_activate() (K1)
//     actually initializes VoiceManager to at startup; this constants group is UI metadata only
//     (VoiceManager keeps its own flat params[] array, never populated via addParams()), so there
//     was no reason to keep a default this port doesn't actually use anywhere.
//   - the "channel" row correctly uses MIDIParamItemChannelFilterValue. PM's own midiConstants[]
//     has a copy-paste bug here (both "group" and "channel" reference
//     MIDIParamItemGroupFilterValue) — not reproduced.
static const ModuleParamConstants midiConstants[] = {
   {"voiceCount", MIDIParamItemVoiceCount,        U_IDX,  0, nullptr,        1.0f, 32.0f, 8.0f, 0},
   {"group",      MIDIParamItemGroupFilterValue,  U_IDX,  0, kMidiNoneOr16,  0.0f, 16.0f, 0.0f, 0},
   {"channel",    MIDIParamItemChannelFilterValue,U_IDX,  0, kMidiNoneOr16,  0.0f, 16.0f, 0.0f, 0},
   {"voiceSteal", MIDIParamItemVoiceStealAlgorithm,U_IDX, 0, kVoiceSteal,    0.0f, 4.0f,  0.0f, 0},
   {"freeRun",    MIDIParamItemFreeRun,           U_BOOL, 0, nullptr,        0.0f, 1.0f,  0.0f, 0},
   {"legato",     MIDIParamItemLegato,            U_BOOL, 0, nullptr,        0.0f, 1.0f,  0.0f, 0},
};
const ModuleParamConstantsGroup midiConstantsGroup = {
   const_cast<char*>("MIDI"), sizeof(midiConstants) / sizeof(midiConstants[0]), midiConstants};
