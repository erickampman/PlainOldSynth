// pm_compat.h — portability shim for vendoring the PolygraphModular DSP/voice framework
// off Apple platforms. The PM framework is only shallowly coupled to Apple: a few scalar
// typedefs, UMP MIDI, and os_log. This header collapses that surface so the framework
// compiles under a plain C++20 toolchain (Linux/clang/gcc) with no AudioToolbox/CoreMIDI.
//
// STATUS: foundation. The scalar typedefs + logging no-ops below are final. The UMP
// MIDIUniversalMessage struct is added when the framework is vendored (Phase 1b) — it must
// mirror the exact field access the PM modules use (message.channelVoice2.note.number,
// .velocity, .attributeType, .attribute, .status; message.type, .group; kMIDICV* constants).

#ifndef PM_COMPAT_H
#define PM_COMPAT_H

#include <cstdint>
#include <cstddef>

// These are plain C++ translation units on every platform (including macOS — the PM
// framework is vendored for a CLAP-only build, not an AUv3 host), so the real Apple SDK
// typedefs are never reliably visible here even when AudioToolbox/CoreMIDI are present:
// NSUInteger/NSInteger/AUParameterAddress etc. live behind Objective-C-only paths in Apple's
// headers. Define the same portable aliases unconditionally instead of branching on
// __APPLE__.

// --- scalar typedefs (Apple aliases → plain C++ types) ---------------------------------
typedef float    AUValue;
typedef float    AudioUnitParameterValue;
typedef uint32_t AudioUnitParameterUnit;
typedef uint32_t AudioUnitParameterOptions;

// AudioUnit parameter-unit constants (real Apple numeric values), so the per-param `unit` data
// copied from PolygraphModular's ParamGroupData.m maps 1:1. The UI reads this to choose a control:
// Indexed → dropdown, Boolean → toggle, everything else → slider (the specific unit is a display
// hint). Signal-input params are marked separately via ParamMetaDataInput and drawn as ports.
enum : AudioUnitParameterUnit {
   kAudioUnitParameterUnit_Generic           = 0,   // slider, no unit label
   kAudioUnitParameterUnit_Indexed           = 1,   // → dropdown (discrete choices)
   kAudioUnitParameterUnit_Boolean           = 2,   // → toggle
   kAudioUnitParameterUnit_Percent           = 3,
   kAudioUnitParameterUnit_Seconds           = 4,
   kAudioUnitParameterUnit_SampleFrames      = 5,
   kAudioUnitParameterUnit_Phase             = 6,
   kAudioUnitParameterUnit_Rate              = 7,
   kAudioUnitParameterUnit_Hertz             = 8,
   kAudioUnitParameterUnit_Cents             = 9,
   kAudioUnitParameterUnit_RelativeSemiTones = 10,
   kAudioUnitParameterUnit_MIDINoteNumber    = 11,
   kAudioUnitParameterUnit_MIDIController     = 12,
   kAudioUnitParameterUnit_Decibels          = 13,
   kAudioUnitParameterUnit_LinearGain        = 14,
   kAudioUnitParameterUnit_Degrees           = 15,
   kAudioUnitParameterUnit_EqualPowerCrossfade = 16,
   kAudioUnitParameterUnit_MixerFaderCurve   = 17,
   kAudioUnitParameterUnit_Pan               = 18,
   kAudioUnitParameterUnit_Meters            = 19,
   kAudioUnitParameterUnit_AbsoluteCents     = 20,
   kAudioUnitParameterUnit_Octaves           = 21,
   kAudioUnitParameterUnit_BPM               = 22,
   kAudioUnitParameterUnit_Beats             = 23,
   kAudioUnitParameterUnit_Milliseconds      = 24,
   kAudioUnitParameterUnit_Ratio             = 25,
   kAudioUnitParameterUnit_CustomUnit        = 26,
};
typedef uint64_t AUParameterAddress;
typedef uint32_t AUAudioFrameCount;
typedef int64_t  AUEventSampleTime;
typedef unsigned long NSUInteger;
typedef long          NSInteger;

// --- logging (os_log → no-op; framework LOG_* macros funnel through these) ---------------
#define os_log_with_type(...)   ((void)0)
#define OS_LOG_TYPE_INFO        0

// --- UMP MIDI --------------------------------------------------------------------------
// TODO (Phase 1b): define a MIDIUniversalMessage struct compatible with the PM modules'
// field access, or adapt netmidi2's UMP types. Deliberately omitted until the framework
// files are vendored so it can be validated field-by-field against real usage.

#endif // PM_COMPAT_H
