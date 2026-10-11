// tools/mpe_test_sender.cpp — a scripted MIDI 1.0 MPE source for listening to PlainOldSynth's MPE
// switch (macOS, CoreMIDI).
//
// It publishes a virtual MIDI source ("PlainOldSynth MPE Test"), or sends to an existing destination
// with --dest, and plays short tests: one note per member channel, a slide on one note, the
// master-channel pitch wheel, pressure and CC74 on one note and then zone-wide, the same key on two
// channels, a quarter-tone scale, and a bend-range (RPN 0) change. Before each it prints what you
// should hear with PlainOldSynth's MPE switch ON and OFF — the difference is the test.
//
// Layout is the MPE lower zone, as PlainOldSynth assumes: master = channel 1, members = 2..16.
//
// Every test starts and ends with a reset that includes the MPE Configuration Message, which puts any
// bend range a test set back to the receiver's defaults, so the tests can run in any order, in either
// mode, without restarting the plugin.
//
// Usage:
//   pos-mpe-sender [--list] [--dest NAME] [--test NAME[,NAME...]] [--member-range N]
//                  [--no-wait] [--loop]
// Tests: chord, slide, wheel, pressure, timbre, samekey, microtonal, rpn. Default: all of them, in
// that order; --test runs the ones named, in the order named. (--scenario is an alias of --test.)

#include <CoreFoundation/CoreFoundation.h>
#include <CoreMIDI/CoreMIDI.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr const char* kSourceName = "PlainOldSynth MPE Test";
constexpr uint8_t kMaster = 0;   // channel 1 (0-based)

std::atomic<bool> gStop{false};

std::string endpointName(MIDIEndpointRef endpoint) {
    CFStringRef name = nullptr;
    if (MIDIObjectGetStringProperty(endpoint, kMIDIPropertyDisplayName, &name) != noErr || name == nullptr)
        return "?";
    char buffer[256] = {};
    CFStringGetCString(name, buffer, sizeof(buffer), kCFStringEncodingUTF8);
    CFRelease(name);
    return buffer;
}

// --- output: a virtual source, or a port to one destination --------------------------------------
class Output {
public:
    bool openVirtualSource() {
        if (!createClient()) return false;
        CFStringRef name = CFStringCreateWithCString(nullptr, kSourceName, kCFStringEncodingUTF8);
        const OSStatus status = MIDISourceCreateWithProtocol(client_, name, kMIDIProtocol_1_0, &source_);
        CFRelease(name);
        if (status != noErr) { std::fprintf(stderr, "MIDISourceCreateWithProtocol failed (%d)\n", int(status)); return false; }
        // A fixed unique id, so a host that remembers sources by id finds it again next launch.
        MIDIObjectSetIntegerProperty(source_, kMIDIPropertyUniqueID, 0x504F534D);   // 'POSM'
        return true;
    }

    bool openDestination(const std::string& match) {
        if (!createClient()) return false;
        for (ItemCount i = 0; i < MIDIGetNumberOfDestinations(); ++i) {
            const MIDIEndpointRef endpoint = MIDIGetDestination(i);
            if (endpointName(endpoint).find(match) != std::string::npos) {
                destination_ = endpoint;
                break;
            }
        }
        if (destination_ == 0) { std::fprintf(stderr, "no MIDI destination matches \"%s\" (try --list)\n", match.c_str()); return false; }
        if (MIDIOutputPortCreate(client_, CFSTR("out"), &port_) != noErr) return false;
        std::printf("Sending to destination \"%s\".\n", endpointName(destination_).c_str());
        return true;
    }

    // One MIDI 1.0 channel-voice message as a UMP word (message type 2, group 0).
    void send(uint8_t status, uint8_t data1, uint8_t data2) {
        const UInt32 word = (0x2u << 28) | (UInt32(status) << 16) | (UInt32(data1 & 0x7F) << 8) | UInt32(data2 & 0x7F);
        MIDIEventList list;
        MIDIEventPacket* packet = MIDIEventListInit(&list, kMIDIProtocol_1_0);
        packet = MIDIEventListAdd(&list, sizeof(list), packet, 0, 1, &word);
        if (packet == nullptr) return;
        if (source_ != 0) MIDIReceivedEventList(source_, &list);
        else              MIDISendEventList(port_, destination_, &list);
    }

    ~Output() {
        if (source_ != 0) MIDIEndpointDispose(source_);
        if (client_ != 0) MIDIClientDispose(client_);
    }

private:
    bool createClient() {
        if (client_ != 0) return true;
        const OSStatus status = MIDIClientCreate(CFSTR("pos-mpe-sender"), nullptr, nullptr, &client_);
        if (status != noErr) { std::fprintf(stderr, "MIDIClientCreate failed (%d)\n", int(status)); return false; }
        return true;
    }

    MIDIClientRef   client_ = 0;
    MIDIEndpointRef source_ = 0;
    MIDIPortRef     port_ = 0;
    MIDIEndpointRef destination_ = 0;
};

// --- MIDI 1.0 / MPE vocabulary ----------------------------------------------------------------------
class Mpe {
public:
    Mpe(Output& out, double memberRange) : out_(out), memberRange_(memberRange) {}

    void noteOn(uint8_t ch, uint8_t note, uint8_t velocity = 100) { out_.send(uint8_t(0x90 | ch), note, velocity); }
    void noteOff(uint8_t ch, uint8_t note) { out_.send(uint8_t(0x80 | ch), note, 0); }
    void cc(uint8_t ch, uint8_t controller, uint8_t value) { out_.send(uint8_t(0xB0 | ch), controller, value); }
    void pressure(uint8_t ch, uint8_t value) { out_.send(uint8_t(0xD0 | ch), value, 0); }

    // Pitch bend in semitones at the range the receiver is expected to use for that channel:
    // ±memberRange on members, ±2 on the master (the MPE defaults).
    void bend(uint8_t ch, double semitones) {
        const double range = (ch == kMaster) ? 2.0 : memberRange_;
        const long value = std::lround(8192.0 + semitones / range * 8192.0);
        const uint16_t v = uint16_t(std::clamp(value, 0L, 16383L));
        out_.send(uint8_t(0xE0 | ch), uint8_t(v & 0x7F), uint8_t(v >> 7));
    }

    // RPN 0 (pitch bend sensitivity), whole semitones, then the RPN null so later CC 6s are inert.
    void bendRange(uint8_t ch, uint8_t semitones) {
        cc(ch, 101, 0); cc(ch, 100, 0); cc(ch, 6, semitones); cc(ch, 38, 0);
        cc(ch, 101, 127); cc(ch, 100, 127);
    }

    // The MPE Configuration Message: RPN 6 on the master channel, data = member-channel count. Per
    // MPE it also resets every bend range in the zone to its default (±48 members, ±2 master), which
    // is what reset() relies on.
    void mpeConfiguration(uint8_t members = 15) {
        cc(kMaster, 101, 0); cc(kMaster, 100, 6); cc(kMaster, 6, members);
        cc(kMaster, 101, 127); cc(kMaster, 100, 127);
    }

    // Move a channel's bend from `from` to `to` semitones over `ms`.
    void glideBend(uint8_t ch, double from, double to, int ms) {
        ramp(ms, [&](double t) { bend(ch, from + (to - from) * t); });
    }
    void rampPressure(uint8_t ch, int from, int to, int ms) {
        ramp(ms, [&](double t) { pressure(ch, uint8_t(std::lround(from + (to - from) * t))); });
    }
    void rampCC(uint8_t ch, uint8_t controller, int from, int to, int ms) {
        ramp(ms, [&](double t) { cc(ch, controller, uint8_t(std::lround(from + (to - from) * t))); });
    }

    // Everything back to rest: notes off, bends centred, pressure and CC74 at 0, and bend ranges back
    // to the receiver's defaults (the MPE Configuration Message), so no test depends on another.
    void reset() {
        for (uint8_t ch = 0; ch < 16; ++ch) {
            cc(ch, 123, 0);   // all notes off
            bend(ch, 0.0);
            pressure(ch, 0);
            cc(ch, 74, 0);
        }
        mpeConfiguration();
    }

    double memberRange() const { return memberRange_; }

private:
    void ramp(int ms, const std::function<void(double)>& step) {
        const int steps = std::max(1, ms / 10);
        for (int i = 0; i <= steps && !gStop; ++i) {
            step(double(i) / steps);
            std::this_thread::sleep_for(std::chrono::milliseconds(ms / steps));
        }
    }

    Output& out_;
    double memberRange_;
};

void pause(int ms) {
    for (int waited = 0; waited < ms && !gStop; waited += 20)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

void header(const char* name, const char* plays, const char* mpeOn, const char* mpeOff) {
    std::printf("\n== %s ==\n   plays:   %s\n   MPE ON:  %s\n   MPE OFF: %s\n", name, plays, mpeOn, mpeOff);
    std::fflush(stdout);
    pause(1200);
}

// --- tests ----------------------------------------------------------------------------------------
// The chord used by several tests: C4 / E4 / G4 on member channels 2 / 3 / 4.
constexpr uint8_t kChC = 1, kChE = 2, kChG = 3;
void chordOn(Mpe& m)  { m.noteOn(kChC, 60); m.noteOn(kChE, 64); m.noteOn(kChG, 67); }
void chordOff(Mpe& m) { m.noteOff(kChC, 60); m.noteOff(kChE, 64); m.noteOff(kChG, 67); }

void chord(Mpe& m) {
    header("chord", "C4 E4 G4, each on its own channel (2, 3, 4), held 2 s",
           "a plain C major chord", "the same: notes alone don't depend on MPE");
    chordOn(m); pause(2000); chordOff(m); pause(600);
}

void slide(Mpe& m) {
    header("slide", "the chord; E4's channel (3) bends +2 semitones over 1 s, holds, returns",
           "only E4 slides up to F#4 (a C-F#-G cluster), C and G stay put",
           "E4 barely moves (a +2 slide meant for a ±48 range is ~0.08 st at ±2)");
    chordOn(m); pause(600);
    m.glideBend(kChE, 0.0, 2.0, 1000); pause(800);
    m.glideBend(kChE, 2.0, 0.0, 1000); pause(400);
    chordOff(m); pause(600);
}

void wheel(Mpe& m) {
    header("wheel", "the chord; channel 1 (master) bends up a whole tone over 1 s, holds, returns",
           "the whole chord rises to D major and comes back",
           "nothing moves: channel 1's bend only bends notes played on channel 1");
    chordOn(m); pause(600);
    m.glideBend(kMaster, 0.0, 2.0, 1000); pause(800);
    m.glideBend(kMaster, 2.0, 0.0, 1000); pause(400);
    chordOff(m); pause(600);
}

void pressureScenario(Mpe& m) {
    header("pressure", "the chord; channel pressure 0->127->0 on G4's channel (4), then on channel 1",
           "needs a Mod section with In B = Pressure -> Cutoff (Level B up). First only G4 brightens;\n"
           "            then all three brighten together (channel 1 pressure reaches every note)",
           "first only G4 brightens; then nothing (channel 1 has no notes)");
    chordOn(m); pause(600);
    m.rampPressure(kChG, 0, 127, 1000); m.rampPressure(kChG, 127, 0, 1000); pause(500);
    m.rampPressure(kMaster, 0, 127, 1000); m.rampPressure(kMaster, 127, 0, 1000); pause(400);
    chordOff(m); pause(600);
}

void timbre(Mpe& m) {
    header("timbre", "the chord; CC74 0->127->0 on C4's channel (2), then on channel 1",
           "needs a Mod section with In B = Timbre -> Cutoff (or Peak). First only C4 changes;\n"
           "            then all three change together",
           "first only C4 changes; then nothing");
    chordOn(m); pause(600);
    m.rampCC(kChC, 74, 0, 127, 1000); m.rampCC(kChC, 74, 127, 0, 1000); pause(500);
    m.rampCC(kMaster, 74, 0, 127, 1000); m.rampCC(kMaster, 74, 127, 0, 1000); pause(400);
    chordOff(m); pause(600);
}

void samekey(Mpe& m) {
    header("samekey", "C4 on channel 5 and C4 on channel 6; channel 6 bends +12 over 1.5 s",
           "one C4 stays, the other climbs an octave: two voices, bent independently",
           "the second C4 rises only half a semitone (still two voices: notes match on channel too)");
    m.noteOn(4, 60); m.noteOn(5, 60); pause(600);
    m.glideBend(5, 0.0, 12.0, 1500); pause(1000);
    m.noteOff(4, 60); m.noteOff(5, 60); m.bend(5, 0.0); pause(600);
}

void microtonal(Mpe& m) {
    header("microtonal", "a 24-EDO (quarter-tone) scale C4..C5, each note on the next channel, its bend sent "
                         "before its note-on",
           "25 distinct quarter-tone steps",
           "12-EDO with every other note repeated (quarter-tone bends are too small at ±2)");
    uint8_t ch = 1;
    for (int step = 0; step <= 24 && !gStop; ++step) {
        const uint8_t key = uint8_t(60 + step / 2);
        const double offset = (step % 2) ? 0.5 : 0.0;
        m.bend(ch, offset);
        m.noteOn(ch, key);
        pause(300);
        m.noteOff(ch, key);
        m.bend(ch, 0.0);
        ch = (ch == 15) ? 1 : uint8_t(ch + 1);
    }
    pause(600);
}

void rpn(Mpe& m) {
    header("rpn", "RPN 0 = 24 sent on channel 2 only; then E4 on channel 3 bends by a value meaning +12 at ±24",
           "E4 climbs an octave: RPN 0 on one member channel sets them all",
           "E4 rises 1 semitone (channel 3 is still at ±2)");
    m.bendRange(1, 24);
    m.noteOn(kChE, 64); pause(600);
    // +12 at ±24 is half the bend range: 8192 + 4096.
    const double asIfMember = 12.0 / 24.0 * m.memberRange();
    m.glideBend(kChE, 0.0, asIfMember, 1500); pause(1000);
    m.noteOff(kChE, 64); m.bend(kChE, 0.0);
    pause(600);   // the reset after every test puts the range back (MPE Configuration Message)
}

struct Scenario { const char* name; void (*run)(Mpe&); };
constexpr Scenario kScenarios[] = {
    {"chord", chord}, {"slide", slide}, {"wheel", wheel}, {"pressure", pressureScenario},
    {"timbre", timbre}, {"samekey", samekey}, {"microtonal", microtonal}, {"rpn", rpn},
};

void listEndpoints() {
    std::printf("MIDI destinations (use with --dest):\n");
    for (ItemCount i = 0; i < MIDIGetNumberOfDestinations(); ++i)
        std::printf("  %s\n", endpointName(MIDIGetDestination(i)).c_str());
}

void usage() {
    std::printf("usage: pos-mpe-sender [--list] [--dest NAME] [--test NAME[,NAME...]]\n"
                "                      [--member-range N] [--no-wait] [--loop]\n"
                "tests (default: all, in this order; --test runs the named ones in the order named):\n ");
    for (const Scenario& s : kScenarios) std::printf(" %s", s.name);
    std::printf("\n");
}

const Scenario* findScenario(const std::string& name) {
    for (const Scenario& s : kScenarios)
        if (name == s.name) return &s;
    return nullptr;
}

} // namespace

int main(int argc, char** argv) {
    std::string dest, only;
    double memberRange = 48.0;
    bool wait = true, loop = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&]() -> std::string { return (i + 1 < argc) ? argv[++i] : std::string(); };
        if (arg == "--list")              { listEndpoints(); return 0; }
        else if (arg == "--dest")         dest = next();
        else if (arg == "--test" || arg == "--scenario") only = next();
        else if (arg == "--member-range") memberRange = std::atof(next().c_str());
        else if (arg == "--no-wait")      wait = false;
        else if (arg == "--loop")         loop = true;
        else                              { usage(); return arg == "--help" ? 0 : 2; }
    }
    if (!(memberRange > 0.0)) { std::fprintf(stderr, "--member-range must be positive\n"); return 2; }

    std::vector<const Scenario*> run;
    if (only.empty()) {
        for (const Scenario& s : kScenarios) run.push_back(&s);
    } else {
        size_t start = 0;
        while (start <= only.size()) {
            const size_t comma = only.find(',', start);
            const std::string name = only.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
            if (!name.empty()) {
                const Scenario* s = findScenario(name);
                if (s == nullptr) { std::fprintf(stderr, "unknown test \"%s\"\n", name.c_str()); usage(); return 2; }
                run.push_back(s);
            }
            if (comma == std::string::npos) break;
            start = comma + 1;
        }
    }
    if (run.empty()) { usage(); return 2; }

    Output out;
    if (dest.empty() ? !out.openVirtualSource() : !out.openDestination(dest)) return 1;
    Mpe mpe(out, memberRange);

    std::signal(SIGINT, [](int) { gStop = true; });

    std::printf("PlainOldSynth MPE test sender. Lower zone: master = channel 1, members = 2..16;\n"
                "member bends assume a ±%.0f range. In PlainOldSynth, run each test with the Global\n"
                "panel's MPE switch ON and again OFF.\n", memberRange);
    if (dest.empty() && wait) {
        std::printf("\nConnect your host to the MIDI source \"%s\" (StageHand: pick it in a row's input\n"
                    "menu, or start StageHand with --midi all), then press Return.", kSourceName);
        std::fflush(stdout);
        std::getchar();
    }

    mpe.reset();
    do {
        for (const Scenario* s : run) {
            if (gStop) break;
            s->run(mpe);
            mpe.reset();
        }
    } while (loop && !gStop);

    mpe.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));   // let the resets drain
    std::printf("\ndone\n");
    return 0;
}
