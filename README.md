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

### Pitch bend: channel and per-note

Every voice has a `PitchBendConverter` (`src/Module/PitchBendConverterModule.cpp`) feeding both
oscillators' second pitch-mod input. That input comes **after** glide, so portamento never smears a bend.
It sums two bends:

- **Channel pitch bend** (MIDI 1.0 or 2.0), **±2 semitones** unless a controller sets the range with
  RPN 0 (pitch bend sensitivity; MIDI 1.0 CC 101/100 + 6/38, or the MIDI 2.0 registered-controller
  message). Each voice remembers the latest bend of every channel and applies the one its own note was
  played on. With everything on one channel that is ordinary pitch bend; with one note per channel,
  each note follows its own channel. For MPE controllers, turn on the **MPE** switch (below).
- **MIDI 2.0 per-note pitch bend** (UMP status `0x6`), **±48 semitones** (the MPE convention). It bends
  only the note it names: two held notes bend independently.

Bend changes ramp over 2 ms so fast controller moves don't step audibly; a fresh note starts at its
bent pitch instead of gliding to it. Per-note bend resets on every note-on and on a per-note
management message with the Reset flag. A per-note bend sent *before* its note-on is dropped, because
no voice owns that note yet. Neither range is a host parameter yet (the module's `quarterTones` and
`perNoteSemitones` params hold them).

### MPE

The **MPE** switch (Global panel, off by default) is for MIDI 1.0 MPE controllers: Seaboard,
LinnStrument, Osmose, Continuum, and hosts that pass MPE through as MIDI 1.0. It assumes the standard
**lower zone**: channel 1 is the master channel and channels 2–16 are member channels, one note each.
With it on:

- **Member-channel bend is ±48 semitones** (the MPE default) instead of ±2, so a slide moves as far as
  the controller intends. RPN 0 on any member channel changes the range of all of them, as MPE
  specifies.
- **The master channel applies to every note.** Its bend (±2) is added to each note's own bend, so the
  controller's pitch wheel moves the whole zone. Its channel pressure and CC74 feed the **Pressure**
  and **Timbre** sources too: each note takes the higher of its own value and the master's.
- Sustain and the mod wheel already respond on any channel, so they work from the master channel
  either way.

Not handled: the MPE Configuration Message (RPN 6) that announces a zone layout, and the upper zone
(master channel 16). The switch, not the controller, decides the layout. None of this is needed for
MIDI 2.0 per-note messages or CLAP/VST3 note expressions, which carry per-note data directly.

**A note is note number + channel + group**, or its **CLAP note id** when both sides have one.
Voice matching used to compare only the note number. Now the same key on two channels, or re-struck
with a new note id, is two voices with independent per-note state. A CLAP note expression that names
only a note id (key -1) still finds its voice. Note-offs the engine generates itself (sustain-pedal
release, all-notes-off) are built from the voice's own note-on so they match. In mono mode, per-note
expression for a held-but-silent note in the stack doesn't move the sounding note.

### CLAP note expressions and VST3 note expression

Hosts using the CLAP dialect (Bitwig, for one) send per-note expression as `CLAP_EVENT_NOTE_EXPRESSION`
rather than as UMP. `pm::fromClapNoteExpression` (`src/Common/clap_ump.h`) turns each one into the
equivalent MIDI 2.0 per-note message, so there is one engine path:

| CLAP expression | becomes | used by |
|---|---|---|
| Tuning (semitones) | per-note pitch bend, scaled by the engine's current per-note range | oscillator pitch |
| Pressure | polyphonic key pressure | the **Pressure** modulation source |
| Brightness | registered per-note controller #74 | the **Timbre** modulation source |

Tuning beyond the ±48 per-note range is clamped (CLAP allows ±120). Volume, pan, vibrato and
expression are dropped, as is an expression that names neither a note id nor a key.

The VST3 build gets the same: clap-wrapper converts VST3 note expression into CLAP note expressions.
By default it offers hosts only Pressure, so the plugin implements clap-wrapper's
`CLAP_PLUGIN_AS_VST3` extension to offer Tuning and Brightness as well. AUv2 has no per-note
expression.

## Voice architecture

One note builds this graph (one copy per voice; wired by `VoiceManager::buildDefaultPatch`):

```
  pitchConverter1 ─┬─► slew1 ─┬─► triSaw1.pitchInput          (left oscillator)
                   │          └─► triSaw2.pitchInput          (right oscillator)
                   └─► lpf1.freqModInput                      (key tracking)
  pitchBend1 ──────────────► triSaw1/2.modPitch2              (channel + MIDI 2.0 per-note bend)

  triSaw1 ─┬─► levelRaw1 ────► combineOsc1.in1 ──► pan1 (−1) ─┐
           └─► comparator1 ─► levelPulse1 ──► combineOsc1.in2 │
  triSaw2 ─┬─► levelRaw2 ────► combineOsc2.in1 ──► pan2 (+1) ─┴─► combineMix ─► lpf1 ─► combineVca ─► final1
           └─► comparator2 ─► levelPulse2 ──► combineOsc2.in2                        ▲
  adsr1 ──► lpf1.freqModInput2 (filter envelope)             adsr2 ────────────────┘ (VCA, Multiply)
  lfo1 ──► triSaw1/2.modPitch (vibrato)     lfo2 ──► comparator1/2.threshCV (PWM)
```

- **Oscillator.** "One Tri/Saw/Pulse oscillator with detune" is two `TriSawOsc` instances — left
  and right — hard-panned by `pan1`/`pan2`. The single **Detune** control is written to both with
  opposite sign (right `+N` cents, left `−N` cents), so it shows up as stereo width instead of as a
  monophonic beat. **Shape** is a two-way choice, not three: triangle and saw are the same oscillator
  read at different **Peak** values (0.5 = symmetric triangle, ~1.0 = saw), so the tri↔saw morph
  belongs to the Peak knob and Shape only decides which path is audible. The `Comparator` turns each
  tri/saw into a variable-width pulse; the raw and pulsed paths are summed through two `Level`
  modules, which lets Shape crossfade Tri/Saw ↔ Pulse **without re-patching the graph while audio
  runs** (a topology change would have to travel through `VoiceManager`'s lock-free command queue).

  **Pitch is one continuous semitone value**, not a stepped parameter, and there is deliberately no
  separate Fine knob: `TriSawOsc` has a single continuous tuning (`pitch`) plus a separate cents param
  for the per-oscillator spread, so Pitch drives the former and Detune the latter — Detune therefore
  stays a stereo spread instead of becoming a second tuning offset. Precision comes from the
  **fine-drag modifier** (see below) rather than from another knob. **Self Mod** feeds the
  oscillator's own output back into its phase (DX7-style), which adds harmonics; the engine clamps the
  combined depth to ±0.9 because the cornered tri/saw breaks into noise past that. The oscillator
  section's output level is no longer a knob; it is fixed in `initPatchConstants`.
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

53 host parameters, grouped as the panel layout: Oscillator, Filter, Envelopes (both ADSRs, one row
each), LFO, Global, **Effect**, and three **Modulation** sections. The single source of truth is
[plugin/PlainOldSynthParams.h](plugin/PlainOldSynthParams.h) — it feeds the CLAP `params` extension,
the GUI, state save/load, and the presets.

The panels are **not** a uniform grid, because they do not all need the same height. Each panel
declares a `kPanelRows` x `kPanelCols` grid and every parameter carries the `PanelCell` it occupies,
so a column can hold two stacked controls: Global puts **Voices over Mono Mode** in column 0 and
**MPE** in column 1, while Glide/Level/Spread span both rows beside them, and each modulation section stacks its three columns
the same way. Panels are drawn in three bands (`kPanelBand`) — the two-knob-row panels on top
(Oscillator, Envelopes, LFO), the single-row ones in the middle (Filter, Global), and the modulation
sections along the bottom. The **Effect** panel is the one exception: a column down the right-hand side
spanning the middle and bottom bands (`kPanelBandSpanning`), as wide as a Mod section would be in a row
of four — it took the slot of a fourth Mod section.

`kPanelRowUnits` says how many knob-rows tall each panel is — normally `kPanelRows`, but Global's
second row holds only dropdowns (a dropdown needs ~42px against a knob's ~104px), so both fit inside
one knob row. Band heights follow from those units, so giving a panel an extra row there re-flows the
whole window; nothing else needs editing. Rows are capped rather than stretched to fill the window,
which keeps knobs full size and leaves each band's spare height as headroom. Band widths are divided
by each panel's column count, so every control ends up about the same size.

The LFOs offer Sine / Tri / Square only. The engine can also produce an Impulse wave, but this synth
does not expose it.

**Fine drag.** Hold **Control** (or **Command**) while dragging any knob and it moves ten times more
slowly. DPF's knob handler already does the Control half — `Knob::onMotion` maps Command onto the same
flag, since that is the usual modifier on macOS. This is what makes a continuous parameter such as
Pitch usable without a separate Fine knob. **Shift**-click resets a knob to its default (DPF's own
binding, kept as-is); double-clicking does the same. `ParamSpec::integer` is what marks a parameter
*stepped* — it is about host behaviour (it sets `CLAP_PARAM_IS_STEPPED`, so hosts snap automation to
whole values) as well as the readout, which is why Pitch must not set it.

### Effect

One stereo delay line with an LFO on its read position, run once on the summed voices
([src/Effects/ModDelay.hpp](src/Effects/ModDelay.hpp), adapted from PolygraphModularUnified's
`GlobalDelayModule`). **Mode** picks the delay range, which is all that separates the effects:

| Mode | Time range | Notes |
|---|---|---|
| Off | — | true bypass (the default, so older presets are unchanged) |
| Flanger | 0.5–10 ms | Depth sweeps ~0.1x..1.9x the time; negative **Feedback** is the hollow flange |
| Chorus | 5–40 ms | Depth swings up to half the time |
| Delay | 20 ms–2 s | Depth adds up to 5 ms of tape wobble |
| Sync Delay | 1/32 … 1/1 (dotted, triplet) | follows the host tempo (120 BPM if it reports none) |

**Time** is stored as 0..1 and read through the mode, so the knob, the host's parameter text and the
DSP share one mapping (`fxTimeMs` / `formatFxTime` / `parseFxTime`). **Spread** is stereo width: in
Flanger/Chorus it is the right LFO's phase offset (0–180°; the split is what makes them wide), in the
Delay modes it blends a plain stereo delay into ping-pong (echoes alternate L, R, L… on the beat). **Tone** is a one-pole low-pass on the
delayed signal (so it darkens every repeat in the loop); fully up is no filtering. **Mix** 0.5 is dry
and wet both at unity. Delay length glides (~60 ms) rather than jumping, and the buffers are flushed on
a mode change or reset so a stale tail never replays.

### Modulation sections

Each of the three sections is one engine `CVMixer`, and reads as three columns:

| Column | Controls |
|---|---|
| inputs | **In A** (None / LFO 1 / ADSR 1) above **In B** (None / LFO 2 / ADSR 2 / CC #1 / Pressure / Timbre) |
| levels | **Level A** above **Level B**, -1..1 (0 = off, negative inverts) |
| output | **Mode** (Add / Multiply) above **Dest** (Off / Osc Pitch / Pulse Width / Peak / Cutoff / Self Mod) |

`Dest` is a **graph edit**, not a parameter write: `applyModRouting()` in
[src/plugin.cpp](src/plugin.cpp) recomputes the whole modulation patch from the parameter values
whenever anything routing-related changes, through `VoiceManager`'s lock-free connection queue. A
module input takes one source, so a destination belongs to exactly one section — pointing a second
section at one already in use switches the earlier section back to **Off** rather than letting the
two silently fight.

The destination modules' own modulation levels are pinned to full scale in `initPatchConstants()`,
because the knob that used to own each one (Env Amount, LFO 1/2 Depth) is gone: a section's input
levels are the depth now. **CC #1** is the mod wheel, through a ported `CCConverterModule` whose
`activeCount` is bumped at patch build so it keeps receiving MIDI even while no section selects it.

**Pressure** and **Timbre** are per-note: every voice has its own, so pressing one key of a chord
moves only that note's destination. Each one follows the channel its note was played on, so an
MPE-style controller (one note per channel) works through the channel messages alone:

| Source | Channel-wide value (on the note's channel) | Per-note value (overrides it) |
|---|---|---|
| Pressure | channel pressure | poly pressure (MIDI 1.0 or 2.0), CLAP/VST3 Pressure expression |
| Timbre | CC74 | MIDI 2.0 registered per-note controller #74, CLAP/VST3 Brightness expression |

Pressure is the parent engine's `ChannelPressureModule`, extended. Timbre is a second
`CCConverterModule` on CC74 with a new **Per Note** mode. CC #1 leaves that mode off, so it still
responds to the mod wheel on any channel. A note's per-note value resets on note-on and on
per-note management Reset, so a fresh note starts from its channel's value. Both are unipolar
(0..1), like CC #1, and de-zippered over 2 ms (`src/Module/ControlRamp.hpp`, shared with pitch
bend).

**The source polarity decides the quadrants.** ADSR 1/2, CC #1, Pressure and Timbre are unipolar (0..1), so a section
using one of those can only *add* to its destination — a two-quadrant response. LFO 1/2 are bipolar
(−1..+1), so those reach both directions. That choice is already in the source menu, which is why no
destination restricts its own range.

**Self Mod accepts either sign.** The engine clamps the *combined* depth — the Self Mod knob plus
anything patched to the destination — symmetrically at ±0.9, because the cornered tri/saw breaks into
noise at roughly 0.98 either way; negative depth is a supported, bounded region rather than an
accident. What it *sounds* like depends on the waveform, and this is measured rather than assumed: a
symmetric triangle is its own time-mirror, so the sign is inaudible there (largest step 0.19851 at
−0.6 against 0.19861 at +0.6), while a saw is not, so the sign is a real timbre change (step 1.30359
against 0.55869, RMS 0.3694 against 0.2058). Both stay bounded. That is the argument for leaving it
four-quadrant: restricting it to two would remove a usable direction on saw shapes and gain nothing
on triangles.

## Layout

```
plugin/               DistrhoPluginInfo.h (identity + UI flags), PlainOldSynthParams.h (param table
                      and the voice graph's module addresses)
src/plugin.cpp        the CLAP plugin: host glue, the MIDI 2.0 path, and every parameter→engine write
src/plugin_gui.h      the C seam between the DSP and the GUI shell
src/UI/               PlainOldSynthUI.cpp (ten panels of Knob/Dropdown/GroupBox) + the widget kit
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

## Build

Dependencies are **git submodules** pinned to exact commits. After cloning, fetch them
once:

```sh
git clone <this repo> PlainOldSynth
cd PlainOldSynth
tools/fetch_deps.sh          # submodule init at pinned commits + apply patches/
```

`tools/fetch_deps.sh` initialises `dpf` (plus its nested `pugl`),
`external/clap`, `external/clap-wrapper`, and `external/vst3sdk` — for the last of
those it initialises only `base`, `cmake`, `pluginterfaces` and `public.sdk`. The
VST3 SDK declares three further nested submodules (`doc`, `tutorials`, `vstgui4`)
that this project does not need, so they are deliberately left empty; a bare
`git submodule update --init --recursive` would pull them too.

`patches/` holds the one local change carried against a dependency: clap-wrapper's
AUv2 view never calls `gui->show()`, so a CLAP plugin whose `show()` callback does
load-bearing work (as this one does — it registers the host timer that pumps the
DPF render loop) draws nothing under an AUv2 host. `tools/apply_dep_patches.sh`
re-applies it idempotently and is called by `fetch_deps.sh`.

Needs CMake 3.21+, Ninja, a C++20 compiler, and Python 3 (for preset/resource codegen). On macOS
DPF's OpenGL UI builds out of the box.

### Linux (x86-64)

The `.clap` and `.vst3` targets are portable; only the AUv2 target is Apple-only. The UI uses DPF's
pugl/NanoVG backend, so it needs the X11 and OpenGL development headers:

```sh
sudo apt-get install -y build-essential cmake ninja-build pkg-config python3 \
    libgl1-mesa-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev \
    libxinerama-dev libxcb1-dev libx11-xcb-dev libxcb-util-dev \
    libxkbcommon-dev libxkbcommon-x11-dev libfontconfig1-dev
```

Those are the packages CI installs (see [`.github/workflows/linux.yml`](.github/workflows/linux.yml),
which builds and runs the tests on `ubuntu-24.04`). **That workflow is the only place the Linux build
is genuinely verified** — the development machine is macOS with no Linux toolchain, container runtime
or cross-compiler, so Linux-specific breakage is exactly what it exists to catch.

`install-clap` and `install-vst3` pick the per-platform plugin directory themselves: `~/Library/Audio/
Plug-Ins/{CLAP,VST3}` on macOS, `~/.clap` and `~/.vst3` on Linux.

### All platforms

```sh
cmake -B build -G Ninja -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure      # see TESTING.md
cmake --build build --target install-clap        # macOS: ~/Library/.../CLAP, Linux: ~/.clap
```

| Option | Default | Effect |
|---|---|---|
| `POS_BUILD_VST3` | `ON` | Build `PlainOldSynth.vst3` via clap-wrapper (uses the vendored `external/vst3sdk`). `install-vst3` symlinks it. |
| `POS_BUILD_AUV2` | `OFF` | Build `PlainOldSynth.component` via clap-wrapper. **macOS only** — configuring it elsewhere is a hard error rather than a confusing unknown-command failure, because AUv2 is an Apple format. clap-wrapper fetches Apple's AudioUnitSDK unless you pass `-DAUDIOUNIT_SDK_ROOT=/path/to/AudioUnitSDK`. `install-auv2` copies it (AU hosts do not reliably discover symlinks). |
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

- **Other performance modulation.** Slew/glide works, and CC #1 (mod wheel) is available to every
  section's second input, as are Pressure and Timbre, and pitch bend (channel and MIDI 2.0 per-note)
  drives the oscillators. Velocity is not routed. MPE covers the lower zone via the panel switch; the
  MPE Configuration Message and the upper zone are not handled.
- **Modulating the modulators.** A section can only target the oscillator, the pulse width and the
  filter cutoff; it cannot yet drive an LFO's rate or a section's own level.
- **More effects.** One effect slot (Flanger / Chorus / Delay). No reverb or enhance;
  PolygraphModularUnified's `GlobalModuleManager` was intentionally not vendored.
- **Preset discovery.** Factory presets are applied from the plugin's own dropdown; CLAP's
  `preset-discovery` factory is not implemented (clap-validator reports those tests as skipped).

## License

Proprietary — © Eric Kampman / Unlikelyware. See [LICENSE.txt](LICENSE.txt) and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
