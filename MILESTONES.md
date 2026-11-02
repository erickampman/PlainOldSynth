# PlainOldSynth milestones

## Tests
- all: cmake --build build && ctest --test-dir build --output-on-failure

## M1: Core subtractive synth
- [x] Stereo-detuned Tri/Saw/Pulse oscillator, 24 dB/oct LPF, VCA
- [x] Two ADSRs, two LFOs, glide, poly/mono with legato note stack
- [x] Three modulation sections (CVMixer): LFO/ADSR/CC1/Pressure/Timbre/Velocity sources, six destinations
- [x] Flanger / Chorus / Delay / Sync Delay effect
- [x] Factory presets baked in at build time; DPF/NanoVG UI

## M2: MIDI 2.0 and MPE
- [x] Raw CLAP entry point (DPF as GUI only, MIDI 2.0 dialect available)
- [x] Per-note UMP decode; note matching on note + channel + group; RC 7 bend range; Pitch 7.25
- [x] CLAP note ids; note expressions (tuning as Pitch 7.25; pressure/brightness decoded)
- [x] `CLAP_EVENT_NOTE_END`; voice-info extension
- [x] MPE lower zone: RPN 0 bend range, released-note control freeze, MPE on/off switch
- [x] Pressure and Timbre as per-note modulation sources
- [x] MIDI 1.0→2.0 Min-Center-Max upscale; velocity loudness dropdown
- [x] All Notes Off (CC 123) and All Sound Off (CC 120), per channel

## M3: Plugin formats
- [x] VST3 via clap-wrapper (note expressions: Tuning + Brightness exposed via CLAP_PLUGIN_AS_VST3)
- [x] AUv2 via clap-wrapper (macOS); auval passes; clap-wrapper AUv2 GUI patch carried in patches/
- [x] Linux CI (ubuntu-24.04, CLAP and VST3 builds)

## Save and Load (2026-10-27)
- [x] Save / Load in the header write the patch as the factory presets' JSON ({"name", "params": {symbol: number}}, plus
      a schema) and read it back (src/UI/PresetFile.hpp), through DPF's file dialog. Load works as a factory preset does:
      every param back to its default, then the file's values, clamped; MPE left alone and left out. Floats are written
      so they read back exactly. pos-presetfile checks the round trip, every factory preset, and non-presets
- [x] AUv2: clap-wrapper gives no timers and (unpatched) never calls gui show(), so the UI's idle never ran and DPF would never
      collect the dialog's result. The GUI now runs its own main-run-loop timer when the host has none, started at
      set_parent; the home folder comes from the account, not the sandboxed $HOME; failures show in the Preset menu
      (StruckGaussian 3f0c97c)
- [x] By hand on Linux (2026-11-01): Save and Load (DPF's own dialog)
- [x] By hand on the Mac (2026-11-01): Save and Load in Logic Pro (AUv2) and StageHand (CLAP)

## M4: Later
- [ ] Pitch 7.25 as persistent per-key tuning table (CLAP tuning stays per-note)
- [ ] Modulating the modulators: LFO rate, section output level as modulation destinations
- [ ] More effects (reverb)
- [ ] CLAP preset discovery factory
- [ ] MPE upper zone
- [x] Move onto the DPF fork (erickampman/DPF) with own DGL_NAMESPACE (2026-10-23)
