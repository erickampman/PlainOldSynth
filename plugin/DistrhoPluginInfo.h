// DistrhoPluginInfo.h — PlainOldSynth
//
// IMPORTANT: the DISTRHO_PLUGIN_* *identity* macros below are INERT. PlainOldSynth does NOT use
// dpf_add_plugin: it implements the CLAP entry point itself in src/plugin.cpp. That is deliberate
// and load-bearing — DPF's generated CLAP wrapper ignores CLAP_EVENT_MIDI2 outright
// (dpf/distrho/src/DistrhoPluginCLAP.cpp: "case CLAP_EVENT_MIDI2: break;") and advertises only
// CLAP_NOTE_DIALECT_MIDI | CLAP_NOTE_DIALECT_MIDI_MPE, so a DPF-generated plugin can never receive
// MIDI 2.0 7.9-pitch NoteOns. See src/plugin.cpp's note-ports extension.
//
// The DISTRHO_UI_* macros DO bind: DPF is used here purely as the GUI layer (NanoVG widgets driven
// through UIExporter), exactly as PolygraphModularUnified does it.

#pragma once

#define DISTRHO_PLUGIN_BRAND         "Unlikelyware"
#define DISTRHO_PLUGIN_NAME          "PlainOldSynth"
#define DISTRHO_PLUGIN_URI           "https://unlikelyware.com/plainoldsynth"
#define DISTRHO_PLUGIN_CLAP_ID       "com.unlikelyware.plainoldsynth"
#define DISTRHO_PLUGIN_CLAP_FEATURES "instrument", "synthesizer", "stereo"
#define DISTRHO_PLUGIN_BRAND_ID      Unlw
#define DISTRHO_PLUGIN_UNIQUE_ID     POSy

#define DISTRHO_PLUGIN_HAS_UI            1
#define DISTRHO_PLUGIN_IS_SYNTH          1
#define DISTRHO_PLUGIN_IS_RT_SAFE        1
#define DISTRHO_PLUGIN_NUM_INPUTS        0
#define DISTRHO_PLUGIN_NUM_OUTPUTS       2
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT   1
#define DISTRHO_PLUGIN_WANT_PROGRAMS     0
#define DISTRHO_PLUGIN_WANT_STATE        0
#define DISTRHO_PLUGIN_WANT_DIRECT_ACCESS 1

#define DISTRHO_UI_USE_NANOVG       1
#define DISTRHO_UI_USER_RESIZABLE   1
#define DISTRHO_UI_FILE_BROWSER     1   // Save / Load's file dialog (UI::openFileBrowser)
#define DISTRHO_UI_DEFAULT_WIDTH    1200
#define DISTRHO_UI_DEFAULT_HEIGHT   750

// Defined by the build (see CMakeLists.txt) because we never call dpf_add_plugin, which is what
// would normally set it: DistrhoUIPrivateData.hpp keys DISTRHO_UI_USES_SIZE_REQUEST off it.
#ifndef DISTRHO_PLUGIN_TARGET_CLAP
#define DISTRHO_PLUGIN_TARGET_CLAP 1
#endif
