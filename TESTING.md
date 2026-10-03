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
- a percussive envelope decays to silence and the voice frees itself once released
- three held notes use three voices; mono holds one and keeps sounding through its note stack

### `pos-clap-host-test` — the real plugin

`dlopen`s the built `.clap` and drives it over the CLAP C API with a stub host, which is the only
check that covers the parts the engine test cannot reach:

- `clap_entry` is exported, the factory and descriptor are present
- the note port **advertises and prefers `CLAP_NOTE_DIALECT_MIDI2`** while still accepting MIDI 1.0
- a UMP NoteOn with a 7.9-pitch attribute produces audio at the right microtonal pitch
- a MIDI 1.0 note and a bare CLAP note event each produce the right pitch on their own instance

It installs a controlled patch through the `params` extension (triangle, filter wide open, no
detune, no modulation) before measuring, and uses a fresh plugin instance per measurement — an
instrument with a release tail is not a clean slate, and the engine has no MIDI "all notes off"
handler to force one. It reports **two** independent pitch estimates (zero crossings and
autocorrelation); a disagreement means the measurement is suspect, not the synth.

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
