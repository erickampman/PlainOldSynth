# PlainOldSynth

A classic **subtractive** synthesizer plugin from Unlikelyware, in **CLAP**, **VST3** and (macOS)
**AUv2**. One stereo-detuned Tri/Saw/Pulse oscillator into a 24 dB/oct low-pass and a VCA, with two
ADSRs, two LFOs, glide, and switchable poly/mono — the plain old synth.

It is built from the same parts as the rest of the family: the DSP/voice engine is vendored from
[PolygraphModularUnified](../PolygraphModularUnified) and the GUI uses the NanoVG widget kit from
[StruckSynth](../StruckSynth), driven by DPF.

**Status: pre-release.** The voice is complete and validated; modulation routing is deliberately
fixed for now (see [Not done yet](#not-done-yet)).

## MIDI 2.0

The oscillators respond to **MIDI 2.0 NoteOn events carrying the 7.9 pitch attribute**, so a
MIDI-2.0-capable host can play them microtonally — a quarter-tone note really is a quarter tone, not
the nearest semitone. This is verified end-to-end against the built plugin: the test drives the real
`.clap` with a UMP NoteOn of `note 60 + 256/512 semitone` and measures **269.291772 Hz** against an
expected **269.291780 Hz**.

The path is: the note port advertises `CLAP | MIDI | MIDI2` and *prefers* MIDI2 → the host sends a
raw UMP → `pm::decodeUmp` keeps `attributeType`/`attribute` → `PitchConverterModule` turns it into
octaves (`PitchConverter::Pitch79ToOctaveOffset`) → that CV drives both oscillators.

**This is why the plugin implements the CLAP entry point itself instead of using DPF's
`dpf_add_plugin`.** DPF's generated CLAP wrapper ignores `CLAP_EVENT_MIDI2` outright and advertises
only the MIDI-1/MPE dialects, so a DPF-wrapped plugin can never receive a 7.9-pitch note. DPF is used
here purely as the GUI layer (`UIExporter` + NanoVG). MIDI-1.0 and CLAP-dialect note events are still
handled, so ordinary hosts work normally.

## Voice architecture

One note builds this graph (one copy per voice; wired by `VoiceManager::buildDefaultPatch`):

```
  pitchConverter1 ─┬─► slew1 ─┬─► triSaw1.pitchInput          (left oscillator)
                   │          └─► triSaw2.pitchInput          (right oscillator)
                   └─► lpf1.freqModInput                      (key tracking)

  triSaw1 ─┬─► levelRaw1 ────► combineOsc1.in1 ──► pan1 (−1) ─┐
           └─► comparator1 ─► levelPulse1 ──► combineOsc1.in2 │
  triSaw2 ─┬─► levelRaw2 ────► combineOsc2.in1 ──► pan2 (+1) ─┴─► combineMix ─► lpf1 ─► combineVca ─► final1
           └─► comparator2 ─► levelPulse2 ──► combineOsc2.in2                        ▲
  adsr1 ──► lpf1.freqModInput2 (filter envelope)             adsr2 ────────────────┘ (VCA, Multiply)
  lfo1 ──► triSaw1/2.modPitch (vibrato)     lfo2 ──► comparator1/2.threshCV (PWM)
```

- **Oscillator.** "One Tri/Saw/Pulse oscillator with L/R detune" is two `TriSawOsc` instances — left
  and right — each with its own fine tune, hard-panned by `pan1`/`pan2`, so the detune shows up as
  stereo width instead of as a monophonic beat. The `Comparator` turns each tri/saw into a
  variable-width pulse; the raw and pulsed paths are summed through two `Level` modules, which lets
  the Shape selector crossfade Tri/Saw ↔ Pulse **without re-patching the graph while audio runs**
  (a topology change would have to travel through `VoiceManager`'s lock-free command queue).
- **Filter.** `ButterworthLowPassFilter`, 24 dB/oct, cutoff in octaves (0 = A440). The raw pitch CV
  gives key tracking; `adsr1` sweeps the cutoff.
- **VCA.** `Combine` in Multiply mode — oscillator × `adsr2`. There is no separate VCA module; this
  is the same idiom PolygraphModularUnified's own default patch uses.
- **Poly/mono.** Poly = 10 voices. Mono = 1 voice driven through `VoiceManager`'s held-note stack, so
  a new note while another is held continues the same voice instead of stealing it. The two halves of
  the anti-click logic are the engine's mono note stack and the ADSR's legato mode: **Legato**
  (default in mono) never retriggers while the envelope is sustaining, so overlapping notes slide;
  **Retrigger** restarts the attack from the current level instead of snapping to zero. Poly uses
  Steal, which still legato-retriggers a repeated pitch on the same voice.

## Parameters

29 host parameters, grouped as the panel layout: Oscillator, Filter, Filter Env, Amp Env, LFO,
Global. The single source of truth is
[plugin/PlainOldSynthParams.h](plugin/PlainOldSynthParams.h) — it feeds the CLAP `params` extension,
the GUI, state save/load, and the presets.

## Layout

```
plugin/               DistrhoPluginInfo.h (identity + UI flags), PlainOldSynthParams.h (param table
                      and the voice graph's module addresses)
src/plugin.cpp        the CLAP plugin: host glue, the MIDI 2.0 path, and every parameter→engine write
src/plugin_gui.h      the C seam between the DSP and the GUI shell
src/UI/               PlainOldSynthUI.cpp (six panels of Knob/Dropdown/GroupBox) + the widget kit
src/Common/           vendored engine: UMP/MIDI, param tables, pitch conversion, portability shims
src/Module/           vendored engine modules: TriSaw, Comparator, ADSR(+Helper), LFO, LPF, Level,
                      Pan, Combine, Final, Slew, PitchConverter, Module core, ModuleFactory
src/Voice/            Voice, VoiceManager, VoiceStateManager, ModuleMap, pos_module_specs.cpp
Presets/              factory presets (JSON)
tools/gen_presets.py  bakes Presets/*.json into GeneratedPresets.h at build time
test/                 pos_smoke_test.cpp (engine, headless), pos_clap_host_test.cpp (the real .clap)
Resources/            logo PNG, embedded into the binary at build time
dpf/                  vendored DPF (ISC) — GUI layer only
external/             vendored CLAP headers, clap-wrapper, VST3 SDK
```

## Build

Needs CMake 3.21+, Ninja, a C++20 compiler, and Python 3 (for preset/resource codegen). On macOS
DPF's OpenGL UI builds out of the box; on Linux you also need the usual X11/GL dev packages
(`libgl1-mesa-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev`).

```sh
cmake -B build -G Ninja -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure      # see TESTING.md
cmake --build build --target install-clap        # symlink into ~/Library/Audio/Plug-Ins/CLAP
```

| Option | Default | Effect |
|---|---|---|
| `POS_BUILD_VST3` | `ON` | Build `PlainOldSynth.vst3` via clap-wrapper (uses the vendored `external/vst3sdk`). `install-vst3` symlinks it. |
| `POS_BUILD_AUV2` | `OFF` | Build `PlainOldSynth.component` via clap-wrapper. macOS only; clap-wrapper fetches Apple's AudioUnitSDK unless you pass `-DAUDIOUNIT_SDK_ROOT=/path/to/AudioUnitSDK`. `install-auv2` copies it (AU hosts do not reliably discover symlinks). |
| `POS_WERROR` | `OFF` | Warnings-as-errors for PlainOldSynth's own sources. |

`install-clap`, `install-vst3` and `install-auv2` are separate targets, so a plain
`cmake --build build` only compiles — nothing is installed behind your back.

A build can be validated with [`clap-validator`](https://github.com/free-audio/clap-validator):

```sh
clap-validator validate build/PlainOldSynth.clap   # 35 passed, 0 failed, 0 warnings, 9 skipped
auval -v aumu POSy nLKL                            # AU VALIDATION SUCCEEDED (AUv2 only)
```

## Presets

Factory presets are plain JSON in `Presets/`, keyed by the parameter **symbols** from
`PlainOldSynthParams.h`:

```json
{ "name": "Big Bass", "params": { "cutoff": -0.5, "fDecay": 0.22, "voiceMode": 0 } }
```

`tools/gen_presets.py` compiles them into `GeneratedPresets.h` at build time, so the plugin needs no
JSON parser and never reads the filesystem for presets. Applying a preset resets every parameter to
its default first and then applies the preset's values, so presets are self-contained. A stale or
misspelled key is ignored rather than being an error. **Adding a new preset file needs one
`cmake -B build -S .` re-run** for the glob to pick it up; editing an existing one does not.

## Not done yet

Deliberately deferred, per the initial spec:

- **LFO routing.** Two LFOs exist with rate/wave/depth, but their destinations are fixed: LFO 1 →
  oscillator pitch (vibrato), LFO 2 → pulse width (PWM). A routing matrix is the next step.
- **ADSR destinations.** The filter and amplifier envelopes are wired; routing an envelope to
  oscillator parameters is not.
- **Performance modulation.** Slew/glide exists, but pitch bend, mod wheel (CC #1), velocity and
  aftertouch are not yet routed. Note that the engine's `pm_midi` layer already decodes pitch bend
  and channel pressure, so this is plugin-side work.
- **Effects.** No global delay/reverb/enhance; PolygraphModularUnified's `GlobalModuleManager` was
  intentionally not vendored.
- **Preset discovery.** Factory presets are applied from the plugin's own dropdown; CLAP's
  `preset-discovery` factory is not implemented (clap-validator reports those tests as skipped).

## License

Proprietary — © Eric Kampman / Unlikelyware. See [LICENSE.txt](LICENSE.txt) and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
