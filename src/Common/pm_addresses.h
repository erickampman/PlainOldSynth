// pm_addresses.h — the v1 voice's module + input addresses. Rather than vendor the large
// ExtensionParameterAddresses.h, we define just the handful the fixed v1 patch needs. Each is
// MODULE_PARAM_ADDRESS(kind, instance, item) — the same packed value used as the CLAP clap_id.

#ifndef PM_ADDRESSES_H
#define PM_ADDRESSES_H

#include "ParamAddressMacros.h"
#include "pm_compat.h"
#include "pm_params.h"

inline constexpr AUParameterAddress pitchConverter1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindPitchConverter, ModuleInstance1, 0);

inline constexpr AUParameterAddress slew1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindSlew, ModuleInstance1, 0);
inline constexpr AUParameterAddress slew1InputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindSlew, ModuleInstance1, SlewParamItemInput);
// Pitch glide: one knob drives Rise and Fall together (symmetric slew).
inline constexpr AUParameterAddress slew1RiseAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindSlew, ModuleInstance1, SlewParamItemRise);
inline constexpr AUParameterAddress slew1FallAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindSlew, ModuleInstance1, SlewParamItemFall);

inline constexpr AUParameterAddress sinOsc1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindSinOsc, ModuleInstance1, 0);
inline constexpr AUParameterAddress sinOsc1PitchInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindSinOsc, ModuleInstance1, SinOscParamItemPitchInput);
// Osc 2 pitch mod-input port (a source's output connects here; depth is FreqModLevel).
inline constexpr AUParameterAddress sinOsc1PitchModInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindSinOsc, ModuleInstance1, SinOscParamItemModPitch);

// TriSawOsc — variable tri↔saw oscillator; LFO — low-frequency control oscillator. Pool base addresses.
inline constexpr AUParameterAddress triSaw1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindTriSawOsc, ModuleInstance1, 0);
inline constexpr AUParameterAddress lfo1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindLFO, ModuleInstance1, 0);

inline constexpr AUParameterAddress ifsOsc1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, 0);
inline constexpr AUParameterAddress ifsOsc1PitchInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemPitchInput);
// Hard-sync input (the sine osc's output feeds this; per-channel zero-crossing resets phase).
inline constexpr AUParameterAddress ifsOsc1SyncInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemSyncInput);
// IFSOsc mod-input ports (a source's output connects here; depth is the matching ModLevel).
inline constexpr AUParameterAddress ifsOsc1PitchModInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemPitchModInput);
inline constexpr AUParameterAddress ifsOsc1BxModInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemBxModInput);
inline constexpr AUParameterAddress ifsOsc1ByModInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemByModInput);
inline constexpr AUParameterAddress ifsOsc1Squash1ModInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemSquash1ModInput);
inline constexpr AUParameterAddress ifsOsc1Squash2ModInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemSquash2ModInput);

inline constexpr AUParameterAddress ifsEnv1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance1, 0);
inline constexpr AUParameterAddress ifsEnv1GateInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance1, IFSEnvParamItemGateInput);
// LegatoMode: 0 = NoteSteal (retrigger), 1 = FindMatchingY (jump to the earliest phase whose
// level matches the current output). Mono mode turns this on so retriggers don't click.
inline constexpr AUParameterAddress ifsEnv1LegatoModeAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance1, IFSEnvParamItemLegatoMode);

inline constexpr AUParameterAddress ifsLFO1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSLFO, ModuleInstance1, 0);

// Second IFS envelope / LFO instances — mod sources (not connected in the graph yet).
inline constexpr AUParameterAddress ifsEnv2Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance2, 0);
inline constexpr AUParameterAddress ifsEnv2LegatoModeAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance2, IFSEnvParamItemLegatoMode);
inline constexpr AUParameterAddress ifsLFO2Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSLFO, ModuleInstance2, 0);

// CV mixers (2). Base address + the two connection-input ports (In 1 / In 2).
inline constexpr AUParameterAddress cvMixer1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindCVMixer, ModuleInstance1, 0);
inline constexpr AUParameterAddress cvMixer1Input1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindCVMixer, ModuleInstance1, CVMixerParamItemInput1);
inline constexpr AUParameterAddress cvMixer1Input2Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindCVMixer, ModuleInstance1, CVMixerParamItemInput2);
inline constexpr AUParameterAddress cvMixer2Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindCVMixer, ModuleInstance2, 0);
inline constexpr AUParameterAddress cvMixer2Input1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindCVMixer, ModuleInstance2, CVMixerParamItemInput1);
inline constexpr AUParameterAddress cvMixer2Input2Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindCVMixer, ModuleInstance2, CVMixerParamItemInput2);

// Low-pass filter (24 dB/oct), per voice, inserted between the IFS oscillator and the VCA.
inline constexpr AUParameterAddress lpf1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindButterworthLowPassFilter, ModuleInstance1, 0);
inline constexpr AUParameterAddress lpf1InputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindButterworthLowPassFilter, ModuleInstance1, LPFParamItemInput);
inline constexpr AUParameterAddress lpf1FreqModInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindButterworthLowPassFilter, ModuleInstance1, LPFParamItemModFreqInput);
// Second freq-mod input, hardwired to the note pitch for key tracking (its level = FreqModLevel2).
inline constexpr AUParameterAddress lpf1KeyTrackInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindButterworthLowPassFilter, ModuleInstance1, LPFParamItemModFreqInput2);
inline constexpr AUParameterAddress lpf1QModInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindButterworthLowPassFilter, ModuleInstance1, LPFParamItemModQInput);

inline constexpr AUParameterAddress adsr1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindADSR, ModuleInstance1, 0);

inline constexpr AUParameterAddress combine1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindCombine, ModuleInstance1, 0);
inline constexpr AUParameterAddress combine1Input1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindCombine, ModuleInstance1, CombineParamItemInput1);
inline constexpr AUParameterAddress combine1Input2Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindCombine, ModuleInstance1, CombineParamItemInput2);

// Level — scales the sine osc's audible mix level (its raw output separately drives IFS sync).
inline constexpr AUParameterAddress level1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindLevel, ModuleInstance1, 0);
inline constexpr AUParameterAddress level1InputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindLevel, ModuleInstance1, LevelParamItemInput);

// Combine 2 — the oscillator mixer (Add): SinOsc + IFSOsc summed, then into the LPF.
inline constexpr AUParameterAddress combine2Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindCombine, ModuleInstance2, 0);
inline constexpr AUParameterAddress combine2Input1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindCombine, ModuleInstance2, CombineParamItemInput1);
inline constexpr AUParameterAddress combine2Input2Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindCombine, ModuleInstance2, CombineParamItemInput2);
inline constexpr AUParameterAddress combine2TypeAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindCombine, ModuleInstance2, CombineParamItemType);

// Tunnel In / Out — cross-plane routing (K4d). instance-1 base addresses; the pool provides several.
inline constexpr AUParameterAddress tunnelIn1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindTunnelInput, ModuleInstance1, 0);
inline constexpr AUParameterAddress tunnelOut1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindTunnelOutput, ModuleInstance1, 0);

inline constexpr AUParameterAddress finalAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindFinalOutput, ModuleInstance1, 0);
inline constexpr AUParameterAddress finalInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindFinalOutput, ModuleInstance1, FinalOutputParamItemInput);

// Global modules — post-final master-bus effects (GlobalModuleManager.hpp), NOT part of the
// per-voice patch graph above; same names as PM's own InstrumentKernel.mm
// (globalDelay1Address/globalReverb1Address/globalEnhance1Address) for easy side-by-side
// porting later. Each kind has exactly one, permanently-active instance — no pool.
inline constexpr AUParameterAddress globalDelay1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindGlobalDelay, ModuleInstance1, 0);
inline constexpr AUParameterAddress globalReverb1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindGlobalReverb, ModuleInstance1, 0);
inline constexpr AUParameterAddress globalEnhance1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindGlobalEnhance, ModuleInstance1, 0);

// --- StruckSynth: Strike exciter (MIDI note-on → excitation burst) -------------------------
inline constexpr AUParameterAddress strike1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindStrike, ModuleInstance1, 0);

// --- StruckSynth: ModalBank resonator ------------------------------------------------------
inline constexpr AUParameterAddress modalBank1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindModalBank, ModuleInstance1, 0);
// Excitation input (Strike's output patches here) and pitch-tracking input (from PitchConverter).
inline constexpr AUParameterAddress modalBank1InputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindModalBank, ModuleInstance1, ModalBankParamItemInput);
inline constexpr AUParameterAddress modalBank1PitchInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindModalBank, ModuleInstance1, ModalBankParamItemPitchInput);

// ADR envelope, SineWarp shaper, Comparator — instance-1 base addresses for the pool.
inline constexpr AUParameterAddress adr1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindADR, ModuleInstance1, 0);
inline constexpr AUParameterAddress sineWarp1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindSineWarp, ModuleInstance1, 0);
inline constexpr AUParameterAddress comparator1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindComparator, ModuleInstance1, 0);

// Pan — true stereo panner (instance-1 base address for the pool).
inline constexpr AUParameterAddress pan1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindPan, ModuleInstance1, 0);

// MIDI Note Number Converter (instance-1 base address for the pool).
inline constexpr AUParameterAddress midiNoteNumber1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindMIDINoteNumberConverter, ModuleInstance1, 0);

// Value (constant control signal) and Sequencer (instance-1 base addresses for the pool).
inline constexpr AUParameterAddress value1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindValue, ModuleInstance1, 0);
inline constexpr AUParameterAddress sequencer1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindSequencer, ModuleInstance1, 0);

// Quad Combine, Quad Stereo Mixer, X-Fade (instance-1 base addresses for the pool).
inline constexpr AUParameterAddress quadCombine1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindQuadCombine, ModuleInstance1, 0);
inline constexpr AUParameterAddress quadStereoMixer1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindQuadStereoMixer, ModuleInstance1, 0);
inline constexpr AUParameterAddress xFade1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindXFade, ModuleInstance1, 0);

// Chorus, Flanger, Phaser, Freq Shift (instance-1 base addresses for the pool).
inline constexpr AUParameterAddress chorus1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindChorus, ModuleInstance1, 0);
inline constexpr AUParameterAddress flanger1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindFlanger, ModuleInstance1, 0);
inline constexpr AUParameterAddress phaser1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindPhaser, ModuleInstance1, 0);
inline constexpr AUParameterAddress freqShift1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindFreqShift, ModuleInstance1, 0);

// Clock Divider, Delta Impulse, One Shot, Logic Gate (instance-1 base addresses for the pool).
inline constexpr AUParameterAddress clockDivider1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindClockDivider, ModuleInstance1, 0);
inline constexpr AUParameterAddress deltaImpulse1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindDeltaImpulse, ModuleInstance1, 0);
inline constexpr AUParameterAddress oneShot1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindOneShot, ModuleInstance1, 0);
inline constexpr AUParameterAddress logicGate1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindLogicGate, ModuleInstance1, 0);

// Discretizer, Overdrive, IFS Resonator, Polynomial (instance-1 base addresses for the pool).
inline constexpr AUParameterAddress discretizer1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindDiscretizer, ModuleInstance1, 0);
inline constexpr AUParameterAddress overdrive1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindOverdrive, ModuleInstance1, 0);
inline constexpr AUParameterAddress ifsRes1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSRes, ModuleInstance1, 0);
inline constexpr AUParameterAddress polynomial1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindPolynomial, ModuleInstance1, 0);
inline constexpr AUParameterAddress edoDiscretizer1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindEDODiscretizer, ModuleInstance1, 0);

// Scale Offset, Split Level, Sample Delay (instance-1 base addresses for the pool).
inline constexpr AUParameterAddress scaleOffset1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindScaleOffset, ModuleInstance1, 0);
inline constexpr AUParameterAddress splitLevel1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindSplitLevel, ModuleInstance1, 0);
inline constexpr AUParameterAddress sampleDelay1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindSampleDelay, ModuleInstance1, 0);

// Unsplit, Sample And Hold, Threshold Impulse (instance-1 base addresses for the pool).
inline constexpr AUParameterAddress unsplit1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindUnsplit, ModuleInstance1, 0);
inline constexpr AUParameterAddress sampleAndHold1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindSampleAndHold, ModuleInstance1, 0);
inline constexpr AUParameterAddress thresholdImpulse1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindThresholdImpulse, ModuleInstance1, 0);

// Filters (instance-1 base addresses for the pool).
inline constexpr AUParameterAddress variableSlope1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindVariableSlopeFilter, ModuleInstance1, 0);
inline constexpr AUParameterAddress formant1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindFormantFilter, ModuleInstance1, 0);
inline constexpr AUParameterAddress rezNotch1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindRezNotchFilter, ModuleInstance1, 0);
inline constexpr AUParameterAddress multimode1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindMultimodeFilter, ModuleInstance1, 0);
inline constexpr AUParameterAddress butterBandpass1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindButterworthBandpassFilter, ModuleInstance1, 0);
inline constexpr AUParameterAddress biquad1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindBiquadFilter, ModuleInstance1, 0);
inline constexpr AUParameterAddress doubleRezBandpass1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindDoubleRezBandpass, ModuleInstance1, 0);

// ClockGran — clock-driven granular synth (instance-1 base address for the pool).
inline constexpr AUParameterAddress clockGran1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindClockGran, ModuleInstance1, 0);

// BufferGran — audio-capture granular synth (instance-1 base address for the pool).
inline constexpr AUParameterAddress bufferGran1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindBufferGran, ModuleInstance1, 0);

// SpectralGran — spectral granular synth (instance-1 base address for the pool).
inline constexpr AUParameterAddress spectralGran1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindSpectralGran, ModuleInstance1, 0);

// IFSGran — IFS granular synth (instance-1 base address for the pool).
inline constexpr AUParameterAddress ifsGran1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSGran, ModuleInstance1, 0);

// FM4Op — DX7-style 4-operator FM voice (instance-1 base address for the pool).
inline constexpr AUParameterAddress fm4Op1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindFM4Op, ModuleInstance1, 0);

// DrumKit — note->drum kit voice (instance-1 base address for the pool).
inline constexpr AUParameterAddress drumKit1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindDrumKit, ModuleInstance1, 0);

// SharedLFO — phase-locked-across-voices LFO (instance-1 base address for the pool).
inline constexpr AUParameterAddress sharedLFO1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindSharedLFO, ModuleInstance1, 0);

// Selector — N-way CV-driven crossfade/router (instance-1 base address for the pool).
inline constexpr AUParameterAddress selector1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindSelector, ModuleInstance1, 0);

// Clock — free-running gate generator (instance-1 base address for the pool).
inline constexpr AUParameterAddress clock1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindClock, ModuleInstance1, 0);

// Noise — white/pink/brown generator (instance-1 base address for the pool).
inline constexpr AUParameterAddress noise1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindNoise, ModuleInstance1, 0);

// MIDI converters (instance-1 base addresses for the pool).
inline constexpr AUParameterAddress velocityConverter1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindVelocityConverter, ModuleInstance1, 0);
inline constexpr AUParameterAddress channelPressure1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindChannelPressure, ModuleInstance1, 0);
inline constexpr AUParameterAddress ccConverter1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindControlChangeConverter, ModuleInstance1, 0);
inline constexpr AUParameterAddress pitchBendConverter1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindPitchBendConverter, ModuleInstance1, 0);
inline constexpr AUParameterAddress noteGate1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindNoteGate, ModuleInstance1, 0);

#endif // PM_ADDRESSES_H
