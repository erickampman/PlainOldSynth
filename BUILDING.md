# Building PlainOldSynth

PlainOldSynth builds the same way on Linux and macOS: CMake + Ninja, git submodules, C++20. The
build makes the plugin `PlainOldSynth.clap`, the same plugin wrapped as `PlainOldSynth.vst3` (and,
on macOS if you ask for it, `PlainOldSynth.component`), and the tests.

## What you need

**macOS:** macOS 26 or later on Apple Silicon (arm64). Intel Macs aren't supported.

- The Xcode Command Line Tools: `xcode-select --install`. They bring clang, the macOS SDK, git and
  Python 3.
- [Homebrew](https://brew.sh), then `brew install cmake ninja`.
- Full Xcode isn't needed.

**Linux:** a C++20 compiler (GCC 11+ or Clang 14+), CMake 3.21+, Ninja, Python 3 and git. On Debian
or Ubuntu this installs them and every library the build needs:

```bash
sudo apt install build-essential cmake ninja-build pkg-config python3 git \
  libgl1-mesa-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxinerama-dev \
  libxcb1-dev libx11-xcb-dev libxcb-util-dev libxkbcommon-dev libxkbcommon-x11-dev \
  libfontconfig1-dev libasound2-dev libjack-jackd2-dev \
  libxfixes-dev libxi-dev libxss-dev libxtst-dev libwayland-dev wayland-protocols \
  libegl1-mesa-dev libgles2-mesa-dev libdrm-dev libgbm-dev libdecor-0-dev libudev-dev \
  libdbus-1-dev libibus-1.0-dev
```

It's one list for every Unlikelyware project (the same one CI installs), so it covers more than
PlainOldSynth needs. On other distros, install the same libraries under their own names.

## Get the source

```bash
git clone https://github.com/erickampman/PlainOldSynth.git
cd PlainOldSynth
tools/fetch_deps.sh
```

`tools/fetch_deps.sh` checks out the git submodules at their pinned commits, then applies the local
dependency patches in `patches/`. Run it again after a `git pull` that moves, adds or re-points a
submodule: a plain pull leaves them where they were.

It initialises `dpf` (with its nested `pugl`), `external/clap`, `external/clap-wrapper` and
`external/vst3sdk`. Of the VST3 SDK's seven nested submodules it takes only the four the build needs
(`base`, `cmake`, `pluginterfaces`, `public.sdk`); a bare `git submodule update --init --recursive`
would also pull its docs, tutorials and VSTGUI.

`patches/` holds the one local change carried against a dependency: clap-wrapper's AUv2 view never
calls `gui->show()`, so a CLAP plugin whose `show()` does load-bearing work (as this one does: it
registers the host timer that drives the DPF render loop) draws nothing under an AUv2 host.
`tools/apply_dep_patches.sh` applies it, skipping it when it's already applied.

## Build and test

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The same three commands on both platforms. Everything goes in `build/` (ignored by git).

The tests are described in [TESTING.md](TESTING.md).

This produces `build/PlainOldSynth.clap` (a bundle on macOS) and the VST3 bundle:
`build/PlainOldSynth.vst3` on macOS, `build/Release/PlainOldSynth.vst3` on Linux (clap-wrapper puts
it in a per-configuration folder).

## Installing the plugin for hosts

```bash
cmake --build build --target install-clap
cmake --build build --target install-vst3
cmake --build build --target install-auv2     # macOS, with POS_BUILD_AUV2=ON
```

| | Linux | macOS |
|---|---|---|
| CLAP | `~/.clap` | `~/Library/Audio/Plug-Ins/CLAP` |
| VST3 | `~/.vst3` | `~/Library/Audio/Plug-Ins/VST3` |
| AUv2 | — | `~/Library/Audio/Plug-Ins/Components` |

`install-clap` and `install-vst3` symlink the built plugin, so a rebuild is picked up after a
rescan; `install-auv2` copies the component (AU hosts don't reliably follow symlinks). A plain
`cmake --build build` only compiles: nothing is installed behind your back.

## Options

| Option | Default | Effect |
|---|---|---|
| `POS_BUILD_VST3` | `ON` | Build `PlainOldSynth.vst3` through clap-wrapper (with the vendored `external/vst3sdk`). |
| `POS_BUILD_AUV2` | `OFF` | Build `PlainOldSynth.component` through clap-wrapper. **macOS only**: configuring it elsewhere is an error. clap-wrapper downloads Apple's AudioUnitSDK on the first configure unless you pass `-DAUDIOUNIT_SDK_ROOT=/path/to/AudioUnitSDK`. |
| `POS_WERROR` | `OFF` | Warnings as errors for PlainOldSynth's own sources. |

## Checking a build

```bash
clap-validator validate build/PlainOldSynth.clap   # 35 passed, 0 failed, 0 warnings, 9 skipped
auval -v aumu POSy nLKL                            # AU VALIDATION SUCCEEDED (AUv2 only)
```

[clap-validator](https://github.com/free-audio/clap-validator) is free-audio's CLAP checker.
