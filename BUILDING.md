# Building PlainOldSynth

PlainOldSynth builds the same way on Linux and macOS: CMake + Ninja, git submodules, C++20. The
build makes the plugin `PlainOldSynth.clap`, the same plugin wrapped as `PlainOldSynth.vst3` and,
on macOS, `PlainOldSynth.component` (AUv2), and the tests.

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
`external/vst3sdk`. Of the VST® 3 SDK's seven nested submodules it takes only the four the build needs
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

A plain `cmake --build build` writes nothing outside `build/`. These targets put the plugin where
hosts look for it:

```bash
cmake --build build --target install-clap
cmake --build build --target install-vst3
cmake --build build --target install-auv2     # macOS
```

| | Linux | macOS |
|---|---|---|
| CLAP | `~/.clap` | `~/Library/Audio/Plug-Ins/CLAP` |
| VST3 | `~/.vst3` | `~/Library/Audio/Plug-Ins/VST3` |
| AUv2 | — | `~/Library/Audio/Plug-Ins/Components` |

`install-clap` and `install-vst3` symlink the plugin in `build/`, so a host picks up a rebuild
after a rescan. The link points into this clone: delete the clone and the host loses the plugin,
and running the target in another build folder points the host there instead. `install-auv2`
copies the component (AU hosts don't reliably follow symlinks), so run it again after a rebuild.

To keep the plugin without the clone, copy it instead (the `rm` clears a link the targets left):

```bash
# Linux
mkdir -p ~/.clap ~/.vst3
rm -rf ~/.clap/PlainOldSynth.clap ~/.vst3/PlainOldSynth.vst3
cp build/PlainOldSynth.clap ~/.clap/
cp -R build/Release/PlainOldSynth.vst3 ~/.vst3/
```

```bash
# macOS
mkdir -p ~/Library/Audio/Plug-Ins/CLAP ~/Library/Audio/Plug-Ins/VST3
rm -rf ~/Library/Audio/Plug-Ins/CLAP/PlainOldSynth.clap ~/Library/Audio/Plug-Ins/VST3/PlainOldSynth.vst3
cp -R build/PlainOldSynth.clap ~/Library/Audio/Plug-Ins/CLAP/
cp -R build/PlainOldSynth.vst3 ~/Library/Audio/Plug-Ins/VST3/
```

For every user instead, the same folders without `~` (`/usr/lib/clap` and `/usr/lib/vst3` on Linux,
`/Library/Audio/Plug-Ins/...` on macOS), with `sudo`. Then rescan in the host. A host that is
already running keeps the plugin it loaded first: quit and restart it after a rebuild. A plugin you
built yourself has no quarantine flag, so macOS loads it without the steps a downloaded one needs.

## Options

Pass these to the first command (`cmake -B build ... -DNAME=VALUE`).

| Option | Default | Effect |
|---|---|---|
| `POS_BUILD_VST3` | `ON` | Build `PlainOldSynth.vst3` through clap-wrapper (with the vendored `external/vst3sdk`). |
| `POS_BUILD_AUV2` | `ON` on macOS, `OFF` elsewhere | Build `PlainOldSynth.component` through clap-wrapper. **macOS only**. The first configure downloads Apple's AudioUnitSDK; `-DAUDIOUNIT_SDK_ROOT=...` uses a local copy. |
| `POS_WERROR` | `OFF` | Warnings as errors (CI turns it on). |

## Checking a build

```bash
clap-validator validate build/PlainOldSynth.clap   # 35 passed, 0 failed, 0 warnings, 9 skipped
auval -v aumu POSy nLKL                            # AU VALIDATION SUCCEEDED (AUv2 only)
```

[clap-validator](https://github.com/free-audio/clap-validator) is free-audio's CLAP checker.

## How this differs from the other projects

Every Unlikelyware project builds the same way: `git clone`, `tools/fetch_deps.sh`, the three
commands under "Build and test", and for a plugin `cmake --build build --target install-clap`. The
same package list, the same `build/` folder, the same option names (`<PROJECT>_BUILD_VST3`,
`<PROJECT>_UW_CORE_DIR` and so on). What's different here, and why:

- One of the seven synths (PlainOldSynth, StruckSynth, StruckRoom, StruckGaussian, StruckKit, IFSynth, ModicumFM), which share the PolygraphModular framework, vendored, and DPF for the UI. `tools/fetch_deps.sh` also applies `patches/` to clap-wrapper (its AUv2 view), so `external/clap-wrapper` shows a local change: that is the patch, not uncommitted work.
- The option prefix is `POS_`, not `PLAINOLDSYNTH_`: it's older than the naming rule, and renaming it would break existing build folders and scripts.
- No `POS_PRESET_DUMP`: PlainOldSynth is older than the IFSynth template the other synths came from, which brought that dev-only button.
- CI runs on Linux only for now (`.github/workflows/linux.yml`); macOS CI comes with the others' (ReleaseUMP R1-4).

VST is a registered trademark of Steinberg Media Technologies GmbH.
