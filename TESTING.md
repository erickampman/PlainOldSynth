# Testing PlainOldSynth

```sh
ctest --test-dir build --output-on-failure
```

Two headless tests, no DAW and no GUI required:

### `pos-smoke-test` — the engine

Links only the vendored engine (`pos_framework`) and drives `VoiceManager` directly, so it runs
anywhere and fails fast when the DSP itself breaks:

- a note-on produces audible output, and note-off releases to silence
- a MIDI 1.0 note sounds at the expected equal-tempered pitch (measured by interpolated zero
  crossings; C4 comes out at 261.625557 Hz against 261.625565 Hz)
- **a MIDI 2.0 7.9-pitch note sounds at the expected microtonal pitch** (269.291772 Hz against
  269.291780 Hz) — this is the engine half of the MIDI 2.0 requirement
- **MIDI 2.0 per-note messages decode**: per-note pitch bend, registered per-note controller (index from
  the low byte, unlike CC) and per-note management flags; bend centre `0x80000000` maps to exactly 0
- **channel pitch bend**: +1 semitone at the default ±2 range (277.182638 Hz against 277.182631 Hz),
  sent before the note; a bend on channel 2 leaves a channel-1 note alone
- **per-note pitch bend** (measured by single-bin DFT, since two notes sound at once): +1 semitone on
  C4 of a C4+G4 chord moves C4 to C#4 and leaves G4 alone; C4 on channels 1 and 2 takes two voices and
  bends/releases independently; bend resets on the next note-on and on per-note management Reset; in
  mono, bending a held-but-silent note leaves the sounding note alone
- engine-generated note-offs (sustain pedal up, all-notes-off) release notes on channels other than 1
- a percussive envelope decays to silence and the voice frees itself once released
- three held notes use three voices; mono holds one and keeps sounding through its note stack
- **Peak is the tri↔saw morph**: a saw steps once per cycle and a triangle never does, so the largest
  sample-to-sample step separates them by more than 20x (0.53 against 0.015)
- **Peak is timbre, not level.** Swept over C2/C4/C6: the symmetric triangle reaches full scale (peak
  0.54–0.56 against the chain's 0.56 gain, i.e. no dip at Peak 0.5), and triangle, saw and ramp all
  carry the same RMS to within 3%. Their *peak* amplitudes do differ — the saw reads ~15% higher
  (0.63–0.65) because the polyBLAMP band-limiting that rounds the triangle's corners turns the saw's
  near-vertical edge into a Gibbs-style overshoot. That is band-limiting, not a level error, so the
  check asserts full scale on the triangle and equal energy across all three rather than equal peaks.
- **Self Mod adds harmonics**: it feeds the oscillator's own output back into its phase, so on a
  symmetric triangle the largest sample-to-sample step rises from 0.015 to 0.126 at depth 0.5
- **the sign of Self Mod**: at ±0.6 a symmetric triangle measures the same (step 0.19851 against
  0.19861, because a triangle is its own time-mirror), while a saw does not (step 1.30359 against
  0.55869, RMS 0.3694 against 0.2058). Both signs stay bounded and finite, so the engine's symmetric
  ±0.9 clamp is doing its job and negative depth is a usable region rather than a fault

### `pos-clap-host-test` — the real plugin

`dlopen`s the built `.clap` and drives it over the CLAP C API with a stub host, which is the only
check that covers the parts the engine test cannot reach:

- `clap_entry` is exported, the factory and descriptor are present
- the note port **advertises and prefers `CLAP_NOTE_DIALECT_MIDI2`** while still accepting MIDI 1.0
- a UMP NoteOn with a 7.9-pitch attribute produces audio at the right microtonal pitch
- a MIDI 1.0 note and a bare CLAP note event each produce the right pitch on their own instance
- a UMP per-note pitch bend of +3 semitones moves C4 to D#4 (311.126977 Hz against 311.126984 Hz),
  which proves the plugin wires the bend input at full depth
- **the single Detune control splits the channels symmetrically**: at +7 cents the right channel
  measures 262.686 Hz and the left 260.570 Hz, matching `261.626 × 2^(∓7/1200)` to sub-milliHz. It
  runs at `spread = 1.0`, because `spread` is a mid/side width — at the value the pitch checks use
  (0) the output collapses to mono and *both* oscillators appear in *both* channels
- **modulation destinations are exclusive**: two sections are pointed at the same destination, and
  the earlier one is read back through the params extension and must have been switched to `Off`
- **a modulation section audibly moves its destination**: LFO 1 → oscillator pitch at full level
  raises the spread of successive zero-crossing intervals from 0.0000 (all sections Off) to ~0.94,
  measured on two fresh instances of the same patch
- **Self Mod is reachable as a destination**: with every section Off the largest step is 0.021; with
  `adsr1 → Self Mod` at full level it is 0.750, so a section really does drive the oscillator's own
  feedback (the engine adds it to the Self Mod knob before clamping)
- **Glide never reads as NaN**: its minimum is 0, so a logarithmic law through zero is undefined —
  which is what produced the `nan s` readout in the GUI. `value_to_text` is checked at 0, 0.25 and 5 s
- **Pitch is continuous, not stepped**: `get_info` is checked for the absence of
  `CLAP_PARAM_IS_STEPPED` (a host would otherwise snap automation to whole semitones), and setting
  Pitch to `+0.5 st` puts the fundamental at 269.292 Hz — the quarter tone between C4 and C#4, which
  it only reaches if the value was never rounded

It installs a controlled patch through the `params` extension (Peak pinned to 0.5 for a symmetric
triangle, filter wide open, no detune, and **every modulation destination set to `Off`**) before
measuring, and uses a fresh plugin
instance per measurement — an
instrument with a release tail is not a clean slate, and the engine has no MIDI "all notes off"
handler to force one. It reports **two** independent pitch estimates (zero crossings and
autocorrelation); a disagreement means the measurement is suspect, not the synth.

Note that the presets are baked by `tools/gen_presets.py`, which deliberately does not know the
parameter table: an unknown symbol is ignored at apply time rather than failing the build, so a
preset key that no longer exists is silent. If a preset change does not seem to take effect, check
its keys against [`plugin/PlainOldSynthParams.h`](plugin/PlainOldSynthParams.h) — the symbols are the
`ParamSpec::symbol` column.

### Validator

```sh
clap-validator validate build/PlainOldSynth.clap
```

Currently **35 passed, 0 failed, 0 warnings, 9 skipped**. The skips are all "not implemented, on
purpose": preset discovery, 64-bit audio, audio-input-only suites (this is an instrument with no
audio inputs). `pos-clap-host-test` covers the MIDI 2.0 behaviour, which the validator does not
check.

The AUv2 wrapper validates cleanly too:

```sh
auval -v aumu POSy nLKL        # AU VALIDATION SUCCEEDED
```

which includes its render, parameter and MIDI tests. The VST3 wrapper is built by clap-wrapper from
the same `.clap` (a target/format change only — no plugin source is compiled differently per
format).

## Manual checks worth doing in a host

- The GUI opens embedded, resizes, and every control moves without audio dropouts; the preset
  dropdown applies also refresh every control.
- Host automation of a knob is recorded (drag a knob while the host is recording) — the UI edits go
  through real CLAP gesture/value events.
- Saving and reloading a project restores the sound (CLAP `state`, a small line-based text format
  keyed by parameter symbol).
- In a MIDI-2.0 host, play a microtonal scale (e.g. a MPE/MIDI 2.0 keyboard sending 7.9 pitch).
- **A modulation section edits the graph, so check it as a graph edit.** Set a section's `Dest` while
  a note is held: the destination should start moving without a click, and switching it away should
  stop cleanly. Point a second section at the same destination and the first one's menu should snap
  back to `Off` — that is the exclusivity rule, and it is what keeps the display honest.
- **CC #1 (mod wheel).** Choose `CC #1` as a section's `In B`, give it level, and sweep the mod wheel:
  it should modulate with no note retrigger. The `CCConverter`'s `activeCount` is bumped at patch
  build precisely so this works even before any section selects it.
- **Window size.** The default is now 1200x750. A host that remembered the old 980x620 geometry may
  open it at the old size; the layout is ratio-locked and should reflow, but this is worth one look.
