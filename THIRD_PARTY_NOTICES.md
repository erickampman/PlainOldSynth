# Third-party components bundled with PlainOldSynth

PlainOldSynth itself is MIT licensed (see [LICENSE.txt](LICENSE.txt)). It embeds the components
below; each keeps its own license.

| Component | Where | License |
|---|---|---|
| PolygraphModularUnified DSP/voice engine | `src/Common/`, `src/Module/`, `src/Voice/` | MIT, © Eric Kampman. Vendored from the sibling repo `../PolygraphModularUnified` and trimmed to the modules this synth uses. |
| StruckSynth NanoVG widget kit | `src/UI/*.hpp` (`Knob`, `Dropdown`, `GroupBox`, `PopupMenu`, `Logo`) | MIT — vendored from the sibling repo `../StruckSynth`. |
| DISTRHO Plugin Framework (DPF) | `dpf/` | ISC. Used **only** as the GUI layer (UIExporter + NanoVG + pugl). |
| CLAP headers | `external/clap/` | MIT. |
| clap-wrapper | `external/clap-wrapper/` | MIT. Wraps the built `.clap` into VST3 and AUv2. |
| VST3 SDK | `external/vst3sdk/` | MIT (Steinberg relicensed the SDK under MIT as of 3.8). |
| Apple AudioUnitSDK | fetched by clap-wrapper at configure time, or supplied via `-DAUDIOUNIT_SDK_ROOT=` | Apache-2.0. Only needed for `-DPOS_BUILD_AUV2=ON`. |

Note that `dpf/`, `external/clap/`, `external/clap-wrapper/` and `external/vst3sdk/` are committed
plain copies rather than git submodules, matching how PolygraphModularUnified vendors them, so a
clone builds without a network fetch.
