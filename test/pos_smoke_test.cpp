// test/pos_smoke_test.cpp — headless verification of PlainOldSynth's voice graph and, in
// particular, of the MIDI 2.0 7.9-pitch note path, with no DAW, no CLAP host and no GUI.
//
// It drives the same VoiceManager the plugin uses (VoiceManager::buildDefaultPatch wires the graph,
// so this exercises the real thing) and checks:
//   1. the graph produces audio at all (note-on → ADSR2 opens the VCA → signal reaches the output)
//   2. a MIDI 1.0 note-on sounds at the expected equal-tempered pitch
//   3. a MIDI 2.0 7.9-pitch note-on sounds at the expected MICROTONAL pitch
//      (note + fraction/512 semitones — the whole point of preferring the MIDI2 dialect)
//   4. a percussive envelope decays to silence and the voice frees itself
//   5. poly vs mono voice counts behave (mono keeps a held-note stack instead of stealing)
//
// Run: ctest --test-dir build --output-on-failure
// Prints "ALL PASS" on success; ctest looks for exactly that.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "PlainOldSynthParams.h"
#include "VoiceManager.hpp"
#include "pm_midi.h"

using namespace plainoldsynth;

namespace {

int gFailures = 0;

void check(bool ok, const std::string& what) {
    std::printf("  [%s] %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) ++gFailures;
}

constexpr double kSampleRate = 48000.0;

// Pitch estimate from interpolated upward zero crossings over the settled part of the buffer.
// One upward crossing per period for any periodic waveform, and counting across ~0.5 s makes the
// estimate far more precise than integer-lag autocorrelation (which quantizes to 48000/lag).
double estimateFrequency(const std::vector<float>& samples, double /*minHz*/, double /*maxHz*/) {
    const size_t start = samples.size() * 2 / 5;
    if (samples.size() < start + 64) return 0.0;

    double first = 0.0, last = 0.0;
    size_t crossings = 0;
    for (size_t i = start + 1; i < samples.size(); ++i) {
        const double a = samples[i - 1];
        const double b = samples[i];
        if (a <= 0.0 && b > 0.0) {
            // Linear interpolation of the exact crossing point.
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

double peak(const std::vector<float>& samples) {
    double value = 0.0;
    for (float s : samples) value = std::max(value, std::fabs(double(s)));
    return value;
}

double rms(const std::vector<float>& samples) {
    double sum = 0.0;
    for (float s : samples) sum += double(s) * double(s);
    return std::sqrt(sum / std::max<size_t>(1, samples.size()));
}

// Everything the plugin itself sets up that is not a host parameter (src/plugin.cpp's
// initPatchConstants), plus the handful of host parameters the checks below depend on. Kept
// deliberately small and explicit — if the graph stops producing sound, this is what to compare
// against the plugin.
void configureVoiceManager(VoiceManager& vm) {
    auto midi = [](MIDIParamItem item) {
        return MODULE_PARAM_ADDRESS(ModuleParamKindMIDI, ModuleInstance1, item);
    };
    vm.setVoiceManagerParameter(midi(MIDIParamItemVoiceCount), 10.0f);
    vm.setVoiceManagerParameter(midi(MIDIParamItemChannelFilterValue), 0.0f);
    vm.setVoiceManagerParameter(midi(MIDIParamItemGroupFilterValue), 0.0f);
    vm.setVoiceManagerParameter(midi(MIDIParamItemVoiceStealAlgorithm), float(VoiceStealAlgorithmOldest));
    vm.setVoiceManagerParameter(midi(MIDIParamItemFreeRun), 0.0f);
    vm.setVoiceManagerParameter(midi(MIDIParamItemLegato), 0.0f);

    auto set = [&vm](AUParameterAddress base, uint8_t item, float value) {
        vm.setParameter(MODULE_PARAM_FROM_BASE_AND_ITEM(base, item), value);
    };

    // Shape = Tri: symmetric triangle on the raw path, pulse path muted.
    set(posTriSaw1, TriSawParamItemPeak, 0.5f);
    set(posTriSaw2, TriSawParamItemPeak, 0.5f);
    set(posLevelRaw1, LevelParamItemLevel, 1.0f);
    set(posLevelRaw2, LevelParamItemLevel, 1.0f);
    set(posLevelPulse1, LevelParamItemLevel, 0.0f);
    set(posLevelPulse2, LevelParamItemLevel, 0.0f);
    set(posLevelRaw1, LevelParamItemInvertLeft, 0.0f);
    set(posLevelRaw2, LevelParamItemInvertRight, 0.0f);

    // No detune, no coarse offset: the checks compare against exact note frequencies.
    set(posTriSaw1, TriSawParamItemPitch, 0.0f);
    set(posTriSaw2, TriSawParamItemPitch, 0.0f);
    set(posTriSaw1, TriSawParamItemPitchFine, 0.0f);
    set(posTriSaw2, TriSawParamItemPitchFine, 0.0f);
    set(posTriSaw1, TriSawParamItemFreqModLevel, 0.0f);   // no vibrato
    set(posTriSaw2, TriSawParamItemFreqModLevel, 0.0f);

    // Hard pans + adders, exactly as the plugin sets them.
    set(posPan1, PanParamItemPosition, -1.0f);
    set(posPan2, PanParamItemPosition, 1.0f);
    set(posCombineOsc1, CombineParamItemType, float(ParamItemCombineType_Add));
    set(posCombineOsc2, CombineParamItemType, float(ParamItemCombineType_Add));
    set(posCombineMix, CombineParamItemType, float(ParamItemCombineType_Add));
    set(posCombineVca, CombineParamItemType, float(ParamItemCombineType_Multiply));
    set(posCombineMix, CombineParamItemLevel, 0.8f);

    // Filter wide open and unmodulated.
    set(posLpf1, LPFParamItemPitch, 5.4f);
    set(posLpf1, LPFParamItemQ, 1.0f);
    set(posLpf1, LPFParamItemFreqModLevel, 0.0f);
    set(posLpf1, LPFParamItemFreqModLevel2, 0.0f);

    // Instant glide.
    set(posSlew1, SlewParamItemRise, 1e-8f);
    set(posSlew1, SlewParamItemFall, 1e-8f);

    // Amp envelope: instant attack, full sustain, short release.
    set(posAdsr2, ADSRParamItemAttack, 0.001f);
    set(posAdsr2, ADSRParamItemDecay, 0.001f);
    set(posAdsr2, ADSRParamItemSustain, 1.0f);
    set(posAdsr2, ADSRParamItemRelease, 0.05f);
    set(posAdsr2, ADSRParamItemLegato, 0.0f);
    set(posAdsr2, ADSRParamItemLinear, 0.0f);

    // Filter envelope present but at zero depth above.
    set(posAdsr1, ADSRParamItemAttack, 0.001f);
    set(posAdsr1, ADSRParamItemDecay, 0.001f);
    set(posAdsr1, ADSRParamItemSustain, 1.0f);
    set(posAdsr1, ADSRParamItemRelease, 0.05f);

    set(posFinal1, FinalOutputParamItemLevel, 0.7f);
    set(posFinal1, FinalOutputParamItemSpread, 0.0f);
}

std::vector<float> render(VoiceManager& vm, uint32_t frames) {
    std::vector<float> left(frames, 0.0f), right(frames, 0.0f);
    uint32_t done = 0;
    while (done < frames) {
        const uint32_t chunk = std::min<uint32_t>(frames - done, MODULE_BUFF_SIZE);
        vm.process(chunk, left.data() + done, right.data() + done);
        done += chunk;
    }
    return left;
}

// A MIDI 2.0 channel-voice-2 NoteOn UMP, optionally carrying a 7.9-pitch attribute.
pm::UmpMessage makeUmpNoteOn(uint8_t note, uint16_t velocity, uint8_t attributeType, uint16_t attribute) {
    const uint32_t word0 = (0x4u << 28) | (0u << 24) | (0x9u << 20) | (0u << 16)
                         | (uint32_t(note) << 8) | uint32_t(attributeType);
    const uint32_t word1 = (uint32_t(velocity) << 16) | uint32_t(attribute);
    const uint32_t words[2] = { word0, word1 };
    return pm::decodeUmp(words);
}

void testPlainNote() {
    std::printf("MIDI 1.0 note:\n");
    VoiceManager vm(kSampleRate, MODULE_BUFF_SIZE);
    configureVoiceManager(vm);

    vm.handleMIDIMessage(pm::fromMidi1Bytes(0x90, 60, 100));   // C4
    const std::vector<float> audio = render(vm, uint32_t(kSampleRate));

    const double level = peak(audio);
    check(level > 0.01, "note-on produces audible output (peak " + std::to_string(level) + ")");

    const double expected = pm::noteToFrequency(60);
    const double measured = estimateFrequency(audio, 150.0, 500.0);
    const double errorPercent = 100.0 * std::fabs(measured - expected) / expected;
    check(errorPercent < 0.5,
          "pitch is C4 (" + std::to_string(measured) + " Hz vs " + std::to_string(expected) + " Hz)");

    vm.handleMIDIMessage(pm::fromMidi1Bytes(0x80, 60, 0));
    render(vm, uint32_t(kSampleRate / 2));
    const std::vector<float> tail = render(vm, uint32_t(kSampleRate / 4));
    check(peak(tail) < 0.001, "note-off releases to silence");
}

void testMidi2Pitch79() {
    std::printf("MIDI 2.0 7.9-pitch note:\n");

    // C4 raised by a quarter tone: attribute = note<<9 | fraction, fraction 256 = 0.5 semitone.
    const uint16_t attribute = uint16_t((60 << 9) | 256);
    const pm::UmpMessage message = makeUmpNoteOn(60, 0xFFFF, pm::kAttrPitch7_9, attribute);

    check(message.messageType == pm::kUmpTypeChannelVoice2, "UMP decodes as channel-voice-2");
    check(message.attributeType == pm::kAttrPitch7_9, "attribute type survives decode (7.9)");
    check(message.attribute == attribute, "attribute payload survives decode");
    check(pm::isNoteOn(message), "message is recognised as a note-on");

    VoiceManager vm(kSampleRate, MODULE_BUFF_SIZE);
    configureVoiceManager(vm);
    vm.handleMIDIMessage(message);
    const std::vector<float> audio = render(vm, uint32_t(kSampleRate));

    check(peak(audio) > 0.01, "note-on produces audible output");

    const double expected = pm::pitchAttributeToFrequency(attribute);   // 261.63 * 2^(256/6144)
    const double measured = estimateFrequency(audio, 150.0, 500.0);
    const double errorPercent = 100.0 * std::fabs(measured - expected) / expected;
    check(errorPercent < 0.5,
          "microtonal pitch reached the oscillators (" + std::to_string(measured) + " Hz vs "
              + std::to_string(expected) + " Hz)");
    check(std::fabs(expected - pm::noteToFrequency(60)) > 3.0,
          "the quarter-tone offset is materially different from the plain note");
}

void testPercussiveRelease() {
    std::printf("Percussive envelope:\n");
    VoiceManager vm(kSampleRate, MODULE_BUFF_SIZE);
    configureVoiceManager(vm);

    vm.setParameter(MODULE_PARAM_FROM_BASE_AND_ITEM(posAdsr2, ADSRParamItemSustain), 0.0f);
    vm.setParameter(MODULE_PARAM_FROM_BASE_AND_ITEM(posAdsr2, ADSRParamItemDecay), 0.05f);

    vm.handleMIDIMessage(pm::fromMidi1Bytes(0x90, 72, 100));
    const std::vector<float> body = render(vm, uint32_t(kSampleRate / 4));
    check(peak(body) > 0.01, "attack is audible");

    // A voice that is still gated is expected to stay allocated even at zero amplitude; what must
    // happen is that it frees itself once released and silent.
    vm.handleMIDIMessage(pm::fromMidi1Bytes(0x80, 72, 0));
    const std::vector<float> quiet = render(vm, uint32_t(kSampleRate * 3 / 2));
    check(peak(quiet) < 0.001, "releases to silence with sustain at zero");
    check(vm.activeCount() == 0, "the voice frees itself once released and silent (active="
                                     + std::to_string(vm.activeCount()) + ")");
}

void testPolyAndMono() {
    std::printf("Polyphony:\n");
    VoiceManager poly(kSampleRate, MODULE_BUFF_SIZE);
    configureVoiceManager(poly);
    poly.handleMIDIMessage(pm::fromMidi1Bytes(0x90, 60, 100));
    poly.handleMIDIMessage(pm::fromMidi1Bytes(0x90, 64, 100));
    poly.handleMIDIMessage(pm::fromMidi1Bytes(0x90, 67, 100));
    render(poly, 2048);
    check(poly.activeCount() == 3, "three held notes use three voices (got "
                                       + std::to_string(poly.activeCount()) + ")");

    VoiceManager mono(kSampleRate, MODULE_BUFF_SIZE);
    configureVoiceManager(mono);
    mono.setVoiceManagerParameter(
        MODULE_PARAM_ADDRESS(ModuleParamKindMIDI, ModuleInstance1, MIDIParamItemVoiceCount), 1.0f);
    mono.setVoiceManagerParameter(
        MODULE_PARAM_ADDRESS(ModuleParamKindMIDI, ModuleInstance1, MIDIParamItemLegato), 1.0f);
    mono.handleMIDIMessage(pm::fromMidi1Bytes(0x90, 60, 100));
    mono.handleMIDIMessage(pm::fromMidi1Bytes(0x90, 64, 100));
    mono.handleMIDIMessage(pm::fromMidi1Bytes(0x90, 67, 100));
    render(mono, 2048);
    check(mono.activeCount() <= 1, "mono holds at most one voice (got "
                                       + std::to_string(mono.activeCount()) + ")");
    check(peak(render(mono, 2048)) > 0.01, "mono keeps sounding through the note stack");
}

// Peak is the tri<->saw morph: 0.5 is a symmetric triangle, ~1.0 a saw. A saw steps once per cycle
// and a triangle never does, so the largest sample-to-sample jump separates them by a wide margin —
// that discontinuity is the audible difference the Peak knob is actually controlling.
void testPeakMorphsWaveform() {
    std::printf("Peak (tri <-> saw morph):\n");

    // Measure the settled waveform only: the note-on edge is itself a large step and would swamp
    // the comparison (the pitch estimator skips the same region for the same reason).
    auto largestStep = [](const std::vector<float>& s) {
        double biggest = 0.0;
        for (size_t i = s.size() / 4 + 1; i < s.size(); ++i)
            biggest = std::max(biggest, std::fabs(double(s[i]) - double(s[i - 1])));
        return biggest;
    };

    VoiceManager triangle(kSampleRate, MODULE_BUFF_SIZE);
    configureVoiceManager(triangle);
    triangle.handleMIDIMessage(pm::fromMidi1Bytes(0x90, 60, 100));
    const double triangleStep = largestStep(render(triangle, uint32_t(kSampleRate / 4)));

    VoiceManager saw(kSampleRate, MODULE_BUFF_SIZE);
    configureVoiceManager(saw);
    saw.setParameter(MODULE_PARAM_FROM_BASE_AND_ITEM(posTriSaw1, TriSawParamItemPeak), 0.99f);
    saw.setParameter(MODULE_PARAM_FROM_BASE_AND_ITEM(posTriSaw2, TriSawParamItemPeak), 0.99f);
    saw.handleMIDIMessage(pm::fromMidi1Bytes(0x90, 60, 100));
    const double sawStep = largestStep(render(saw, uint32_t(kSampleRate / 4)));

    std::printf("        largest sample-to-sample step: triangle %.5f | saw %.5f\n",
                triangleStep, sawStep);
    check(sawStep > 20.0 * triangleStep,
          "Peak 0.99 is a saw (steps once per cycle), Peak 0.5 a triangle ("
              + std::to_string(sawStep) + " vs " + std::to_string(triangleStep) + ")");
}

// Peak changes the TIMBRE, not the energy. The shaper spans the same -1..+1 at every Peak setting (a
// line from -1 up to +1 over [0,p), then back down to -1 over [p,1)), so both ends of the shape must
// carry the same energy as the symmetric triangle halfway between them.
//
// Their PEAK amplitudes legitimately differ: the polyBLAMP corner rounding that slightly shaves the
// triangle's corners turns the saw's near-vertical edge into a Gibbs-style overshoot, so the saw
// reads ~15% higher at the peak while carrying the same RMS. That is band-limiting, not a level
// error — which is why this asserts full scale on the triangle and equal energy across all three,
// and only reports the peak spread.
void testPeakIsLevelFlat() {
    std::printf("Peak level flatness (peak amplitude / RMS):\n");

    // configureVoiceManager's chain gain: CombineMix level 0.8 x FinalOutput level 0.7.
    constexpr double kChainGain = 0.8 * 0.7;

    struct Level { double peak, rms; };
    auto measure = [](float peakValue, uint8_t note) {
        VoiceManager vm(kSampleRate, MODULE_BUFF_SIZE);
        configureVoiceManager(vm);
        vm.setParameter(MODULE_PARAM_FROM_BASE_AND_ITEM(posTriSaw1, TriSawParamItemPeak), peakValue);
        vm.setParameter(MODULE_PARAM_FROM_BASE_AND_ITEM(posTriSaw2, TriSawParamItemPeak), peakValue);
        vm.handleMIDIMessage(pm::fromMidi1Bytes(0x90, note, 100));
        const std::vector<float> audio = render(vm, uint32_t(kSampleRate / 2));
        // Drop the note-on edge: the first samples are the envelope's instant attack, not the wave.
        const std::vector<float> settled(audio.begin() + long(audio.size() / 4), audio.end());
        return Level{ peak(settled), rms(settled) };
    };

    bool triangleAtFullScale = true, equalEnergy = true;
    for (uint8_t note : { 36, 60, 84 }) {   // C2, C4, C6
        const Level triangle = measure(0.5f,  note);
        const Level saw      = measure(0.99f, note);
        const Level ramp     = measure(0.01f, note);
        std::printf("        note %3u  triangle %.4f/%.4f | saw %.4f/%.4f | ramp %.4f/%.4f\n",
                    unsigned(note),
                    triangle.peak, triangle.rms, saw.peak, saw.rms, ramp.peak, ramp.rms);

        if (triangle.peak < 0.95 * kChainGain)
            triangleAtFullScale = false;

        const double lo = std::min({ triangle.rms, saw.rms, ramp.rms });
        const double hi = std::max({ triangle.rms, saw.rms, ramp.rms });
        if ((hi - lo) / hi > 0.03)
            equalEnergy = false;
    }

    check(triangleAtFullScale,
          "the symmetric triangle reaches full scale at C2, C4 and C6 (no dip at Peak 0.5)");
    check(equalEnergy,
          "triangle, saw and ramp carry the same RMS at C2, C4 and C6 (Peak is timbre, not level)");
}

} // namespace

int main() {
    std::printf("PlainOldSynth smoke test (%.0f Hz)\n", kSampleRate);
    testPlainNote();
    testMidi2Pitch79();
    testPercussiveRelease();
    testPolyAndMono();
    testPeakMorphsWaveform();
    testPeakIsLevelFlat();

    if (gFailures != 0) {
        std::printf("%d check(s) FAILED\n", gFailures);
        return 1;
    }
    std::printf("ALL PASS\n");
    return 0;
}
