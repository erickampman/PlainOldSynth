//
//  CVMixerModule.hpp — combines mod sources into one CV output.
//
//  Inputs: two patchable CV connections (In 1, In 2) plus two virtual MIDI-controller inputs, each
//  of which selects a Source — CC / channel Aftertouch / Pitch Bend / Pitch / Velocity. Each input
//  has a LEVEL that acts as a DEPTH (interpolating from the mode's identity to the full input), so
//  Level 0 = off in both Add and Multiply with no discontinuity; a detached physical input is
//  skipped (identity), never a 0 that would collapse a multiply. Mode adds or multiplies the
//  inputs (an idle Multiply sits at the identity 1, an idle Add at 0). Velocity/Pitch latch at note-on and ignore
//  note-off; Pitch is bipolar per-octave, centred on the shared Reference Note. Smoothing one-pole-
//  filters the per-input Levels. See pm_params.h's CVMixerParamItem comment. IFSynth-specific (not
//  from PolygraphModular).
//

#pragma once
#include "Module.hpp"

const void InitCVMixerModule(uint32_t count, Module* module);
const void DeinitCVMixerModule(Module* module);
const KPBuffPair* ProcessCVMixerModule(uint64_t processCallID, uint32_t count, Module* module);
void MIDICVMixerModule(const pm::UmpMessage& message, Module* module);
