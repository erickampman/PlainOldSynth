// pm_params.h — portable replacement for PolygraphModular's ParamGroupData.h.
//
// The original is Objective-C (NSString*/NSArray in the param-constants struct, NS_ENUM
// throughout), so rather than vendor it we provide the same type NAMES the framework expects,
// with plain C++ types (const char*, float, uint32). The per-module parameter-ITEM enums are
// added here as each module is ported. The actual per-module constant DATA (names/ranges) is
// vendored/converted separately, in pm_param_data.cpp.

#ifndef PM_PARAMS_H
#define PM_PARAMS_H

#include <cstddef>
#include <cstdint>

#include "pm_compat.h"

// First param enum value is 1 (0 is "unspecified"); enums double as array indices.
#define PARAM_ENUM_TO_INDEX(e) ((e) - 1)

// Module kinds — the high byte of a param address. Values must stay stable (they are the
// module identity in every param clap_id). Mirrors PolygraphModular's ModuleParamKind.
enum ModuleParamKind : NSUInteger {
   ModuleParamKindUnspecified = 0,
   ModuleParamKindGlobal      = 1,
   ModuleParamKindPitchConverter,
   ModuleParamKindVelocityConverter,
   ModuleParamKindSlew,
   ModuleParamKindSinOsc,
   ModuleParamKindTriSawOsc,
   ModuleParamKindNoise,
   ModuleParamKindADR,
   ModuleParamKindADSR,
   ModuleParamKindLFO, /* 0x0A */
   ModuleParamKindButterworthLowPassFilter,
   ModuleParamKindFreqShift, // (was Chebyshev LPF slot — reused, as in PM; keeps kind value stable)
   ModuleParamKindButterworthBandpassFilter,
   ModuleParamKindMultimodeFilter,
   ModuleParamKindBiquadFilter,
   ModuleParamKindFormantFilter, /* 0x10 */
   ModuleParamKindVariableSlopeFilter,
   ModuleParamKindRezNotchFilter,
   ModuleParamKindSineWarp,
   ModuleParamKindPolynomial,
   ModuleParamKindScaleOffset,
   ModuleParamKindLevel,
   ModuleParamKindComparator,
   ModuleParamKindThresholdImpulse,
   ModuleParamKindDeltaImpulse,
   ModuleParamKindOneShot,
   ModuleParamKindSampleAndHold,
   ModuleParamKindSequencer,
   ModuleParamKindDiscretizer,
   ModuleParamKindEDODiscretizer,
   ModuleParamKindCombine,
   ModuleParamKindQuadCombine, /* 0x20 */
   ModuleParamKindQuadStereoMixer,
   ModuleParamKindXFade,
   ModuleParamKindPitchBendConverter,
   ModuleParamKindControlChangeConverter,
   ModuleParamKindChannelPressure,
   ModuleParamKindNoteGate,
   ModuleParamKindLogicGate,
   ModuleParamKindClockDivider,
   ModuleParamKindSampleDelay,
   ModuleParamKindFinalOutput,
   ModuleParamKindTunnelInput,
   ModuleParamKindTunnelOutput,
   ModuleParamKindChorus,
   ModuleParamKindFlanger,
   ModuleParamKindPhaser,
   ModuleParamKindDelay, /* 0x30 */
   ModuleParamKindGlobalDelay,
   ModuleParamKindGlobalReverb,
   ModuleParamKindGlobalEnhance,
   ModuleParamKindMIDI,
   ModuleParamKindNode,
   ModuleParamKindValue,
   ModuleParamKindOverdrive,
   ModuleParamKindSplitLevel,
   ModuleParamKindMIDINoteNumberConverter,
   ModuleParamKindUnsplit,
   ModuleParamKindPan,
   ModuleParamKindIFSOsc,
   ModuleParamKindIFSEnv,
   ModuleParamKindIFSLFO,
   ModuleParamKindIFSConv,
   ModuleParamKindIFSRes,
   ModuleParamKindIFSGran,
   ModuleParamKindBufferGran, // (was MandOsc slot — reused, as in PM; keeps kind value stable)
   ModuleParamKindSpectralGran, // (was JuliaGran slot — reused, as in PM; keeps kind value stable)
   ModuleParamKindClockGran,
   ModuleParamKindCVMixer, // IFSynth: combine mod sources (add/multiply, per-input enable)
   ModuleParamKindStrike,    // StruckSynth: note/gate → shaped excitation burst
   ModuleParamKindModalBank, // StruckSynth: bank of damped modal resonators
   ModuleParamKindDoubleRezBandpass, // doubly-resonant bandpass (resonant HP × resonant LP) — appended (keeps existing kind values stable)
   ModuleParamKindFM4Op, // DX7-style 4-operator phase-modulation voice — appended (keeps existing kind values stable)
   ModuleParamKindDrumKit, // note->drum kit voice with cross-voice hi-hat choke — appended (keeps existing kind values stable)
   ModuleParamKindSharedLFO, // LFO with one phase shared across all voices (phase-locked) — appended (keeps existing kind values stable)
   ModuleParamKindSelector, // N-way CV-driven crossfade/router (keyboard/velocity/CV split) — appended (keeps existing kind values stable)
   ModuleParamKindClock,    // free-running clock/gate generator (BPM × musical division) — appended (keeps existing kind values stable)
   // Sentinel, not a real kind of its own — an ALIAS for whichever real entry above is currently
   // highest (keep it pointing at the actual last one whenever a new kind is appended). Lets
   // address-sanity checks (see pm_addr_sanity.h) bound a decoded kind byte against "the largest
   // ModuleParamKind" without hardcoding that number a second place to drift out of sync (the
   // user's own call, 2026-09-06, while reviewing loaded-patch address validation).
   ModuleParamKindLast = ModuleParamKindClock,
};

// Module instance — the middle byte of a param address (1..24).
enum ModuleInstance : NSUInteger {
   ModuleInstance1 = 1,
   ModuleInstance2, ModuleInstance3, ModuleInstance4, ModuleInstance5, ModuleInstance6,
   ModuleInstance7, ModuleInstance8, ModuleInstance9, ModuleInstance10, ModuleInstance11,
   ModuleInstance12, ModuleInstance13, ModuleInstance14, ModuleInstance15, ModuleInstance16,
   ModuleInstance17, ModuleInstance18, ModuleInstance19, ModuleInstance20, ModuleInstance21,
   ModuleInstance22, ModuleInstance23, ModuleInstance24,
};

enum ModuleItem : NSUInteger {
   ModuleItemUnspecified = 0,
};

// --- per-module parameter-item enums (added as each module is ported) --------------------

enum OscSyncType : NSUInteger {
   OscSyncTypeDown = 0,
   OscSyncTypeUp,
   OscSyncTypeBoth,
};

enum SinOscParamItem : NSUInteger {
   SinOscParamItemPitch = 1,
   SinOscParamItemPitchFine,
   SinOscParamItemPitchInput,
   SinOscParamItemModPitch,
   SinOscParamItemModPitch2,
   SinOscParamItemModPhase,
   SinOscParamItemFreqModLevel,
   SinOscParamItemPhaseModLevel,
   SinOscParamItemFreqModLevel2,
   SinOscParamItemModSync,
   SinOscParamItemModSyncType,
   SinOscParamItemDetune,   // stereo detune, cents (splits L/R by ±half); 0 = mono
   SinOscParamItemWaveform, // RESERVED — was a Sine/Tri/Saw selector; removed to match PM (SinOsc is
                            // sine-only; tri/saw belong to TriSawOsc). Kept so the item numbers below
                            // (and any saved state keyed by them) don't renumber. Do not reuse.
   // Self-modulation (phase feedback, DX7-style). APPENDED so existing item numbers — and any saved
   // state keyed by them — stay valid; a patch that lacks these keys loads with 0 (no self-mod).
   SinOscParamItemSelfModInput,      // connection: external self-mod depth signal
   SinOscParamItemSelfModLevel,      // base phase-feedback depth (−1.2..1.2)
   SinOscParamItemSelfModInputLevel, // depth applied to SelfModInput (−2..2)
};

// TriSawOsc — variable tri↔saw oscillator (the "peak" control morphs the apex position). Item order
// mirrors PM's TriSawParamItem. Has hard sync + DX7-style phase self-modulation (like SinOsc).
enum TriSawParamItem : NSUInteger {
   TriSawParamItemPitchInput = 1,     // connection: pitch (1/octave)
   TriSawParamItemPitch,              // coarse tune (semitones)
   TriSawParamItemPitchFine,          // fine tune (cents)
   TriSawParamItemModPitch,           // connection: exponential FM 1
   TriSawParamItemModPitch2,          // connection: exponential FM 2
   TriSawParamItemModPhase,           // connection: phase mod
   TriSawParamItemPeak,               // apex position 0.01..0.99 (0.5 = symmetric triangle; →0/→1 = saw)
   TriSawParamItemModPeak,            // connection: peak mod
   TriSawParamItemFreqModLevel,       // depth for ModPitch
   TriSawParamItemPhaseModLevel,      // depth for ModPhase
   TriSawParamItemPeakModLevel,       // depth for ModPeak
   TriSawParamItemFreqModLevel2,      // depth for ModPitch2
   TriSawParamItemModSync,            // connection: hard-sync source
   TriSawParamItemModSyncType,        // OscSyncType (Down/Up/Both)
   TriSawParamItemSelfModInput,       // connection: external self-mod depth
   TriSawParamItemSelfModLevel,       // base phase-feedback depth (−0.9..0.9)
   TriSawParamItemSelfModInputLevel,  // depth applied to SelfModInput (−2..2)
};

// LFO — low-frequency control oscillator: Sine/Triangle/Square/Impulse, exponential rate (0 = 1 Hz),
// stereo L/R phase offset, and a reset-trigger input. Bipolar (−1..1) except Impulse (0/1).
enum LFOParamItem : NSUInteger {
   LFOParamItemPitch = 1,     // rate, octaves; 0 = 1 Hz
   LFOParamItemPitchInput,    // connection: exponential rate FM
   LFOParamItemWave,          // LFOParamWave selector
   LFOParamItemPhase,         // LFOParamLRPhase: L/R stereo phase offset
   LFOParamItemResetInput,    // connection: rising edge resets phase to 0
   LFOParamItemClockInput,    // connection: clock pulses — when patched, locks rate+phase to the clock
   LFOParamItemClockRatio,    // indexed (kLFOClockRatios): Auto / 1/16..3/4 / ×1..×16 of the clock rate
};

// Clock — free-running gate/pulse generator: emits a 0/1 pulse train at BPM × a musical Division.
// Reset (rising edge) restarts the cycle; Run (gate) holds the clock low when low (unpatched = always
// run). Width is the fraction of each period the pulse is high. Drives Sequencer/ClockDivider/S&H
// triggers and LFO reset inputs. (Host-tempo sync is a later addition — this runs on its own BPM.)
enum ClockParamItem : NSUInteger {
   ClockParamItemResetInput = 1,  // connection: rising edge restarts the cycle
   ClockParamItemRunInput,        // connection: gate — high runs, low holds low (unpatched = run)
   ClockParamItemSync,            // indexed: 0=Internal (BPM knob), 1=DAW (host tempo). Default DAW.
   ClockParamItemBPM,             // 20..300 beats/min (default 120); used when Sync=Internal or no host tempo
   ClockParamItemDivision,        // indexed: note value per pulse (kClockDivision: 1/1..1/32)
   ClockParamItemWidth,           // 0.05..0.95: fraction of the period the pulse is high
};

enum LFOParamWave : NSUInteger {
   LFOParamWaveSine = 0,
   LFOParamWaveTriangle,
   LFOParamWaveSquare,
   LFOParamWaveImpulse,
};
enum LFOParamLRPhase : NSUInteger {   // L/R phase offset choices (index 0..4)
   LFOParamLRPhase0 = 0,
   LFOParamLRPhase60,
   LFOParamLRPhase90,
   LFOParamLRPhase120,
   LFOParamLRPhase180,
};

// Level — a simple gain stage: output = input × level. Used to scale the sine osc's audible mix
// level *after* its raw output has been tapped for the IFS hard-sync input.
enum LevelParamItem : NSUInteger {
   LevelParamItemInput = 1,     // connection
   LevelParamItemLevel,         // attenuation, -4..4 (bipolar, so it also inverts)
   LevelParamItemInvertLeft,    // bool: negate the left channel
   LevelParamItemInvertRight,   // bool: negate the right channel
};

// ADR — Delay-Attack-Decay-Release envelope (optional Sustain hold; exp/linear).
enum ADRParamItem : NSUInteger {
   ADRParamItemDelay = 1,
   ADRParamItemAttack,
   ADRParamItemDecay,
   ADRParamItemRelease,
   ADRParamItemSustains,   // Boolean: hold at peak while gated (vs one-shot)
   ADRParamItemGateInput,  // connection
   ADRParamItemLegato,     // 0=Steal, 1=LegatoRetrigger, 2=LegatoTrue
   ADRParamItemLinear,     // Boolean: 0=exponential, 1=linear
};

// SineWarp — sine waveshaper: out = sin(π/2·scale·in + π·shift), scale/shift CV-modulatable.
enum SineWarpParamItem : NSUInteger {
   SineWarpParamItemInput = 1,    // connection
   SineWarpParamItemScale,        // 1..8
   SineWarpParamItemShift,        // -1..1
   SineWarpParamItemModScale,     // connection
   SineWarpParamItemModShift,     // connection
   SineWarpParamItemModScaleLevel,
   SineWarpParamItemModShiftLevel,
   SineWarpParamItemOversample,   // anti-alias quality: 0 = Off, 1 = 2×, 2 = 4× (CPU cost only when on)
};

// Comparator — Schmitt trigger with hysteresis, CV-able threshold/clip.
enum ComparatorType : NSUInteger {
   ComparatorTypeFullClip = 0,
   ComparatorTypeTop,
   ComparatorTypeBottom,
};
enum ComparatorParamItem : NSUInteger {
   ComparatorParamItemInput = 1,  // connection
   ComparatorParamItemType,       // Indexed: Full Clip / Top / Bottom
   ComparatorParamItemThreshold,
   ComparatorParamItemThresholdInput,        // connection
   ComparatorParamItemThresholdInputLevel,
   ComparatorParamItemHysteresis,
   ComparatorParamItemClipLevel,
   ComparatorParamItemClipLevelInput,        // connection
   ComparatorParamItemClipLevelInputLevel,
   ComparatorParamItemBandlimit,             // Boolean: polyBLEP band-limit the crossings (on for
                                             // audio pulse use; off for exact bilevel stepped CV)
};

// Multimode filter — switchable biquad. ParamItemFilterType is the mode selector.
enum ParamItemFilterType : NSUInteger {
   ParamItemFilterType_Passthrough = 0, ParamItemFilterType_Lowpass,
   ParamItemFilterType_Highpass, ParamItemFilterType_Bandpass,
};
enum MultimodeFilterParamItem : NSUInteger {
   MultimodeFilterParamItemInput = 1,      // connection
   MultimodeFilterParamItemType,           // Indexed: Passthrough / LP / HP / BP
   MultimodeFilterParamItemPitch,          // octaves
   MultimodeFilterParamItemFreqModLevel,
   MultimodeFilterParamItemQ,
   MultimodeFilterParamItemModPitchInput,  // connection
   MultimodeFilterParamItemModPitchInput2, // connection
   MultimodeFilterParamItemFreqModLevel2,
};

// Formant — 2D vowel-morph filter. Gain is an indexed output boost (dB), applied in series mode only.
enum FormantFilterGain : NSUInteger {
   FormantFilterGain0 = 0, FormantFilterGain6, FormantFilterGain12, FormantFilterGain18,
   FormantFilterGain24, FormantFilterGain32, FormantFilterGain36, FormantFilterGain40,
   FormantFilterGain44, FormantFilterGain48,
};
enum FormantFilterParamItem : NSUInteger {
   FormantFilterParamItemInput = 1,   // connection
   FormantFilterParamItemMorphX,      // 0..1
   FormantFilterParamItemMorphY,      // 0..1
   FormantFilterParamItemXInput,      // connection
   FormantFilterParamItemYInput,      // connection
   FormantFilterParamItemParallel,    // Boolean: parallel vs series stages
   FormantFilterParamItemGain,        // Indexed: FormantFilterGain
   FormantFilterParamItemFreqModInput, // connection
   FormantFilterParamItemFreqModLevel,
};

// VariableSlope — tilt filter (±6 dB/oct around a pivot) + DC/rumble low cut. CV over pivot/slope.
enum VariableSlopeParamItem : NSUInteger {
   VariableSlopeParamItemInput = 1,   // connection
   VariableSlopeParamItemPivotPitch,      // octaves
   VariableSlopeParamItemSlope,           // dB/oct, -6..6
   VariableSlopeParamItemLowCutFrequency, // Hz
   VariableSlopeParamItemPivotInputLevel,
   VariableSlopeParamItemSlopeInputLevel,
   VariableSlopeParamItemPivotModInput,   // connection
   VariableSlopeParamItemSlopeModInput,   // connection
};

// Butterworth bandpass — center pitch + bandwidth + Q, with pitch/bandwidth CV.
enum BPParamItem : NSUInteger {
   BPParamItemInput = 1,                // connection
   BPParamItemPitch,                    // octaves (A440 = 0)
   BPParamItemFreqModLevel,             // -5..5
   BPParamItemBandwidth,                // -1..1
   BPParamItemQ,                        // 1..10
   BPParamItemModPitchInput,            // connection
   BPParamItemModBandwidthInput,        // connection
   BPParamItemBandwidthModLevel,        // 0..1
};

// Biquad — RBJ biquad: Passthrough / LP / HP / BP (ParamItemFilterType), two FM inputs + Q.
enum BiquadFilterParamItem : NSUInteger {
   BiquadFilterParamItemInput = 1,      // connection
   BiquadFilterParamItemType,           // ParamItemFilterType (indexed)
   BiquadFilterParamItemPitch,          // octaves
   BiquadFilterParamItemQ,              // 0.1..10
   BiquadFilterParamItemModPitchInput,  // connection
   BiquadFilterParamItemFreqModLevel,   // -5..5
   BiquadFilterParamItemModPitchInput2, // connection (second FM)
   BiquadFilterParamItemFreqModLevel2,  // -5..5
};

// Doubly-resonant bandpass — resonant HP × resonant LP; centre + bandwidth (separation) + edge Q,
// each with control-rate mod; Slope 12/24 dB/oct, QLimit taper, dry/wet Mix. See DoubleRezBandpass.md.
enum DoubleRezBandpassParamItem : NSUInteger {
   DoubleRezBandpassParamItemInput = 1,          // audio in (connection)
   DoubleRezBandpassParamItemCenterModInput,     // centre-frequency FM in (connection)
   DoubleRezBandpassParamItemBandwidthModInput,  // bandwidth mod in (connection)
   DoubleRezBandpassParamItemQModInput,          // Q mod in (connection)
   DoubleRezBandpassParamItemCenter,             // octaves re A440
   DoubleRezBandpassParamItemCenterModLevel,     // scales CenterModInput
   DoubleRezBandpassParamItemBandwidth,          // octaves; SEPARATION of corners
   DoubleRezBandpassParamItemBandwidthModLevel,
   DoubleRezBandpassParamItemQ,                  // resonance of each edge
   DoubleRezBandpassParamItemQModLevel,
   DoubleRezBandpassParamItemSlope,              // indexed: 0 = 12 dB/oct, 1 = 24
   DoubleRezBandpassParamItemMix,                // 0 = dry, 1 = filtered
   DoubleRezBandpassParamItemQLimit,             // 0..1; taper Q as the edges converge
};

// RezNotch — 2nd-order pole/zero resonator/notch, 4 modes, equal-power dry/wet mix.
enum RezNotchMode : NSUInteger {
   // Rez was dropped from the picker (2026-09-16): it folded into Bump (a resonant boost is a
   // high-Q bell). AllPass moves 3->2. The one factory preset that used Rez (Synthish/Fat Octaves)
   // was migrated to Bump.
   RezNotchModeBump = 0, RezNotchModeNotch, RezNotchModeAllPass,
};
enum RezNotchParamItem : NSUInteger {
   RezNotchParamItemInput = 1,        // connection: audio in
   RezNotchParamItemModPitchInput,    // connection: pitch FM
   RezNotchParamItemPitch,            // octaves
   RezNotchParamItemPitchModLevel,
   RezNotchParamItemR,                // outer radius 0.8..0.999
   RezNotchParamItemSep,              // pole/zero separation 0.001..0.1
   RezNotchParamItemRz,               // Rez mode: zero radius 0..1
   RezNotchParamItemRp,               // Rez mode: pole radius 0..1
   RezNotchParamItemMode,             // Indexed: Bump / Notch / Rez / All Pass
   RezNotchParamItemMix,              // 0..1 dry→wet
   RezNotchParamItemPolarity,         // Boolean: add / subtract
   // Parametric reparametrization (prototype, PMU-first): Notch, Bump and Rez are all RBJ peaking
   // biquads driven by Frequency (= Pitch) + Q + a signed gain — replacing the raw Bump-core
   // outerRadius/separation/zeroRadius/poleRadius (now vestigial; only All Pass still uses R).
   RezNotchParamItemQ,                // shared width for Notch/Bump/Rez (independent of gain)
   RezNotchParamItemNotchDepth,       // Notch mode: cut depth in dB (0 = flat, more negative = deeper)
   RezNotchParamItemBumpGain,         // Bump/Rez mode: boost in dB (0 = flat, higher = taller peak)
};

// Tunnel In / Out — cross-plane signal routing (K4d). A signal fed into a Tunnel In on one plane
// re-emerges from every Tunnel Out sharing its Channel on any plane. The engine graph is flat, so
// a tunnel is a channel-matched module pairing: the plugin connects each Tunnel In to the Tunnel
// Outs on its channel; both modules are identity pass-throughs (output = input at item 1).
enum TunnelInParamItem : NSUInteger {
   TunnelInParamItemInput = 1,   // connection: the signal to send through the tunnel (visible port)
   TunnelInParamItemBus,         // 1..8 (shown A..H) — the tunnel bus this In feeds
};
enum TunnelOutParamItem : NSUInteger {
   TunnelOutParamItemSource = 1, // HIDDEN internal slot (no port): wired to the matching Tunnel In
   TunnelOutParamItemBus,        // 1..8 (shown A..H) — the tunnel bus this Out receives
};

// IFS fractal oscillator — trimmed v1 set (PM's modulation matrix is deferred). PitchInput is a
// connection; the five fractal-shape items + smoothing + iterations drive the wavetable.
enum IFSOscParamItem : NSUInteger {
   IFSOscParamItemPitchInput = 1, // connection: pitch from the voice chain
   IFSOscParamItemPitch,          // coarse tune (semitones)
   IFSOscParamItemPitchFine,      // fine tune (cents)
   IFSOscParamItemIFS1Squash,     // left-region self-similar vertical scale
   IFSOscParamItemIFS2Squash,     // right-region self-similar vertical scale
   IFSOscParamItemBx,             // split point (x)
   IFSOscParamItemBy,             // junction height (y)
   IFSOscParamItemStartEndY,      // shared endpoint level
   IFSOscParamItemSmoothing,      // 0 = Catmull-Rom, 1 = piecewise-linear
   IFSOscParamItemIterations,     // picker index: 0 → 3 iters, 1 → 4 iters
   // Modulation inputs (connection ports) + their bipolar depth levels. Pitch mod is applied
   // per-sample (wavetable-compatible); the shape mods switch IFSOsc to the chunked eval path.
   IFSOscParamItemPitchModInput,
   IFSOscParamItemPitchModLevel,
   IFSOscParamItemBxModInput,
   IFSOscParamItemBxModLevel,
   IFSOscParamItemByModInput,
   IFSOscParamItemByModLevel,
   IFSOscParamItemSquash1ModInput,
   IFSOscParamItemSquash1ModLevel,
   IFSOscParamItemSquash2ModInput,
   IFSOscParamItemSquash2ModLevel,
   IFSOscParamItemDetune,         // stereo detune, cents (splits L/R by ±half); 0 = mono
   IFSOscParamItemSyncInput,      // connection: hard-sync source (stereo, from the sine osc)
   IFSOscParamItemSyncMode,       // 0 Off, 1 Up, 2 Down, 3 Both (zero-crossing of the sync input)
   IFSOscParamItemLevel,          // output mix level
};

// IFS fractal envelope — 3-transform shape (blue + green interior points), phase over Duration.
enum IFSEnvParamItem : NSUInteger {
   IFSEnvParamItemGateInput = 1, // connection (unused in v1 voice; MIDI drives the gate)
   IFSEnvParamItemDuration,      // full-traversal time, seconds
   IFSEnvParamItemStartEndY,     // level at both ends
   IFSEnvParamItemBlueX,         // first interior point x [0,1]
   IFSEnvParamItemBlueY,         // first interior point y [-1,1]
   IFSEnvParamItemGreenX,        // second interior point x [0,1]
   IFSEnvParamItemGreenY,        // second interior point y [-1,1]
   IFSEnvParamItemIFS1Squash,
   IFSEnvParamItemIFS2Squash,
   IFSEnvParamItemIFS3Squash,
   IFSEnvParamItemSmoothing,
   IFSEnvParamItemIterations,    // picker index: 0 → 1 iteration
   IFSEnvParamItemHoldMode,      // 0 None, 1 Blue, 2 Green, 3 Loop
   IFSEnvParamItemLegatoMode,    // 0 NoteSteal, 1 FindMatchingY
   IFSEnvParamItemScaleRelease,  // 0 Off (run full cycle) / 1 On: an early note-off (before the hold
                                 // point, hold modes 1/2 only) jumps to the hold point and scales the
                                 // release tail to start at the current level — see IFSEnvModule.cpp.
                                 // Appended last to keep every prior item's value stable for old patches.
};

// IFS fractal LFO — free-running mod source, 2-transform shape (same family as IFSOsc). Trimmed
// v1 set (no mod matrix / FM / reset / L-R phase).
enum IFSLFOParamItem : NSUInteger {
   IFSLFOParamItemPitch = 1,    // rate, octaves, -7..7, 0 = 1 Hz
   IFSLFOParamItemBx,
   IFSLFOParamItemBy,
   IFSLFOParamItemIFS1Squash,
   IFSLFOParamItemIFS2Squash,
   IFSLFOParamItemStartEndY,
   IFSLFOParamItemSmoothing,
   IFSLFOParamItemIterations,   // picker index: 0 → 1 iteration
};

// CV mixer — combines two patchable CV inputs and TWO virtual MIDI-controller inputs into one
// output. Each input has a LEVEL (a depth control, not a plain gain): the contribution
// interpolates from the mode's IDENTITY to the full input, so Level 0 = "off" in BOTH modes with
// no discontinuity — Add: contribution = Level·input (identity 0); Multiply: contribution =
// 1 + Level·(input − 1) (identity 1). A detached physical input is treated as Level 0 (skipped,
// not a 0 that would collapse a multiply). Each virtual input's SOURCE selects CC / channel
// Aftertouch / Pitch Bend / Pitch / Velocity; its CC Number only matters when Source = CC. Velocity
// and Pitch latch at note-on and hold through the release (note-offs ignored). Pitch is bipolar,
// per-octave, centered on Reference Note: it emits (voicePitch − refNote) in octaves (0 at the
// reference, +1 per octave up), so a held note adds no constant offset in Add mode — the Level sets
// the key-track strength. Each virtual input has its OWN Reference Note (only used when that input's
// Source = Pitch). Mode is global. Smoothing is a one-pole time constant that de-zippers the
// per-input Level changes. IFSynth-specific (not from PolygraphModular).
enum CVMixerParamItem : NSUInteger {
   CVMixerParamItemInput1 = 1,     // connection port — In 1
   CVMixerParamItemInput2,         // connection port — In 2
   CVMixerParamItemInput1Level,    // -1..1 depth for In 1 (0 = off; negative inverts; default 0)
   CVMixerParamItemInput2Level,    // -1..1 depth for In 2 (0 = off; negative inverts; default 0)
   CVMixerParamItemMIDISourceA,    // indexed: 0=CC, 1=Aftertouch, 2=Pitch Bend, 3=Pitch, 4=Velocity
   CVMixerParamItemMIDILevelA,     // -1..1 depth for virtual input A (0 = off; negative inverts; default 0)
   CVMixerParamItemCCNumberA,       // 1..127 (only when Source A = CC; default 1 = mod wheel)
   CVMixerParamItemReferenceNoteA,  // MIDI note; pivot for input A's Pitch source (default 60)
   CVMixerParamItemMIDISourceB,     // indexed: same choices as Source A
   CVMixerParamItemMIDILevelB,      // -1..1 depth for virtual input B (0 = off; negative inverts; default 0)
   CVMixerParamItemCCNumberB,       // 1..127 (only when Source B = CC; default 1 = mod wheel)
   CVMixerParamItemReferenceNoteB,  // MIDI note; pivot for input B's Pitch source (default 60)
   CVMixerParamItemMode,            // 0 = add, 1 = multiply
   CVMixerParamItemSmoothing,       // per-input Level smoothing, seconds (one-pole)
   // Appended (keep item numbers stable). Per-MIDI-input L/R Skew: the virtual sources are mono, so
   // Skew is the only way to make them stereo. It PANS the source between channels, scaled by the
   // source's own value: value_L = v·(1+Skew), value_R = v·(1−Skew) — so a source value of 0 gives
   // no L/R difference (Skew does nothing until the source moves). Physical inputs have no Skew —
   // their stereo comes from the patched signal's own L/R.
   CVMixerParamItemSkewA,           // -1..1 L/R skew for virtual input A (default 0)
   CVMixerParamItemSkewB,           // -1..1 L/R skew for virtual input B (default 0)
};

// 24 dB/oct Butterworth low-pass. Item 1 is the audio input; Pitch is the (absolute) cutoff in
// octaves through the shared PitchConverter (0 = base A440); Q adds resonance. The Freq/Q mod
// inputs + their levels exist for a future LPF-mod panel (dormant until connected).
enum LPFParamItem : NSUInteger {
   LPFParamItemInput = 1,       // audio input
   LPFParamItemPitch,           // cutoff, octaves (0 = 440 Hz)
   LPFParamItemFreqModLevel,    // first freq mod depth
   LPFParamItemQ,               // resonance, 1..20
   LPFParamItemModFreqInput,    // first freq modulation input (connection)
   LPFParamItemModFreqInput2,   // second freq modulation input (connection)
   LPFParamItemFreqModLevel2,   // second freq mod depth
   LPFParamItemModQInput,       // q modulation input (connection)
   LPFParamItemModQInputLevel,  // q mod depth
};

enum ADSRParamItem : NSUInteger {
   ADSRParamItemAttack = 1,
   ADSRParamItemDecay,
   ADSRParamItemSustain,
   ADSRParamItemRelease,
   ADSRParamItemGateInput,
   ADSRParamItemLegato, // 0=Steal, 1=LegatoRetrigger, 2=LegatoTrue
   ADSRParamItemLinear, // 0=exponential, 1=linear
};

enum ParamItemCombineType : NSInteger {
   ParamItemCombineType_Add = 0,
   ParamItemCombineType_Multiply,
};

enum CombineParamItem : NSUInteger {
   CombineParamItemInput1 = 1,
   CombineParamItemInput2,
   CombineParamItemInput3,
   CombineParamItemType,
   CombineParamItemLevel,
};

enum FinalOutputParamItem : NSUInteger {
   FinalOutputParamItemInput = 1,
   FinalOutputParamItemSpread,
   FinalOutputParamItemLevel,
   FinalOutputParamItemClipLevel,
   FinalOutputParamItemFilterDC,
};

enum SlewParamItem : NSUInteger {
   SlewParamItemInput = 1,
   SlewParamItemRise,
   SlewParamItemFall,
   SlewParamItemCurve,
};

enum PitchConverterParamItem : NSUInteger {
   PitchConverterParamItemDummy = 1,
};

// VoiceManager's own params (polyphony / filtering / mode).
enum MIDIParamItem : NSUInteger {
   MIDIParamItemVoiceCount = 1,
   MIDIParamItemGroupFilterValue,   // 0 = disabled, 1..16 = group
   MIDIParamItemChannelFilterValue, // 0 = disabled, 1..16 = channel
   MIDIParamItemVoiceStealAlgorithm,
   MIDIParamItemFreeRun,
   MIDIParamItemLegato, // 0=Steal, 1=Legato
};

enum VoiceStealAlgorithm : NSUInteger {
   VoiceStealAlgorithmOldest = 0,
   VoiceStealAlgorithmLowest,
   VoiceStealAlgorithmHighest,
   VoiceStealAlgorithmClosest,
   VoiceStealAlgorithmFurthest,
};

// --- StruckSynth: Strike (exciter) -------------------------------------------------------
// Turns a note/gate event into a shaped excitation burst for a resonator. MIDI note-on drives
// it directly (velocity → strength); the optional Gate input takes over for non-MIDI triggering.
enum StrikeParamItem : NSUInteger {
   StrikeParamItemGateInput = 1,     // connection: optional external trigger (rising edge)
   StrikeParamItemHardnessInput,     // connection: optional hardness modulation
   StrikeParamItemType,              // 0 = Mallet (bump), 1 = Noise burst
   StrikeParamItemHardness,          // 0 = 1-sample impulse (hard/bright) .. 1 = soft/dark
   StrikeParamItemHardnessModLevel,  // scales HardnessInput
   StrikeParamItemVelToLevel,        // 0 = fixed amplitude .. 1 = fully velocity-scaled
   StrikeParamItemVelToHardness,     // harder hit → shorter/brighter contact
   StrikeParamItemLevel,             // output gain
   StrikeParamItemContactMode,       // how contact time is measured (see StrikeContactMode)
};

enum StrikeType : NSUInteger {
   StrikeTypeMallet = 0,
   StrikeTypeNoise,
};

enum StrikeContactMode : NSUInteger {
   StrikeContactNoteRelative = 0,  // contact spans a fixed number of periods of the note
   StrikeContactWallClock,         // fixed contact time in ms (physical mallet)
};

// --- StruckSynth: ModalBank (resonator) --------------------------------------------------
// A bank of independently damped two-pole resonators, one per vibrational mode, excited by an
// input signal. Items 1-5 are connection ports; the rest are object/decay/strike/output
// controls. Core (framework-free): ModalBank.hpp/.cpp, ModalMaterials/Generators.hpp.
enum ModalBankParamItem : NSUInteger {
   ModalBankParamItemInput = 1,      // connection: excitation audio in (L/R)
   ModalBankParamItemPitchInput,     // connection: from Pitch Converter; octaves re A440
   ModalBankParamItemStrikeInput,    // connection: patch velocity here
   ModalBankParamItemDecayModInput,  // connection
   ModalBankParamItemBrightModInput, // connection
   // Object
   ModalBankParamItemPitch,          // transpose offset (octaves), ADDED to input
   ModalBankParamItemPitchModLevel,  // scales PitchInput (1 = unity tracking, 0 = fixed pitch)
   ModalBankParamItemMaterial,       // indexed; see ModalBankMaterial
   ModalBankParamItemModeCount,      // 1..64; clamps to the table's depth
   ModalBankParamItemStretch,        // INDEX into kModalStretchValues (inharmonicity)
   // Decay
   ModalBankParamItemDecayT60,       // seconds, T60 of mode 0
   ModalBankParamItemDecayModLevel,
   ModalBankParamItemDampTilt,       // T60_i *= ratio^(-tilt); 0 metal, ~2 wood
   ModalBankParamItemDampBellAmount, // octaves of T60 at the centre; 0 = off
   ModalBankParamItemDampBellCentre, // octaves above the fundamental
   ModalBankParamItemDampBellWidth,  // Gaussian sigma, in octaves
   // Strike
   ModalBankParamItemStrikePosition, // 0..1; 0.5 cancels even modes
   ModalBankParamItemStrikeModLevel,
   ModalBankParamItemBrightness,     // amplitude tilt across modes
   ModalBankParamItemBrightModLevel,
   // Output
   ModalBankParamItemSpread,         // stereo distribution of modes
   ModalBankParamItemMix,            // 1 = source, <1 = resonator in a path
   ModalBankParamItemLevel,
};

// Material selector for ModalBankParamItemMaterial. Order MUST match kModalMaterials
// (ModalMaterials.hpp) for the static tables, then the procedural generators whose order MUST
// match ModalSeriesID (ModalGenerators.hpp): materialIdx - MODAL_BANK_FIRST_GENERATOR = series.
enum ModalBankMaterial : NSUInteger {
   ModalBankMaterialString = 0,
   ModalBankMaterialBar,
   ModalBankMaterialTunedBar,
   ModalBankMaterialMembrane,
   ModalBankMaterialOpenTube,
   ModalBankMaterialClosedTube,
   ModalBankMaterialTine,
   ModalBankMaterialBell,
   ModalBankMaterialGuitarBody,
   ModalBankMaterialViolinBody,
   ModalBankMaterialSoundboard,
   ModalBankMaterialGenericBody,
   ModalBankMaterialGen0,          // first generator (Harmonic n)
   ModalBankMaterialGenOctave,
   ModalBankMaterialGenFibonacci,
   ModalBankMaterialGenPrime,
   ModalBankMaterialGenTriangular,
   ModalBankMaterialGenTetrahedral,
   ModalBankMaterialGenPartition,
   ModalBankMaterialGenGolden,
   ModalBankMaterialGenSilver,
   ModalBankMaterialGenPiano,
   ModalBankMaterialGenPower,
   ModalBankMaterialGenSqrt,
   ModalBankMaterialGenBox3D,
   ModalBankMaterialGenHarmonicNumber,
   ModalBankMaterialCount,
};

// First generator index; everything at or above this is procedural (see ModalGenerators.hpp).
#define MODAL_BANK_FIRST_GENERATOR ModalBankMaterialGen0

// ClockGran — clock-driven, periodicity-focused granular synthesizer. Order MUST match PM's
// ClockGranParamItem (ParamGroupData.h) — saved state is keyed by base|item address.
enum ClockGranParamItem : NSUInteger {
   ClockGranParamItemGateInput = 1,  // connection
   ClockGranParamItemPitchInput,     // connection: octaves re A440
   ClockGranParamItemClockInput,     // connection: external clock (grain timing derives from it)
   ClockGranParamItemResetInput,     // connection: rising edge zeroes cycle + clock phase
   ClockGranParamItemPitch,          // semitones, -48..48
   ClockGranParamItemPitchFine,      // cents, -100..100
   ClockGranParamItemRate,           // indexed: 0=16th,1=8th,2=Quarter,3=Half,4=Whole
   ClockGranParamItemMeter,          // indexed: 0=Straight,1=5(3+2)..6=9(3+3+3)
   ClockGranParamItemDensity,        // sub-triggers/slot, 1..16
   ClockGranParamItemDuration,       // grain length, ms, 1..500
   ClockGranParamItemSmearAmount,    // time smear window width, 0..1
   ClockGranParamItemSmearPeak,      // time smear triangular peak, -1..1
   ClockGranParamItemPitchScatterAmount, // pitch scatter width, 0..1
   ClockGranParamItemPitchScatterSkew,   // pitch scatter triangular skew, -1..1
   ClockGranParamItemEDOSteps,       // 0=off, N=quantise offset table to N-EDO
   ClockGranParamItemBx,             // grain IFS shape, -1..1
   ClockGranParamItemBy,             // grain IFS shape, -1..1
   ClockGranParamItemIFS1Squash,     // -0.99..0.99
   ClockGranParamItemIFS2Squash,     // -0.99..0.99
   ClockGranParamItemStartEndY,      // -1..1
   ClockGranParamItemGrainEnvelopeSkew,  // -1..1: percussive..toned envelope shape
   ClockGranParamItemPanSpread,      // 0..1
   ClockGranParamItemOutputLevel,    // 0..2
   ClockGranParamItemLooping,        // boolean: 0=one-shot, 1=loop
   ClockGranParamItemGateResetMode,  // indexed: 0=Partial, 1=Full
   // Per-tier tables (5 slots: bar downbeat / group downbeat / beat / off-beat / spare)
   ClockGranParamItemPitchOffset1,
   ClockGranParamItemPitchOffset2,
   ClockGranParamItemPitchOffset3,
   ClockGranParamItemPitchOffset4,
   ClockGranParamItemPitchOffset5,
   ClockGranParamItemChaosAmount1,
   ClockGranParamItemChaosAmount2,
   ClockGranParamItemChaosAmount3,
   ClockGranParamItemChaosAmount4,
   ClockGranParamItemChaosAmount5,
   ClockGranParamItemLevel1,
   ClockGranParamItemLevel2,
   ClockGranParamItemLevel3,
   ClockGranParamItemLevel4,
   ClockGranParamItemLevel5,
   // Mod pairs (Pitch/Density/Duration/Bx)
   ClockGranParamItemPitchModInput,     // connection
   ClockGranParamItemPitchModLevel,     // semitones, -24..24
   ClockGranParamItemDensityModInput,   // connection
   ClockGranParamItemDensityModLevel,
   ClockGranParamItemDurationModInput,  // connection
   ClockGranParamItemDurationModLevel,
   ClockGranParamItemBxModInput,        // connection
   ClockGranParamItemBxModLevel,        // -2..2
   // Mode-ratio discretizer (see ModalDiscretizer.hpp / ModalSynthesis.md)
   ClockGranParamItemQuantiseMaterial,  // indexed: 0=Off, N=material N-1
   ClockGranParamItemCentreMode,        // indexed: which mode grains centre on
   ClockGranParamItemModeScatter,       // indexed: +/- N modes
   ClockGranParamItemModeSkew,          // -1 toward low modes .. +1 high
   ClockGranParamItemDiscretizerStretch, // INDEX into kModalStretchValues; match target ModalBank
   ClockGranParamItemQuantiseMode,      // indexed: 0=Snap, 1=Scatter
};

// BufferGran — granular synth reading grains from a rolling capture of an audio input.
// Order matches PM's ParamGroupData.h (BufferGranParamItem); saved state is keyed by base|item.
enum BufferGranFreeze : NSUInteger {
   BufferGranRolling = 0,   // granulate the live input
   BufferGranFrozen,        // latch + granulate the captured moment
};
enum BufferGranParamItem : NSUInteger {
   BufferGranParamItemInput = 1,          // connection: audio to capture/granulate (Primary)
   BufferGranParamItemGateInput,          // connection: optional gate override (else MIDI note)
   BufferGranParamItemPitchInput,         // connection: optional pitch override, 1/oct (else MIDI note)
   BufferGranParamItemFreezeInput,        // connection: optional freeze override (>0.5 = frozen)
   BufferGranParamItemPositionModInput,   // connection: position modulation
   BufferGranParamItemFreeze,             // indexed: 0=rolling, 1=frozen
   BufferGranParamItemBufferSeconds,      // ring length, s
   BufferGranParamItemPosition,           // read centre, 0 (oldest) .. 1 (newest)
   BufferGranParamItemPositionModLevel,   // scales PositionModInput
   BufferGranParamItemScanRate,           // centre drift: 0 hold, 1 real-time, <0 reverse
   BufferGranParamItemSpray,              // random position spread, 0..1
   BufferGranParamItemPitch,              // transpose, semitones
   BufferGranParamItemPitchFine,          // cents
   BufferGranParamItemPitchScatter,       // per-grain pitch spread, semitones
   BufferGranParamItemDensity,            // grains/sec
   BufferGranParamItemDuration,           // grain length, ms
   BufferGranParamItemPanSpread,          // stereo spread, 0..1
   BufferGranParamItemLevel,              // output gain
   BufferGranParamItemVelToLevel,         // velocity → level, 0..1
   // Onset regularity + shared pitch discretizer (ModalDiscretizer).
   BufferGranParamItemOnsetRandom,        // 0=periodic, 1=Poisson dust
   BufferGranParamItemEDOSteps,           // indexed: 0=off; N=quantise scatter to N-EDO
   BufferGranParamItemQuantiseMaterial,   // indexed: 0=off (use EDO); else 1 + material index
   BufferGranParamItemQuantiseMode,       // indexed: Snap / Scatter
   BufferGranParamItemCentreMode,         // indexed: Scatter centre mode index
   BufferGranParamItemModeScatter,        // indexed: Scatter spread in mode-index space
   BufferGranParamItemModeSkew,           // Scatter low/high bias, -1..1
   BufferGranParamItemDiscretizerStretch, // INDEX into kModalStretchValues
   // Appended (2026-09-17): control inputs + attenuators for Scatter/Density/Duration, mirroring
   // the Position mod pair above. MUST stay at the end — item values key saved state / PM parity.
   BufferGranParamItemScatterModInput,    // connection: pitch-scatter modulation
   BufferGranParamItemDensityModInput,    // connection: density modulation
   BufferGranParamItemDurationModInput,   // connection: duration modulation
   BufferGranParamItemScatterModLevel,    // scales ScatterModInput
   BufferGranParamItemDensityModLevel,    // scales DensityModInput
   BufferGranParamItemDurationModLevel,   // scales DurationModInput
};

// SpectralGran — spectral granular synth reading analysis frames from a rolling capture of audio.
// Order matches PM's ParamGroupData.h (SpectralGranParamItem); saved state is keyed by base|item.
enum SpectralGranFreeze : NSUInteger {
   SpectralGranRolling = 0,   // granulate the live spectrum
   SpectralGranFrozen,        // latch + granulate the captured spectrum
};
enum SpectralGranFFTSize : NSUInteger {   // module maps index → 512 << index
   SpectralGranFFT512 = 0,
   SpectralGranFFT1024,
   SpectralGranFFT2048,
   SpectralGranFFT4096,
};
enum SpectralGranParamItem : NSUInteger {
   SpectralGranParamItemInput = 1,          // connection: audio to capture/granulate (Primary)
   SpectralGranParamItemGateInput,          // connection: optional gate override (else MIDI note)
   SpectralGranParamItemPitchInput,         // connection: optional pitch override, 1/oct (else MIDI note)
   SpectralGranParamItemFreezeInput,        // connection: optional freeze override (>0.5 = frozen)
   SpectralGranParamItemPositionModInput,   // connection: position modulation
   SpectralGranParamItemFreeze,             // indexed: 0=rolling, 1=frozen
   SpectralGranParamItemBufferSeconds,      // ring length, s
   SpectralGranParamItemFFTSize,            // indexed: 512/1024/2048/4096
   SpectralGranParamItemPosition,           // analysis-frame centre, 0..1
   SpectralGranParamItemPositionModLevel,   // scales PositionModInput
   SpectralGranParamItemScanRate,           // centre drift: 0 hold, 1 real-time, <0 reverse
   SpectralGranParamItemSpray,              // per-hop frame jitter, 0..1
   SpectralGranParamItemPitch,              // excitation transpose, semitones
   SpectralGranParamItemPitchFine,          // cents
   SpectralGranParamItemFormant,            // envelope shift, semitones (independent of pitch)
   SpectralGranParamItemPartialSpread,      // 0..1, fan partials apart
   SpectralGranParamItemFormantRes,         // 0..1, envelope smoothing detail
   SpectralGranParamItemWidth,              // 0=mono, 1=decorrelated stereo
   SpectralGranParamItemLevel,              // output gain
   SpectralGranParamItemVelToLevel,         // velocity → level, 0..1
};

// IFSGran — IFS granular synth: an IFS-oscillator grain waveform scattered in time/pitch by a
// 2-transform IFS attractor. Order matches PM's ParamGroupData.h (IFSGranParamItem); the last
// several items are APPENDED (T1/T2 fixed-point Y, grain peak, discretizer) — keep that order.
enum IFSGranParamItem : NSUInteger {
   IFSGranParamItemGateInput = 1,          // connection: note gate or CV
   IFSGranParamItemPitchInput,             // connection: PitchConverter output (1 oct/unit)
   IFSGranParamItemPitch,                  // centre pitch offset, semitones
   IFSGranParamItemPitchFine,              // fine tune, cents
   IFSGranParamItemBx,                     // grain waveform IFS bx, -1..1
   IFSGranParamItemBy,                     // grain waveform IFS by, -1..1
   IFSGranParamItemIFS1Squash,             // grain waveform squash 1, -0.99..0.99
   IFSGranParamItemIFS2Squash,             // grain waveform squash 2, -0.99..0.99
   IFSGranParamItemDensity,                // grains per second
   IFSGranParamItemDuration,               // grain length, ms
   IFSGranParamItemT1FixedPointX,          // scatter T1 fixed point X (time), 0..1
   IFSGranParamItemT1Contraction,          // scatter T1 contraction, 0.01..0.99
   IFSGranParamItemT1Rotation,             // scatter T1 rotation, degrees -180..180
   IFSGranParamItemT2FixedPointX,          // scatter T2 fixed point X (time), 0..1
   IFSGranParamItemT2Contraction,          // scatter T2 contraction, 0.01..0.99
   IFSGranParamItemT2Rotation,             // scatter T2 rotation, degrees -180..180
   IFSGranParamItemProbability,            // P(T1), 0..1; P(T2)=1-P(T1)
   IFSGranParamItemScatterAmount,          // scales pitch spread of scatter output
   IFSGranParamItemEDOSteps,               // indexed: 0=off, N=quantise scatter pitch to N-EDO
   IFSGranParamItemPanSpread,              // stereo scatter width, 0..1
   IFSGranParamItemOutputLevel,            // output gain, 0..2
   IFSGranParamItemLooping,                // boolean: 0=one-shot, 1=loop
   IFSGranParamItemPitchModInput,          // connection
   IFSGranParamItemPitchModLevel,          // semitones, -24..24
   IFSGranParamItemDensityModInput,        // connection
   IFSGranParamItemDensityModLevel,        // -2..2
   IFSGranParamItemDurationModInput,       // connection
   IFSGranParamItemDurationModLevel,       // -2..2
   IFSGranParamItemScatterAmountModInput,  // connection
   IFSGranParamItemScatterAmountModLevel,  // -2..2
   IFSGranParamItemBxModInput,             // connection
   IFSGranParamItemBxModLevel,             // -2..2
   IFSGranParamItemByModInput,             // connection
   IFSGranParamItemByModLevel,             // -2..2
   IFSGranParamItemIFS1SquashModInput,     // connection
   IFSGranParamItemIFS1SquashModLevel,     // -2..2
   IFSGranParamItemIFS2SquashModInput,     // connection
   IFSGranParamItemIFS2SquashModLevel,     // = 38
   // Appended after the original 38 — keeps all earlier addresses stable.
   IFSGranParamItemT1FixedPointY,          // scatter T1 fixed point Y (pitch), 0..1  = 39
   IFSGranParamItemT2FixedPointY,          // scatter T2 fixed point Y (pitch), 0..1  = 40
   IFSGranParamItemGrainPeak,              // grain envelope peak position, 0..1       = 41
   // Mode-ratio discretizer (shared with ClockGran/BufferGran).
   IFSGranParamItemQuantiseMaterial,       // indexed: 0=off, N=material N-1
   IFSGranParamItemQuantiseMode,           // indexed: 0=Snap, 1=Scatter
   IFSGranParamItemCentreMode,             // indexed
   IFSGranParamItemModeScatter,            // indexed
   IFSGranParamItemModeSkew,               // -1..1
   IFSGranParamItemDiscretizerStretch,     // INDEX into kModalStretchValues
   IFSGranParamItemPitchRange,             // scatter pitch spread, semitones (12..60)
};

// FM4Op — DX7-style 4-operator phase-modulation voice. Order matches PM's ParamGroupData.h
// (FM4OpParamItem). The four per-operator blocks are CONTIGUOUS and 13 items each — the DSP
// strides by 13 from FM4OpParamItemOp1AmpInput (see opItem()); do NOT reorder within a block.
enum FM4OpParamItem : NSUInteger {
   // Global
   FM4OpParamItemAlgorithm = 1,   // indexed: 0..7 (operator routing)
   FM4OpParamItemFeedback,        // feedback amount (top op), 0..1
   FM4OpParamItemOutputLevel,     // master out
   FM4OpParamItemPitchInput,      // connection: fundamental pitch CV (oct re A440); MIDI note if unpatched
   FM4OpParamItemGateInput,       // connection: gate CV (rising=trigger, falling=release); overrides MIDI
   FM4OpParamItemRetrigger,       // indexed: 0=Steal, 1=LegatoRetrigger, 2=LegatoTrue
   FM4OpParamItemDetune,          // L/R stereo detune (0..1 -> cents)
   FM4OpParamItemSpread,          // stereo width (0=mono, 1=full L/R)
   // Operator 1 (13 items)
   FM4OpParamItemOp1AmpInput, FM4OpParamItemOp1FreqInput, FM4OpParamItemOp1Ratio,
   FM4OpParamItemOp1Fine, FM4OpParamItemOp1Wave, FM4OpParamItemOp1Level,
   FM4OpParamItemOp1AmpDepth, FM4OpParamItemOp1FreqDepth, FM4OpParamItemOp1Delay,
   FM4OpParamItemOp1Attack, FM4OpParamItemOp1Decay, FM4OpParamItemOp1Sustain, FM4OpParamItemOp1Release,
   // Operator 2 (13 items)
   FM4OpParamItemOp2AmpInput, FM4OpParamItemOp2FreqInput, FM4OpParamItemOp2Ratio,
   FM4OpParamItemOp2Fine, FM4OpParamItemOp2Wave, FM4OpParamItemOp2Level,
   FM4OpParamItemOp2AmpDepth, FM4OpParamItemOp2FreqDepth, FM4OpParamItemOp2Delay,
   FM4OpParamItemOp2Attack, FM4OpParamItemOp2Decay, FM4OpParamItemOp2Sustain, FM4OpParamItemOp2Release,
   // Operator 3 (13 items)
   FM4OpParamItemOp3AmpInput, FM4OpParamItemOp3FreqInput, FM4OpParamItemOp3Ratio,
   FM4OpParamItemOp3Fine, FM4OpParamItemOp3Wave, FM4OpParamItemOp3Level,
   FM4OpParamItemOp3AmpDepth, FM4OpParamItemOp3FreqDepth, FM4OpParamItemOp3Delay,
   FM4OpParamItemOp3Attack, FM4OpParamItemOp3Decay, FM4OpParamItemOp3Sustain, FM4OpParamItemOp3Release,
   // Operator 4 (13 items)
   FM4OpParamItemOp4AmpInput, FM4OpParamItemOp4FreqInput, FM4OpParamItemOp4Ratio,
   FM4OpParamItemOp4Fine, FM4OpParamItemOp4Wave, FM4OpParamItemOp4Level,
   FM4OpParamItemOp4AmpDepth, FM4OpParamItemOp4FreqDepth, FM4OpParamItemOp4Delay,
   FM4OpParamItemOp4Attack, FM4OpParamItemOp4Decay, FM4OpParamItemOp4Sustain, FM4OpParamItemOp4Release,
   // Appended (preset-safe: name-keyed loads, no-op defaults).
   FM4OpParamItemPitch,           // global coarse transpose (semitones)
   FM4OpParamItemFine,            // global fine tune (cents)
   FM4OpParamItemFMInput,         // connection: linear FM CV → added to fundamental Hz (× FMLevel)
   FM4OpParamItemFMLevel,         // FM input depth (Hz per unit input)
   FM4OpParamItemFeedbackInput,   // connection: feedback-amount mod CV (× FeedbackDepth)
   FM4OpParamItemFeedbackDepth,   // feedback-mod input depth
   FM4OpParamItemOversample,      // anti-alias quality: 0 = Off, 1 = 2×, 2 = 4× (CPU cost only when on)
};

// DrumKit — one MIDI-driven module that renders a whole kit; the played note's pitch class picks
// the drum. Only the "finalized" per-drum user controls are exposed (PM's Dev1..16 scratch knobs
// are dropped). See DrumKitModule.cpp.
enum DrumKitParamItem : NSUInteger {
   DrumKitParamItemLevel = 1,     // master output gain

   // --- Crash ---
   DrumKitParamItemCrashSwell,    // bloom time — LPF sweep length (ride->crash)
   DrumKitParamItemCrashTone,     // noise vs metal balance
   DrumKitParamItemCrashDecay,    // overall decay (drives metal + noise together)
   DrumKitParamItemCrashBright,   // bloom end cutoff (how bright it opens up)

   // --- Snare ---
   DrumKitParamItemSnareTune,     // head pitch (f0)
   DrumKitParamItemSnareBody,     // body vs wires balance (crisp <-> fat)
   DrumKitParamItemSnareBuzz,     // body->wire coupling (fuses the two)
   DrumKitParamItemSnareSnares,   // wire/noise decay (tight <-> loose)

   // --- Toms ---
   DrumKitParamItemTomTune,       // pitch (transposes low/mid/high together)
   DrumKitParamItemTomDecay,      // body ring length (punchy <-> boomy)
   DrumKitParamItemTomBright,     // upper-mode tilt (de-tone: dark <-> lively)
   DrumKitParamItemTomThump,      // attack skin/stick punch

   // --- Hi-hats ---
   DrumKitParamItemHatTune,       // overall hat pitch (open lowest, half/closed higher)
   DrumKitParamItemHatBalance,    // noise sizzle vs metal ring
   DrumKitParamItemHatBright,     // sizzle high-pass cutoff
};

// SharedLFO — an LFO whose single phase is shared across all voices (phase-locked). Same shapes as
// the plain LFO but no modulation inputs (a shared phase can't reconcile per-voice modulation) — only
// a Clock input for tempo lock — plus a Free Run toggle. Wave/LrPhase reuse LFOParamWave / LFOParamLRPhase. See SharedLFOModule.cpp.
enum SharedLFOParamItem : NSUInteger {
   SharedLFOParamItemPitch = 1,   // rate, octaves; 0 = 1 Hz
   SharedLFOParamItemWave,        // LFOParamWave selector
   SharedLFOParamItemPhase,       // LFOParamLRPhase: L/R stereo phase offset
   SharedLFOParamItemFreeRun,     // 0 = voices-active-only (reset on silence); 1 = continuous free-run
   SharedLFOParamItemClockInput,  // connection: clock pulses — when patched, locks rate+phase to the clock
   SharedLFOParamItemClockRatio,  // indexed (kLFOClockRatios): Auto / 1/16..3/4 / ×1..×16 of the clock rate
};

// Noise — white/pink/brown generator. Order matches PM's NoiseType (ParamGroupData.h).
enum NoiseType : NSUInteger {
   NoiseTypeWhite = 0,
   NoiseTypePink,
   NoiseTypeBrown,
};
enum NoiseParamItem : NSUInteger {
   NoiseParamItemMono = 1,  // boolean: both channels identical
   NoiseParamItemType,      // indexed: White / Pink / Brown
   NoiseParamItemTypeMono,  // reserved/unused (PM .h lists it; no DSP use)
};

// --- MIDI converters (order matches PM's ParamGroupData.h enums) ---------------------------
enum VelocityParamItem : NSUInteger {
   VelocityParamItemIgnoreNoteOff = 1, // boolean
   VelocityParamItemMin,               // percent, 0..100
   VelocityParamItemCurve,             // 0 concave .. 1 convex
};
enum ChannelPressureParamItem : NSUInteger {
   ChannelPressureParamItemDummy = 1,  // no params needed; one dummy slot for UI storage
};
enum CCParamItem : NSUInteger {
   CCParamItemNumber = 1,   // indexed: CC controller number 0..127
   CCParamItemIsBoolean,    // boolean: 0/1 gate vs continuous
   CCParamItemInput,        // connection: when patched, CC scales it (VCA)
   CCParamItemAmount,       // CC scale (1 = unity)
   CCParamItemOffset,       // floor added to the scaled CC (0 = none)
   CCParamItemPerNote,      // boolean: per-channel CC + same-index per-note controller (PlainOldSynth)
};
enum PitchBendConverterParamItem : NSUInteger {
   PitchBendConverterParamItemQuarterTones = 1, // indexed bend range, 0..8 quarter-tones
   PitchBendConverterParamItemPerNoteSemitones, // MIDI 2.0 per-note bend range, semitones (PlainOldSynth)
};
enum NoteGateParamItem : NSUInteger {
   NoteGateParamItemMIDILowNote = 1,   // indexed: 0..127
   NoteGateParamItemMIDIHighNote,      // indexed: 0..127
   NoteGateParamItemGateOffDelay,      // seconds — hold the gate high this long after NoteOff (0 = off)
};

// Pan — true stereo pan: at Position=0, pass-through; toward ±1 the image narrows and collapses
// to mono on the far channel, scaled by 0.5 so loudness matches the input. See PanModule.cpp.
enum PanParamItem : NSUInteger {
   PanParamItemInput = 1,   // connection
   PanParamItemModInput,    // connection — CV summed with Position, clamped to [-1, 1]
   PanParamItemPosition,    // -1 (hard left) .. +1 (hard right)
};

// MIDI Note Number Converter — latches the note number of each incoming NoteOn (velocity-0
// NoteOn counts as NoteOff and is ignored), normalizes it over [LowNote, HighNote] to [0, 1] or,
// in Bipolar range mode, [-1, +1]. Holds the last value between notes (no note = no change).
enum MIDIRangeType : NSUInteger {
   MIDIRangeTypeBipolar = 0,
   MIDIRangeTypeMonopolar,
};
enum MIDINoteNumberConverterParamItem : NSUInteger {
   MIDINoteNumberConverterParamItemRangeType = 1,   // indexed: MIDIRangeType
   MIDINoteNumberConverterParamItemLowNote,         // indexed: 0..127
   MIDINoteNumberConverterParamItemHighNote,        // indexed: 0..127
};

// Value — a constant control signal (a knob, effectively). Range -4..4, default 1.
enum ValueParamItem : NSUInteger {
   ValueParamItemValue = 1,
};

// Sequencer — 16-step control-value sequencer (First..Last defines the active loop span, in
// 0-based UI-index form 0..7/0..15, or the old 1-based patch form 1..16 — see to1Based() in
// SequencerModule.cpp). Clock rising edge on TriggerInput advances one step; Reset rising edge
// (or RunGate falling, if connected) jumps back to First; RunGate low pauses the clock (defaults
// to always-running when RunGate is unconnected).
enum SequencerParamItem : NSUInteger {
   SequencerParamItemLevel1 = 1,
   SequencerParamItemLevel2,
   SequencerParamItemLevel3,
   SequencerParamItemLevel4,
   SequencerParamItemLevel5,
   SequencerParamItemLevel6,
   SequencerParamItemLevel7,
   SequencerParamItemLevel8,
   SequencerParamItemLevel9,
   SequencerParamItemLevel10,
   SequencerParamItemLevel11,
   SequencerParamItemLevel12,
   SequencerParamItemLevel13,
   SequencerParamItemLevel14,
   SequencerParamItemLevel15,
   SequencerParamItemLevel16,
   SequencerParamItemTriggerInput,   // connection
   SequencerParamItemFirst,          // indexed: first active step
   SequencerParamItemLast,           // indexed: last active step (inclusive)
   SequencerParamItemPage,           // indexed: UI paging hint (steps 1-8 vs 9-16); unused by DSP
   SequencerParamItemResetInput,     // connection
   SequencerParamItemRunGateInput,   // connection
};

// Quad Combine — 4-input version of Combine (Add/Multiply), same ParamItemCombineType. Any
// unconnected input uses the operation's identity value (0 for Add, 1 for Multiply).
enum QuadCombineParamItem : NSUInteger {
   QuadCombineParamItemInput1 = 1,   // connection
   QuadCombineParamItemInput2,       // connection
   QuadCombineParamItemInput3,       // connection
   QuadCombineParamItemInput4,       // connection
   QuadCombineParamItemOutputLevel,
   QuadCombineParamItemType,         // indexed: ParamItemCombineType
   QuadCombineParamItemClip,         // boolean: clamp result to [-1, 1]
};

// Quad Stereo Mixer — 4 stereo inputs, each with its own Level and Pan (balance panning: L/R
// channels of each input get independent pan-derived gains, no crossfeed between them).
enum QuadStereoMixerParamItem : NSUInteger {
   QuadStereoMixerParamItemInput1 = 1,   // connection
   QuadStereoMixerParamItemInput2,       // connection
   QuadStereoMixerParamItemInput3,       // connection
   QuadStereoMixerParamItemInput4,       // connection
   QuadStereoMixerParamItemLevel1,
   QuadStereoMixerParamItemLevel2,
   QuadStereoMixerParamItemLevel3,
   QuadStereoMixerParamItemLevel4,
   QuadStereoMixerParamItemPan1,
   QuadStereoMixerParamItemPan2,
   QuadStereoMixerParamItemPan3,
   QuadStereoMixerParamItemPan4,
};

// X-Fade — crossfade between InputA/InputB (Control CV + Position bias, additive, one-pole
// smoothed ~5ms), each side with its own Level and Pan. PanType selects Mix (each side summed to
// mono, then equal-power panned) vs Select (per-channel balance, no crossfeed). Curve selects the
// crossfade law itself: Linear vs EqualPower.
enum XFadeType : NSUInteger {
   XFadeTypeLinear = 0,
   XFadeTypeEqualPower,
};
enum XFadePanType : NSUInteger {
   XFadePanTypeMix = 0,
   XFadePanTypeSelect,
};
enum XFadeParamItem : NSUInteger {
   XFadeParamItemInputA = 1,      // connection
   XFadeParamItemInputB,          // connection
   XFadeParamItemControl,         // connection — 0..1 crossfade CV, additive with Position
   XFadeParamItemPosition,        // 0..1, added to Control (fallback when Control unconnected)
   XFadeParamItemCurve,           // indexed: XFadeType
   XFadeParamItemOutputLevel,
   XFadeParamItemLevelA,
   XFadeParamItemLevelB,
   XFadeParamItemPanA,            // -1..+1
   XFadeParamItemPanB,            // -1..+1
   XFadeParamItemPanType,         // indexed: XFadePanType
};

// Selector — N-way CV-driven crossfade/router. One Selector CV picks among 6 signal inputs by its
// position within [Boundary1..Boundary5]. Curve sets the *spatial* shape across a boundary (hard
// switch vs linear/equal-power blend over Spread); Spread is the crossfade width around each
// boundary, clamped to half the narrowest zone so blends stay strictly pairwise; Transition is the
// *temporal* weight-glide applied only when the selected zone actually changes (0 = hard switch),
// which leaves same-zone retriggers and any upstream/downstream slew untouched. Boundaries are MIDI
// note numbers (0..127); the DSP converts each to the pitch CV domain via (note-69)/12 (A4 = note 69
// = 0.0) so they line up with a PitchConverter on the Selector input, and the UI shows them as note
// names snapped to whole semitones. The module takes NO MIDI: feed the Selector input from a PitchConverter
// (keyboard split), a VelocityConverter scaled up into range (velocity layers), or any CV. Pitch
// bend / MPE / MIDI 2.0 §7.9 therefore affect zone choice only if the user sums them into the
// Selector CV — deliberately their call, no code here. IFSynth-specific (not from PolygraphModular).
enum SelectorCurve : NSUInteger {
   SelectorCurveHard = 0,   // instant switch at each boundary (Spread ignored)
   SelectorCurveLinear,     // linear crossfade across Spread
   SelectorCurveEqualPower, // constant-power crossfade across Spread
};
enum SelectorParamItem : NSUInteger {
   SelectorParamItemInput1 = 1,   // connection — zone 0 (selector below Boundary1)
   SelectorParamItemInput2,       // connection — zone 1 (Boundary1..Boundary2)
   SelectorParamItemInput3,       // connection — zone 2 (Boundary2..Boundary3)
   SelectorParamItemInput4,       // connection — zone 3 (Boundary3..Boundary4)
   SelectorParamItemInput5,       // connection — zone 4 (Boundary4..Boundary5)
   SelectorParamItemInput6,       // connection — zone 5 (selector above Boundary5)
   SelectorParamItemSelector,     // connection — CV selecting the zone (octaves-rel-A4 for pitch)
   SelectorParamItemBoundary1,    // MIDI note# threshold, zone0|zone1 (DSP → (n-69)/12 CV; UI = note name)
   SelectorParamItemBoundary2,    // MIDI note# threshold, zone1|zone2
   SelectorParamItemBoundary3,    // MIDI note# threshold, zone2|zone3
   SelectorParamItemBoundary4,    // MIDI note# threshold, zone3|zone4
   SelectorParamItemBoundary5,    // MIDI note# threshold, zone4|zone5
   SelectorParamItemCurve,        // indexed: SelectorCurve
   SelectorParamItemSpread,       // semitones — crossfade width around each boundary (0 = none)
   SelectorParamItemTransition,   // ms — weight glide on zone change only (0 = hard switch)
   SelectorParamItemOutputLevel,  // output trim
};

// Chorus — modulated stereo delay (ModDelayLine + pre/post LPF, feedback), BaseDelay + a
// ModDelay CV input (e.g. from an LFO) scaled by ModDelayLevel.
enum ChorusParamItem : NSUInteger {
   ChorusParamItemInput = 1,        // connection
   ChorusParamItemBaseDelay,        // ms
   ChorusParamItemMix,              // 0..1
   ChorusParamItemFeedback,         // 0..0.99
   ChorusParamItemModDelayInput,    // connection
   ChorusParamItemModDelayLevel,
};

// Flanger — like Chorus but with a second, opposite-polarity-modulated dry tap (no feedback) in
// addition to the feedback tap, each with its own mix level (DryLevel/Tap1Level/Tap2Level).
enum FlangerParamItem : NSUInteger {
   FlangerParamItemInput = 1,       // connection
   FlangerParamItemBaseDelay,       // ms
   FlangerParamItemDryLevel,        // 0..1
   FlangerParamItemTap1Level,       // 0..1 — modulated forward tap, with feedback
   FlangerParamItemTap2Level,       // 0..1 — opposite-polarity tap, no feedback
   FlangerParamItemFeedback,        // 0..0.99
   FlangerParamItemModDelayInput,   // connection — modulation signal, e.g. from an LFO
   FlangerParamItemModDelayLevel,
};

// Phaser — a bank of 2nd-order all-pass stages (2..8), center frequency spread per stage,
// modulated by ModInput (e.g. an LFO) scaled by ModLevel, with feedback and dry/wet Mix.
enum PhaserParamItem : NSUInteger {
   PhaserParamItemInput = 1,        // connection
   PhaserParamItemModInput,         // connection
   PhaserParamItemStages,           // indexed: 1..8 all-pass stages
   PhaserParamItemFeedback,         // 0..0.99
   PhaserParamItemMix,              // 0..1
   PhaserParamItemCenterFrequency,  // Hz
   PhaserParamItemQ,
   PhaserParamItemModLevel,
};

// Freq Shift — single-sideband (Hilbert) frequency shifter: ADDS a fixed Hz to every partial
// (unlike a pitch shifter, which multiplies), so a harmonic input becomes inharmonic/metallic.
// Shift is bipolar (+ up, - down); ShiftModInput (scaled by ShiftModLevel) sums with it.
enum FreqShiftParamItem : NSUInteger {
   FreqShiftParamItemInput = 1,       // connection — audio to shift
   FreqShiftParamItemShiftModInput,   // connection — optional shift modulation (e.g. an LFO)
   FreqShiftParamItemShift,           // Hz, bipolar: + up, - down
   FreqShiftParamItemShiftModLevel,   // scales ShiftModInput, in Hz
   FreqShiftParamItemFeedback,        // 0..0.95
   FreqShiftParamItemMix,             // 0 = dry, 1 = fully shifted
};

// Clock Divider — divides an Input clock by an integer Divisor, re-firing a fixed-width pulse.
// Optional Reset (rising edge resets the counter) and RunGate (low freezes the counter and holds
// output low; defaults to always-running when unconnected).
enum ClockDividerParamItem : NSUInteger {
   ClockDividerParamItemInput = 1,   // connection — clock/impulse stream (rising edge)
   ClockDividerParamItemResetInput,  // connection
   ClockDividerParamItemRunGate,     // connection
   ClockDividerParamItemDivisor,     // indexed: integer N, 1..16
   ClockDividerParamItemPulseWidth,  // seconds — one-shot width of the output pulse
};

// Shared by Delta Impulse (and, when ported, Threshold Impulse): which direction of crossing
// fires the impulse.
enum ImpulseTrigger : NSUInteger {
   ImpulseTriggerRising = 0,
   ImpulseTriggerFalling,
   ImpulseTriggerBoth,
};

// Delta Impulse — fires a one-sample impulse whenever the input changes by more than Delta since
// the previous sample, in the direction selected by Trigger.
enum DeltaImpulseParamItem : NSUInteger {
   DeltaImpulseParamItemInput = 1,   // connection
   DeltaImpulseParamItemTrigger,     // indexed: ImpulseTrigger
   DeltaImpulseParamItemDelta,       // -0.99..0.99
};

// One Shot — a rising edge on Input (0.5 threshold) fires a fixed-width one-shot pulse (Time,
// seconds); a later edge is ignored while the pulse is still active. Reset (rising edge) cancels
// the in-flight pulse immediately, dropping the output back to 0 and re-arming for the next edge.
enum OneShotParamItem : NSUInteger {
   OneShotParamItemInput = 1,   // connection
   OneShotParamItemTime,        // seconds
   OneShotParamItemReset,       // connection
};

// Logic Gate — combines up to 4 (optionally connected, optionally inverted) gate inputs with a
// boolean Operation (AND/OR/XOR, parity-accumulated across however many are connected), then
// either outputs the combined gate directly (Gate mode) or fires a fixed-width one-shot pulse on
// a selected edge of it (Impulse mode, with an optional retrigger-hold/debounce period).
enum LogicOperator : NSUInteger {
   LogicOperatorAND = 0,
   LogicOperatorOR,
   LogicOperatorXOR,
};
enum LogicOutputMode : NSUInteger {
   LogicOutputModeGate = 0,
   LogicOutputModeImpulse,
};
// EdgeMode is shared with ClockDivider/etc. in spirit but each module keeps its own item enum;
// this one specifically matches LogicGateParamItemEdge's value range (Rising/Falling/Both).
enum LogicGateEdgeMode : NSUInteger {
   LogicGateEdgeModeRising = 0,
   LogicGateEdgeModeFalling,
   LogicGateEdgeModeBoth,
};
enum LogicGateParamItem : NSUInteger {
   LogicGateParamItemInput1 = 1,   // connection
   LogicGateParamItemInput2,       // connection
   LogicGateParamItemInput3,       // connection
   LogicGateParamItemInput4,       // connection
   LogicGateParamItemInvert1,      // boolean
   LogicGateParamItemInvert2,      // boolean
   LogicGateParamItemInvert3,      // boolean
   LogicGateParamItemInvert4,      // boolean
   LogicGateParamItemOperation,    // indexed: LogicOperator
   LogicGateParamItemOutputMode,   // indexed: LogicOutputMode
   LogicGateParamItemEdge,         // indexed: LogicGateEdgeMode (Impulse mode only)
   LogicGateParamItemPulseWidth,   // seconds
   LogicGateParamItemHold,         // seconds — retrigger hold/debounce (Impulse mode only)
};

// Discretizer — quantizes Input to a grid, either a raw StepSize (with a per-sample StepSizeInput
// CV added to it) or a musical Kind: an equal-division-of-octave-ish step count picked from
// NoteInterval (5/7/12/19/22/31/53 steps/octave; 12 = ordinary semitones).
enum DiscretizerKind : NSUInteger {
   DiscretizerKindStepSize = 0,
   DiscretizerKindMusical,
};
enum DiscretizerNoteInterval : NSUInteger {
   DiscretizerNoteInterval5 = 0,
   DiscretizerNoteInterval7,
   DiscretizerNoteInterval12,
   DiscretizerNoteInterval19,
   DiscretizerNoteInterval22,
   DiscretizerNoteInterval31,
   DiscretizerNoteInterval53,
};
enum DiscretizerParamItem : NSUInteger {
   DiscretizerParamItemInput = 1,        // connection
   DiscretizerParamItemKind,             // indexed: DiscretizerKind
   DiscretizerParamItemStepSize,         // 0.001..0.999 (StepSize kind only)
   DiscretizerParamItemNoteInterval,     // indexed: DiscretizerNoteInterval (Musical kind only)
   DiscretizerParamItemStepSizeInput,    // connection — added to StepSize per-sample
};

// EDO Discretizer — quantizes Input to a CUSTOM microtonal scale (StepsPerOctave equal
// divisions of the octave, individually enabled/disabled via NoteEnable0..30), unlike plain
// Discretizer's fixed note-interval choices. Origin Note affects display/labeling and the
// DSP fallback (used only when zero steps are enabled) — it does NOT transpose the enabled
// scale. Ported from PolygraphModular's EDODiscretizerModule.mm/ParamGroupData.m. The UI's
// Piano/Alternating layout toggle (PM's LayoutViewType) was deliberately dropped, not ported
// — display-only, Piano is the only layout dgl-ui's EDOGridView supports (2026-08-31, per
// the user: "I don't care about supporting the Piano/Alternating control and UI").
enum EDODiscretizerParamItem : NSUInteger {
   EDODiscretizerParamItemInput = 1,          // connection
   EDODiscretizerParamItemStepsPerOctave,     // indexed: 12..31, default 24
   EDODiscretizerParamItemOriginNote,         // indexed: 0=A..11=G# (see kOriginNoteNames)
   EDODiscretizerParamItemNoteEnable0,        // boolean, default on — step 0 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable1,        // boolean, default on — step 1 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable2,        // boolean, default on — step 2 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable3,        // boolean, default on — step 3 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable4,        // boolean, default on — step 4 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable5,        // boolean, default on — step 5 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable6,        // boolean, default on — step 6 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable7,        // boolean, default on — step 7 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable8,        // boolean, default on — step 8 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable9,        // boolean, default on — step 9 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable10,       // boolean, default on — step 10 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable11,       // boolean, default on — step 11 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable12,       // boolean, default on — step 12 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable13,       // boolean, default on — step 13 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable14,       // boolean, default on — step 14 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable15,       // boolean, default on — step 15 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable16,       // boolean, default on — step 16 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable17,       // boolean, default on — step 17 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable18,       // boolean, default on — step 18 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable19,       // boolean, default on — step 19 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable20,       // boolean, default on — step 20 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable21,       // boolean, default on — step 21 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable22,       // boolean, default on — step 22 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable23,       // boolean, default on — step 23 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable24,       // boolean, default on — step 24 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable25,       // boolean, default on — step 25 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable26,       // boolean, default on — step 26 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable27,       // boolean, default on — step 27 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable28,       // boolean, default on — step 28 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable29,       // boolean, default on — step 29 of StepsPerOctave
   EDODiscretizerParamItemNoteEnable30,       // boolean, default on — step 30 of StepsPerOctave
};

// Overdrive — cubic soft-clipper (3x - x^3)/2, dry/wet-blended by Drive, with a Bias offset that
// skews the curve asymmetrically (even-harmonic generation), DC-corrected so Bias alone doesn't
// shift the output's average level.
enum OverdriveParamItem : NSUInteger {
   OverdriveParamItemInput = 1,   // connection
   OverdriveParamItemDrive,       // 0 (linear) .. 1 (full cubic saturation)
   OverdriveParamItemBias,        // 0 .. 0.5
};

// IFS Resonator — two coupled recursive (IIR) delay lines per channel, fractional-delay
// (linear-interpolated), pitch-tracked: D = sampleRate/frequency splits into D1/D2 via Bx, with
// By cross-coupling gain between the two branches and independent per-branch feedback gains
// (IFS1Squash/IFS2Squash). All five shaping params (Pitch/Bx/By/IFS1Squash/IFS2Squash) have their
// own CV input + level.
enum IFSResParamItem : NSUInteger {
   IFSResParamItemAudioInput = 1,        // connection
   IFSResParamItemPitch,                 // semitones, -48..48 (0 = A3 ~= 220 Hz)
   IFSResParamItemBx,                    // delay ratio split, -1..1
   IFSResParamItemBy,                    // cross-coupling gain, -1..1
   IFSResParamItemIFS1Squash,            // feedback gain 1, -0.99..0.99
   IFSResParamItemIFS2Squash,            // feedback gain 2, -0.99..0.99
   IFSResParamItemOutputLevel,           // 0..2
   IFSResParamItemDryWet,                // 0 = dry, 1 = wet
   IFSResParamItemPitchModInput,         // connection
   IFSResParamItemPitchModLevel,         // semitones, -24..24
   IFSResParamItemBxModInput,            // connection
   IFSResParamItemBxModLevel,            // -2..2
   IFSResParamItemByModInput,            // connection
   IFSResParamItemByModLevel,            // -2..2
   IFSResParamItemIFS1SquashModInput,    // connection
   IFSResParamItemIFS1SquashModLevel,    // -2..2
   IFSResParamItemIFS2SquashModInput,    // connection
   IFSResParamItemIFS2SquashModLevel,    // -2..2
};

// Polynomial — out = clamp(SquareTerm*in^2 + LinearTerm*in + ConstantTerm, Minimum, Maximum); the
// squared term is skipped entirely (cheaper linear-only path) when SquareTerm == 0.
enum PolynomialParamItem : NSUInteger {
   PolynomialParamItemInput = 1,   // connection
   PolynomialParamItemSquareTerm,
   PolynomialParamItemLinearTerm,
   PolynomialParamItemConstantTerm,
   PolynomialParamItemMinimum,
   PolynomialParamItemMaximum,
};

// Scale Offset — out = clamp(Scale*in + Offset, Minimum, Maximum); clamping is skipped entirely
// when Minimum > Maximum (the "no clamp" convention this module uses instead of a separate
// enable flag).
enum ScaleOffsetParamItem : NSUInteger {
   ScaleOffsetParamItemInput = 1,   // connection
   ScaleOffsetParamItemScale,       // default 1.0
   ScaleOffsetParamItemOffset,      // default 0.0 (bias)
   ScaleOffsetParamItemMinimum,     // clamp min
   ScaleOffsetParamItemMaximum,     // clamp max
};

// Split Level — independent Level (scale) + Offset for the L and R channels of one input, so a
// single stereo signal can be re-balanced/re-biased per side.
enum SplitLevelParamItem : NSUInteger {
   SplitLevelParamItemInput = 1,   // connection
   SplitLevelParamItemLevelLeft,
   SplitLevelParamItemLevelRight,
   SplitLevelParamItemOffsetLeft,
   SplitLevelParamItemOffsetRight,
};

// Sample Delay — independent, exact-integer-sample delay (0..1s, rounded to nearest sample) per
// channel; a simple circular buffer, no interpolation (this is a whole-sample utility delay, not
// a modulatable one).
enum SampleDelayParamItem : NSUInteger {
   SampleDelayParamItemInput = 1,    // connection — stereo pair
   SampleDelayParamItemDelayLeft,    // seconds, 0..1
   SampleDelayParamItemDelayRight,   // seconds, 0..1
};

// Unsplit — the inverse of Split Level: picks Left, Right, or (Left+Right)*0.5 independently for
// each output channel, from a separate InputLeft/InputRight pair.
enum ChannelChoice : NSUInteger {
   ChannelChoiceLeft = 0,
   ChannelChoiceRight,
   ChannelChoiceSum,
};
enum UnsplitParamItem : NSUInteger {
   UnsplitParamItemInputLeft = 1,   // connection
   UnsplitParamItemInputRight,      // connection
   UnsplitParamItemChooseLeft,      // indexed: ChannelChoice
   UnsplitParamItemChooseRight,     // indexed: ChannelChoice
};

// Sample And Hold — latches Input at each rising edge (0.5 threshold) of Trigger, per channel
// independently, and holds it until the next edge.
enum SampleAndHoldParamItem : NSUInteger {
   SampleAndHoldParamItemInput = 1,   // connection
   SampleAndHoldParamItemTrigger,     // connection
};

// Threshold Impulse — fires a one-sample impulse when Input crosses Threshold (+ per-sample
// ThresholdInput CV, clamped to +-0.99) in the direction selected by Trigger, then arms again
// once the input clears the Deadband around that threshold (both sides for Both mode).
enum ThresholdImpulseParamItem : NSUInteger {
   ThresholdImpulseParamItemInput = 1,   // connection
   ThresholdImpulseParamItemTrigger,     // indexed: ImpulseTrigger (see DeltaImpulseParamItem)
   ThresholdImpulseParamItemThreshold,   // -0.99..0.99
   ThresholdImpulseParamItemDeadband,    // 0.001..0.1
   ThresholdImpulseParamItemThresholdInput, // connection — summed with Threshold per-sample
};

// Global Delay — post-final stereo delay/echo (GlobalModuleManager.hpp), NOT a graph node —
// see pm_addresses.h's globalDelay1Address. Ported from PolygraphModular's GlobalDelayModule
// (PolygraphModularExtension/Global/GlobaDelayModule.mm) — same item order/meaning.
enum GlobalDelayParamItem : NSUInteger {
   GlobalDelayParamItemBypass = 1,
   GlobalDelayParamItemSecondsLeft,     // seconds, 0..5
   GlobalDelayParamItemSecondsRight,    // seconds, 0..5
   GlobalDelayParamItemLevelLeft,       // 0..0.99
   GlobalDelayParamItemLevelRight,      // 0..0.99
   GlobalDelayParamItemLevelDry,        // 0..1
   GlobalDelayParamItemCrossDelay,      // boolean: play each side's OTHER-channel delayed tap
   GlobalDelayParamItemCrossFeedback,   // boolean: feed each side's delayed tap into the OTHER
                                        // channel's own delay line
   GlobalDelayParamItemFeedbackLevel,   // 0..0.99
};

// Global Reverb — post-final Freeverb-style reverb (GlobalModuleManager.hpp), NOT a graph node —
// see pm_addresses.h's globalReverb1Address. Ported from PolygraphModular's GlobalReverbModule
// (PolygraphModularExtension/Global/GlobalReverbModule.mm) — same item order/meaning.
enum GlobalReverbParamItem : NSUInteger {
   GlobalReverbParamItemBypass = 1,
   GlobalReverbParamItemMix,            // 0..1 dry/wet
   GlobalReverbParamItemPreDelayMS,     // 0..80 ms
   GlobalReverbParamItemDecaySeconds,   // 0.2..12 (RT60)
   GlobalReverbParamItemDamping,        // 0..1, high-frequency damping in the comb feedback path
   GlobalReverbParamItemWidth,          // 0..1 stereo width
};

// Global Enhance — post-final "Enigma"-style enhancer: a modulated all-pass notch cascade with
// a diffused feedback path (GlobalModuleManager.hpp), NOT a graph node — see pm_addresses.h's
// globalEnhance1Address. Ported from PolygraphModular's GlobalEnhanceModule
// (PolygraphModularExtension/Global/GlobalEnhanceModule.mm) — same item order/meaning.
enum GlobalEnhanceParamItem : NSUInteger {
   GlobalEnhanceParamItemBypass = 1,
   GlobalEnhanceParamItemRate,          // 0.01..5 Hz, all-pass sweep LFO rate
   GlobalEnhanceParamItemDepth,         // 0..1, all-pass sweep depth
   GlobalEnhanceParamItemStages,        // indexed 2..10, all-pass stage count
   GlobalEnhanceParamItemFeedback,      // 0..0.85
   GlobalEnhanceParamItemFixedDelay,    // 0..25 ms, feedback-path base delay
   GlobalEnhanceParamItemMix,           // 0..1 dry/wet
   GlobalEnhanceParamItemDelayMod,      // 0..5 ms delay-LFO depth; 0 = static integer delay
   GlobalEnhanceParamItemDelayRate,     // 0.01..5 Hz, delay LFO rate (independent of the AP LFO)
   GlobalEnhanceParamItemApLRPhase,     // indexed 0..4: L/R phase offset for the AP LFO (kLfoPhase)
   GlobalEnhanceParamItemDelayLRPhase,  // indexed 0..4: L/R phase offset for the delay LFO (kLfoPhase)
};

// Bitfield stored in ModuleParamConstants::paramInfo. ParamMetaDataInput marks a
// parameter that is actually a module signal INPUT (a connection port on the node),
// as opposed to a scalar knob. Portable replacement for PM's NS_ENUM ParamMetaData
// (was in ParamGroupData.h). Add more bits (1<<1, 1<<2, …) as needed.
enum ParamMetaData : uint64_t {
   ParamMetaDataInput        = 1,        // 1 << 0 : this param is a module input / connection port
   ParamMetaDataForceDropdown = 1 << 1,  // indexed param: render as a Dropdown even if few enough
                                          // labels would otherwise fit a segmented Picker
   ParamMetaDataNoteNameOnly  = 1 << 2,  // MIDI-note param: note-name stepper only, no editable CV
                                          // field (a plain reference note vs a Selector CV boundary)
};

// Per-parameter constants (portable): the source for CLAP clap_param_info generation.
struct ModuleParamConstants {
   const char*               name;
   uint8_t                   item;
   AudioUnitParameterUnit    unit;
   AudioUnitParameterOptions flags;
   const char* const*        valueStrings; // null or null-terminated list (stepped params)
   AudioUnitParameterValue   minValue;
   AudioUnitParameterValue   maxValue;
   AudioUnitParameterValue   defValue;
   uint64_t                  paramInfo;
};

struct ModuleParamConstantsGroup {
   char*                       name;
   size_t                      count;
   const ModuleParamConstants* items;
};

#endif // PM_PARAMS_H
