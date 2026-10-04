// src/plugin.cpp — PlainOldSynth: the CLAP plugin (host glue + DSP) for a classic subtractive
// voice: one stereo-detuned Tri/Saw/Pulse oscillator → 24 dB/oct low-pass → VCA, with two ADSRs
// (filter + amplifier), two LFOs, glide, and switchable poly/mono.
//
// ---------------------------------------------------------------------------------------------
// WHY THIS IS RAW CLAP AND NOT dpf_add_plugin
// ---------------------------------------------------------------------------------------------
// DPF's generated CLAP wrapper cannot receive MIDI 2.0 7.9-pitch note events: it advertises only
// CLAP_NOTE_DIALECT_MIDI | CLAP_NOTE_DIALECT_MIDI_MPE (dpf/distrho/src/DistrhoPluginCLAP.cpp) and
// drops CLAP_EVENT_MIDI2 on the floor ("case CLAP_EVENT_MIDI2: break;"). PlainOldSynth must accept
// them, so it implements the clap_plugin_entry itself, exactly as PolygraphModularUnified does, and
// uses DPF only for the GUI (UIExporter + NanoVG, see src/UI/PlainOldSynthUI.cpp).
//
// The MIDI 2.0 path, end to end:
//   note-ports extension advertises CLAP | MIDI | MIDI2, preferring MIDI2
//     → host sends CLAP_EVENT_MIDI2 with a raw UMP
//     → pm::fromClapMidi2() decodes the two UMP words (pm_midi.cpp decodeUmp)
//     → UmpMessage carries attributeType == kAttrPitch7_9 and the 16-bit attribute
//     → PitchConverterModule's MIDI proc turns it into octaves via
//       PitchConverter::Pitch79ToOctaveOffset()  (note + fraction/512, relative to A440)
//     → that CV drives TriSaw's pitchInput, so BOTH oscillators track the microtonal pitch.
// A MIDI-1.0 host still works: the same port advertises the MIDI dialect and CLAP note events are
// converted to UMP (pm::fromClapNote / pm::fromClapMidi1).

#include <clap/clap.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "GeneratedPresets.h"          // build-time codegen from Presets/*.json
#include "PlainOldSynthParams.h"
#include "VoiceManager.hpp"
#include "clap_ump.h"
#include "pm_midi.h"
#include "pm_params.h"
#include "plugin_gui.h"

#ifndef POS_VERSION_STRING
#define POS_VERSION_STRING "0.1.0"
#endif

namespace {

using namespace plainoldsynth;

// ---------------------------------------------------------------------------------------------
// Parameter plumbing
// ---------------------------------------------------------------------------------------------

// VoiceManager's own parameters (polyphony, MIDI filtering, legato) do not live in the per-voice
// module graph, so they are addressed through the reserved MIDI kind.
constexpr AUParameterAddress midiParam(MIDIParamItem item) {
    return MODULE_PARAM_ADDRESS(ModuleParamKindMIDI, ModuleInstance1, item);
}

inline float clampParam(uint32_t index, double value) {
    const ParamSpec& s = kParams[index];
    if (!(value == value)) return s.def;                       // NaN → default
    if (value < double(s.min)) return s.min;
    if (value > double(s.max)) return s.max;
    return float(value);
}

// A knob drag on the UI thread: begin_gesture → set (repeatedly) → end_gesture. These travel to the
// audio thread (or to params.flush when the plugin is idle) through this lock-free ring and come out
// as real CLAP param events, so host automation records the move instead of it being invisible.
struct GuiEdit {
    enum Kind : uint8_t { Begin = 0, Value, End };
    uint8_t  kind;
    uint32_t index;
    double   value;
};

class GuiEditRing {
public:
    bool push(const GuiEdit& edit) {
        const uint32_t t = tail.load(std::memory_order_relaxed);
        const uint32_t next = (t + 1) & (kCapacity - 1);
        if (next == head.load(std::memory_order_acquire)) return false;   // full: drop, never block
        buffer[t] = edit;
        tail.store(next, std::memory_order_release);
        return true;
    }
    bool pop(GuiEdit& out) {
        const uint32_t h = head.load(std::memory_order_relaxed);
        if (h == tail.load(std::memory_order_acquire)) return false;
        out = buffer[h];
        head.store((h + 1) & (kCapacity - 1), std::memory_order_release);
        return true;
    }

private:
    static constexpr uint32_t kCapacity = 512;   // power of two
    GuiEdit buffer[kCapacity];
    std::atomic<uint32_t> head{0};   // consumer
    std::atomic<uint32_t> tail{0};   // producer (UI thread)
};

struct Plugin {
    const clap_host_t* host = nullptr;
    clap_plugin_t      clap{};
    void*              gui = nullptr;          // the shell's per-instance GUI state
    double             sampleRate = DEFAULT_SAMPLE_RATE;
    std::unique_ptr<VoiceManager> voiceManager;

    std::atomic<double> paramValues[kParamCount];
    std::atomic<bool>   paramDirtyForGui[kParamCount];   // host changed it → tell the open editor
    std::atomic<bool>   guiEditing[kParamCount];         // user is dragging it → don't fight them
    float               appliedValues[kParamCount]{};    // what the engine currently has

    GuiEditRing guiEdits;
    std::string scratch;    // state-save buffer; reused so save() need not allocate
};

Plugin* self(const clap_plugin_t* plugin) { return static_cast<Plugin*>(plugin->plugin_data); }

void requestMainThread(Plugin* plugin) {
    if (plugin->host != nullptr && plugin->host->request_callback != nullptr)
        plugin->host->request_callback(plugin->host);
}

// ---------------------------------------------------------------------------------------------
// Parameter → engine
// ---------------------------------------------------------------------------------------------

void setEngine(Plugin* plugin, AUParameterAddress address, float value) {
    if (plugin->voiceManager && address != 0)
        plugin->voiceManager->setParameter(address, value);
}

void setVoiceManager(Plugin* plugin, AUParameterAddress address, float value) {
    if (plugin->voiceManager && address != 0)
        plugin->voiceManager->setVoiceManagerParameter(address, value);
}

float paramOf(const Plugin* plugin, uint32_t index) {
    return float(plugin->paramValues[index].load(std::memory_order_relaxed));
}

bool isMonoMode(const Plugin* plugin) {
    return int(paramOf(plugin, kParamVoiceMode) + 0.5f) == kVoiceMono;
}

// Glide is a mono gesture: in poly mode each newly allocated voice would audibly scoop up from its
// rest pitch, so the slew is forced to (essentially) instant there. This is the same reasoning
// StruckSynth uses for the same Slew module.
void applyGlide(Plugin* plugin) {
    const float glide = isMonoMode(plugin) ? paramOf(plugin, kParamGlide) : 1e-8f;
    setEngine(plugin, POS_ITEM(posSlew1, SlewParamItemRise), glide);
    setEngine(plugin, POS_ITEM(posSlew1, SlewParamItemFall), glide);
}

// The other half of mono note-steal handling. ADSRModule's MIDI proc reads this mode straight out
// of the module's param array:
//   0 Steal            poly: retrigger from zero (still legato-retriggering a repeated pitch on the
//                      same voice, which is what avoids the click)
//   1 LegatoRetrigger  mono/Retrigger: restart the attack from the CURRENT level (no snap to zero)
//   2 LegatoTrue       mono/Legato: do not retrigger at all while the envelope is sustaining, so
//                      overlapping notes slide into one another
void applyLegato(Plugin* plugin) {
    float mode = 0.0f;
    if (isMonoMode(plugin))
        mode = (int(paramOf(plugin, kParamMonoLegato) + 0.5f) == kMonoLegatoTrue) ? 2.0f : 1.0f;
    setEngine(plugin, POS_ITEM(posAdsr1, ADSRParamItemLegato), mode);
    setEngine(plugin, POS_ITEM(posAdsr2, ADSRParamItemLegato), mode);
}

// Mono = a single voice driven through VoiceManager's held-note stack (its "legato" config), so a
// new note while another is held continues the same voice instead of stealing it. Poly = 10 voices.
void applyVoiceMode(Plugin* plugin) {
    const bool mono = isMonoMode(plugin);
    setVoiceManager(plugin, midiParam(MIDIParamItemVoiceCount), mono ? 1.0f : 10.0f);
    setVoiceManager(plugin, midiParam(MIDIParamItemLegato),     mono ? 1.0f : 0.0f);
    applyLegato(plugin);
    applyGlide(plugin);
}

// ------------------------------------------------------------------------------------------------
// Modulation sections
//
// A section's six host parameters are NOT engine writes like the rest: the two input menus and the
// destination menu are GRAPH EDITS (they re-point the patch), so they travel through VoiceManager's
// lock-free connection queue rather than into a module parameter. Levels and mode are ordinary
// writes and stay in applyParam's switch.
//
// The routing rule is: for each destination, find the section that selects it and point that
// destination's module input(s) at that section's mixer output; a destination nobody selects is
// cleared. That makes the whole routing idempotent — it is recomputed from the parameter values
// rather than patched incrementally, so it cannot drift.
// ------------------------------------------------------------------------------------------------

void storeParam(Plugin* plugin, uint32_t index, double value);          // defined below
void notifyHostValuesChanged(Plugin* plugin);                            // defined below

inline bool isModParam(uint32_t index)
{
    return index >= uint32_t(kParamMod1SourceA) && index <= uint32_t(kParamMod4Dest);
}
inline int modSectionOf(uint32_t index)
{
    return int((index - uint32_t(kParamMod1SourceA)) / uint32_t(kModFieldCount));
}
inline ModField modFieldOf(uint32_t index)
{
    return ModField((index - uint32_t(kParamMod1SourceA)) % uint32_t(kModFieldCount));
}

inline AUParameterAddress modMixerAddress(int section)
{
    switch (section) {
    case 0:  return posCvMixer1;
    case 1:  return posCvMixer2;
    case 2:  return posCvMixer3;
    default: return posCvMixer4;
    }
}

// The module a source menu entry refers to; 0 means "nothing to connect" (None).
AUParameterAddress modSourceModuleA(int selection)
{
    switch (selection) {
    case kModSourceALfo1:  return posLfo1;
    case kModSourceAAdsr1: return posAdsr1;
    default:               return 0;
    }
}
AUParameterAddress modSourceModuleB(int selection)
{
    switch (selection) {
    case kModSourceBLfo2:  return posLfo2;
    case kModSourceBAdsr2: return posAdsr2;
    case kModSourceBCC1:   return posCcConverter1;
    default:               return 0;
    }
}

// One destination is one input slot, except the three that are per-channel and so need two.
struct ModDestTarget { AUParameterAddress left, right; };

ModDestTarget modDestTargets(int dest)
{
    switch (dest) {
    case kModDestOscPitch:
        return { POS_ITEM(posTriSaw1, TriSawParamItemModPitch),
                 POS_ITEM(posTriSaw2, TriSawParamItemModPitch) };
    case kModDestPulseWidth:
        return { POS_ITEM(posComparator1, ComparatorParamItemThresholdInput),
                 POS_ITEM(posComparator2, ComparatorParamItemThresholdInput) };
    case kModDestPeak:
        return { POS_ITEM(posTriSaw1, TriSawParamItemModPeak),
                 POS_ITEM(posTriSaw2, TriSawParamItemModPeak) };
    case kModDestCutoff:
        return { POS_ITEM(posLpf1, LPFParamItemModFreqInput2), 0 };
    default:
        return { 0, 0 };
    }
}

// Rebuild the whole modulation patch from the current parameter values. Cheap (a handful of queue
// commands) and idempotent, so it can just be called whenever anything routing-related changes.
void applyModRouting(Plugin* plugin)
{
    if (!plugin->voiceManager) return;
    VoiceManager& vm = *plugin->voiceManager;

    for (int s = 0; s < kModSectionCount; ++s) {
        const AUParameterAddress mixer = modMixerAddress(s);
        const int selA = int(paramOf(plugin, modParamIndex(s, kModFieldSourceA)) + 0.5f);
        const int selB = int(paramOf(plugin, modParamIndex(s, kModFieldSourceB)) + 0.5f);
        vm.setModSource(POS_ITEM(mixer, CVMixerParamItemInput1), modSourceModuleA(selA));
        vm.setModSource(POS_ITEM(mixer, CVMixerParamItemInput2), modSourceModuleB(selB));
    }

    for (int dest = kModDestOscPitch; dest <= kModDestCutoff; ++dest) {
        int owner = -1;
        for (int s = 0; s < kModSectionCount; ++s) {
            if (int(paramOf(plugin, modParamIndex(s, kModFieldDest)) + 0.5f) == dest) {
                owner = s;
                break;
            }
        }
        const ModDestTarget target = modDestTargets(dest);
        const AUParameterAddress source = (owner < 0) ? 0 : modMixerAddress(owner);
        if (target.left  != 0) vm.setModSource(target.left,  source);
        if (target.right != 0) vm.setModSource(target.right, source);
    }
}

// A destination input takes one source, so a destination belongs to exactly one section. Rather than
// let two sections silently fight over it (the later one would win on the next routing pass and the
// earlier one's menu would keep claiming it), switch the earlier section back to Off.
void applyModDestChange(Plugin* plugin, int section, int dest)
{
    if (dest != kModDestOff) {
        for (int other = 0; other < kModSectionCount; ++other) {
            if (other == section) continue;
            const uint32_t otherIndex = modParamIndex(other, kModFieldDest);
            if (int(paramOf(plugin, otherIndex) + 0.5f) == dest) {
                storeParam(plugin, otherIndex, float(kModDestOff));
                notifyHostValuesChanged(plugin);
            }
        }
    }
    applyModRouting(plugin);
}

// One host parameter → the engine. Derived controls fan out; everything else is a single write.
void applyParam(Plugin* plugin, uint32_t index, float value) {
    if (!plugin->voiceManager) return;

    // Modulation sections are handled up front: two of their six fields are graph edits, not writes.
    if (isModParam(index)) {
        const int section = modSectionOf(index);
        const AUParameterAddress mixer = modMixerAddress(section);
        switch (modFieldOf(index)) {
        case kModFieldLevelA:
            setEngine(plugin, POS_ITEM(mixer, CVMixerParamItemInput1Level), value);
            break;
        case kModFieldLevelB:
            setEngine(plugin, POS_ITEM(mixer, CVMixerParamItemInput2Level), value);
            break;
        case kModFieldMode:
            setEngine(plugin, POS_ITEM(mixer, CVMixerParamItemMode), value);
            break;
        case kModFieldSourceA:
        case kModFieldSourceB:
            applyModRouting(plugin);
            break;
        case kModFieldDest:
            applyModDestChange(plugin, section, int(value + 0.5f));
            break;
        default:
            break;
        }
        return;
    }

    switch (index) {
    // --- Shape picks WHICH of the two parallel signal paths is audible: the raw tri/saw, or the
    //     comparator-generated pulse. Both paths stay wired at all times and are crossfaded by these
    //     two Level modules, so switching shape is click-free and never has to re-patch the graph
    //     while audio is running. It deliberately does NOT touch Peak: the triangle<->saw morph is
    //     kParamPeak, so one knob owns it and there is nothing to keep in sync between the two.
    case kParamShape: {
        const int shape = int(value + 0.5f);
        const float raw   = (shape == kShapePulse) ? 0.0f : 1.0f;
        const float pulse = (shape == kShapePulse) ? 1.0f : 0.0f;
        setEngine(plugin, POS_ITEM(posLevelRaw1, LevelParamItemLevel), raw);
        setEngine(plugin, POS_ITEM(posLevelRaw2, LevelParamItemLevel), raw);
        setEngine(plugin, POS_ITEM(posLevelPulse1, LevelParamItemLevel), pulse);
        setEngine(plugin, POS_ITEM(posLevelPulse2, LevelParamItemLevel), pulse);
        break;
    }
    case kParamPulseWidth:
        setEngine(plugin, POS_ITEM(posComparator1, ComparatorParamItemThreshold), value);
        setEngine(plugin, POS_ITEM(posComparator2, ComparatorParamItemThreshold), value);
        break;
    case kParamPeak:
        // 0.5 = symmetric triangle, ~1.0 = saw. Applied to both oscillators so the tri/saw character
        // is identical on the left and right channels; only Detune separates them.
        setEngine(plugin, POS_ITEM(posTriSaw1, TriSawParamItemPeak), value);
        setEngine(plugin, POS_ITEM(posTriSaw2, TriSawParamItemPeak), value);
        break;
    // Coarse and Fine are two views of ONE engine parameter: TriSawOsc has a single continuous
    // semitone tuning (`pitch`), so Coarse is its integer part and Fine its fractional cents. Either
    // knob therefore has to recompute from both, reading the other back rather than assuming the
    // order the host or a preset happens to apply them in.
    case kParamCoarse:
    case kParamFine: {
        const float coarse = (index == kParamCoarse) ? value : paramOf(plugin, kParamCoarse);
        const float fine   = (index == kParamFine)   ? value : paramOf(plugin, kParamFine);
        const float semitones = coarse + fine / 100.0f;
        setEngine(plugin, POS_ITEM(posTriSaw1, TriSawParamItemPitch), semitones);
        setEngine(plugin, POS_ITEM(posTriSaw2, TriSawParamItemPitch), semitones);
        break;
    }
    // One detune control, written to both oscillators with opposite sign: the right channel goes UP
    // N cents and the left goes DOWN N cents, so this single knob is the whole stereo image. The
    // engine clamps PitchFine to +/-25 cents, which is exactly this parameter's range, so N passes
    // through untouched. Detune is deliberately independent of Coarse/Fine: it uses the oscillator's
    // separate cents param, so it stays a spread rather than becoming another tuning offset.
    case kParamDetune:
        setEngine(plugin, POS_ITEM(posTriSaw1, TriSawParamItemPitchFine), -value);   // left:  -N
        setEngine(plugin, POS_ITEM(posTriSaw2, TriSawParamItemPitchFine),  value);   // right: +N
        break;

    case kParamCutoff:           setEngine(plugin, POS_ITEM(posLpf1, LPFParamItemPitch), value); break;
    case kParamResonance:        setEngine(plugin, POS_ITEM(posLpf1, LPFParamItemQ), value); break;
    case kParamKeyTrack:         setEngine(plugin, POS_ITEM(posLpf1, LPFParamItemFreqModLevel), value); break;

    case kParamFilterAttack:   setEngine(plugin, POS_ITEM(posAdsr1, ADSRParamItemAttack),  value); break;
    case kParamFilterDecay:    setEngine(plugin, POS_ITEM(posAdsr1, ADSRParamItemDecay),   value); break;
    case kParamFilterSustain:  setEngine(plugin, POS_ITEM(posAdsr1, ADSRParamItemSustain), value); break;
    case kParamFilterRelease:  setEngine(plugin, POS_ITEM(posAdsr1, ADSRParamItemRelease), value); break;

    case kParamAmpAttack:      setEngine(plugin, POS_ITEM(posAdsr2, ADSRParamItemAttack),  value); break;
    case kParamAmpDecay:       setEngine(plugin, POS_ITEM(posAdsr2, ADSRParamItemDecay),   value); break;
    case kParamAmpSustain:     setEngine(plugin, POS_ITEM(posAdsr2, ADSRParamItemSustain), value); break;
    case kParamAmpRelease:     setEngine(plugin, POS_ITEM(posAdsr2, ADSRParamItemRelease), value); break;

    case kParamLfo1Rate:  setEngine(plugin, POS_ITEM(posLfo1, LFOParamItemPitch), value); break;
    case kParamLfo1Wave:  setEngine(plugin, POS_ITEM(posLfo1, LFOParamItemWave),  value); break;
    case kParamLfo2Rate:  setEngine(plugin, POS_ITEM(posLfo2, LFOParamItemPitch), value); break;
    case kParamLfo2Wave:  setEngine(plugin, POS_ITEM(posLfo2, LFOParamItemWave),  value); break;
    // No LFO Depth / Env Amount cases: how far each source moves its destination is a modulation
    // section's input level now (see the kModParam block above).

    case kParamVoiceMode:   applyVoiceMode(plugin); break;
    case kParamMonoLegato:  applyLegato(plugin);    break;
    case kParamGlide:       applyGlide(plugin);     break;
    case kParamMasterLevel: setEngine(plugin, POS_ITEM(posFinal1, FinalOutputParamItemLevel), value); break;
    case kParamSpread:      setEngine(plugin, POS_ITEM(posFinal1, FinalOutputParamItemSpread), value); break;

    default: break;
    }
}

// Everything in the graph that is NOT exposed as a host parameter. Applied once per activation,
// before the host parameters are pushed.
void initPatchConstants(Plugin* plugin) {
    // The two oscillators are hard-panned to opposite sides; their audible stereo width is entirely
    // kParamDetune acting on two genuinely different frequencies.
    setEngine(plugin, POS_ITEM(posPan1, PanParamItemPosition), -1.0f);
    setEngine(plugin, POS_ITEM(posPan2, PanParamItemPosition),  1.0f);

    // 1/2 sum raw + pulse per channel; 3 sums left + right; 4 is the VCA and must multiply.
    setEngine(plugin, POS_ITEM(posCombineOsc1, CombineParamItemType), float(ParamItemCombineType_Add));
    setEngine(plugin, POS_ITEM(posCombineOsc2, CombineParamItemType), float(ParamItemCombineType_Add));
    setEngine(plugin, POS_ITEM(posCombineMix,  CombineParamItemType), float(ParamItemCombineType_Add));
    setEngine(plugin, POS_ITEM(posCombineVca,  CombineParamItemType), float(ParamItemCombineType_Multiply));

    // The oscillator section's output level used to be a host knob; it is fixed now, at the value
    // that knob defaulted to, so dropping the knob does not change how loud the section is.
    setEngine(plugin, POS_ITEM(posCombineMix, CombineParamItemLevel), 0.8f);

    // Comparators run at audio rate: full-wave clip, no hysteresis, band-limited crossings (polyBLEP).
    const AUParameterAddress comparators[] = { posComparator1, posComparator2 };
    for (const AUParameterAddress c : comparators) {
        setEngine(plugin, POS_ITEM(c, ComparatorParamItemType),       float(ComparatorTypeFullClip));
        setEngine(plugin, POS_ITEM(c, ComparatorParamItemHysteresis), 0.0f);
        setEngine(plugin, POS_ITEM(c, ComparatorParamItemClipLevel),  0.99f);
        setEngine(plugin, POS_ITEM(c, ComparatorParamItemBandlimit),  1.0f);
        setEngine(plugin, POS_ITEM(c, ComparatorParamItemThresholdInputLevel), 0.99f);  // pulse width dest
    }

    // LFOs are free-running and in phase on both channels.
    const AUParameterAddress lfos[] = { posLfo1, posLfo2 };
    for (const AUParameterAddress l : lfos) {
        setEngine(plugin, POS_ITEM(l, LFOParamItemPhase),      0.0f);
        setEngine(plugin, POS_ITEM(l, LFOParamItemClockRatio), 0.0f);
    }

    // Envelopes are exponential (the classic analog shape).
    setEngine(plugin, POS_ITEM(posAdsr1, ADSRParamItemLinear), 0.0f);
    setEngine(plugin, POS_ITEM(posAdsr2, ADSRParamItemLinear), 0.0f);

    // Modulation destinations are pinned to FULL SCALE. The depth knobs that used to own these
    // (Env Amount, LFO 1/2 Depth) are gone — a modulation section's two input levels are the depth
    // now, and they only run -1..1, so each destination must sit at its maximum for a section at
    // level 1.0 to reach the range the old knob reached at its own maximum.
    setEngine(plugin, POS_ITEM(posLpf1, LPFParamItemFreqModLevel2), 8.0f);   // cutoff, was +-8 oct

    // Output stage: DC-block (a band-limited pulse train is not DC-free), clip at unity.
    setEngine(plugin, POS_ITEM(posFinal1, FinalOutputParamItemFilterDC),  1.0f);
    setEngine(plugin, POS_ITEM(posFinal1, FinalOutputParamItemClipLevel), 1.0f);

    // Oscillators: pitch comes from the PitchConverter → Slew CV, so every other pitch input and all
    // self-modulation stays at zero.
    const AUParameterAddress oscs[] = { posTriSaw1, posTriSaw2 };
    for (const AUParameterAddress o : oscs) {
        setEngine(plugin, POS_ITEM(o, TriSawParamItemFreqModLevel2),    0.0f);
        setEngine(plugin, POS_ITEM(o, TriSawParamItemPhaseModLevel),    0.0f);
        setEngine(plugin, POS_ITEM(o, TriSawParamItemFreqModLevel),     2.0f);   // osc pitch dest
        setEngine(plugin, POS_ITEM(o, TriSawParamItemPeakModLevel),     1.0f);   // peak dest
        setEngine(plugin, POS_ITEM(o, TriSawParamItemSelfModLevel),     0.0f);
        setEngine(plugin, POS_ITEM(o, TriSawParamItemSelfModInputLevel), 0.0f);
        setEngine(plugin, POS_ITEM(o, TriSawParamItemModSyncType),      0.0f);
    }

    // The four Level modules are plain gains.
    const AUParameterAddress levels[] = { posLevelRaw1, posLevelPulse1, posLevelRaw2, posLevelPulse2 };
    for (const AUParameterAddress lv : levels) {
        setEngine(plugin, POS_ITEM(lv, LevelParamItemInvertLeft),  0.0f);
        setEngine(plugin, POS_ITEM(lv, LevelParamItemInvertRight), 0.0f);
    }
}

// Push every host parameter into the engine and record what the engine now has.
void applyAllParams(Plugin* plugin) {
    for (uint32_t i = 0; i < kParamCount; ++i) {
        const float value = paramOf(plugin, i);
        plugin->appliedValues[i] = value;
        applyParam(plugin, i, value);
    }
}

// Called once per process() before rendering: picks up anything the UI thread changed. Comparing
// against appliedValues keeps the per-block cost at zero when nothing moved.
void applyPendingParams(Plugin* plugin) {
    for (uint32_t i = 0; i < kParamCount; ++i) {
        const float value = paramOf(plugin, i);
        if (value != plugin->appliedValues[i]) {
            plugin->appliedValues[i] = value;
            applyParam(plugin, i, value);
        }
    }
}

void notifyHostValuesChanged(Plugin* plugin) {
    if (plugin->host == nullptr) return;
    const auto* params = static_cast<const clap_host_params_t*>(
        plugin->host->get_extension(plugin->host, CLAP_EXT_PARAMS));
    if (params != nullptr && params->rescan != nullptr)
        params->rescan(plugin->host, CLAP_PARAM_RESCAN_VALUES);
}

// Store a value that originated on the GUI or from a preset (i.e. NOT from the host).
void storeParam(Plugin* plugin, uint32_t index, double value) {
    const float clamped = clampParam(index, value);
    plugin->paramValues[index].store(clamped, std::memory_order_relaxed);
    plugin->paramDirtyForGui[index].store(true, std::memory_order_release);
}

// ---------------------------------------------------------------------------------------------
// CLAP events
// ---------------------------------------------------------------------------------------------

void processEvent(Plugin* plugin, const clap_event_header_t* header) {
    if (header->space_id != CLAP_CORE_EVENT_SPACE_ID) return;

    switch (header->type) {
    case CLAP_EVENT_NOTE_ON: {
        const auto* note = reinterpret_cast<const clap_event_note_t*>(header);
        if (plugin->voiceManager)
            plugin->voiceManager->handleMIDIMessage(pm::fromClapNote(*note, true));
        break;
    }
    case CLAP_EVENT_NOTE_OFF:
    case CLAP_EVENT_NOTE_CHOKE: {
        const auto* note = reinterpret_cast<const clap_event_note_t*>(header);
        if (plugin->voiceManager)
            plugin->voiceManager->handleMIDIMessage(pm::fromClapNote(*note, false));
        break;
    }
    case CLAP_EVENT_MIDI: {          // MIDI 1.0, three bytes
        const auto* midi = reinterpret_cast<const clap_event_midi_t*>(header);
        if (plugin->voiceManager)
            plugin->voiceManager->handleMIDIMessage(pm::fromClapMidi1(*midi));
        break;
    }
    case CLAP_EVENT_MIDI2: {         // MIDI 2.0 UMP — this is where 7.9-pitch notes arrive
        const auto* midi = reinterpret_cast<const clap_event_midi2_t*>(header);
        if (plugin->voiceManager)
            plugin->voiceManager->handleMIDIMessage(pm::fromClapMidi2(*midi));
        break;
    }
    case CLAP_EVENT_PARAM_VALUE: {
        const auto* event = reinterpret_cast<const clap_event_param_value_t*>(header);
        const int index = paramIndexForId(event->param_id);
        if (index < 0) break;
        const uint32_t i = uint32_t(index);
        const float clamped = clampParam(i, event->value);
        plugin->paramValues[i].store(clamped, std::memory_order_relaxed);
        plugin->paramDirtyForGui[i].store(true, std::memory_order_release);
        if (plugin->voiceManager) {
            plugin->appliedValues[i] = clamped;
            applyParam(plugin, i, clamped);
        }
        requestMainThread(plugin);
        break;
    }
    default:
        break;
    }
}

// GUI edits → CLAP param/gesture events. Called from process() and from params.flush() (those two
// are mutually exclusive per the CLAP spec, which is what makes the SPSC ring sufficient).
void emitGuiEdits(Plugin* plugin, const clap_output_events_t* out) {
    GuiEdit edit;
    while (plugin->guiEdits.pop(edit)) {
        if (edit.index >= kParamCount) continue;
        if (out == nullptr || out->try_push == nullptr) continue;
        const clap_id id = clap_id(paramIdForIndex(edit.index));

        if (edit.kind == GuiEdit::Value) {
            clap_event_param_value_t event{};
            event.header.size     = sizeof(event);
            event.header.time     = 0;
            event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            event.header.type     = CLAP_EVENT_PARAM_VALUE;
            event.param_id        = id;
            event.value           = edit.value;
            out->try_push(out, &event.header);
        } else {
            clap_event_param_gesture_t event{};
            event.header.size     = sizeof(event);
            event.header.time     = 0;
            event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            event.header.type     = (edit.kind == GuiEdit::Begin) ? CLAP_EVENT_PARAM_GESTURE_BEGIN
                                                                 : CLAP_EVENT_PARAM_GESTURE_END;
            event.param_id        = id;
            out->try_push(out, &event.header);
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Plugin lifecycle
// ---------------------------------------------------------------------------------------------

bool plug_init(const clap_plugin_t*) { return true; }

void plug_destroy(const clap_plugin_t* plugin) { delete self(plugin); }

bool plug_activate(const clap_plugin_t* plugin, double sampleRate, uint32_t, uint32_t) {
    Plugin* p = self(plugin);
    p->sampleRate = sampleRate > 0.0 ? sampleRate : DEFAULT_SAMPLE_RATE;
    p->voiceManager = std::make_unique<VoiceManager>(p->sampleRate, MODULE_BUFF_SIZE);

    // VoiceManager's own params are not zero-initialised — set the ones we never expose.
    setVoiceManager(p, midiParam(MIDIParamItemChannelFilterValue), 0.0f);
    setVoiceManager(p, midiParam(MIDIParamItemGroupFilterValue),   0.0f);
    setVoiceManager(p, midiParam(MIDIParamItemVoiceStealAlgorithm), float(VoiceStealAlgorithmOldest));
    setVoiceManager(p, midiParam(MIDIParamItemFreeRun),            0.0f);

    initPatchConstants(p);
    applyAllParams(p);       // includes applyVoiceMode(), so polyphony/legato/glide are set up
    // applyAllParams has already routed the modulation sections as it walked their parameters, but
    // do it once more at the end so the final state is what the graph ends up with regardless of the
    // order the parameter values were applied in.
    applyModRouting(p);
    return true;
}

void plug_deactivate(const clap_plugin_t* plugin) {
    self(plugin)->voiceManager.reset();
}

bool plug_start_processing(const clap_plugin_t*) { return true; }

void plug_stop_processing(const clap_plugin_t*) {}

void plug_reset(const clap_plugin_t* plugin) {
    Plugin* p = self(plugin);
    if (p->voiceManager) p->voiceManager->reset();
}

void plug_on_main_thread(const clap_plugin_t* plugin) {
    Plugin* p = self(plugin);
    if (p->gui == nullptr) return;
    for (uint32_t i = 0; i < kParamCount; ++i) {
        if (!p->paramDirtyForGui[i].exchange(false, std::memory_order_acq_rel)) continue;
        if (p->guiEditing[i].load(std::memory_order_relaxed)) continue;   // the user is dragging it
        pos_gui_push_param(p->gui, i, p->paramValues[i].load(std::memory_order_relaxed));
    }
}

clap_process_status plug_process(const clap_plugin_t* plugin, const clap_process_t* process) {
    Plugin* p = self(plugin);

    const clap_input_events_t* in = process->in_events;
    const uint32_t numEvents = in != nullptr ? in->size(in) : 0;

    emitGuiEdits(p, process->out_events);

    const uint32_t frames = process->frames_count;
    float* left = nullptr;
    float* right = nullptr;
    for (uint32_t i = 0; i < process->audio_outputs_count; ++i) {
        const clap_audio_buffer_t& out = process->audio_outputs[i];
        if (out.data32 == nullptr || out.channel_count < 2) continue;
        left  = out.data32[0];
        right = out.data32[1];
        break;
    }

    // No usable output (or not activated): still consume the events so notes are not lost.
    if (left == nullptr || right == nullptr || !p->voiceManager) {
        for (uint32_t i = 0; i < numEvents; ++i) processEvent(p, in->get(in, i));
        return CLAP_PROCESS_CONTINUE;
    }

    applyPendingParams(p);

    std::memset(left,  0, sizeof(float) * frames);
    std::memset(right, 0, sizeof(float) * frames);

    if (const clap_event_transport_t* transport = process->transport) {
        const bool hasTempo = (transport->flags & CLAP_TRANSPORT_HAS_TEMPO) != 0;
        const bool playing  = (transport->flags & CLAP_TRANSPORT_IS_PLAYING) != 0;
        const bool hasBeats = (transport->flags & CLAP_TRANSPORT_HAS_BEATS_TIMELINE) != 0;
        const double bpm     = hasTempo ? transport->tempo : 120.0;
        const double beatPos = hasBeats ? double(transport->song_pos_beats) / double(CLAP_BEATTIME_FACTOR) : 0.0;
        p->voiceManager->setHostTransport(bpm, beatPos, playing, hasTempo, hasBeats);
    } else {
        p->voiceManager->setHostTransport(120.0, 0.0, false, false, false);
    }

    // Sample-accurate render: apply every event at its own frame offset, rendering forward to the
    // next event (capped at one engine chunk) so notes land on the sample the host asked for instead
    // of being quantized to the top of the block.
    uint32_t nextEvent = 0;
    for (uint32_t offset = 0; offset < frames; ) {
        while (nextEvent < numEvents) {
            const clap_event_header_t* header = in->get(in, nextEvent);
            const uint32_t eventTime = header->time < frames ? header->time : frames - 1;
            if (eventTime > offset) break;
            processEvent(p, header);
            ++nextEvent;
        }

        uint32_t end = frames;
        if (nextEvent < numEvents) {
            const clap_event_header_t* header = in->get(in, nextEvent);
            const uint32_t eventTime = header->time < frames ? header->time : frames - 1;
            if (eventTime > offset && eventTime < end) end = eventTime;
        }
        if (end - offset > MODULE_BUFF_SIZE) end = offset + MODULE_BUFF_SIZE;

        p->voiceManager->process(end - offset, left + offset, right + offset);
        offset = end;
    }

    for (; nextEvent < numEvents; ++nextEvent) processEvent(p, in->get(in, nextEvent));

    return CLAP_PROCESS_CONTINUE;
}

// ---------------------------------------------------------------------------------------------
// audio-ports extension — one stereo output
// ---------------------------------------------------------------------------------------------

uint32_t audio_ports_count(const clap_plugin_t*, bool isInput) { return isInput ? 0 : 1; }

bool audio_ports_get(const clap_plugin_t*, uint32_t index, bool isInput, clap_audio_port_info_t* info) {
    if (isInput || index != 0) return false;
    info->id = 0;
    std::strncpy(info->name, "Output", sizeof(info->name) - 1);
    info->channel_count = 2;
    info->flags         = CLAP_AUDIO_PORT_IS_MAIN;
    info->port_type     = CLAP_PORT_STEREO;
    info->in_place_pair = CLAP_INVALID_ID;
    return true;
}

const clap_plugin_audio_ports_t kAudioPorts = { audio_ports_count, audio_ports_get };

// ---------------------------------------------------------------------------------------------
// note-ports extension — one input port that prefers MIDI 2.0
// ---------------------------------------------------------------------------------------------
// Advertising (and preferring) CLAP_NOTE_DIALECT_MIDI2 is what makes a MIDI-2.0-capable host send
// raw UMP notes, which is the only way a 7.9-pitch NoteOn reaches the oscillators unchanged. The
// CLAP and MIDI-1.0 dialects stay advertised so MIDI-1.0-only hosts still deliver notes.
uint32_t note_ports_count(const clap_plugin_t*, bool isInput) { return isInput ? 1 : 0; }

bool note_ports_get(const clap_plugin_t*, uint32_t index, bool isInput, clap_note_port_info_t* info) {
    if (!isInput || index != 0) return false;
    info->id = 0;
    info->supported_dialects = CLAP_NOTE_DIALECT_CLAP
                             | CLAP_NOTE_DIALECT_MIDI
                             | CLAP_NOTE_DIALECT_MIDI2;
    info->preferred_dialect  = CLAP_NOTE_DIALECT_MIDI2;
    std::strncpy(info->name, "Notes", sizeof(info->name) - 1);
    return true;
}

const clap_plugin_note_ports_t kNotePorts = { note_ports_count, note_ports_get };

// ---------------------------------------------------------------------------------------------
// params extension
// ---------------------------------------------------------------------------------------------

void formatParamValue(uint32_t index, double value, char* out, uint32_t size) {
    const ParamSpec& spec = kParams[index];
    if (spec.choices != nullptr) {
        int count = 0;
        while (spec.choices[count] != nullptr) ++count;
        int choice = int(value + 0.5);
        choice = std::clamp(choice, 0, count - 1);
        std::snprintf(out, size, "%s", spec.choices[choice]);
    } else if (spec.integer) {
        std::snprintf(out, size, "%d", int(value + (value < 0.0 ? -0.5 : 0.5)));
    } else if (std::strcmp(spec.unit, "s") == 0) {
        // Sub-second times read better in ms; %.4g keeps the value -> text -> value -> text
        // round-trip stable, which CLAP hosts and validators check (params_text_to_value parses the
        // "ms" suffix back).
        if (value < 1.0) std::snprintf(out, size, "%.4g ms", value * 1000.0);
        else             std::snprintf(out, size, "%.4g s", value);
    } else if (spec.unit[0] != '\0') {
        std::snprintf(out, size, "%.4g %s", value, spec.unit);
    } else {
        std::snprintf(out, size, "%.4g", value);
    }
}

uint32_t params_count(const clap_plugin_t*) { return kParamCount; }

bool params_get_info(const clap_plugin_t*, uint32_t index, clap_param_info_t* info) {
    if (index >= kParamCount) return false;
    const ParamSpec& spec = kParams[index];
    std::memset(info, 0, sizeof(*info));
    info->id            = clap_id(paramIdForIndex(index));
    info->flags         = CLAP_PARAM_IS_AUTOMATABLE;
    if (spec.integer) info->flags |= CLAP_PARAM_IS_STEPPED;
    if (spec.choices != nullptr) info->flags |= CLAP_PARAM_IS_ENUM;
    info->min_value     = spec.min;
    info->max_value     = spec.max;
    info->default_value = spec.def;
    std::strncpy(info->name, spec.name, sizeof(info->name) - 1);
    std::strncpy(info->module, kPanelNames[spec.panel], sizeof(info->module) - 1);
    return true;
}

bool params_get_value(const clap_plugin_t* plugin, clap_id id, double* out) {
    const int index = paramIndexForId(id);
    if (index < 0) return false;
    *out = self(plugin)->paramValues[index].load(std::memory_order_relaxed);
    return true;
}

bool params_value_to_text(const clap_plugin_t*, clap_id id, double value, char* out, uint32_t size) {
    const int index = paramIndexForId(id);
    if (index < 0) return false;
    formatParamValue(uint32_t(index), value, out, size);
    return true;
}

bool params_text_to_value(const clap_plugin_t*, clap_id id, const char* text, double* out) {
    const int index = paramIndexForId(id);
    if (index < 0) return false;
    const ParamSpec& spec = kParams[index];

    // Accept the dropdown label as well as a number, so hosts that round-trip text work.
    if (spec.choices != nullptr) {
        int count = 0;
        while (spec.choices[count] != nullptr) ++count;
        for (int i = 0; i < count; ++i) {
            if (std::strcmp(spec.choices[i], text) == 0) { *out = double(i); return true; }
        }
    }
    // Plain number, or the "<number> ms" form formatParamValue() prints for sub-second times.
    char* end = nullptr;
    const double parsed = std::strtod(text, &end);
    if (end != nullptr) {
        while (*end == ' ') ++end;
        if (end[0] == 'm' && end[1] == 's') {
            *out = parsed / 1000.0;
            return true;
        }
    }
    *out = parsed;
    return true;
}

void params_flush(const clap_plugin_t* plugin, const clap_input_events_t* in, const clap_output_events_t* out) {
    Plugin* p = self(plugin);
    if (in != nullptr) {
        const uint32_t count = in->size(in);
        for (uint32_t i = 0; i < count; ++i) processEvent(p, in->get(in, i));
    }
    emitGuiEdits(p, out);
}

const clap_plugin_params_t kParamsExt = {
    params_count, params_get_info, params_get_value,
    params_value_to_text, params_text_to_value, params_flush,
};

// ---------------------------------------------------------------------------------------------
// state extension
// ---------------------------------------------------------------------------------------------
// A tiny line-based text format rather than JSON, so the DSP side has no parser dependency:
//     PlainOldSynth 1
//     <symbol> <value>
//     ...
// Parameters are keyed by symbol, so adding/reordering parameters can never misinterpret an old
// state file; unknown symbols are ignored.

bool streamWrite(const clap_ostream_t* stream, const void* data, uint64_t size) {
    const auto* bytes = static_cast<const uint8_t*>(data);
    for (uint64_t done = 0; done < size; ) {
        const int64_t written = stream->write(stream, bytes + done, size - done);
        if (written <= 0) return false;
        done += uint64_t(written);
    }
    return true;
}

bool state_save(const clap_plugin_t* plugin, const clap_ostream_t* stream) {
    Plugin* p = self(plugin);
    p->scratch.clear();
    p->scratch += "PlainOldSynth 1\n";
    char line[128];
    for (uint32_t i = 0; i < kParamCount; ++i) {
        std::snprintf(line, sizeof(line), "%s %.9g\n", kParams[i].symbol,
                      p->paramValues[i].load(std::memory_order_relaxed));
        p->scratch += line;
    }
    return streamWrite(stream, p->scratch.data(), p->scratch.size());
}

bool state_load(const clap_plugin_t* plugin, const clap_istream_t* stream) {
    Plugin* p = self(plugin);

    std::string text;
    char buffer[512];
    for (;;) {
        const int64_t read = stream->read(stream, buffer, sizeof(buffer));
        if (read < 0) return false;
        if (read == 0) break;
        text.append(buffer, size_t(read));
    }

    // Empty data, or data that is not ours, is a failure — not a silent "loaded defaults".
    // Idempotent hosts and validators both rely on this distinction.
    const size_t firstBreak = text.find('\n');
    const std::string header = text.substr(0, firstBreak == std::string::npos ? text.size() : firstBreak);
    if (header.rfind("PlainOldSynth", 0) != 0) return false;

    // Store everything first, then apply once, so even a truncated file leaves the engine
    // consistent rather than half-updated.
    bool any = false;
    size_t pos = firstBreak == std::string::npos ? text.size() : firstBreak + 1;
    while (pos < text.size()) {
        size_t lineEnd = text.find('\n', pos);
        if (lineEnd == std::string::npos) lineEnd = text.size();
        const std::string line = text.substr(pos, lineEnd - pos);
        pos = lineEnd + 1;

        const size_t space = line.find(' ');
        if (space == std::string::npos) continue;
        const int index = paramIndexForSymbol(line.substr(0, space).c_str());
        if (index < 0) continue;                       // unknown symbol: ignore, stay compatible
        storeParam(p, uint32_t(index), std::strtod(line.c_str() + space + 1, nullptr));
        any = true;
    }

    if (any) applyAllParams(p);
    notifyHostValuesChanged(p);
    requestMainThread(p);
    return true;
}

const clap_plugin_state_t kStateExt = { state_save, state_load };

// ---------------------------------------------------------------------------------------------
// gui + timer extensions (the actual UI lives in the GUI shell)
// ---------------------------------------------------------------------------------------------

void plug_on_timer(const clap_plugin_t* plugin, clap_id timerId) {
    pos_gui_on_timer(plugin, timerId);
}

const clap_plugin_timer_support_t kTimerExt = { plug_on_timer };

const void* plug_get_extension(const clap_plugin_t*, const char* id) {
    if (std::strcmp(id, CLAP_EXT_AUDIO_PORTS)    == 0) return &kAudioPorts;
    if (std::strcmp(id, CLAP_EXT_NOTE_PORTS)     == 0) return &kNotePorts;
    if (std::strcmp(id, CLAP_EXT_PARAMS)         == 0) return &kParamsExt;
    if (std::strcmp(id, CLAP_EXT_STATE)          == 0) return &kStateExt;
    if (std::strcmp(id, CLAP_EXT_GUI)            == 0) return pos_gui_extension();
    if (std::strcmp(id, CLAP_EXT_TIMER_SUPPORT)  == 0) return &kTimerExt;
    return nullptr;
}

// ---------------------------------------------------------------------------------------------
// descriptor + factory + entry
// ---------------------------------------------------------------------------------------------

const char* kFeatures[] = {
    CLAP_PLUGIN_FEATURE_INSTRUMENT,
    CLAP_PLUGIN_FEATURE_SYNTHESIZER,
    CLAP_PLUGIN_FEATURE_STEREO,
    nullptr,
};

const clap_plugin_descriptor_t kDescriptor = {
    .clap_version = CLAP_VERSION_INIT,
    .id           = "com.unlikelyware.plainoldsynth",
    .name         = "PlainOldSynth",
    .vendor       = "Unlikelyware",
    .url          = "https://unlikelyware.com",
    .manual_url   = "",
    .support_url  = "",
    .version      = POS_VERSION_STRING,
    .description  = "Classic subtractive synthesizer: stereo-detuned Tri/Saw/Pulse oscillator, "
                    "24 dB/oct low-pass, two ADSRs, two LFOs, poly/mono.",
    .features     = kFeatures,
};

const clap_plugin_t* factory_create(const clap_plugin_factory_t*, const clap_host_t* host, const char* id) {
    if (std::strcmp(id, kDescriptor.id) != 0) return nullptr;

    auto* p = new (std::nothrow) Plugin();
    if (p == nullptr) return nullptr;
    p->host = host;
    for (uint32_t i = 0; i < kParamCount; ++i) {
        p->paramValues[i].store(kParams[i].def, std::memory_order_relaxed);
        p->paramDirtyForGui[i].store(false, std::memory_order_relaxed);
        p->guiEditing[i].store(false, std::memory_order_relaxed);
        p->appliedValues[i] = kParams[i].def;
    }
    p->clap = clap_plugin_t{
        .desc             = &kDescriptor,
        .plugin_data      = p,
        .init             = plug_init,
        .destroy          = plug_destroy,
        .activate         = plug_activate,
        .deactivate       = plug_deactivate,
        .start_processing = plug_start_processing,
        .stop_processing  = plug_stop_processing,
        .reset            = plug_reset,
        .process          = plug_process,
        .get_extension    = plug_get_extension,
        .on_main_thread   = plug_on_main_thread,
    };
    return &p->clap;
}

uint32_t factory_count(const clap_plugin_factory_t*) { return 1; }

const clap_plugin_descriptor_t* factory_get(const clap_plugin_factory_t*, uint32_t index) {
    return index == 0 ? &kDescriptor : nullptr;
}

const clap_plugin_factory_t kFactory = { factory_count, factory_get, factory_create };

bool entry_init(const char*) { return true; }

void entry_deinit() {}

const void* entry_get_factory(const char* factoryId) {
    if (std::strcmp(factoryId, CLAP_PLUGIN_FACTORY_ID) == 0) return &kFactory;
    return nullptr;
}

} // namespace

// ---------------------------------------------------------------------------------------------
// The C seam the GUI shell calls (see src/plugin_gui.h). These live in the same translation unit,
// so the anonymous-namespace Plugin above is directly visible.
// ---------------------------------------------------------------------------------------------
extern "C" {

const clap_host_t* pos_plugin_host(const clap_plugin_t* plugin) { return self(plugin)->host; }

void  pos_plugin_set_gui(const clap_plugin_t* plugin, void* gui) { self(plugin)->gui = gui; }
void* pos_plugin_get_gui(const clap_plugin_t* plugin)           { return self(plugin)->gui; }

uint32_t pos_param_count(void) { return kParamCount; }

double pos_param_value(const clap_plugin_t* plugin, uint32_t index) {
    if (index >= kParamCount) return 0.0;
    return self(plugin)->paramValues[index].load(std::memory_order_relaxed);
}

void pos_param_begin_gesture(const clap_plugin_t* plugin, uint32_t index) {
    if (index >= kParamCount) return;
    Plugin* p = self(plugin);
    p->guiEditing[index].store(true, std::memory_order_relaxed);
    p->guiEdits.push({ GuiEdit::Begin, index, 0.0 });
}

void pos_param_set_from_gui(const clap_plugin_t* plugin, uint32_t index, double value) {
    if (index >= kParamCount) return;
    Plugin* p = self(plugin);
    const float clamped = clampParam(index, value);
    p->paramValues[index].store(clamped, std::memory_order_relaxed);
    p->guiEdits.push({ GuiEdit::Value, index, double(clamped) });
}

void pos_param_end_gesture(const clap_plugin_t* plugin, uint32_t index) {
    if (index >= kParamCount) return;
    Plugin* p = self(plugin);
    p->guiEditing[index].store(false, std::memory_order_relaxed);
    p->guiEdits.push({ GuiEdit::End, index, 0.0 });
}

uint32_t pos_preset_count(void) { return kPresetCount; }

const char* pos_preset_name(uint32_t index) {
    return index < kPresetCount ? kPresets[index].name : "";
}

void pos_preset_apply(const clap_plugin_t* plugin, uint32_t index) {
    if (index >= kPresetCount) return;
    Plugin* p = self(plugin);

    // Every preset is self-contained: reset to the defaults first, then apply the values the preset
    // actually names. Without the reset, switching from a preset that sets a parameter to one that
    // does not would silently keep the previous sound.
    for (uint32_t i = 0; i < kParamCount; ++i)
        storeParam(p, i, kParams[i].def);

    const PresetEntry& preset = kPresets[index];
    for (int v = 0; v < preset.count; ++v) {
        const int paramIndex = paramIndexForSymbol(preset.values[v].symbol);
        if (paramIndex >= 0) storeParam(p, uint32_t(paramIndex), preset.values[v].value);
    }

    applyAllParams(p);
    notifyHostValuesChanged(p);
    requestMainThread(p);   // refresh every control of an open editor
}

} // extern "C"

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    .clap_version = CLAP_VERSION_INIT,
    .init         = entry_init,
    .deinit       = entry_deinit,
    .get_factory  = entry_get_factory,
};
