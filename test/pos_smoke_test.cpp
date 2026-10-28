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
#include <memory>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "PlainOldSynthParams.h"
#include "VoiceManager.hpp"
#include "clap_ump.h"
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
    set(posTriSaw1, TriSawParamItemFreqModLevel2, 1.0f);  // pitch bend in octaves, as the plugin pins it
    set(posTriSaw2, TriSawParamItemFreqModLevel2, 1.0f);

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

    // Velocity, as plugin.cpp sets it up: latched at note-on; the amp instance at the Off shape.
    set(posVelocityAmp, VelocityParamItemIgnoreNoteOff, 1.0f);
    set(posVelocityAmp, VelocityParamItemMin, kVelocityAmpShapes[0].minPercent);
    set(posVelocityAmp, VelocityParamItemCurve, velocityCurveForExponent(kVelocityAmpShapes[0].exponent));
    set(posVelocitySource, VelocityParamItemIgnoreNoteOff, 1.0f);
    set(posVelocitySource, VelocityParamItemMin, 0.0f);
    set(posVelocitySource, VelocityParamItemCurve, velocityCurveForExponent(1.0f));

    // The Timbre source, as plugin.cpp sets it up.
    set(posCcConverter2, CCParamItemNumber, float(kTimbreCC));
    set(posCcConverter2, CCParamItemPerNote, 1.0f);
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

// --- MIDI 2.0 pitch bend ----------------------------------------------------------------------

// Any MIDI 2.0 channel-voice UMP: word0's low byte is the attribute type / per-note controller index
// / per-note management flags depending on the status.
pm::UmpMessage makeUmp(uint8_t status, uint8_t channel, uint8_t note, uint8_t lowByte, uint32_t word1) {
    const uint32_t word0 = (0x4u << 28) | (uint32_t(status & 0xF) << 20) | (uint32_t(channel & 0xF) << 16)
                         | (uint32_t(note) << 8) | uint32_t(lowByte);
    const uint32_t words[2] = { word0, word1 };
    return pm::decodeUmp(words);
}
pm::UmpMessage umpNoteOn(uint8_t note, uint8_t channel = 0) {
    return makeUmp(pm::kStatusNoteOn, channel, note, 0, 0xFFFFu << 16);
}
pm::UmpMessage umpNoteOff(uint8_t note, uint8_t channel = 0) {
    return makeUmp(pm::kStatusNoteOff, channel, note, 0, 0);
}
// 32-bit bend value for `semitones` at a ±`range` semitone bend range.
uint32_t bendWord(double semitones, double range) {
    return uint32_t(std::llround(2147483648.0 + semitones / range * 2147483648.0));
}
// Per-note bend for `semitones` at a ±48 range. The default per-note range is the channel's (±2), so
// tests that use this first announce ±48 with umpPerNoteRange (registered controller 7), as a
// controller such as KeyboardEDO announces its own range.
pm::UmpMessage umpPerNoteBend(uint8_t note, double semitones, uint8_t channel = 0) {
    return makeUmp(pm::kStatusPerNotePitchBend, channel, note, 0, bendWord(semitones, 48.0));
}
pm::UmpMessage umpPerNoteRange(double semitones, uint8_t channel = 0) {   // RC 7, bank 0 / index 7
    const uint32_t whole = uint32_t(semitones);
    const uint32_t cents = uint32_t(std::lround((semitones - whole) * 100.0));
    return makeUmp(pm::kStatusRegController, channel, 0, 7, (whole << 25) | (cents << 18));
}

// Hann-windowed single-bin DFT magnitude (Goertzel) over the settled part of the buffer — enough to
// tell which of several simultaneous notes are sounding, where a zero-crossing count cannot.
double toneLevel(const std::vector<float>& samples, double hz) {
    const size_t start = samples.size() / 4;
    const size_t n = samples.size() - start;
    const double w = 2.0 * M_PI * hz / kSampleRate;
    double re = 0.0, im = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double hann = 0.5 - 0.5 * std::cos(2.0 * M_PI * double(i) / double(n - 1));
        const double x = double(samples[start + i]) * hann;
        re += x * std::cos(w * double(i));
        im -= x * std::sin(w * double(i));
    }
    return 2.0 * std::sqrt(re * re + im * im) / (0.5 * double(n));   // ≈ sine amplitude
}

double semitonesUp(double hz, double semis) { return hz * std::pow(2.0, semis / 12.0); }

std::string hz(double value) { return std::to_string(value) + " Hz"; }

void testUmpPerNoteDecode() {
    std::printf("MIDI 2.0 per-note decode:\n");
    const pm::UmpMessage bend = makeUmp(pm::kStatusPerNotePitchBend, 3, 61, 0, 0xC0000000u);
    check(pm::isPerNoteMessage(bend) && bend.note == 61 && bend.channel == 3 && bend.data32 == 0xC0000000u,
          "per-note pitch bend keeps note, channel and 32-bit value");
    check(pm::pitchBendToUnit(0x80000000u) == 0.0 && pm::pitchBendToUnit(0xC0000000u) == 0.5
              && pm::pitchBendToUnit(0) == -1.0,
          "bend centre is exactly 0, a quarter of the range up is 0.5, the bottom is -1");
    const pm::UmpMessage rpnc = makeUmp(pm::kStatusRegPerNoteController, 0, 60, 74, 123u);
    check(pm::isPerNoteMessage(rpnc) && rpnc.ccIndex == 74 && rpnc.note == 60 && rpnc.ccData == 123u,
          "registered per-note controller: index from the low byte, note from the note byte");
    const pm::UmpMessage cc = makeUmp(pm::kStatusControlChange, 0, 74, 0, 5u);
    check(!pm::isPerNoteMessage(cc) && cc.ccIndex == 74, "control change still takes its index from the note byte");
    const pm::UmpMessage mgmt = makeUmp(pm::kStatusPerNoteManagement, 0, 60, pm::kPerNoteMgmtReset, 0);
    check(pm::isPerNoteMessage(mgmt) && mgmt.optionFlags == pm::kPerNoteMgmtReset,
          "per-note management carries its option flags");
}

void testChannelBend() {
    std::printf("Channel pitch bend:\n");
    VoiceManager vm(kSampleRate, MODULE_BUFF_SIZE);
    configureVoiceManager(vm);
    vm.handleMIDIMessage(pm::fromMidi1Bytes(0xE0, 0x00, 0x60));   // MIDI 1.0, half way up: +1 semitone
    vm.handleMIDIMessage(pm::fromMidi1Bytes(0x90, 60, 100));      // bend sent before the note applies
    const double expected = semitonesUp(pm::noteToFrequency(60), 1.0);
    const double measured = estimateFrequency(render(vm, uint32_t(kSampleRate)), 150.0, 500.0);
    check(std::fabs(measured - expected) / expected < 0.001,
          "a +1 semitone channel bend at the default ±2 range (" + hz(measured) + " vs " + hz(expected) + ")");

    // A bend on another channel belongs to that channel's notes.
    VoiceManager other(kSampleRate, MODULE_BUFF_SIZE);
    configureVoiceManager(other);
    other.handleMIDIMessage(pm::fromMidi1Bytes(0x90, 60, 100));
    other.handleMIDIMessage(pm::fromMidi1Bytes(0xE1, 0x7F, 0x7F));
    const double unbent = estimateFrequency(render(other, uint32_t(kSampleRate)), 150.0, 500.0);
    check(std::fabs(unbent - pm::noteToFrequency(60)) / pm::noteToFrequency(60) < 0.001,
          "a channel-2 bend leaves a channel-1 note alone (" + hz(unbent) + ")");
}

void testPerNoteBend() {
    std::printf("MIDI 2.0 per-note pitch bend:\n");
    const double c4 = pm::noteToFrequency(60), g4 = pm::noteToFrequency(67);
    const double cSharp4 = semitonesUp(c4, 1.0);

    VoiceManager vm(kSampleRate, MODULE_BUFF_SIZE);
    configureVoiceManager(vm);
    vm.handleMIDIMessage(umpPerNoteRange(48.0));
    vm.handleMIDIMessage(umpNoteOn(60));
    vm.handleMIDIMessage(umpNoteOn(67));
    vm.handleMIDIMessage(umpPerNoteBend(60, 1.0));
    const std::vector<float> chord = render(vm, uint32_t(kSampleRate));
    const double bentLevel = toneLevel(chord, cSharp4), oldLevel = toneLevel(chord, c4);
    const double otherLevel = toneLevel(chord, g4);
    check(bentLevel > 0.05 && oldLevel < 0.1 * bentLevel,
          "+1 semitone on C4 moves it to C#4 (C#4 " + std::to_string(bentLevel) + ", C4 "
              + std::to_string(oldLevel) + ")");
    check(otherLevel > 0.5 * bentLevel, "the other held note (G4) is untouched (" + std::to_string(otherLevel) + ")");

    // Two notes on the SAME key but different channels are different notes.
    VoiceManager same(kSampleRate, MODULE_BUFF_SIZE);
    configureVoiceManager(same);
    same.handleMIDIMessage(umpPerNoteRange(48.0, 1));
    same.handleMIDIMessage(umpNoteOn(60, 0));
    same.handleMIDIMessage(umpNoteOn(60, 1));
    check(same.activeCount() == 2, "C4 on channels 1 and 2 takes two voices (got "
                                       + std::to_string(same.activeCount()) + ")");
    same.handleMIDIMessage(umpPerNoteBend(60, 12.0, 1));   // up an octave: a triangle has no 2nd harmonic
    const std::vector<float> pair = render(same, uint32_t(kSampleRate));
    const double c5 = pm::noteToFrequency(72);
    check(toneLevel(pair, c4) > 0.05 && toneLevel(pair, c5) > 0.05,
          "a per-note bend on channel 2's C4 leaves channel 1's C4 in place (C4 "
              + std::to_string(toneLevel(pair, c4)) + ", C5 " + std::to_string(toneLevel(pair, c5)) + ")");
    same.handleMIDIMessage(umpNoteOff(60, 1));
    render(same, uint32_t(kSampleRate / 2));
    const std::vector<float> afterOff = render(same, uint32_t(kSampleRate / 2));
    check(toneLevel(afterOff, c5) < 0.01 && toneLevel(afterOff, c4) > 0.05,
          "a note-off on channel 2 releases only channel 2's note");

    // Per-note bend must not survive into the voice's next note.
    VoiceManager reuse(kSampleRate, MODULE_BUFF_SIZE);
    configureVoiceManager(reuse);
    reuse.handleMIDIMessage(umpPerNoteRange(48.0));
    reuse.handleMIDIMessage(umpNoteOn(60));
    reuse.handleMIDIMessage(umpPerNoteBend(60, 2.0));
    render(reuse, 4096);
    reuse.handleMIDIMessage(umpNoteOff(60));
    render(reuse, uint32_t(kSampleRate));
    reuse.handleMIDIMessage(umpNoteOn(60));
    const double fresh = estimateFrequency(render(reuse, uint32_t(kSampleRate)), 150.0, 500.0);
    check(std::fabs(fresh - c4) / c4 < 0.001, "per-note bend resets on the next note-on (" + hz(fresh) + ")");

    // Per-note management with the Reset flag recentres the bend.
    VoiceManager mgmt(kSampleRate, MODULE_BUFF_SIZE);
    configureVoiceManager(mgmt);
    mgmt.handleMIDIMessage(umpPerNoteRange(48.0));
    mgmt.handleMIDIMessage(umpNoteOn(60));
    mgmt.handleMIDIMessage(umpPerNoteBend(60, 3.0));
    render(mgmt, 4096);
    mgmt.handleMIDIMessage(makeUmp(pm::kStatusPerNoteManagement, 0, 60, pm::kPerNoteMgmtReset, 0));
    const double reset = estimateFrequency(render(mgmt, uint32_t(kSampleRate)), 150.0, 500.0);
    check(std::fabs(reset - c4) / c4 < 0.001, "per-note management Reset recentres the bend (" + hz(reset) + ")");

    // Mono: a bend for a held-but-silent note in the stack must not move the sounding note.
    VoiceManager mono(kSampleRate, MODULE_BUFF_SIZE);
    configureVoiceManager(mono);
    mono.handleMIDIMessage(umpPerNoteRange(48.0));
    mono.setVoiceManagerParameter(
        MODULE_PARAM_ADDRESS(ModuleParamKindMIDI, ModuleInstance1, MIDIParamItemVoiceCount), 1.0f);
    mono.setVoiceManagerParameter(
        MODULE_PARAM_ADDRESS(ModuleParamKindMIDI, ModuleInstance1, MIDIParamItemLegato), 1.0f);
    mono.handleMIDIMessage(umpNoteOn(60));
    mono.handleMIDIMessage(umpNoteOn(64));
    mono.handleMIDIMessage(umpPerNoteBend(60, 5.0));
    const double e4 = pm::noteToFrequency(64);
    const double sounding = estimateFrequency(render(mono, uint32_t(kSampleRate)), 150.0, 500.0);
    check(std::fabs(sounding - e4) / e4 < 0.001,
          "mono: bending the held-but-silent C4 leaves the sounding E4 alone (" + hz(sounding) + ")");
    mono.handleMIDIMessage(umpPerNoteBend(64, -4.0));
    const double bentE = estimateFrequency(render(mono, uint32_t(kSampleRate)), 150.0, 500.0);
    check(std::fabs(bentE - c4) / c4 < 0.001, "mono: bending the sounding E4 down 4 semitones (" + hz(bentE) + ")");
}

// --- per-note bend range and Pitch 7.25 -------------------------------------------------------

// The pitch of note 60 on `channel` after `events`, in semitones from C4.
double semitonesAfter(VoiceManager& vm) {
    const double hz = estimateFrequency(render(vm, uint32_t(kSampleRate)), 100.0, 2000.0);
    return 12.0 * std::log2(hz / pm::noteToFrequency(60));
}
pm::UmpMessage umpPerNoteBendAt(uint8_t note, double semitones, double range, uint8_t channel = 0) {
    return makeUmp(pm::kStatusPerNotePitchBend, channel, note, 0, bendWord(semitones, range));
}
pm::UmpMessage umpPitch725(uint8_t note, double semitones, uint8_t channel = 0) {
    return makeUmp(pm::kStatusRegPerNoteController, channel, note, pm::kRegPerNotePitch7_25,
                   uint32_t(std::llround(semitones * 33554432.0)));
}

pm::UmpMessage midi1Bend(uint8_t channel, uint32_t value14);   // defined with the MPE tests below
pm::UmpMessage midi1CC(uint8_t channel, uint8_t cc, uint8_t value);

void testPerNoteBendRange() {
    std::printf("Per-note bend range (RC 7) and Pitch 7.25:\n");
    auto close = [](double a, double b) { return std::fabs(a - b) < 0.01; };
    auto fresh = []() { auto vm = std::make_unique<VoiceManager>(kSampleRate, MODULE_BUFF_SIZE);
                        configureVoiceManager(*vm); return vm; };
    {   // M2-104 v1.0 §4.2.12: per-note bend "acts like Pitch Bend in every way": the channel's ±2.
        auto vm = fresh();
        vm->handleMIDIMessage(umpNoteOn(60));
        vm->handleMIDIMessage(umpPerNoteBendAt(60, 1.0, 2.0));
        const double st = semitonesAfter(*vm);
        check(close(st, 1.0), "with no RC 7, half way up is +1 semitone (the channel's ±2: " + std::to_string(st) + ")");
    }
    {   // KeyboardEDO's setup: RC 7 = 2 announced, its bends scaled to match. Used to play 24x too far.
        auto vm = fresh();
        vm->handleMIDIMessage(umpPerNoteRange(12.0));
        vm->handleMIDIMessage(umpNoteOn(60));
        vm->handleMIDIMessage(umpPerNoteBendAt(60, 6.0, 12.0));
        const double st = semitonesAfter(*vm);
        check(close(st, 6.0), "RC 7 = 12: half way up is +6 (" + std::to_string(st) + ")");
    }
    {
        auto vm = fresh();
        vm->handleMIDIMessage(umpPerNoteRange(12.0, 5));           // another channel
        vm->handleMIDIMessage(umpNoteOn(60));
        vm->handleMIDIMessage(umpPerNoteBendAt(60, 1.0, 2.0));
        const double st = semitonesAfter(*vm);
        check(close(st, 1.0), "RC 7 on channel 6 leaves channel 1 at ±2 (" + std::to_string(st) + ")");
    }
    {
        auto vm = fresh();
        vm->handleMIDIMessage(umpPerNoteRange(2.5));
        vm->handleMIDIMessage(umpNoteOn(60));
        vm->handleMIDIMessage(makeUmp(pm::kStatusPerNotePitchBend, 0, 60, 0, 0xFFFFFFFFu));
        const double st = semitonesAfter(*vm);
        check(close(st, 2.5), "RC 7's cents count: full scale at 2 semitones 50 cents (" + std::to_string(st) + ")");
    }
    {   // Ranges are configuration a controller sends once: they survive a host reset().
        auto vm = fresh();
        vm->handleMIDIMessage(umpPerNoteRange(12.0));
        vm->handleMIDIMessage(midi1CC(0, 101, 0)); vm->handleMIDIMessage(midi1CC(0, 100, 0));
        vm->handleMIDIMessage(midi1CC(0, 6, 12));                  // RPN 0: ±12
        vm->reset();
        vm->handleMIDIMessage(midi1Bend(0, 8192 + 4096));          // channel: +6 at ±12
        vm->handleMIDIMessage(umpNoteOn(60));
        vm->handleMIDIMessage(umpPerNoteBendAt(60, 3.0, 12.0));    // per-note: +3 at ±12
        const double st = semitonesAfter(*vm);
        check(close(st, 9.0), "RC 7 and RPN 0 survive a reset (+6 channel, +3 per-note = " + std::to_string(st) + ")");
    }
    {   // Pitch 7.25 (registered per-note controller #3): an absolute pitch; bends offset from it.
        auto vm = fresh();
        vm->handleMIDIMessage(umpNoteOn(60));
        vm->handleMIDIMessage(umpPitch725(60, 67.25));
        const double st = semitonesAfter(*vm);
        check(close(st, 7.25), "Pitch 7.25 = 67.25 retunes C4 there (" + std::to_string(st) + ")");
        vm->handleMIDIMessage(umpPerNoteBendAt(60, 1.0, 2.0));
        render(*vm, 4096);
        const double bent = semitonesAfter(*vm);
        check(close(bent, 8.25), "a per-note bend offsets from it (" + std::to_string(bent) + ")");
    }
    {
        auto vm = fresh();
        vm->handleMIDIMessage(makeUmpNoteOn(60, 0xC000, pm::kAttrPitch7_9, (60u << 9) | 256u));   // 60.5
        vm->handleMIDIMessage(umpPitch725(60, 64.0));
        const double st = semitonesAfter(*vm);
        check(close(st, 4.0), "on a Pitch 7.9 note (60.5), Pitch 7.25 = 64 still sounds E4 (" + std::to_string(st) + ")");
    }
    {   // It lasts for the sounding note only (no tuning table): the next note-on starts untuned.
        auto vm = fresh();
        vm->setVoiceManagerParameter(MODULE_PARAM_ADDRESS(ModuleParamKindMIDI, ModuleInstance1, MIDIParamItemVoiceCount), 1.0f);
        vm->handleMIDIMessage(umpNoteOn(60));
        vm->handleMIDIMessage(umpPitch725(60, 72.0));
        render(*vm, 4096);
        vm->handleMIDIMessage(umpNoteOff(60));
        render(*vm, uint32_t(kSampleRate));
        vm->handleMIDIMessage(umpNoteOn(60));
        const double st = semitonesAfter(*vm);
        check(close(st, 0.0), "the next note-on starts untuned (" + std::to_string(st) + ")");
    }
}

// --- CLAP note expressions and note ids ------------------------------------------------------

pm::UmpMessage clapNote(bool on, int16_t key, int32_t noteId, int16_t channel = 0) {
    clap_event_note_t ev{};
    ev.note_id = noteId;
    ev.channel = channel;
    ev.key = key;
    ev.velocity = 0.8;
    return pm::fromClapNote(ev, on);
}
clap_event_note_expression_t clapExpression(clap_note_expression id, double value, int16_t key,
                                            int32_t noteId, int16_t channel = 0) {
    clap_event_note_expression_t ev{};
    ev.expression_id = id;
    ev.note_id = noteId;
    ev.channel = channel;
    ev.key = key;
    ev.value = value;
    return ev;
}

void testClapNoteExpressionDecode() {
    std::printf("CLAP note expression -> MIDI 2.0:\n");
    // Tuning becomes Pitch 7.25 (registered per-note controller #3) at key + tuning: exact at any size.
    auto tuning = [](double semis) {
        return pm::fromClapNoteExpression(clapExpression(CLAP_NOTE_EXPRESSION_TUNING, semis, 60, -1), 60);
    };
    check(tuning(0.0) && tuning(0.0)->status == pm::kStatusRegPerNoteController
              && tuning(0.0)->ccIndex == pm::kRegPerNotePitch7_25 && tuning(0.0)->ccData == 60u << 25,
          "tuning 0 on key 60 is Pitch 7.25 = 60");
    check(tuning(7.5)->ccData == uint32_t(67.5 * 33554432.0) && tuning(-24.0)->ccData == 36u << 25,
          "tuning +7.5 and -24 land exactly (67.5, 36): no bend range to clamp to");
    check(tuning(100.0)->ccData == 0xFFFFFFFFu && tuning(-100.0)->ccData == 0u,
          "tuning past Pitch 7.25's 0..128 span clamps to it");
    check(!pm::fromClapNoteExpression(clapExpression(CLAP_NOTE_EXPRESSION_TUNING, 2.0, -1, 5), -1),
          "id-only tuning with no known key is dropped (the plugin looks the key up first)");

    const auto pressure = pm::fromClapNoteExpression(clapExpression(CLAP_NOTE_EXPRESSION_PRESSURE, 1.0, 60, 4), 60);
    check(pressure && pressure->status == pm::kStatusPolyPressure && pressure->data32 == 0xFFFFFFFFu
              && pressure->noteId == 4 && pressure->note == 60,
          "pressure is polyphonic key pressure, keeping key and note id");
    const auto bright = pm::fromClapNoteExpression(clapExpression(CLAP_NOTE_EXPRESSION_BRIGHTNESS, 0.5, 60, -1), 60);
    check(bright && bright->status == pm::kStatusRegPerNoteController && bright->ccIndex == 74
              && bright->ccData == 0x80000000u,
          "brightness is registered per-note controller #74");
    check(!pm::fromClapNoteExpression(clapExpression(CLAP_NOTE_EXPRESSION_PAN, 0.5, 60, -1), 60),
          "expressions without a destination (pan) are dropped");
    check(!pm::fromClapNoteExpression(clapExpression(CLAP_NOTE_EXPRESSION_TUNING, 1.0, -1, -1), -1),
          "an expression naming neither note id nor key is dropped");
}

void testClapNoteIds() {
    std::printf("CLAP note ids:\n");
    const double c4 = pm::noteToFrequency(60), c5 = pm::noteToFrequency(72);

    // Two overlapping notes on the same key and channel, told apart only by note id.
    VoiceManager vm(kSampleRate, MODULE_BUFF_SIZE);
    configureVoiceManager(vm);
    vm.handleMIDIMessage(clapNote(true, 60, 1));
    vm.handleMIDIMessage(clapNote(true, 60, 2));
    check(vm.activeCount() == 2, "C4 struck twice with different note ids takes two voices (got "
                                     + std::to_string(vm.activeCount()) + ")");
    // Tuning addressed by note id alone (key = -1): only note 2 moves. The plugin finds the key with
    // keyForNoteId, as here.
    check(vm.keyForNoteId(2) == 60 && vm.keyForNoteId(7) == -1, "keyForNoteId finds a sounding id, and only that");
    vm.handleMIDIMessage(*pm::fromClapNoteExpression(
        clapExpression(CLAP_NOTE_EXPRESSION_TUNING, 12.0, -1, 2), vm.keyForNoteId(2)));
    const std::vector<float> both = render(vm, uint32_t(kSampleRate));
    check(toneLevel(both, c4) > 0.05 && toneLevel(both, c5) > 0.05,
          "tuning addressed by note id moves only that note (C4 " + std::to_string(toneLevel(both, c4))
              + ", C5 " + std::to_string(toneLevel(both, c5)) + ")");

    // A note-off for note 1 must not release note 2, though they share key and channel.
    vm.handleMIDIMessage(clapNote(false, 60, 1));
    render(vm, uint32_t(kSampleRate / 2));
    const std::vector<float> after = render(vm, uint32_t(kSampleRate / 2));
    check(toneLevel(after, c4) < 0.01 && toneLevel(after, c5) > 0.05 && vm.activeCount() == 1,
          "a note-off for note id 1 releases only note 1");
}

// --- Pressure and Timbre sources ------------------------------------------------------------

// Patch a MIDI source module straight into both oscillators' pitch at 1 octave per unit, so its
// value can be read off as a pitch: C4 · 2^value.
void routeSourceToPitch(VoiceManager& vm, AUParameterAddress source) {
    vm.setParameter(MODULE_PARAM_FROM_BASE_AND_ITEM(posTriSaw1, TriSawParamItemFreqModLevel), 1.0f);
    vm.setParameter(MODULE_PARAM_FROM_BASE_AND_ITEM(posTriSaw2, TriSawParamItemFreqModLevel), 1.0f);
    vm.setModSource(MODULE_PARAM_FROM_BASE_AND_ITEM(posTriSaw1, TriSawParamItemModPitch), source);
    vm.setModSource(MODULE_PARAM_FROM_BASE_AND_ITEM(posTriSaw2, TriSawParamItemModPitch), source);
}
// The source value implied by a measured pitch over `note`.
double sourceValueFromPitch(double hz, int note) { return std::log2(hz / pm::noteToFrequency(note)); }

// A fresh engine with `source` routed to pitch; mono or poly.
struct SourceRig {
    VoiceManager vm{kSampleRate, MODULE_BUFF_SIZE};
    explicit SourceRig(AUParameterAddress source) {
        configureVoiceManager(vm);
        routeSourceToPitch(vm, source);
    }
    double valueAt(int note) {
        return sourceValueFromPitch(estimateFrequency(render(vm, uint32_t(kSampleRate)), 100.0, 2000.0), note);
    }
};

pm::UmpMessage umpPolyPressure(uint8_t note, double value, uint8_t channel = 0) {
    return makeUmp(pm::kStatusPolyPressure, channel, note, 0, uint32_t(std::llround(value * 4294967295.0)));
}
pm::UmpMessage umpPerNoteTimbre(uint8_t note, double value, uint8_t channel = 0) {
    return makeUmp(pm::kStatusRegPerNoteController, channel, note, kTimbreCC,
                   uint32_t(std::llround(value * 4294967295.0)));
}

void testPressureSource() {
    std::printf("Pressure source:\n");
    auto near = [](double a, double b) { return std::fabs(a - b) < 0.002; };

    {   // Channel pressure (MIDI 1.0) applies to a note on that channel.
        SourceRig rig(posChannelPressure1);
        rig.vm.handleMIDIMessage(pm::fromMidi1Bytes(0x90, 60, 100));
        rig.vm.handleMIDIMessage(pm::fromMidi1Bytes(0xD0, 64, 0));
        const double v = rig.valueAt(60);
        check(near(v, 0.5), "channel pressure 64/128 reads 0.5 (" + std::to_string(v) + ")");
    }
    {   // ...but not to a note on another channel (MPE: channel pressure is that note's pressure).
        SourceRig rig(posChannelPressure1);
        rig.vm.handleMIDIMessage(pm::fromMidi1Bytes(0x90, 60, 100));
        rig.vm.handleMIDIMessage(pm::fromMidi1Bytes(0xD1, 127, 0));
        const double v = rig.valueAt(60);
        check(near(v, 0.0), "channel-2 pressure leaves a channel-1 note at 0 (" + std::to_string(v) + ")");
    }
    {   // Poly pressure overrides the channel's for its note; a note-on resets back to the channel.
        SourceRig rig(posChannelPressure1);
        rig.vm.handleMIDIMessage(pm::fromMidi1Bytes(0xD0, 64, 0));
        rig.vm.handleMIDIMessage(umpNoteOn(60));
        rig.vm.handleMIDIMessage(umpPolyPressure(60, 0.25));
        const double v = rig.valueAt(60);
        check(near(v, 0.25), "MIDI 2.0 poly pressure overrides channel pressure (" + std::to_string(v) + ")");
        rig.vm.handleMIDIMessage(umpNoteOff(60));
        render(rig.vm, uint32_t(kSampleRate));
        rig.vm.handleMIDIMessage(umpNoteOn(60));
        const double fresh = rig.valueAt(60);
        check(near(fresh, 0.5), "the next note-on drops back to the channel's pressure (" + std::to_string(fresh) + ")");
    }
    {   // MIDI 1.0 polyphonic aftertouch reaches the same place.
        SourceRig rig(posChannelPressure1);
        rig.vm.handleMIDIMessage(pm::fromMidi1Bytes(0x90, 60, 100));
        rig.vm.handleMIDIMessage(pm::fromMidi1Bytes(0xA0, 60, 32));
        const double v = rig.valueAt(60);
        check(near(v, 0.25), "MIDI 1.0 poly aftertouch 32/128 reads 0.25 (" + std::to_string(v) + ")");
    }
    {   // Per note: in a C4+G4 chord, pressing C4 moves only C4.
        SourceRig rig(posChannelPressure1);
        rig.vm.handleMIDIMessage(umpNoteOn(60));
        rig.vm.handleMIDIMessage(umpNoteOn(67));
        rig.vm.handleMIDIMessage(umpPolyPressure(60, 0.25));   // +3 semitones: C4 -> D#4
        const std::vector<float> chord = render(rig.vm, uint32_t(kSampleRate));
        const double dSharp4 = pm::noteToFrequency(63), c4 = pm::noteToFrequency(60), g4 = pm::noteToFrequency(67);
        check(toneLevel(chord, dSharp4) > 0.05 && toneLevel(chord, c4) < 0.01 && toneLevel(chord, g4) > 0.05,
              "poly pressure on C4 of a C4+G4 chord moves only C4 (D#4 " + std::to_string(toneLevel(chord, dSharp4))
                  + ", C4 " + std::to_string(toneLevel(chord, c4)) + ", G4 " + std::to_string(toneLevel(chord, g4)) + ")");
    }
    {   // CLAP Pressure note expression, addressed by note id.
        SourceRig rig(posChannelPressure1);
        rig.vm.handleMIDIMessage(clapNote(true, 60, 9));
        rig.vm.handleMIDIMessage(*pm::fromClapNoteExpression(
            clapExpression(CLAP_NOTE_EXPRESSION_PRESSURE, 0.75, -1, 9), -1));
        const double v = rig.valueAt(60);
        check(near(v, 0.75), "CLAP Pressure note expression reads 0.75 (" + std::to_string(v) + ")");
    }
}

void testTimbreSource() {
    std::printf("Timbre source:\n");
    auto near = [](double a, double b) { return std::fabs(a - b) < 0.002; };

    {   // CC74 on the note's channel, sent before the note (as MPE controllers do).
        SourceRig rig(posCcConverter2);
        rig.vm.handleMIDIMessage(pm::fromMidi1Bytes(0xB2, kTimbreCC, 64));
        rig.vm.handleMIDIMessage(pm::fromMidi1Bytes(0x92, 60, 100));
        const double v = rig.valueAt(60);
        check(near(v, 0.5), "CC74 on the note's channel reads 0.5 (" + std::to_string(v) + ")");
    }
    {
        SourceRig rig(posCcConverter2);
        rig.vm.handleMIDIMessage(pm::fromMidi1Bytes(0x90, 60, 100));
        rig.vm.handleMIDIMessage(pm::fromMidi1Bytes(0xB3, kTimbreCC, 127));
        const double v = rig.valueAt(60);
        check(near(v, 0.0), "CC74 on another channel leaves the note at 0 (" + std::to_string(v) + ")");
    }
    {   // MIDI 2.0 per-note controller #74 overrides CC74 for its note, and resets on note-on.
        SourceRig rig(posCcConverter2);
        rig.vm.handleMIDIMessage(pm::fromMidi1Bytes(0xB0, kTimbreCC, 64));
        rig.vm.handleMIDIMessage(umpNoteOn(60));
        rig.vm.handleMIDIMessage(umpPerNoteTimbre(60, 0.25));
        const double v = rig.valueAt(60);
        check(near(v, 0.25), "per-note controller #74 overrides CC74 (" + std::to_string(v) + ")");
        rig.vm.handleMIDIMessage(makeUmp(pm::kStatusPerNoteManagement, 0, 60, pm::kPerNoteMgmtReset, 0));
        const double reset = rig.valueAt(60);
        check(near(reset, 0.5), "per-note management Reset drops back to CC74 (" + std::to_string(reset) + ")");
    }
    {   // A per-note controller with a different index is not timbre.
        SourceRig rig(posCcConverter2);
        rig.vm.handleMIDIMessage(umpNoteOn(60));
        rig.vm.handleMIDIMessage(makeUmp(pm::kStatusRegPerNoteController, 0, 60, 7, 0xFFFFFFFFu));
        const double v = rig.valueAt(60);
        check(near(v, 0.0), "per-note controller #7 does not move timbre (" + std::to_string(v) + ")");
    }
    {   // CLAP Brightness note expression.
        SourceRig rig(posCcConverter2);
        rig.vm.handleMIDIMessage(clapNote(true, 60, 3));
        rig.vm.handleMIDIMessage(*pm::fromClapNoteExpression(
            clapExpression(CLAP_NOTE_EXPRESSION_BRIGHTNESS, 0.5, 60, 3), 60));
        const double v = rig.valueAt(60);
        check(near(v, 0.5), "CLAP Brightness note expression reads 0.5 (" + std::to_string(v) + ")");
    }
    {   // CC #1 keeps its old any-channel behaviour (Per Note off).
        SourceRig rig(posCcConverter1);
        rig.vm.handleMIDIMessage(pm::fromMidi1Bytes(0x90, 60, 100));
        rig.vm.handleMIDIMessage(pm::fromMidi1Bytes(0xB5, 1, 64));
        const double v = rig.valueAt(60);
        check(near(v, 0.5), "CC #1 from any channel still applies (" + std::to_string(v) + ")");
    }
}

// --- MPE (lower zone: master channel 1, members 2..16) -----------------------------------------

void setMpe(VoiceManager& vm, bool on) {
    for (const AUParameterAddress address : kMpeEngineSwitches) vm.setParameter(address, on ? 1.0f : 0.0f);
}
// MIDI 1.0 pitch bend, 14-bit (centre 8192), on a 0-based channel.
pm::UmpMessage midi1Bend(uint8_t channel, uint32_t value14) {
    return pm::fromMidi1Bytes(uint8_t(0xE0 | channel), uint8_t(value14 & 0x7F), uint8_t(value14 >> 7));
}
pm::UmpMessage midi1CC(uint8_t channel, uint8_t cc, uint8_t value) {
    return pm::fromMidi1Bytes(uint8_t(0xB0 | channel), cc, value);
}
// Play C4 on `channel` after `setup`, and return how far it sounds from C4, in semitones.
double semitonesFromC4(VoiceManager& vm, uint8_t channel) {
    vm.handleMIDIMessage(pm::fromMidi1Bytes(uint8_t(0x90 | channel), 60, 100));
    const double hz = estimateFrequency(render(vm, uint32_t(kSampleRate)), 100.0, 2000.0);
    return 12.0 * std::log2(hz / pm::noteToFrequency(60));
}

void testMpe() {
    std::printf("MPE:\n");
    auto near = [](double a, double b) { return std::fabs(a - b) < 0.01; };
    const uint8_t kMaster = 0, kMember = 1, kMember2 = 2;   // channels 1, 2, 3

    {   // A member-channel bend of +3 semitones as an MPE controller means it (8192 + 3/48 of 8192).
        VoiceManager vm(kSampleRate, MODULE_BUFF_SIZE);
        configureVoiceManager(vm);
        vm.handleMIDIMessage(midi1Bend(kMember, 8192 + 512));
        const double off = semitonesFromC4(vm, kMember);
        check(near(off, 0.125), "MPE off: that bend is read at ±2, i.e. +0.125 st (" + std::to_string(off) + ")");

        VoiceManager mpe(kSampleRate, MODULE_BUFF_SIZE);
        configureVoiceManager(mpe);
        setMpe(mpe, true);
        mpe.handleMIDIMessage(midi1Bend(kMember, 8192 + 512));
        const double on = semitonesFromC4(mpe, kMember);
        check(near(on, 3.0), "MPE on: a member channel bends at ±48, i.e. +3 st (" + std::to_string(on) + ")");
    }
    {   // The master channel's bend (±2) adds to every member note's own.
        VoiceManager vm(kSampleRate, MODULE_BUFF_SIZE);
        configureVoiceManager(vm);
        setMpe(vm, true);
        vm.handleMIDIMessage(midi1Bend(kMaster, 8192 + 4096));   // +1 st at ±2
        vm.handleMIDIMessage(midi1Bend(kMember, 8192 + 512));    // +3 st at ±48
        const double both = semitonesFromC4(vm, kMember);
        check(near(both, 4.0), "MPE on: master +1 st plus member +3 st = +4 st (" + std::to_string(both) + ")");

        VoiceManager plain(kSampleRate, MODULE_BUFF_SIZE);
        configureVoiceManager(plain);
        plain.handleMIDIMessage(midi1Bend(kMaster, 8192 + 4096));
        const double none = semitonesFromC4(plain, kMember);
        check(near(none, 0.0), "MPE off: a channel-1 bend leaves a channel-2 note alone (" + std::to_string(none) + ")");
    }
    {   // RPN 0 (pitch bend sensitivity) outside MPE: channel 1 set to ±12, then bent fully up.
        VoiceManager vm(kSampleRate, MODULE_BUFF_SIZE);
        configureVoiceManager(vm);
        vm.handleMIDIMessage(midi1CC(kMaster, 101, 0));
        vm.handleMIDIMessage(midi1CC(kMaster, 100, 0));
        vm.handleMIDIMessage(midi1CC(kMaster, 6, 12));
        vm.handleMIDIMessage(midi1CC(kMaster, 38, 0));
        vm.handleMIDIMessage(midi1Bend(kMaster, 8192 + 4096));   // half way up
        const double half = semitonesFromC4(vm, kMaster);
        check(near(half, 6.0), "RPN 0 sets the bend range: half way up at ±12 is +6 st (" + std::to_string(half) + ")");
    }
    {   // MPE: RPN 0 on one member channel sets every member channel.
        VoiceManager vm(kSampleRate, MODULE_BUFF_SIZE);
        configureVoiceManager(vm);
        setMpe(vm, true);
        vm.handleMIDIMessage(midi1CC(kMember, 101, 0));
        vm.handleMIDIMessage(midi1CC(kMember, 100, 0));
        vm.handleMIDIMessage(midi1CC(kMember, 6, 24));
        vm.handleMIDIMessage(midi1Bend(kMember2, 8192 + 1024));   // +3 st at ±24
        const double other = semitonesFromC4(vm, kMember2);
        check(near(other, 3.0), "MPE on: RPN 0 = 24 sent on channel 2 also sets channel 3 (" + std::to_string(other) + ")");
    }
    {   // MIDI 2.0 RPN 0 (registered controller, bank 0 index 0): ±12 again, as a host translating
        // MIDI 1.0 for a MIDI 2.0 port would send it.
        VoiceManager vm(kSampleRate, MODULE_BUFF_SIZE);
        configureVoiceManager(vm);
        vm.handleMIDIMessage(makeUmp(pm::kStatusRegController, kMaster, 0, 0, (12u << 7) << 18));
        vm.handleMIDIMessage(midi1Bend(kMaster, 8192 + 4096));
        const double half = semitonesFromC4(vm, kMaster);
        check(near(half, 6.0), "MIDI 2.0 RPN 0 sets the bend range too (" + std::to_string(half) + ")");
    }
    {   // The MPE Configuration Message (RPN 6 on channel 1) puts set bend ranges back to default.
        VoiceManager vm(kSampleRate, MODULE_BUFF_SIZE);
        configureVoiceManager(vm);
        vm.handleMIDIMessage(midi1CC(kMaster, 101, 0));
        vm.handleMIDIMessage(midi1CC(kMaster, 100, 0));
        vm.handleMIDIMessage(midi1CC(kMaster, 6, 12));           // RPN 0: ±12
        vm.handleMIDIMessage(midi1CC(kMaster, 100, 6));
        vm.handleMIDIMessage(midi1CC(kMaster, 6, 15));           // RPN 6: MPE Configuration, 15 members
        vm.handleMIDIMessage(midi1Bend(kMaster, 8192 + 4096));   // half way up
        const double half = semitonesFromC4(vm, kMaster);
        check(near(half, 1.0), "the MPE Configuration Message resets the range to ±2 (" + std::to_string(half) + ")");
    }
    {   // So does flipping the MPE switch: a range set under MPE must not leak into non-MPE play.
        VoiceManager vm(kSampleRate, MODULE_BUFF_SIZE);
        configureVoiceManager(vm);
        setMpe(vm, true);
        vm.handleMIDIMessage(midi1CC(kMember, 101, 0));
        vm.handleMIDIMessage(midi1CC(kMember, 100, 0));
        vm.handleMIDIMessage(midi1CC(kMember, 6, 48));           // every member channel: ±48
        setMpe(vm, false);
        vm.handleMIDIMessage(midi1Bend(kMember2, 8192 + 4096));
        const double half = semitonesFromC4(vm, kMember2);
        check(near(half, 1.0), "turning MPE off drops member ranges back to ±2 (" + std::to_string(half) + ")");
    }
    {   // mpe-sender's own sequence (an Unlikelyware test tool): an MPE-on "rpn" test (RPN 0 = 24 on channel 2, which MPE
        // spreads to every member), its reset (the MPE Configuration Message), then the switch off and
        // the "samekey" and "rpn" tests. Each must read at the non-MPE ±2, not the leftover ±24 (which
        // gives a tritone and an octave instead).
        VoiceManager vm(kSampleRate, MODULE_BUFF_SIZE);
        configureVoiceManager(vm);
        auto rpn0 = [&](uint8_t ch, uint8_t semis) {
            vm.handleMIDIMessage(midi1CC(ch, 101, 0)); vm.handleMIDIMessage(midi1CC(ch, 100, 0));
            vm.handleMIDIMessage(midi1CC(ch, 6, semis)); vm.handleMIDIMessage(midi1CC(ch, 38, 0));
            vm.handleMIDIMessage(midi1CC(ch, 101, 127)); vm.handleMIDIMessage(midi1CC(ch, 100, 127));
        };
        auto mpeConfiguration = [&]() {
            vm.handleMIDIMessage(midi1CC(kMaster, 101, 0)); vm.handleMIDIMessage(midi1CC(kMaster, 100, 6));
            vm.handleMIDIMessage(midi1CC(kMaster, 6, 15));
            vm.handleMIDIMessage(midi1CC(kMaster, 101, 127)); vm.handleMIDIMessage(midi1CC(kMaster, 100, 127));
        };
        setMpe(vm, true);
        rpn0(kMember, 24);
        render(vm, 2048);
        mpeConfiguration();
        setMpe(vm, false);
        render(vm, 2048);

        vm.handleMIDIMessage(midi1Bend(5, 8192 + 2048));   // samekey: "+12 at ±48" on channel 6
        const double samekey = semitonesFromC4(vm, 5);
        check(near(samekey, 0.5), "sender sequence, MPE off: samekey's bend reads at ±2 (+0.5 st, not a tritone; "
                                      + std::to_string(samekey) + ")");
        vm.handleMIDIMessage(pm::fromMidi1Bytes(0x85, 60, 0));
        render(vm, uint32_t(kSampleRate));

        rpn0(kMember, 24);                                         // rpn, MPE off: channel 2 only
        vm.handleMIDIMessage(midi1Bend(kMember2, 8192 + 4096));
        const double rpnOff = semitonesFromC4(vm, kMember2);
        check(near(rpnOff, 1.0), "sender sequence, MPE off: rpn leaves channel 3 at ±2 (+1 st, not an octave; "
                                     + std::to_string(rpnOff) + ")");
    }
    {   // Pressure: a member note takes the higher of its own and the master channel's.
        auto pressureOf = [](bool mpe) {
            SourceRig rig(posChannelPressure1);
            setMpe(rig.vm, mpe);
            rig.vm.handleMIDIMessage(pm::fromMidi1Bytes(0xD0, 64, 0));   // master: 0.5
            rig.vm.handleMIDIMessage(pm::fromMidi1Bytes(0x91, 60, 100));
            rig.vm.handleMIDIMessage(pm::fromMidi1Bytes(0xD1, 32, 0));   // member: 0.25
            return rig.valueAt(60);
        };
        const double on = pressureOf(true), off = pressureOf(false);
        check(near(on, 0.5) && near(off, 0.25),
              "Pressure: MPE takes the master channel's 0.5 over the note's 0.25; off, only the note's ("
                  + std::to_string(on) + ", " + std::to_string(off) + ")");
    }
    {   // Timbre: the same rule for CC74.
        auto timbreOf = [](bool mpe) {
            SourceRig rig(posCcConverter2);
            setMpe(rig.vm, mpe);
            rig.vm.handleMIDIMessage(pm::fromMidi1Bytes(0xB0, kTimbreCC, 64));   // master: 0.5
            rig.vm.handleMIDIMessage(pm::fromMidi1Bytes(0x91, 60, 100));
            rig.vm.handleMIDIMessage(pm::fromMidi1Bytes(0xB1, kTimbreCC, 32));   // member: 0.25
            return rig.valueAt(60);
        };
        const double on = timbreOf(true), off = timbreOf(false);
        check(near(on, 0.5) && near(off, 0.25),
              "Timbre: MPE takes the master channel's CC74 over the note's; off, only the note's ("
                  + std::to_string(on) + ", " + std::to_string(off) + ")");
    }
}

// --- Velocity --------------------------------------------------------------------------------

void testMidi1Scaling() {
    std::printf("MIDI 1.0 -> 2.0 value scaling:\n");
    check(pm::scaleUp(127, 7, 16) == 0xFFFFu && pm::scaleUp(64, 7, 16) == 0x8000u && pm::scaleUp(0, 7, 16) == 0,
          "velocity: 127 is full scale, 64 exactly half, 0 zero");
    check(pm::scaleUp(127, 7, 32) == 0xFFFFFFFFu && pm::scaleUp(64, 7, 32) == 0x80000000u,
          "CC / pressure: 127 is full scale, 64 exactly the centre");
    check(pm::scaleUp(16383, 14, 32) == 0xFFFFFFFFu && pm::scaleUp(8192, 14, 32) == 0x80000000u,
          "pitch bend: 16383 is full scale, 8192 exactly the centre");
    check(pm::fromMidi1Bytes(0x90, 60, 127).velocity == 0xFFFF, "a MIDI 1.0 note-on at 127 is full velocity");
}

// Peak of a C4 played at `velocity` with the amp instance at Velocity dropdown `choice`.
double peakAtVelocity(int choice, uint8_t velocity) {
    VoiceManager vm(kSampleRate, MODULE_BUFF_SIZE);
    configureVoiceManager(vm);
    vm.setParameter(MODULE_PARAM_FROM_BASE_AND_ITEM(posVelocityAmp, VelocityParamItemMin),
                    kVelocityAmpShapes[choice].minPercent);
    vm.setParameter(MODULE_PARAM_FROM_BASE_AND_ITEM(posVelocityAmp, VelocityParamItemCurve),
                    velocityCurveForExponent(kVelocityAmpShapes[choice].exponent));
    vm.handleMIDIMessage(pm::fromMidi1Bytes(0x90, 60, velocity));
    render(vm, 4096);
    return peak(render(vm, uint32_t(kSampleRate / 4)));
}

void testVelocity() {
    std::printf("Velocity:\n");
    const double loudest = peakAtVelocity(0, 127);
    check(std::fabs(peakAtVelocity(0, 20) / loudest - 1.0) < 0.01,
          "Off: velocity 20 is as loud as 127 (the synth's old behaviour)");
    // Expected gain at velocity 64 (exactly 0.5): min + (1 - min) * 0.5^exponent.
    const char* names[] = { "Off", "Light", "Medium", "Full" };
    for (int choice = 1; choice < 4; ++choice) {
        const VelocityShape& shape = kVelocityAmpShapes[choice];
        const double min = shape.minPercent / 100.0;
        const double expected = min + (1.0 - min) * std::pow(0.5, shape.exponent);
        const double full = peakAtVelocity(choice, 127);
        const double ratio = peakAtVelocity(choice, 64) / full;
        check(std::fabs(full / loudest - 1.0) < 0.01 && std::fabs(ratio - expected) < 0.01,
              std::string(names[choice]) + ": velocity 127 is full level, 64 is " + std::to_string(ratio)
                  + " of it (expected " + std::to_string(expected) + ")");
    }

    // MIDI 2.0: a note-on with velocity 0 is a note-on (M2-104 §4.2.8), not a note-off. MIDI 1.0's is.
    {
        const pm::UmpMessage zero = makeUmpNoteOn(64, 0, 0, 0);
        check(pm::isNoteOn(zero) && !pm::isNoteOff(zero) && pm::isNoteOff(pm::fromMidi1Bytes(0x90, 64, 0)),
              "a MIDI 2.0 velocity-0 note-on is a note-on; a MIDI 1.0 one decodes to a note-off");
        VoiceManager vm(kSampleRate, MODULE_BUFF_SIZE);   // Velocity Off: loudness ignores velocity
        configureVoiceManager(vm);
        vm.handleMIDIMessage(zero);
        const std::vector<float> s = render(vm, uint32_t(kSampleRate / 2));
        const double hz = estimateFrequency(s, 150.0, 500.0), e4 = pm::noteToFrequency(64);
        check(vm.activeCount() == 1 && peak(s) > 0.05 && std::fabs(hz - e4) / e4 < 0.001,
              "with Velocity Off it holds a voice and sounds at its own pitch (" + std::to_string(hz) + " Hz)");
        vm.handleMIDIMessage(umpNoteOff(64));
        render(vm, uint32_t(kSampleRate));
        check(vm.activeCount() == 0, "...and its note-off releases it");
    }

    // The modulation source is plain linear velocity: 64 reads 0.5.
    SourceRig rig(posVelocitySource);
    rig.vm.handleMIDIMessage(pm::fromMidi1Bytes(0x90, 60, 64));
    const double v = rig.valueAt(60);
    check(std::fabs(v - 0.5) < 0.002, "the Velocity source reads velocity 64 as 0.5 (" + std::to_string(v) + ")");
}

// --- after note-off: MPE §3.3.3, "control of a note ceases once Note Off has occurred" -----------
// Senders (mpe-sender included) centre a member channel right after the note-off to ready it for the
// next note; the release tail must not follow. The master channel still applies. The amp release is
// lengthened so there is a tail to measure.
void setLongRelease(VoiceManager& vm) {
    vm.setParameter(MODULE_PARAM_FROM_BASE_AND_ITEM(posAdsr2, ADSRParamItemRelease), 30.0f);
}

void testReleasedNotes() {
    std::printf("Released notes (MPE §3.3.3):\n");
    auto close = [](double a, double b) { return std::fabs(a - b) < 0.01; };
    auto fresh = [](bool mpe) {
        auto vm = std::make_unique<VoiceManager>(kSampleRate, MODULE_BUFF_SIZE);
        configureVoiceManager(*vm);
        setLongRelease(*vm);
        setMpe(*vm, mpe);
        return vm;
    };
    {   // samekey's ending: up an octave on a member channel, note-off, the channel centred at once.
        auto vm = fresh(true);
        vm->handleMIDIMessage(pm::fromMidi1Bytes(0x95, 60, 100));
        vm->handleMIDIMessage(midi1Bend(5, 8192 + 2048));            // +12 at ±48
        render(*vm, 4096);
        vm->handleMIDIMessage(pm::fromMidi1Bytes(0x85, 60, 0));
        vm->handleMIDIMessage(midi1Bend(5, 8192));
        const double tail = semitonesAfter(*vm);
        check(close(tail, 12.0), "MPE on: centring the member channel after note-off leaves the tail at +12 ("
                                     + std::to_string(tail) + ")");
        vm->handleMIDIMessage(midi1Bend(0, 8192 + 4096));            // master: +1 at ±2
        render(*vm, 4096);
        const double wheel = semitonesAfter(*vm);
        check(close(wheel, 13.0), "...and the master channel's bend still moves it (" + std::to_string(wheel) + ")");
    }
    {   // MPE off: an ordinary pitch wheel bends a releasing note.
        auto vm = fresh(false);
        vm->handleMIDIMessage(pm::fromMidi1Bytes(0x90, 60, 100));
        vm->handleMIDIMessage(midi1Bend(0, 8192 + 4096));
        render(*vm, 4096);
        vm->handleMIDIMessage(pm::fromMidi1Bytes(0x80, 60, 0));
        vm->handleMIDIMessage(midi1Bend(0, 8192));
        render(*vm, 4096);
        const double tail = semitonesAfter(*vm);
        check(close(tail, 0.0), "MPE off: the tail follows its channel back to centre (" + std::to_string(tail) + ")");
    }
    {   // A pedal-held note has had its note-off: it freezes too.
        auto vm = fresh(true);
        vm->handleMIDIMessage(midi1CC(0, 64, 127));
        vm->handleMIDIMessage(pm::fromMidi1Bytes(0x92, 60, 100));
        vm->handleMIDIMessage(midi1Bend(2, 8192 + 2048));
        render(*vm, 4096);
        vm->handleMIDIMessage(pm::fromMidi1Bytes(0x82, 60, 0));
        vm->handleMIDIMessage(midi1Bend(2, 8192));
        const double tail = semitonesAfter(*vm);
        check(close(tail, 12.0), "MPE on: a pedal-held note keeps its bend when the channel is centred ("
                                     + std::to_string(tail) + ")");
    }
    {   // MIDI 2.0: a per-note bend after note-off leaves the tail alone.
        auto vm = fresh(false);
        vm->handleMIDIMessage(umpNoteOn(60));
        vm->handleMIDIMessage(umpPerNoteBendAt(60, 1.0, 2.0));
        render(*vm, 4096);
        vm->handleMIDIMessage(umpNoteOff(60));
        vm->handleMIDIMessage(umpPerNoteBendAt(60, 0.0, 2.0));
        const double tail = semitonesAfter(*vm);
        check(close(tail, 1.0), "MIDI 2.0: a per-note bend after note-off leaves the tail at +1 (" + std::to_string(tail) + ")");
    }
    {   // A CLAP note addressed by its note id still takes expression until NOTE_END.
        auto vm = fresh(false);
        vm->handleMIDIMessage(clapNote(true, 60, 9));
        render(*vm, 4096);
        vm->handleMIDIMessage(clapNote(false, 60, 9));
        vm->handleMIDIMessage(*pm::fromClapNoteExpression(
            clapExpression(CLAP_NOTE_EXPRESSION_TUNING, 7.0, -1, 9), vm->keyForNoteId(9)));
        const double tail = semitonesAfter(*vm);
        check(close(tail, 7.0), "CLAP: an id-addressed tuning still reaches the released note (" + std::to_string(tail) + ")");
    }
    // Pressure and Timbre follow the same rule under MPE: a released note keeps its member value.
    auto releasedSource = [](AUParameterAddress source, const pm::UmpMessage& memberHalf,
                             const pm::UmpMessage& memberZero, const pm::UmpMessage& masterMore) {
        SourceRig rig(source);
        setLongRelease(rig.vm);
        setMpe(rig.vm, true);
        rig.vm.handleMIDIMessage(pm::fromMidi1Bytes(0x91, 60, 100));
        rig.vm.handleMIDIMessage(memberHalf);
        render(rig.vm, 4096);
        rig.vm.handleMIDIMessage(pm::fromMidi1Bytes(0x81, 60, 0));
        rig.vm.handleMIDIMessage(memberZero);
        const double kept = rig.valueAt(60);
        rig.vm.handleMIDIMessage(masterMore);
        const double master = rig.valueAt(60);
        return std::make_pair(kept, master);
    };
    {
        const auto [kept, master] = releasedSource(posChannelPressure1, pm::fromMidi1Bytes(0xD1, 64, 0),
                                                   pm::fromMidi1Bytes(0xD1, 0, 0), pm::fromMidi1Bytes(0xD0, 96, 0));
        check(std::fabs(kept - 0.5) < 0.002 && std::fabs(master - pm::scaleUp(96, 7, 32) / 4294967295.0) < 0.002,
              "MPE on: a released note keeps its member pressure (" + std::to_string(kept)
                  + "); the master's still applies (" + std::to_string(master) + ")");
    }
    {
        const auto [kept, master] = releasedSource(posCcConverter2, pm::fromMidi1Bytes(0xB1, kTimbreCC, 64),
                                                   pm::fromMidi1Bytes(0xB1, kTimbreCC, 0),
                                                   pm::fromMidi1Bytes(0xB0, kTimbreCC, 96));
        check(std::fabs(kept - 0.5) < 0.002 && std::fabs(master - pm::scaleUp(96, 7, 32) / 4294967295.0) < 0.002,
              "MPE on: a released note keeps its member timbre (" + std::to_string(kept)
                  + "); the master's still applies (" + std::to_string(master) + ")");
    }
}

// --- channel mode messages: CC 123 All Notes Off, CC 120 All Sound Off ------------------------------
void testChannelModeMessages() {
    std::printf("All Notes Off / All Sound Off:\n");
    {   // CC 123 releases only its channel's notes.
        VoiceManager vm(kSampleRate, MODULE_BUFF_SIZE);
        configureVoiceManager(vm);
        vm.handleMIDIMessage(pm::fromMidi1Bytes(0x91, 60, 100));   // channel 2
        vm.handleMIDIMessage(pm::fromMidi1Bytes(0x92, 64, 100));   // channel 3
        render(vm, 2048);
        vm.handleMIDIMessage(midi1CC(1, 123, 0));
        render(vm, uint32_t(kSampleRate));
        check(vm.activeCount() == 1, "CC 123 on channel 2 releases channel 2's note only (active="
                                         + std::to_string(vm.activeCount()) + ")");
    }
    {   // ...including a note the sustain pedal is holding; MIDI 2.0 CC 123 alike.
        VoiceManager vm(kSampleRate, MODULE_BUFF_SIZE);
        configureVoiceManager(vm);
        vm.handleMIDIMessage(midi1CC(0, 64, 127));
        vm.handleMIDIMessage(pm::fromMidi1Bytes(0x90, 60, 100));
        vm.handleMIDIMessage(pm::fromMidi1Bytes(0x80, 60, 0));
        render(vm, uint32_t(kSampleRate / 2));
        check(vm.activeCount() == 1, "the pedal is holding the note");
        vm.handleMIDIMessage(makeUmp(pm::kStatusControlChange, 0, 123, 0, 0));
        render(vm, uint32_t(kSampleRate));
        check(vm.activeCount() == 0, "MIDI 2.0 CC 123 releases a pedal-held note");
    }
    {   // CC 120 cuts its channel's voices at once, release included; other channels play on.
        VoiceManager vm(kSampleRate, MODULE_BUFF_SIZE);
        configureVoiceManager(vm);
        vm.handleMIDIMessage(umpNoteOn(60, 1));
        vm.handleMIDIMessage(umpNoteOn(67, 2));
        render(vm, 4096);
        vm.handleMIDIMessage(midi1CC(1, 120, 0));
        check(vm.activeCount() == 1, "CC 120 on channel 2 drops its voice immediately");
        const std::vector<float> after = render(vm, uint32_t(kSampleRate / 2));
        const double c4 = toneLevel(after, pm::noteToFrequency(60)), g4 = toneLevel(after, pm::noteToFrequency(67));
        check(c4 < 1e-4 && g4 > 0.01, "C4 is silent at once, G4 still sounds (C4 " + std::to_string(c4)
                                         + ", G4 " + std::to_string(g4) + ")");
    }
    {   // CC 120 must not make a voice forget other channels' bends (a full Voice::reset would).
        VoiceManager vm(kSampleRate, MODULE_BUFF_SIZE);
        configureVoiceManager(vm);
        vm.setVoiceManagerParameter(MODULE_PARAM_ADDRESS(ModuleParamKindMIDI, ModuleInstance1, MIDIParamItemVoiceCount), 1.0f);
        vm.handleMIDIMessage(midi1Bend(4, 8192 + 4096));            // channel 5: +1 semitone at ±2
        vm.handleMIDIMessage(umpNoteOn(60, 1));
        render(vm, 2048);
        vm.handleMIDIMessage(midi1CC(1, 120, 0));
        const double bent = semitonesFromC4(vm, 4);
        check(std::fabs(bent - 1.0) < 0.01, "after CC 120 the voice still knows channel 5's bend ("
                                                + std::to_string(bent) + " st)");
    }
}

// Note-offs the engine generates itself (sustain pedal, all-notes-off) must carry the note's channel.
void testGeneratedNoteOffsMatchChannel() {
    std::printf("Engine note-offs on other channels:\n");
    VoiceManager pedal(kSampleRate, MODULE_BUFF_SIZE);
    configureVoiceManager(pedal);
    pedal.handleMIDIMessage(pm::fromMidi1Bytes(0xB5, 64, 127));   // pedal down, channel 6
    pedal.handleMIDIMessage(pm::fromMidi1Bytes(0x95, 60, 100));
    render(pedal, 2048);
    pedal.handleMIDIMessage(pm::fromMidi1Bytes(0x85, 60, 0));     // deferred by the pedal
    render(pedal, 4096);
    check(pedal.activeCount() == 1, "the pedal holds a channel-6 note after its note-off");
    pedal.handleMIDIMessage(pm::fromMidi1Bytes(0xB5, 64, 0));
    render(pedal, uint32_t(kSampleRate));
    check(pedal.activeCount() == 0, "pedal up releases the channel-6 note (active="
                                        + std::to_string(pedal.activeCount()) + ")");

    VoiceManager all(kSampleRate, MODULE_BUFF_SIZE);
    configureVoiceManager(all);
    all.handleMIDIMessage(pm::fromMidi1Bytes(0x9A, 64, 100));
    render(all, 2048);
    all.allNotesOff();
    render(all, uint32_t(kSampleRate));
    check(all.activeCount() == 0, "all-notes-off releases a channel-11 note (active="
                                      + std::to_string(all.activeCount()) + ")");
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

// Self Mod feeds the oscillator's own output back into its phase, so it must add harmonics: on a
// symmetric triangle, which has almost none to spare, the largest sample-to-sample step is a clean
// read on whether the feedback is doing anything.
//
// The SIGN matters. The engine clamps the COMBINED depth (knob + any patched input) symmetrically at
// +/-0.9, because the cornered tri/saw breaks into noise at about 0.98 magnitude either way — so
// negative depth is a supported, bounded operating region, not an accident. It is not a mirror image
// of the positive side either: the feedback is a phase offset, so inverting it bends the ramp the
// other way and lands on a different waveform.
void testSelfModulation() {
    std::printf("Self modulation:\n");

    struct Shape { double step, rms, peak; };
    auto measure = [](float peakValue, float selfMod) {
        VoiceManager vm(kSampleRate, MODULE_BUFF_SIZE);
        configureVoiceManager(vm);
        vm.setParameter(MODULE_PARAM_FROM_BASE_AND_ITEM(posTriSaw1, TriSawParamItemPeak), peakValue);
        vm.setParameter(MODULE_PARAM_FROM_BASE_AND_ITEM(posTriSaw2, TriSawParamItemPeak), peakValue);
        vm.setParameter(MODULE_PARAM_FROM_BASE_AND_ITEM(posTriSaw1, TriSawParamItemSelfModLevel), selfMod);
        vm.setParameter(MODULE_PARAM_FROM_BASE_AND_ITEM(posTriSaw2, TriSawParamItemSelfModLevel), selfMod);
        vm.handleMIDIMessage(pm::fromMidi1Bytes(0x90, 60, 100));
        const std::vector<float> audio = render(vm, uint32_t(kSampleRate / 2));
        const std::vector<float> settled(audio.begin() + long(audio.size() / 4), audio.end());
        double step = 0.0;
        for (size_t i = 1; i < settled.size(); ++i)
            step = std::max(step, std::fabs(double(settled[i]) - double(settled[i - 1])));
        return Shape{ step, rms(settled), peak(settled) };
    };

    const Shape clean = measure(0.5f, 0.0f);
    const Shape triUp = measure(0.5f,  0.6f);
    const Shape triDn = measure(0.5f, -0.6f);
    const Shape sawUp = measure(0.99f,  0.6f);
    const Shape sawDn = measure(0.99f, -0.6f);
    std::printf("        triangle (Peak 0.50) selfMod  0.0 -> step %.5f rms %.4f peak %.4f\n",
                clean.step, clean.rms, clean.peak);
    std::printf("        triangle              +0.6 -> step %.5f rms %.4f peak %.4f\n",
                triUp.step, triUp.rms, triUp.peak);
    std::printf("        triangle              -0.6 -> step %.5f rms %.4f peak %.4f\n",
                triDn.step, triDn.rms, triDn.peak);
    std::printf("        saw      (Peak 0.99) +0.6 -> step %.5f rms %.4f peak %.4f\n",
                sawUp.step, sawUp.rms, sawUp.peak);
    std::printf("        saw                   -0.6 -> step %.5f rms %.4f peak %.4f\n",
                sawDn.step, sawDn.rms, sawDn.peak);

    check(triUp.step > 4.0 * clean.step,
          "Self Mod feeds the oscillator back on itself and adds harmonics ("
              + std::to_string(triUp.step) + " vs " + std::to_string(clean.step) + ")");
    check(std::isfinite(sawDn.step) && std::isfinite(triDn.step) && sawDn.peak < 2.0 && triDn.peak < 2.0,
          "negative self-mod stays bounded and finite rather than running away");
    // On a SYMMETRIC triangle the sign is a no-op: a triangle is its own time-mirror, so inverting the
    // feedback only shifts it in time. On an asymmetric shape (a saw) the mirror is a different
    // waveform, so the sign genuinely flips the ramp direction there.
    check(std::fabs(triDn.step - triUp.step) < 0.01 * triUp.step,
          "on a symmetric triangle the sign of self-mod is a time-mirror, not a different timbre");
    check(std::fabs(sawDn.step - sawUp.step) > 0.01 * sawUp.step,
          "on a saw the sign of self-mod does change the waveform (" + std::to_string(sawUp.step)
              + " vs " + std::to_string(sawDn.step) + ")");
}

// --- Mod sections: Multiply keeps a zero-mean source centred -------------------------------------
//
// The user's patch: Mod 1 = LFO 1 (Level A 0.58) Multiply CC #1 (Level B 1.00) -> Osc Pitch. The mod
// wheel should only deepen the vibrato around the played pitch. When a patched input's factor was
// 1 + Level·(x−1), the LFO's factor carried a DC term of 1−Level, so the wheel pushed the centre up
// by 0.42·wheel·2 oct (Osc Pitch is pinned at 2 octaves full scale, as plugin.cpp does).
//
// Wired exactly as plugin.cpp's applyModRouting does: In A → CVMixer In 1, In B → In 2, the mixer
// into both oscillators' ModPitch. The centre is the TIME-AVERAGED log-pitch over whole LFO periods
// (the per-cycle frequency from zero crossings, weighted by cycle length), which is the played note
// exactly when the modulation is zero-mean; a plain mean frequency would read high (2^x is convex).
struct PitchTrack { double centre, low, high; };   // octaves relative to `note`

PitchTrack trackPitch(const std::vector<float>& samples, size_t begin, size_t end, int note) {
    const double ref = pm::noteToFrequency(note);
    double weighted = 0.0, total = 0.0, low = 1e9, high = -1e9, previous = -1.0;
    for (size_t i = begin + 1; i < end; ++i) {
        const double a = samples[i - 1], b = samples[i];
        if (!(a <= 0.0 && b > 0.0)) continue;
        const double position = double(i - 1) + (b != a ? (-a) / (b - a) : 0.0);
        if (previous >= 0.0) {
            const double period = position - previous;
            const double octaves = std::log2(kSampleRate / period / ref);
            weighted += octaves * period;
            total += period;
            low = std::min(low, octaves);
            high = std::max(high, octaves);
        }
        previous = position;
    }
    return { total > 0.0 ? weighted / total : 0.0, low, high };
}

struct ModRig {
    VoiceManager vm{kSampleRate, MODULE_BUFF_SIZE};
    ModRig(AUParameterAddress sourceA, float levelA, AUParameterAddress sourceB, float levelB, bool multiply) {
        configureVoiceManager(vm);
        auto set = [this](AUParameterAddress base, uint8_t item, float value) {
            vm.setParameter(MODULE_PARAM_FROM_BASE_AND_ITEM(base, item), value);
        };
        set(posLfo1, LFOParamItemWave, float(LFOParamWaveTriangle));
        set(posLfo1, LFOParamItemPitch, 1.0f);                       // 2 Hz: 24000 samples a cycle
        set(posLfo1, LFOParamItemPhase, 0.0f);
        set(posLfo1, LFOParamItemClockRatio, 0.0f);
        set(posTriSaw1, TriSawParamItemFreqModLevel, 2.0f);          // Osc Pitch, as plugin.cpp pins it
        set(posTriSaw2, TriSawParamItemFreqModLevel, 2.0f);
        set(posCvMixer1, CVMixerParamItemInput1Level, levelA);
        set(posCvMixer1, CVMixerParamItemInput2Level, levelB);
        set(posCvMixer1, CVMixerParamItemMode, multiply ? 1.0f : 0.0f);
        vm.setModSource(MODULE_PARAM_FROM_BASE_AND_ITEM(posCvMixer1, CVMixerParamItemInput1), sourceA);
        vm.setModSource(MODULE_PARAM_FROM_BASE_AND_ITEM(posCvMixer1, CVMixerParamItemInput2), sourceB);
        vm.setModSource(MODULE_PARAM_FROM_BASE_AND_ITEM(posTriSaw1, TriSawParamItemModPitch), posCvMixer1);
        vm.setModSource(MODULE_PARAM_FROM_BASE_AND_ITEM(posTriSaw2, TriSawParamItemModPitch), posCvMixer1);
    }
    // C4 with the mod wheel at `wheel` (0..127): the pitch over two whole LFO cycles, after 0.5 s.
    PitchTrack play(uint8_t wheel) {
        // setModSource only queues the connections; the first process() applies them. Until then an
        // ADSR in a section has no activeCount and would miss the note-on (MIDI dispatch is gated on it).
        render(vm, MODULE_BUFF_SIZE);
        vm.handleMIDIMessage(pm::fromMidi1Bytes(0xB0, 1, wheel));
        vm.handleMIDIMessage(pm::fromMidi1Bytes(0x90, 60, 100));
        const std::vector<float> audio = render(vm, uint32_t(kSampleRate * 1.5));
        return trackPitch(audio, size_t(kSampleRate / 2), size_t(kSampleRate / 2) + 48000, 60);
    }
};

void testModMultiplyKeepsCentre() {
    std::printf("Mod sections (Multiply / Add):\n");
    auto cents = [](double octaves) { return std::to_string(int(std::lround(octaves * 1200.0))) + " ct"; };

    for (uint8_t wheel : { uint8_t(0), uint8_t(32), uint8_t(64), uint8_t(96), uint8_t(127) }) {
        ModRig rig(posLfo1, 0.58f, posCcConverter1, 1.0f, true);
        const PitchTrack t = rig.play(wheel);
        std::printf("        LFO 1 x CC #1, wheel %3d: centre %+.4f oct, swing %+.3f..%+.3f\n",
                    wheel, t.centre, t.low, t.high);
        check(std::fabs(t.centre) < 0.01,
              "LFO 1 (0.58) x CC #1 (1.00) -> Osc Pitch: wheel " + std::to_string(wheel)
                  + " leaves the centre on C4 (" + cents(t.centre) + ")");
        if (wheel == 0)
            check(t.high - t.low < 0.01, "wheel 0: no vibrato (" + cents(t.high - t.low) + " swing)");
        if (wheel == 127)   // 0.58 x 2 oct each way = 2.32 oct peak to peak (cycle-averaged, a bit less)
            check(t.high - t.low > 1.8, "wheel 127: full vibrato (" + cents(t.high - t.low) + " swing)");
    }

    {   // In B None: a lone LFO at Level 0.58 swings around C4 too (it used to sit 0.84 oct up).
        ModRig rig(posLfo1, 0.58f, 0, 1.0f, true);
        const PitchTrack t = rig.play(0);
        check(std::fabs(t.centre) < 0.01 && t.high - t.low > 1.8,
              "Multiply, LFO 1 alone at 0.58: centred (" + cents(t.centre) + "), swinging "
                  + cents(t.high - t.low));
    }
    {   // Unipolar x unipolar is unchanged: ADSR 1 (sustain 1) x CC #1 at Levels 1, 0.5, wheel full.
        ModRig rig(posAdsr1, 1.0f, posCcConverter1, 0.5f, true);
        const PitchTrack t = rig.play(127);
        check(std::fabs(t.centre - 1.0) < 0.01,
              "Multiply, ADSR 1 (1.0) x CC #1 (0.5), wheel 127: +1 oct (" + cents(t.centre) + ")");
    }
    {   // Add is untouched: LFO 1 (0.58) + CC #1 (0.5), wheel full → centre +1 oct, LFO around it.
        ModRig rig(posLfo1, 0.58f, posCcConverter1, 0.5f, false);
        const PitchTrack t = rig.play(127);
        check(std::fabs(t.centre - 1.0) < 0.01 && t.high - t.low > 1.8,
              "Add, LFO 1 (0.58) + CC #1 (0.5), wheel 127: centre +1 oct (" + cents(t.centre) + ")");
    }
}

} // namespace

int main() {
    std::printf("PlainOldSynth smoke test (%.0f Hz)\n", kSampleRate);
    testPlainNote();
    testMidi2Pitch79();
    testUmpPerNoteDecode();
    testChannelBend();
    testPerNoteBend();
    testGeneratedNoteOffsMatchChannel();
    testPerNoteBendRange();
    testClapNoteExpressionDecode();
    testClapNoteIds();
    testPressureSource();
    testTimbreSource();
    testMpe();
    testMidi1Scaling();
    testVelocity();
    testChannelModeMessages();
    testReleasedNotes();
    testPercussiveRelease();
    testPolyAndMono();
    testPeakMorphsWaveform();
    testPeakIsLevelFlat();
    testSelfModulation();
    testModMultiplyKeepsCentre();

    if (gFailures != 0) {
        std::printf("%d check(s) FAILED\n", gFailures);
        return 1;
    }
    std::printf("ALL PASS\n");
    return 0;
}
