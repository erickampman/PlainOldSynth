# Contributing to PlainOldSynth

Bug reports and pull requests are welcome. Build steps are in the README; this page is the rules the
code depends on.

## Before each commit

```sh
ctest --test-dir build --output-on-failure
clap-validator validate build/PlainOldSynth.clap
```

The tests are `pos-smoke` and `pos-clap`. `pos-clap` loads the built `.clap` and plays it headless, sending MIDI 2.0, MPE and CLAP events itself, so it needs no MIDI hardware. They are silent. On macOS, if you touched the AUv2 build, also run
`ninja -C build install-auv2` and `auval -v aumu POSy nLKL`.

After rebuilding, quit and restart the host before listening: a running host keeps the plugin binary
it loaded first.

## Parameters: append, never insert

The CLAP parameter id is the parameter's index, and hosts save sessions and automation by id. So a
new parameter goes at the **end** of the list, and existing ones are never reordered or removed: that
would silently rewire everyone's saved projects. For each new parameter:
- add it to the `static_assert(symbolIs(...))` order guard in `plugin/PlainOldSynthParams.h`;
- if it shapes the sound, add a line for it to every `Presets/*.json`, edited as text so the files aren't reformatted.

MIDI controller setup (MPE on/off and the like) stays out of presets.

## MIDI 2.0 and MPE

MIDI 2.0 per-note messages and CLAP note expressions are the main path. MPE is accepted as a
compatibility input (an Off/On switch, lower zone); nothing in this family of projects sends MPE.

## Threads

`state_load` and `pos_preset_apply` (the editor's Preset menu) run on the main thread while
`process()` may be running. They store the values only; the audio thread applies them at the start
of the next block (or `activate()` does). Never rebuild the voice graph from the main thread.

## Dependencies

The submodules are pinned (`tools/fetch_deps.sh`). Don't commit changes inside them. A fix needed in
clap-wrapper goes in `patches/` as a patch file, which `fetch_deps.sh` applies, so `external/clap-wrapper`
showing a local change is expected. DPF and pugl are Unlikelyware's forks; changes go to those repos
first and come here as a new pin.

## Drawing (NanoVG)

NanoVG fills non-convex paths through the stencil buffer and assumes it starts at zero, but DPF clears
only colour and depth. On Linux a stale stencil shows as speckles. Any widget that fills a non-convex
shape (a filled waveform, a polygon) must set `glStencilMask(0xff)` and clear the stencil before
drawing. `EnvelopeGraph::onNanoDisplay` (`src/UI/EnvelopeGraph.hpp`) does this. Convex fills (rectangles, rounded rectangles, circles) and strokes are
unaffected.

## Rules learned the hard way

- **Panels before their controls.** Declare each `GroupBox` member before the controls it parents.
  Members are destroyed in reverse order, so a panel freed first crashes its children's destructors
  when the editor closes. The host test only calls the GUI's `create()`, so only a real host catches
  this.
- **Every sound parameter in every preset.** The preset loader leaves keys a preset doesn't mention
  alone, so a missing key keeps the previous preset's value.
- **Output events stay in time order** within a block.
- **Controller setup survives `reset()`.** Configuration a controller sends once (RPN 0, registered
  controller 7) must outlive a reset: some hosts reset on transport stop.
- **Test helpers:** a full-scale bend computes 2³², which wraps to 0 in a `uint32_t`; clamp it.
- **Pitch tests:** measure on the simplest waveform you can. Zero crossings with interpolation are
  exact; autocorrelation needs peak interpolation.

## Platform-specific code

- `src/UI/BackingScale_mac.mm` (Retina scale) and the AUv2 target are macOS-only.

## Layout

```
plugin/               DistrhoPluginInfo.h (identity + UI flags), PlainOldSynthParams.h (param table
                      and the voice graph's module addresses)
src/plugin.cpp        the CLAP plugin: host glue, the MIDI 2.0 path, and every parameter→engine write
src/plugin_gui.h      the C seam between the DSP and the GUI shell
src/UI/               PlainOldSynthUI.cpp (ten panels of Knob/Dropdown/GroupBox/EnvelopeGraph) + the widget kit
src/Effects/          ModDelay.hpp: the post-voice-sum Flanger / Chorus / Delay
src/Common/           vendored engine: UMP/MIDI, param tables, pitch conversion, portability shims
src/Module/           vendored engine modules: TriSaw, Comparator, ADSR(+Helper), LFO, LPF, Level, Pow2CircBuff,
                      Pan, Combine, CVMixer, CCConverter, Final, Slew, PitchConverter, Module core,
                      ModuleFactory
src/Voice/            Voice, VoiceManager, VoiceStateManager, ModuleMap, pos_module_specs.cpp
Presets/              factory presets (JSON)
tools/gen_presets.py  bakes Presets/*.json into GeneratedPresets.h at build time
test/                 pos_smoke_test.cpp (engine, headless), pos_clap_host_test.cpp (the real .clap)
Resources/            logo PNG, embedded into the binary at build time
dpf/                  DPF (ISC), submodule — GUI layer only
external/clap         CLAP headers, submodule
external/clap-wrapper VST3/AUv2 wrappers, submodule (v0.16.0 + local AUv2 GUI patch)
external/vst3sdk      VST3 SDK 3.8.0, submodule
patches/              local patches carried against the dependency submodules
tools/fetch_deps.sh   initialises all of the above after a clone
```
