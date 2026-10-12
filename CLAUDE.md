# PlainOldSynth

A CLAP subtractive synth (with VST3 and, on macOS, AUv2 builds through clap-wrapper) on the vendored
PolygraphModular engine (`src/Common`, `src/Module`, `src/Voice`), a hand-written CLAP entry point
(`src/plugin.cpp`), and DPF used only for the GUI. Start with README.md and TESTING.md: README's
"MIDI 2.0" section is the reference the sibling synths port from, so keep it accurate.

Sibling synths, on the same engine and the same raw-CLAP layout. Paths: `~/Dev-Audio/<name>` on the Mac,
`~/dev/dev-clap/<name>` on the Linux PC (as `docs/BuildNotes.md` in IFSynth and StruckSynth shows):
- **StruckSynth** (`~/Dev-Audio/StruckSynth`, branch `main`)
- **IFSynth** (`~/Dev-Audio/IFSynth`, branch **`master`**)

## Working with the author

- Commit to `main` directly. Never open pull requests. Ask before pushing.
- Ask before anything UI-visible: new parameters, panel controls, dropdown entries.
- Parameters are **appended**, never inserted: CLAP parameter ids are the indexes. Each new one needs
  its `symbolIs` check in `plugin/PlainOldSynthParams.h`. Controller setup (MPE) stays out of presets.
- Before each commit: `ctest --test-dir build` and `clap-validator validate build/PlainOldSynth.clap`.
- After a rebuild the author must **quit and restart the host** (StageHand, REAPER, Bitwig): a running
  host keeps the binary it first loaded. Say so when asking them to listen.
- `external/clap-wrapper` shows a local edit to its AUv2 view: that is `patches/` applied by
  `tools/fetch_deps.sh`, not uncommitted work. Never commit inside a submodule.

## Cross-project policy

- **MPE**: the synths accept MPE as a compatibility input (an Off/On switch, lower zone) and never send
  it. Nothing in StageHand, Umpire or KeyboardEDO originates MPE. MIDI 2.0 per-note messages and CLAP
  note expressions are the preferred path.
- MIDI 2.0 spec on the Mac: `~/Documents/MIDI/MIDI_2-0_Specification_Version_1-0 2/` (M2-104 is v1.0,
  Feb 2020; registered controller 7 is from a later revision and not in it).

## The MIDI 2.0 / MPE / CLAP work (2026-10-09..11): done

Most of it started here and was ported to StruckSynth (2026-10-11) and IFSynth (2026-10-12); some
fixes went the other way (registered controller 7, the per-note range, Pitch 7.25, velocity-0
note-ons, CC 123 / 120, released notes, the host test's pitch estimator). Commits, oldest first:
`793a139` per-note bend, note + channel + group matching · `623a64f` CLAP note expressions and ids, VST3
note expression · `a65a645` Pressure and Timbre sources · `d60a932` Retina / multi-monitor editor size ·
`f201e8c` no `rescan` from the audio thread · `ad0426c` kParams order guard · `5dfb576` `714c808` MPE +
RPN 0 · `595fd50` Velocity (loudness dropdown + source) and MIDI 1.0 → 2.0 scaling · `3820e90`
velocity-0 note-on · `a3b94c7` CC 123 / 120 · `69b455e` RC 7, per-note range, Pitch 7.25 · `228facb`
released notes · `559b80b` NOTE_END + voice-info · `62015bd` host-test pitch estimator.

Known follow-up: `state_load` (in `src/plugin.cpp`) calls `applyAllParams` on the main thread, which
can rewire the voice graph under a running `process()`. IFSynth (`9cab332`) only stores the values
there and lets the next block apply them; port that.

## On Linux

This repo's Linux CI (`.github/workflows/linux.yml`, ubuntu-24.04) has passed throughout, building the
CLAP and VST3 targets and running `pos-smoke` and `pos-clap` (which loads the built `.clap` and plays it
headless). That is the only Linux verification so far: no one has opened the editor on Linux.

A checkout from before 2026-10-09 is far behind. To catch up:

1. `git pull`, then `tools/fetch_deps.sh` (not a bare `git submodule update --recursive`: see README).
2. **Configure a fresh build directory** (`rm -rf build`), then build, `ctest`, and
   `cmake --build build --target install-clap` (and `install-vst3`): they re-point `~/.clap` and
   `~/.vst3`. Packages: README's "Linux (x86-64)" section.
3. `clap-validator validate build/PlainOldSynth.clap` (35 passed, 0 failed on the Mac).

Never checked on Linux, so check these by hand and record what you find in this file:
- The editor under X11: correct size on load, resizing, clicks landing on the right controls.
- Sound and MIDI in **StageHand** (`~/dev/dev-audio/StageHand`; Linux-first: JACK through PipeWire, the
  ALSA sequencer) and REAPER,
  CLAP and VST3.

macOS-only pieces: `src/UI/BackingScale_mac.mm` (Retina scale) and the AUv2 target.

Test MIDI sources: mpe-sender (`~/Dev-MIDI/mpe-sender` on the Mac) is macOS-only (CoreMIDI), and KeyboardEDO has no Linux MIDI
output until uw-core's ALSA output (its roadmap milestone 3b). On Linux use a hardware controller,
StageHand, or the headless `pos-smoke` and `pos-clap` tests, which send MIDI 2.0, MPE and CLAP events.
