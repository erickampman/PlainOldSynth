cmake_minimum_required(VERSION 3.21)

# C is needed for DPF's pugl/dgl sources.
# macOS: 26 or later, Apple Silicon only. Set before project();
# ignored on other platforms. Both are cache variables, so -D overrides them.
set(CMAKE_OSX_DEPLOYMENT_TARGET 26.0 CACHE STRING "macOS 26 or later")
set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "Apple Silicon only")
project(PlainOldSynth VERSION 0.9.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)          # the vendored engine uses std::numbers / std::span
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# The .clap/.vst3/.component are all shared objects, and pos_core is linked into each of them.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(POS_BUILD_VST3 "Build a VST3 wrapper around the same plugin via clap-wrapper." ON)
option(POS_BUILD_AUV2 "Build an AUv2 (.component) wrapper via clap-wrapper (macOS only)." ${APPLE})
option(POS_WERROR "Treat warnings as errors in PlainOldSynth's own sources." OFF)

if(APPLE)
    # DPF's DistrhoUI_macOS.mm is Objective-C++.
    enable_language(OBJCXX)
endif()

# ------------------------------------------------------------------------------------------------
# The engine: PolygraphModularUnified's DSP/voice framework, vendored under src/ and trimmed to the
# modules this synth's fixed voice graph actually uses. Vendored code keeps relaxed warnings; our own
# sources stay strict.
# ------------------------------------------------------------------------------------------------
add_library(pos_framework STATIC
    src/Common/pm_midi.cpp
    src/Common/pm_param_data.cpp
    src/Module/ADSRHelper.cpp
    src/Module/ADSRModule.cpp
    src/Module/CCConverterModule.cpp
    src/Module/ChannelPressureModule.cpp
    src/Module/ComparatorModule.cpp
    src/Module/CombineModule.cpp
    src/Module/CVMixerModule.cpp
    src/Module/FinalModule.cpp
    src/Module/LFOModule.cpp
    src/Module/LPFilterModule.cpp
    src/Module/LevelModule.cpp
    src/Module/ModuleFactory.cpp
    src/Module/PanModule.cpp
    src/Module/PitchBendConverterModule.cpp
    src/Module/PitchConverterModule.cpp
    src/Module/SlewModule.cpp
    src/Module/TriSawModule.cpp
    src/Module/VelocityConverterModule.cpp
    src/Voice/ModuleMap.cpp
    src/Voice/pos_module_specs.cpp
    src/Voice/Voice.cpp
    src/Voice/VoiceManager.cpp
    src/Voice/VoiceStateManager.cpp)

# clap-sdk is used by our own sources; external/clap also defines a target literally named `clap`,
# which is what clap-wrapper's guarantee_clap() looks for (see CLAP_SDK_ROOT below).
add_library(clap-sdk INTERFACE)
target_include_directories(clap-sdk INTERFACE "${CMAKE_SOURCE_DIR}/external/clap/include")

# The vendored files use bare `#include "X.hpp"` across Common/ Module/ Voice/ (PolygraphModular's
# own layout), so all three folders are on the include path.
target_include_directories(pos_framework PUBLIC src/Common src/Module src/Voice
    plugin)   # PlainOldSynthParams.h: VoiceManager::buildDefaultPatch wires this graph
target_link_libraries(pos_framework PUBLIC clap-sdk)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(pos_framework PRIVATE -Wall -Wno-unknown-pragmas -Wno-unused-parameter)
endif()

# ------------------------------------------------------------------------------------------------
# Factory presets: Presets/*.json → GeneratedPresets.h at build time, so the plugin needs no JSON
# parser and never touches the filesystem for presets. Adding a preset file requires re-running
# cmake once (CONFIGURE_DEPENDS catches edits to existing files on every build).
# ------------------------------------------------------------------------------------------------
find_package(Python3 COMPONENTS Interpreter REQUIRED)
file(GLOB POS_PRESET_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/Presets/*.json")
set(POS_GENERATED_PRESETS "${CMAKE_BINARY_DIR}/generated/GeneratedPresets.h")
add_custom_command(
    OUTPUT "${POS_GENERATED_PRESETS}"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/gen_presets.py"
            ${POS_PRESET_FILES} -o "${POS_GENERATED_PRESETS}"
    DEPENDS ${POS_PRESET_FILES} "${CMAKE_SOURCE_DIR}/tools/gen_presets.py"
    COMMENT "Generating embedded factory presets from Presets/*.json")
add_custom_target(generate_presets DEPENDS "${POS_GENERATED_PRESETS}")

# ------------------------------------------------------------------------------------------------
# Embedded resources: Resources/* compiled in as raw bytes (DPF's own res2c.py). NanoVG decodes the
# logo PNG from memory at UI-construction time, so the built plugin has no filesystem dependency.
# ------------------------------------------------------------------------------------------------
set(POS_RESOURCES_HPP "${CMAKE_BINARY_DIR}/generated/EmbeddedResources.hpp")
set(POS_RESOURCES_CPP "${CMAKE_BINARY_DIR}/generated/EmbeddedResources.cpp")
# res2c.py embeds a whole folder and names each symbol by the file's stem, so help.md (the source of
# help.html) would collide with help.html. Copy everything but *.md into a staging folder first.
file(GLOB POS_RESOURCE_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/Resources/*")
list(FILTER POS_RESOURCE_FILES EXCLUDE REGEX "\\.md$")
set(POS_RESOURCES_STAGING "${CMAKE_BINARY_DIR}/generated/Resources")
add_custom_command(
    OUTPUT "${POS_RESOURCES_HPP}" "${POS_RESOURCES_CPP}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${POS_RESOURCES_STAGING}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${POS_RESOURCES_STAGING}"
    COMMAND ${CMAKE_COMMAND} -E copy ${POS_RESOURCE_FILES} "${POS_RESOURCES_STAGING}"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/dpf/utils/res2c.py"
            EmbeddedResources "${POS_RESOURCES_STAGING}" "${CMAKE_BINARY_DIR}/generated"
    DEPENDS ${POS_RESOURCE_FILES} "${CMAKE_SOURCE_DIR}/dpf/utils/res2c.py"
    COMMENT "Embedding Resources/* as compiled-in data")
add_custom_target(generate_resources DEPENDS "${POS_RESOURCES_HPP}" "${POS_RESOURCES_CPP}")

# ------------------------------------------------------------------------------------------------
# DPF is used ONLY for the GUI: UIExporter + NanoVG + pugl. The CLAP entry point is ours, because
# DPF's generated CLAP wrapper cannot receive MIDI 2.0 note events — see src/plugin.cpp's header.
# ------------------------------------------------------------------------------------------------
add_subdirectory(dpf)
# dgl-opengl is normally created by dpf_add_plugin()'s UI_TYPE branch; call the same helper directly.
# SHARED_RESOURCES=TRUE embeds DGL's DejaVu font (fontFace(NANOVG_DEJAVU_SANS_TTF)).
dpf__add_dgl_opengl(TRUE FALSE FALSE)
# PlainOldSynth's own namespace (UmpireDAW's docs/decisions.md, "DPF fork"): on macOS pugl names its
# Objective-C classes after it, so two DPF binaries with the same one in a host collide.
target_compile_definitions(dgl-opengl PUBLIC DGL_NAMESPACE=PlainOldSynthDGL)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # Third-party noise, kept off our own targets: pugl.hpp includes <cstdbool>, which libstdc++
    # flags with a #warning under C++17+, and GCC misreads a bounds check in NanoVG's stb_image.h.
    target_compile_options(dgl-opengl PRIVATE -Wno-cpp -Wno-stringop-overflow)
endif()

# ------------------------------------------------------------------------------------------------
# pos_core: the whole plugin (DSP + host glue + GUI shell) as one static archive, shared by every
# output format below. Hidden visibility so only clap_entry — marked CLAP_EXPORT in plugin.cpp — is
# ever exported from the final binary.
# ------------------------------------------------------------------------------------------------
add_library(pos_core STATIC
    src/plugin.cpp
    src/UI/PlainOldSynthUI.cpp
    "${POS_RESOURCES_CPP}"                      # generated: the logo bytes (see above)
    "${DPF_ROOT_DIR}/distrho/src/DistrhoUI.cpp"
    "${DPF_ROOT_DIR}/distrho/src/DistrhoUtils.cpp")
set_target_properties(pos_core PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)
target_include_directories(pos_core PUBLIC
    src                               # plugin_gui.h (the DSP <-> GUI seam)
    plugin                            # DistrhoPluginInfo.h + PlainOldSynthParams.h
    "${CMAKE_BINARY_DIR}/generated"   # GeneratedPresets.h
    "${DPF_ROOT_DIR}/distrho")        # DistrhoUI.hpp / DistrhoUIInternal.hpp
# clap-wrapper's plugin-side extension header only (clapwrapper/vst3.h: which VST3 note expressions to
# offer). Header-only and plain C, so the CLAP build needs it even with POS_BUILD_VST3 off.
target_include_directories(pos_core PRIVATE "${CMAKE_SOURCE_DIR}/external/clap-wrapper/include")
target_compile_definitions(pos_core PUBLIC
    DISTRHO_PLUGIN_TARGET_CLAP
    DISTRHO_IS_STANDALONE=0
    POS_VERSION_STRING="${PROJECT_VERSION}")
target_link_libraries(pos_core PUBLIC pos_framework dgl-opengl)
add_dependencies(pos_core generate_presets generate_resources)
dpf__add_plugin_specific_ui_sources(pos_core FALSE)   # macOS: DistrhoUI_macOS.mm
if(APPLE)
    # The live Retina scale of the host window (see the file's header comment).
    target_sources(pos_core PRIVATE src/UI/BackingScale_mac.mm)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(pos_core PRIVATE -Wall -Wextra -Wno-unknown-pragmas -Wno-unused-parameter)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # These two reach DPF's private pugl.hpp, whose <cstdbool> trips libstdc++'s #warning
        # (see dgl-opengl above).
        set_source_files_properties(
            "${DPF_ROOT_DIR}/distrho/src/DistrhoUI.cpp" src/UI/PlainOldSynthUI.cpp
            TARGET_DIRECTORY pos_core PROPERTIES COMPILE_OPTIONS -Wno-cpp)
    endif()
    if(POS_WERROR)
        target_compile_options(pos_core PRIVATE -Werror)
    endif()
endif()

# ------------------------------------------------------------------------------------------------
# The .clap output. A MODULE with no sources of its own would be rejected by CMake, and simply
# linking the archive is not enough: nothing references clap_entry, so the linker would drop the
# object that defines it. Hence the empty placeholder plus a whole-archive force-load.
# ------------------------------------------------------------------------------------------------
function(pos_force_load_whole_archive TARGET_NAME LIB_NAME)
    if(APPLE)
        target_link_options(${TARGET_NAME} PRIVATE "-Wl,-force_load,$<TARGET_FILE:${LIB_NAME}>")
    elseif(UNIX)
        target_link_options(${TARGET_NAME} PRIVATE
            "-Wl,--whole-archive" "$<TARGET_FILE:${LIB_NAME}>" "-Wl,--no-whole-archive")
    endif()
endfunction()

set(POS_EMPTY_SOURCE "${CMAKE_BINARY_DIR}/generated/pos_empty.c")
# A genuinely empty file trips -Wempty-translation-unit under -Wpedantic -Werror (which clap-wrapper
# applies to the AUv2 target), and ISO C requires at least one declaration anyway.
set(_pos_empty_content "typedef int pos_empty_translation_unit_marker;\n")
set(_pos_empty_existing "")
if(EXISTS "${POS_EMPTY_SOURCE}")
    file(READ "${POS_EMPTY_SOURCE}" _pos_empty_existing)
endif()
if(NOT _pos_empty_existing STREQUAL _pos_empty_content)
    file(WRITE "${POS_EMPTY_SOURCE}" "${_pos_empty_content}")
endif()

add_library(PlainOldSynth MODULE "${POS_EMPTY_SOURCE}")
target_link_libraries(PlainOldSynth PRIVATE pos_core)
pos_force_load_whole_archive(PlainOldSynth pos_core)

if(APPLE)
    set_target_properties(PlainOldSynth PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION "clap"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.unlikelyware.plainoldsynth"
        MACOSX_BUNDLE_BUNDLE_NAME "PlainOldSynth"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
else()
    set_target_properties(PlainOldSynth PROPERTIES PREFIX "" SUFFIX ".clap")
endif()

# Symlink the built .clap into the user's CLAP directory, so a rescan in any host picks up the
# freshest build. rm -rf first: create_symlink cannot replace a real bundle directory.
if(APPLE)
    set(POS_CLAP_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/CLAP")
    set(POS_CLAP_ARTIFACT "$<TARGET_BUNDLE_DIR:PlainOldSynth>")
else()
    set(POS_CLAP_DIR "$ENV{HOME}/.clap")
    set(POS_CLAP_ARTIFACT "$<TARGET_FILE:PlainOldSynth>")
endif()
add_custom_target(install-clap
    COMMAND ${CMAKE_COMMAND} -E make_directory "${POS_CLAP_DIR}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${POS_CLAP_DIR}/PlainOldSynth.clap"
    COMMAND ${CMAKE_COMMAND} -E create_symlink "${POS_CLAP_ARTIFACT}" "${POS_CLAP_DIR}/PlainOldSynth.clap"
    DEPENDS PlainOldSynth
    VERBATIM
    COMMENT "Symlinking PlainOldSynth.clap into ${POS_CLAP_DIR}")

# ------------------------------------------------------------------------------------------------
# VST3 / AUv2 via the vendored clap-wrapper, wrapping the very same CLAP. Purely additive: neither
# block touches the .clap target above.
# ------------------------------------------------------------------------------------------------
if(POS_BUILD_VST3 OR POS_BUILD_AUV2)
    if(NOT CLAP_SDK_ROOT)
        set(CLAP_SDK_ROOT "${CMAKE_SOURCE_DIR}/external/clap" CACHE PATH "" FORCE)
    endif()
    add_subdirectory(external/clap-wrapper)
endif()

if(POS_BUILD_VST3)
    if(NOT VST3_SDK_ROOT)
        set(VST3_SDK_ROOT "${CMAKE_SOURCE_DIR}/external/vst3sdk" CACHE PATH "" FORCE)
    endif()

    add_library(PlainOldSynth_vst3 MODULE "${POS_EMPTY_SOURCE}")
    if(UNIX AND NOT APPLE)
        # clap-wrapper's Linux branch tests a misspelled TCLP_RESOURCE_DIRECTORY, which is never
        # set, so it always warns that RESOURCE_DIRECTORY is unsupported. Defining it empty
        # silences that; we pass no RESOURCE_DIRECTORY.
        set(TCLP_RESOURCE_DIRECTORY "")
    endif()
    target_link_libraries(PlainOldSynth_vst3 PRIVATE pos_core)
    pos_force_load_whole_archive(PlainOldSynth_vst3 pos_core)
    target_add_vst3_wrapper(
        TARGET PlainOldSynth_vst3
        OUTPUT_NAME "PlainOldSynth"
        BUNDLE_IDENTIFIER "com.unlikelyware.plainoldsynth.vst3"
        BUNDLE_VERSION "${PROJECT_VERSION}")
    if(TARGET base-sdk-vst3 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        # The VST3 SDK still uses std::wstring_convert, deprecated since C++17.
        target_compile_options(base-sdk-vst3 PRIVATE -Wno-deprecated-declarations)
    endif()

    # Hosts want the bundle folder. On Linux $<TARGET_FILE> is the .so inside it; clap-wrapper
    # builds the bundle in build/Release (or build/Debug). As Retune 9a2b6eb.
    if(APPLE)
        set(POS_VST3_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST3")
        set(POS_VST3_ARTIFACT "$<TARGET_BUNDLE_DIR:PlainOldSynth_vst3>")
    else()
        set(POS_VST3_DIR "$ENV{HOME}/.vst3")
        set(POS_VST3_ARTIFACT "${CMAKE_BINARY_DIR}/$<IF:$<CONFIG:Debug>,Debug,Release>/PlainOldSynth.vst3")
    endif()
    add_custom_target(install-vst3
        COMMAND ${CMAKE_COMMAND} -E make_directory "${POS_VST3_DIR}"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${POS_VST3_DIR}/PlainOldSynth.vst3"
        COMMAND ${CMAKE_COMMAND} -E create_symlink "${POS_VST3_ARTIFACT}" "${POS_VST3_DIR}/PlainOldSynth.vst3"
        DEPENDS PlainOldSynth_vst3
        VERBATIM
        COMMENT "Symlinking PlainOldSynth.vst3 into ${POS_VST3_DIR}")
endif()

if(POS_BUILD_AUV2)
    # AUv2 is an Apple-only format, and clap-wrapper's target_add_auv2_wrapper simply does not exist
    # off Apple platforms. Say so plainly instead of failing with an unknown-command error.
    if(NOT APPLE)
        message(FATAL_ERROR
            "POS_BUILD_AUV2 is macOS-only (AUv2 is an Apple plugin format). "
            "Reconfigure with -DPOS_BUILD_AUV2=OFF.")
    endif()
    # clap-wrapper fetches Apple's AudioUnitSDK itself (CPM) unless pointed at a local copy, e.g.
    # -DAUDIOUNIT_SDK_ROOT=/path/to/AudioUnitSDK
    if(NOT AUDIOUNIT_SDK_ROOT)
        set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES ON)
    endif()

    add_library(PlainOldSynth_auv2 MODULE "${POS_EMPTY_SOURCE}")
    target_link_libraries(PlainOldSynth_auv2 PRIVATE pos_core)
    pos_force_load_whole_archive(PlainOldSynth_auv2 pos_core)
    # "POSy" (PlainOldSynth) as the subtype, under Unlikelyware's own "nLKL" manufacturer code.
    target_add_auv2_wrapper(
        TARGET PlainOldSynth_auv2
        OUTPUT_NAME "PlainOldSynth"
        BUNDLE_IDENTIFIER "com.unlikelyware.plainoldsynth.auv2"
        BUNDLE_VERSION "${PROJECT_VERSION}"
        MANUFACTURER_NAME "Unlikelyware"
        MANUFACTURER_CODE "nLKL"
        SUBTYPE_CODE "POSy"
        INSTRUMENT_TYPE "aumu")

    if(APPLE)
        set(POS_AUV2_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/Components")
        add_custom_target(install-auv2
            COMMAND ${CMAKE_COMMAND} -E make_directory "${POS_AUV2_DIR}"
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${POS_AUV2_DIR}/PlainOldSynth.component"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "$<TARGET_BUNDLE_DIR:PlainOldSynth_auv2>" "${POS_AUV2_DIR}/PlainOldSynth.component"
            DEPENDS PlainOldSynth_auv2
            VERBATIM
            COMMENT "Copying PlainOldSynth.component into ${POS_AUV2_DIR}")
    endif()
endif()

# ------------------------------------------------------------------------------------------------
# Headless smoke test: drives the engine with no DAW, including the MIDI 2.0 7.9-pitch path.
# Links pos_framework only (no CLAP/GUI), so it builds and runs anywhere.
# ------------------------------------------------------------------------------------------------
add_executable(pos-smoke-test test/pos_smoke_test.cpp)
target_link_libraries(pos-smoke-test PRIVATE pos_framework)
target_include_directories(pos-smoke-test PRIVATE plugin)
# Drives the real built .clap over the CLAP C API (note-port dialect negotiation + a MIDI 2.0
# 7.9-pitch note). No daemon, no GUI, no host application required.
add_executable(pos-clap-host-test test/pos_clap_host_test.cpp)
# dlopen/dlsym/dlclose: on glibc before 2.34 these live in libdl, so ask CMake for the right
# library rather than assuming it is part of libc (it is not, on older Linux distributions).
target_link_libraries(pos-clap-host-test PRIVATE pos_framework ${CMAKE_DL_LIBS})
target_include_directories(pos-clap-host-test PRIVATE
    external/clap/include
    src/Common                          # PlainOldSynthParams.h pulls in the engine's param headers
    plugin)                             # ...and the parameter ids the test installs
if(APPLE)
    set(POS_CLAP_UNDER_TEST "$<TARGET_BUNDLE_DIR:PlainOldSynth>")
else()
    set(POS_CLAP_UNDER_TEST "$<TARGET_FILE:PlainOldSynth>")
endif()
add_dependencies(pos-clap-host-test PlainOldSynth)

enable_testing()
add_test(NAME pos-smoke COMMAND pos-smoke-test)
add_test(NAME pos-clap COMMAND pos-clap-host-test "${POS_CLAP_UNDER_TEST}")
set_tests_properties(pos-clap PROPERTIES PASS_REGULAR_EXPRESSION "ALL PASS")
set_tests_properties(pos-smoke PROPERTIES PASS_REGULAR_EXPRESSION "ALL PASS")
