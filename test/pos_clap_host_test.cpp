// test/pos_clap_host_test.cpp — drives the ACTUAL built PlainOldSynth.clap through the CLAP C API,
// with no DAW and no GUI. This is the end-to-end proof of the thing that matters most here: that a
// MIDI 2.0 NoteOn carrying a 7.9-pitch attribute survives the plugin's note port, reaches the
// oscillators, and comes back out at the right (microtonal) frequency.
//
// It is also the only check that exercises note-port dialect negotiation — the reason this plugin
// implements CLAP directly instead of using DPF's wrapper, which drops the event outright.
//
// The pitch checks install a controlled patch through the params extension (triangle, filter wide
// open, no detune, no modulation) instead of relying on the default patch, so they measure the pitch
// path and not the timbre. Each measurement gets a fresh plugin instance: an instrument with a
// release tail is not a clean slate, and this engine has no MIDI "all notes off" handler to force
// one. Two independent pitch estimators are reported, so a disagreement points at the measurement
// rather than at the synth.
//
// One check covers the single Detune parameter specifically: it renders both channels and requires
// the right channel to land N cents SHARP and the left N cents FLAT, which is that knob's whole
// contract. It runs at spread = 1.0 so the hard-panned oscillators stay separable.
//
// Usage: pos-clap-host-test /path/to/PlainOldSynth.clap
// Prints "ALL PASS" on success.

#include <clap/clap.h>

#include <dlfcn.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "PlainOldSynthParams.h"   // parameter ids/enums; header-only, nothing is linked from it

using namespace plainoldsynth;

namespace {

int gFailures = 0;

void check(bool ok, const std::string& what) {
    std::printf("  [%s] %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) ++gFailures;
}

constexpr double kSampleRate = 48000.0;

// --- minimal host -------------------------------------------------------------------------------
void hostNoop(const clap_host_t*) {}
const void* hostGetExtension(const clap_host_t*, const char*) { return nullptr; }

// --- minimal event list -------------------------------------------------------------------------
struct EventList {
    const clap_event_header_t** events = nullptr;
    uint32_t count = 0;

    static uint32_t size(const clap_input_events_t* list) {
        return static_cast<const EventList*>(list->ctx)->count;
    }
    static const clap_event_header_t* get(const clap_input_events_t* list, uint32_t index) {
        return static_cast<const EventList*>(list->ctx)->events[index];
    }
};

// --- pitch estimates ----------------------------------------------------------------------------
// Upward zero crossings: one per period, interpolated, very precise on a clean waveform but fooled
// by a waveform with extra crossings.
double estimateByZeroCrossings(const std::vector<float>& samples) {
    const size_t start = samples.size() * 2 / 5;
    double first = 0.0, last = 0.0;
    size_t crossings = 0;
    for (size_t i = start + 1; i < samples.size(); ++i) {
        const double a = samples[i - 1];
        const double b = samples[i];
        if (a <= 0.0 && b > 0.0) {
            const double position = double(i - 1) + (b != a ? (-a) / (b - a) : 0.0);
            if (crossings == 0) first = position;
            last = position;
            ++crossings;
        }
    }
    if (crossings < 3) return 0.0;
    const double span = last - first;
    return span > 0.0 ? (double(crossings - 1) / span) * kSampleRate : 0.0;
}

// Normalized autocorrelation: robust to waveform shape, coarser in pitch.
double estimateByAutocorrelation(const std::vector<float>& samples, double minHz, double maxHz) {
    const size_t start = samples.size() * 2 / 5;
    const size_t count = samples.size() - start;
    if (count < 64) return 0.0;
    const size_t minLag = size_t(kSampleRate / maxHz);
    const size_t maxLag = std::min(count - 1, size_t(kSampleRate / minHz));

    double best = 0.0;
    std::vector<double> correlation(maxLag + 1, 0.0);
    for (size_t lag = minLag; lag <= maxLag; ++lag) {
        double dot = 0.0, energyA = 0.0, energyB = 0.0;
        for (size_t i = 0; i + lag < count; ++i) {
            const double a = samples[start + i];
            const double b = samples[start + i + lag];
            dot += a * b;
            energyA += a * a;
            energyB += b * b;
        }
        correlation[lag] = dot / (std::sqrt(energyA * energyB) + 1e-30);
        best = std::max(best, correlation[lag]);
    }
    if (best <= 0.0) return 0.0;
    for (size_t lag = minLag; lag <= maxLag; ++lag)
        if (correlation[lag] >= 0.95 * best) return kSampleRate / double(lag);
    return 0.0;
}

// --- the library + plugin under test ------------------------------------------------------------
struct Library {
    void* handle = nullptr;
    const clap_plugin_entry_t* entry = nullptr;
    const clap_plugin_factory_t* factory = nullptr;
    const clap_plugin_descriptor_t* descriptor = nullptr;
    clap_host_t host{};
};

bool openLibrary(const std::string& path, Library& lib) {
    std::string binary = path;
    if (path.size() > 5 && path.compare(path.size() - 5, 5, ".clap") == 0) {
        const size_t slash = path.find_last_of('/');
        const std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
        binary = path + "/Contents/MacOS/" + name.substr(0, name.size() - 5);
    }
    lib.handle = dlopen(binary.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (lib.handle == nullptr) lib.handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (lib.handle == nullptr) return false;

    lib.entry = static_cast<const clap_plugin_entry_t*>(dlsym(lib.handle, "clap_entry"));
    if (lib.entry == nullptr || !lib.entry->init(path.c_str())) return false;

    lib.factory = static_cast<const clap_plugin_factory_t*>(
        lib.entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    if (lib.factory == nullptr) return false;
    lib.descriptor = lib.factory->get_plugin_descriptor(lib.factory, 0);
    if (lib.descriptor == nullptr) return false;

    lib.host = clap_host_t{};
    lib.host.clap_version = CLAP_VERSION_INIT;
    lib.host.name = "pos-clap-host-test";
    lib.host.vendor = "test";
    lib.host.url = "";
    lib.host.version = "1";
    lib.host.get_extension = hostGetExtension;
    lib.host.request_restart = hostNoop;
    lib.host.request_process = hostNoop;
    lib.host.request_callback = hostNoop;
    return true;
}

struct Instance {
    const clap_plugin_t* plugin = nullptr;

    bool create(Library& lib) {
        plugin = lib.factory->create_plugin(lib.factory, &lib.host, lib.descriptor->id);
        return plugin != nullptr && plugin->init(plugin)
            && plugin->activate(plugin, kSampleRate, 1, 4096);
    }
    void destroy() {
        if (plugin != nullptr) { plugin->deactivate(plugin); plugin->destroy(plugin); }
        plugin = nullptr;
    }
};

// Both output channels, so a check can look at the left and right oscillators separately. They are
// hard-panned (pan1 = -1, pan2 = +1), but FinalOutput's Spread is a mid/side width: at spread = 0
// the output collapses to mono and BOTH oscillators appear in BOTH channels. Channel isolation
// therefore only holds at spread = 1.0.
struct StereoAudio {
    std::vector<float> left, right;
};

StereoAudio renderStereo(const clap_plugin_t* plugin, uint32_t frames,
                         const clap_event_header_t** events, uint32_t eventCount) {
    StereoAudio audio;
    audio.left.assign(frames, 0.0f);
    audio.right.assign(frames, 0.0f);

    EventList holder;
    holder.events = events;
    holder.count = eventCount;
    clap_input_events_t in{};
    in.ctx = &holder;
    in.size = EventList::size;
    in.get = EventList::get;

    float* channels[2] = { audio.left.data(), audio.right.data() };
    clap_audio_buffer_t out{};
    out.data32 = channels;
    out.data64 = nullptr;
    out.channel_count = 2;
    out.latency = 0;          // clap_audio_buffer_t::latency is a plain uint32_t, not a pointer
    out.constant_mask = 0;

    clap_process_t process{};
    process.steady_time = -1;
    process.frames_count = frames;
    process.transport = nullptr;
    process.audio_inputs = nullptr;
    process.audio_inputs_count = 0;
    process.audio_outputs = &out;
    process.audio_outputs_count = 1;
    process.in_events = &in;
    process.out_events = nullptr;

    plugin->process(plugin, &process);
    return audio;
}

// Render `frames` with all events scheduled at frame 0, returning the left channel.
std::vector<float> render(const clap_plugin_t* plugin, uint32_t frames,
                          const clap_event_header_t** events, uint32_t eventCount) {
    return renderStereo(plugin, frames, events, eventCount).left;
}

// Install a controlled patch through the params extension: everything that could colour the pitch
// measurement is pinned down. `detune` is the one host parameter under test; `spread` defaults to 0
// (mono) because the pitch checks want a single unambiguous fundamental, and is set to 1.0 by the
// detune check so the two hard-panned oscillators stay separable.
void installTestPatch(const clap_plugin_t* plugin, double detune = 0.0, double spread = 0.0) {
    struct Holder { clap_event_param_value_t event{}; };
    std::vector<Holder> storage;

    auto add = [&storage](uint32_t index, double value) {
        Holder holder{};
        holder.event.header.size = sizeof(clap_event_param_value_t);
        holder.event.header.time = 0;
        holder.event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        holder.event.header.type = CLAP_EVENT_PARAM_VALUE;
        holder.event.param_id = paramIdForIndex(index);
        holder.event.value = value;
        storage.push_back(holder);
    };

    add(kParamShape, kShapeTriSaw);
    add(kParamPulseWidth, 0.0);
    add(kParamPeak, 0.5);          // symmetric triangle: the cleanest waveform to measure pitch on
    add(kParamCoarse, 0.0);
    add(kParamDetune, detune);
    add(kParamFine, 0.0);      // Osc Level is fixed now; pin Fine so the tuning is exactly C4
    add(kParamCutoff, 5.4);
    add(kParamResonance, 1.0);
    add(kParamKeyTrack, 0.0);
    add(kParamFilterAttack, 0.001); add(kParamFilterDecay, 0.001);
    add(kParamFilterSustain, 1.0);  add(kParamFilterRelease, 0.05);
    add(kParamAmpAttack, 0.001);    add(kParamAmpDecay, 0.001);
    add(kParamAmpSustain, 1.0);     add(kParamAmpRelease, 0.05);
    // Every modulation section points at Off, so nothing can reach the cutoff or the oscillator
    // pitch and colour these measurements. (Section 1 ships aimed at adsr1 -> cutoff, which used to
    // be the Filter Env Amount knob.) With all destinations Off the levels and sources cannot matter.
    for (int section = 0; section < kModSectionCount; ++section)
        add(modParamIndex(section, kModFieldDest), float(kModDestOff));
    add(kParamGlide, 0.0);
    add(kParamMasterLevel, 1.0);
    add(kParamSpread, spread);

    std::vector<const clap_event_header_t*> headers;
    headers.reserve(storage.size());
    for (Holder& holder : storage) headers.push_back(&holder.event.header);
    render(plugin, 64, headers.data(), uint32_t(headers.size()));
}

// Send a batch of parameter values and render a short block so the plugin applies them.
void sendParams(const clap_plugin_t* plugin, const uint32_t* indices, const double* values,
                size_t count)
{
    struct Holder { clap_event_param_value_t event{}; };
    std::vector<Holder> storage(count);
    std::vector<const clap_event_header_t*> headers;
    headers.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        storage[i].event.header.size = sizeof(clap_event_param_value_t);
        storage[i].event.header.time = 0;
        storage[i].event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        storage[i].event.header.type = CLAP_EVENT_PARAM_VALUE;
        storage[i].event.param_id = paramIdForIndex(indices[i]);
        storage[i].event.value = values[i];
        headers.push_back(&storage[i].event.header);
    }
    render(plugin, 64, headers.data(), uint32_t(headers.size()));
}

// Read a parameter back through the params extension. Returns -1 when unavailable.
double readParam(const clap_plugin_t* plugin, uint32_t index)
{
    const auto* params = static_cast<const clap_plugin_params_t*>(
        plugin->get_extension(plugin, CLAP_EXT_PARAMS));
    if (params == nullptr || params->get_value == nullptr)
        return -1.0;
    double value = 0.0;
    return params->get_value(plugin, paramIdForIndex(index), &value) ? value : -1.0;
}

// Spread of successive zero-crossing intervals, relative to the longest: ~0 for a steady pitch and
// large when something is actually bending the oscillator. This is what makes "the section moved its
// destination" measurable without inspecting the graph.
double crossingIntervalSpread(const std::vector<float>& samples)
{
    const size_t start = samples.size() / 4;
    double previous = 0.0, minGap = 1e30, maxGap = 0.0;
    bool have = false;
    for (size_t i = start + 1; i < samples.size(); ++i) {
        const double a = samples[i - 1];
        const double b = samples[i];
        if (a <= 0.0 && b > 0.0) {
            const double position = double(i - 1) + (b != a ? (-a) / (b - a) : 0.0);
            if (have) {
                const double gap = position - previous;
                minGap = std::min(minGap, gap);
                maxGap = std::max(maxGap, gap);
            }
            previous = position;
            have = true;
        }
    }
    return maxGap > 0.0 ? (maxGap - minGap) / maxGap : 0.0;
}

// A MIDI 2.0 channel-voice-2 NoteOn UMP with an optional 7.9-pitch attribute.
clap_event_midi2_t makeUmpNoteOn(uint8_t note, uint16_t velocity, uint8_t attributeType,
                                 uint16_t attribute) {
    const uint32_t words[2] = {
        (0x4u << 28) | (0x9u << 20) | (uint32_t(note) << 8) | uint32_t(attributeType),
        (uint32_t(velocity) << 16) | uint32_t(attribute),
    };
    clap_event_midi2_t event{};
    event.header.size = sizeof(event);
    event.header.time = 0;
    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    event.header.type = CLAP_EVENT_MIDI2;
    event.port_index = 0;
    std::memcpy(event.data, words, sizeof(words));
    return event;
}

clap_event_midi_t makeMidi1(uint8_t status, uint8_t data1, uint8_t data2) {
    clap_event_midi_t event{};
    event.header.size = sizeof(event);
    event.header.time = 0;
    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    event.header.type = CLAP_EVENT_MIDI;
    event.port_index = 0;
    event.data[0] = status; event.data[1] = data1; event.data[2] = data2;
    return event;
}

void reportPitchAndCheck(const std::vector<float>& audio, double expected, const char* what,
                         double tolerancePercent) {
    double peak = 0.0;
    bool finite = true;
    for (float sample : audio) {
        peak = std::max(peak, std::fabs(double(sample)));
        if (!std::isfinite(sample)) finite = false;
    }
    check(finite, std::string(what) + ": output is finite");
    check(peak > 0.01, std::string(what) + ": produces audio (peak " + std::to_string(peak) + ")");

    const double byCrossings = estimateByZeroCrossings(audio);
    const double byAutocorrelation = estimateByAutocorrelation(audio, expected * 0.6, expected * 1.6);
    std::printf("        expected %.3f Hz | zero-crossings %.3f Hz | autocorrelation %.3f Hz\n",
                expected, byCrossings, byAutocorrelation);

    const double errorPercent = 100.0 * std::fabs(byCrossings - expected) / expected;
    check(errorPercent < tolerancePercent,
          std::string(what) + ": pitch is within " + std::to_string(tolerancePercent) + "% ("
              + std::to_string(byCrossings) + " Hz vs " + std::to_string(expected) + " Hz)");
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: %s <path to PlainOldSynth.clap>\n", argv[0]);
        return 2;
    }
    const std::string given = argv[1];
    std::printf("PlainOldSynth CLAP host test\n  library: %s\n", given.c_str());

    Library lib;
    check(openLibrary(given, lib), "dlopen + clap_entry + factory + descriptor");
    if (lib.handle == nullptr) return 1;
    std::printf("  plugin: %s (%s)\n", lib.descriptor->name, lib.descriptor->id);

    // --- the note port must offer (and prefer) the MIDI 2.0 dialect ------------------------------
    {
        Instance instance;
        check(instance.create(lib), "create/init/activate");
        const auto* notePorts = static_cast<const clap_plugin_note_ports_t*>(
            instance.plugin->get_extension(instance.plugin, CLAP_EXT_NOTE_PORTS));
        check(notePorts != nullptr, "implements note-ports");
        clap_note_port_info_t portInfo{};
        const bool havePort = notePorts != nullptr && notePorts->count(instance.plugin, true) > 0
                           && notePorts->get(instance.plugin, 0, true, &portInfo);
        check(havePort, "has an input note port");
        if (havePort) {
            check((portInfo.supported_dialects & CLAP_NOTE_DIALECT_MIDI2) != 0,
                  "advertises the MIDI 2.0 dialect");
            check(portInfo.preferred_dialect == CLAP_NOTE_DIALECT_MIDI2,
                  "prefers the MIDI 2.0 dialect");
            check((portInfo.supported_dialects & CLAP_NOTE_DIALECT_MIDI) != 0,
                  "still accepts MIDI 1.0 hosts");
        }
        instance.destroy();
    }

    // --- MIDI 2.0 NoteOn with a 7.9-pitch attribute: C4 raised by a quarter tone ------------------
    std::printf("MIDI 2.0 7.9-pitch note through the real plugin:\n");
    {
        Instance instance;
        check(instance.create(lib), "fresh instance");
        installTestPatch(instance.plugin);

        const uint16_t attribute = uint16_t((60 << 9) | 256);   // 256/512 = half a semitone
        clap_event_midi2_t noteOn = makeUmpNoteOn(60, 0xFFFF, 0x3 /* Pitch 7.9 */, attribute);
        const clap_event_header_t* events[] = { &noteOn.header };
        const std::vector<float> audio = render(instance.plugin, uint32_t(kSampleRate), events, 1);

        const double expected = 440.0 * std::pow(2.0, (60.0 - 69.0) / 12.0)
                                     * std::pow(2.0, 256.0 / 6144.0);
        reportPitchAndCheck(audio, expected, "7.9 note", 0.5);
        instance.destroy();
    }

    // --- a MIDI 1.0 note must still work, on its own instance -------------------------------------
    std::printf("MIDI 1.0 note through the real plugin:\n");
    {
        Instance instance;
        check(instance.create(lib), "fresh instance");
        installTestPatch(instance.plugin);

        clap_event_midi_t midi1 = makeMidi1(0x90, 60, 100);
        const clap_event_header_t* events[] = { &midi1.header };
        const std::vector<float> audio = render(instance.plugin, uint32_t(kSampleRate), events, 1);

        const double expected = 440.0 * std::pow(2.0, (60.0 - 69.0) / 12.0);
        reportPitchAndCheck(audio, expected, "MIDI 1.0 note", 0.5);
        instance.destroy();
    }

    // --- a bare CLAP note event (the CLAP dialect) is the third path a host may choose ------------
    std::printf("CLAP-dialect note event:\n");
    {
        Instance instance;
        check(instance.create(lib), "fresh instance");
        installTestPatch(instance.plugin);

        clap_event_note_t note{};
        note.header.size = sizeof(note);
        note.header.time = 0;
        note.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        note.header.type = CLAP_EVENT_NOTE_ON;
        note.port_index = 0;
        note.channel = 0;
        note.key = 69;              // A4 = 440 Hz, the tuning reference
        note.velocity = 0.8;
        const clap_event_header_t* events[] = { &note.header };
        const std::vector<float> audio = render(instance.plugin, uint32_t(kSampleRate), events, 1);

        reportPitchAndCheck(audio, 440.0, "CLAP note", 0.5);
        instance.destroy();
    }

    // --- Detune is ONE parameter, applied symmetrically to the two hard-panned oscillators --------
    std::printf("Detune orientation through the real plugin:\n");
    {
        Instance instance;
        check(instance.create(lib), "fresh instance");
        const double detune = 7.0;
        // spread = 1.0 is what keeps the oscillators hard-panned; the pitch checks above run at
        // spread = 0, where the output is mono and both oscillators sit in both channels.
        installTestPatch(instance.plugin, detune, 1.0);

        clap_event_midi_t midi1 = makeMidi1(0x90, 60, 100);
        const clap_event_header_t* events[] = { &midi1.header };
        const StereoAudio audio = renderStereo(instance.plugin, uint32_t(kSampleRate), events, 1);

        const double base = 440.0 * std::pow(2.0, (60.0 - 69.0) / 12.0);
        const double expectedRight = base * std::pow(2.0,  detune / 1200.0);
        const double expectedLeft  = base * std::pow(2.0, -detune / 1200.0);

        const double leftHz  = estimateByZeroCrossings(audio.left);
        const double rightHz = estimateByZeroCrossings(audio.right);
        std::printf("        detune %+.1f ct | left %.3f Hz (want %.3f) | right %.3f Hz (want %.3f)\n",
                    detune, leftHz, expectedLeft, rightHz, expectedRight);

        check(rightHz > leftHz, "right channel is SHARP of the left (right up, left down)");
        check(100.0 * std::fabs(rightHz - expectedRight) / expectedRight < 0.5,
              "right channel is +N cents (" + std::to_string(rightHz) + " Hz vs "
                  + std::to_string(expectedRight) + " Hz)");
        check(100.0 * std::fabs(leftHz - expectedLeft) / expectedLeft < 0.5,
              "left channel is -N cents (" + std::to_string(leftHz) + " Hz vs "
                  + std::to_string(expectedLeft) + " Hz)");
        instance.destroy();
    }

    // --- Fine is the fractional half of the same engine tuning as Coarse -------------------------
    std::printf("Fine pitch:\n");
    {
        Instance instance;
        check(instance.create(lib), "fresh instance");
        installTestPatch(instance.plugin);          // Coarse 0, Fine 0 -> exactly C4

        const uint32_t idx[] = { kParamFine };
        const double   val[] = { 100.0 };
        sendParams(instance.plugin, idx, val, 1);

        clap_event_midi_t midi1 = makeMidi1(0x90, 60, 100);
        const clap_event_header_t* events[] = { &midi1.header };
        const std::vector<float> audio = render(instance.plugin, uint32_t(kSampleRate), events, 1);

        // +100 cents is one semitone: C4 -> C#4. This also proves Fine recomputes the shared tuning
        // parameter from Coarse rather than overwriting it.
        reportPitchAndCheck(audio, 440.0 * std::pow(2.0, (61.0 - 69.0) / 12.0), "Fine +100 ct", 0.5);
        instance.destroy();
    }

    // --- Modulation sections: destinations are exclusive, and the routing is audible ------------
    std::printf("Modulation sections:\n");
    {
        Instance instance;
        check(instance.create(lib), "fresh instance");
        installTestPatch(instance.plugin);          // every section Off

        // Two sections ask for the SAME destination. One module input takes one source, so the later
        // request keeps it and the earlier section is switched back to Off rather than left
        // displaying a route it does not have.
        const uint32_t clashIdx[] = { modParamIndex(0, kModFieldDest),
                                      modParamIndex(1, kModFieldDest) };
        const double clashVal[] = { double(kModDestPulseWidth), double(kModDestPulseWidth) };
        sendParams(instance.plugin, clashIdx, clashVal, 2);

        check(int(readParam(instance.plugin, modParamIndex(1, kModFieldDest)) + 0.5) == kModDestPulseWidth,
              "the section that claimed the destination keeps it");
        check(int(readParam(instance.plugin, modParamIndex(0, kModFieldDest)) + 0.5) == kModDestOff,
              "the section that already had it is switched back to Off");
        instance.destroy();
    }

    // The routing must be audible, not merely stored: LFO 1 -> oscillator pitch at full level,
    // measured against the same patch with every section Off.
    {
        double spreadOff = 0.0, spreadOn = 0.0;
        for (int routed = 0; routed <= 1; ++routed) {
            Instance instance;
            check(instance.create(lib), "fresh instance");
            installTestPatch(instance.plugin);
            if (routed) {
                const uint32_t idx[] = { modParamIndex(0, kModFieldSourceA),
                                         modParamIndex(0, kModFieldLevelA),
                                         modParamIndex(0, kModFieldDest) };
                const double val[] = { double(kModSourceALfo1), 1.0, double(kModDestOscPitch) };
                sendParams(instance.plugin, idx, val, 3);
            }

            clap_event_midi_t midi1 = makeMidi1(0x90, 60, 100);
            const clap_event_header_t* events[] = { &midi1.header };
            // Two seconds against the LFO's 1 Hz default, so the pitch has time to swing.
            const std::vector<float> audio =
                render(instance.plugin, uint32_t(kSampleRate * 2), events, 1);

            double peak = 0.0;
            for (float sample : audio) peak = std::max(peak, std::fabs(double(sample)));
            check(peak > 0.01, routed ? "routed patch produces audio" : "unrouted patch produces audio");

            if (routed) spreadOn = crossingIntervalSpread(audio);
            else        spreadOff = crossingIntervalSpread(audio);
            instance.destroy();
        }
        std::printf("        zero-crossing interval spread: unmodulated %.4f | LFO1 -> pitch %.4f\n",
                    spreadOff, spreadOn);
        check(spreadOff < 0.05, "with every section Off the pitch is steady");
        check(spreadOn > 0.2, "routing LFO 1 to oscillator pitch actually bends the pitch");
    }

    lib.entry->deinit();
    dlclose(lib.handle);

    if (gFailures != 0) {
        std::printf("%d check(s) FAILED\n", gFailures);
        return 1;
    }
    std::printf("ALL PASS\n");
    return 0;
}
