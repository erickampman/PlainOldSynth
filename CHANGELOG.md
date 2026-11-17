# Changelog

All notable changes to PlainOldSynth. Versions follow the git tags; the format is
[Keep a Changelog](https://keepachangelog.com/).

## [Unreleased]

### Changed

- Help opens the help page in a window of the plugin's own, not in the web browser. Nothing is
  written to the temp folder, and on Linux no web server runs in the host any more.

## [0.9.0] - 2026-11-14

First public preview.

### Added

- Classic subtractive synth: one stereo-detuned Tri/Saw/Pulse oscillator, 24 dB/oct low-pass, two
  ADSRs, two LFOs, glide, poly or mono.
- A flanger, chorus or delay.
- MIDI 2.0 notes taken directly, so a Pitch 7.9 note plays its exact pitch; MPE and CLAP note
  expressions too.
- CLAP plugin for Linux (x86-64) and macOS 26 or later (Apple Silicon).

### Known gaps

- See the README's Known gaps section.
