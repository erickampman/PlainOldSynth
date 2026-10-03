// LFOClockSync.hpp — locks an LFO's frequency and phase to an incoming clock (a 0/1 pulse train,
// typically the Clock module's output). Shared by LFO and SharedLFO.
//
// The clock period is measured as the sample count between successive rising edges; the LFO then
// runs at Ratio × the clock rate, where Ratio is a fraction num/den (LFO cycles per clock pulse):
// a whole multiple (×1..×16), a fraction below 1 (1/16..1/2, plus 2/3 and 3/4), or Auto — the
// whole multiple (×1..×16) or 1/n (1/2..1/16) nearest (in octaves) to the LFO's own Frequency
// setting, so the knob still sets roughly how fast it goes, snapped onto the clock grid. The ratio
// is (re)chosen on each clock edge, never mid-period.
//
// Phase is locked to where it SHOULD be at each rising edge: over each group of `den` clock pulses
// the LFO completes exactly `num` cycles, so edge k of the group lands at frac(k·num/den).
// (×N: every edge is a cycle start; 1/N: edge k at k/N; 3/4: edges at 0, ¾, ½, ¼.)
//
// Both corrections are SLEWED so the output never jumps:
//   - rate: the increment glides toward the clock-derived rate (one-pole),
//   - phase: the error measured at an edge is worked off gradually by briefly speeding up or
//     slowing down — never reversing (the correction is held within ±90% of the current rate).
// The time constant is ~¼ of an LFO cycle (at least 64 samples), so a slow LFO eases in gently
// and a fast one locks quickly. Each edge re-measures the error, so it converges like a PLL. With
// a steady clock the error is ~0 and nothing moves; after a tempo change the new period is picked
// up on the next edge and glided to. Until two edges have been seen (no period yet) the LFO keeps
// its own Frequency (then glides onto the clock). If the clock stops, it free-runs at the last rate.

#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// LFO cycles per clock pulse, as a fraction. {0,0} = Auto.
struct LFOClockRatio { int num, den; };

// Ratio param index -> ratio. Order matches kLfoClockRatio in pm_param_data.cpp.
inline constexpr LFOClockRatio kLFOClockRatios[] = {
    {0, 0},                                                   // Auto
    {1, 16}, {1, 8}, {1, 4}, {1, 3}, {1, 2}, {2, 3}, {3, 4},  // fractions of the clock
    {1, 1}, {2, 1}, {3, 1}, {4, 1}, {8, 1}, {16, 1},          // multiples of the clock
};
inline constexpr int kLFOClockRatioCount = sizeof(kLFOClockRatios) / sizeof(kLFOClockRatios[0]);
inline constexpr int kLFOClockMaxRatio = 16;   // Auto's search range: 1/16..×16

inline LFOClockRatio lfoClockRatioForIndex(float param) {
    int idx = (int)(param + 0.5f);
    if (idx < 0) idx = 0;
    if (idx >= kLFOClockRatioCount) idx = kLFOClockRatioCount - 1;
    return kLFOClockRatios[idx];
}

struct LFOClockSync {
    float    prevClock        = 0.0f;
    double   samplesSinceEdge = 0.0;
    double   periodSamples    = 0.0;   // 0 = not measured yet
    uint32_t edgeCount        = 0;     // edges since the last reset (for ÷N group position)
    LFOClockRatio activeRatio = {1, 1};  // resolved ratio in use (never Auto)
    double   curDelta         = 0.0;   // slewed rate actually in use (cycles/sample)
    double   pendingPhase     = 0.0;   // phase error still to be worked off (cycles, −0.5..0.5)

    void reset() {
        prevClock = 0.0f; samplesSinceEdge = 0.0; periodSamples = 0.0; edgeCount = 0;
        activeRatio = {1, 1}; curDelta = 0.0; pendingPhase = 0.0;
    }

    // Restart the ÷N grouping (the next edge becomes a cycle start) but keep the measured period.
    // Drops any pending phase correction (the caller has just set the phase itself).
    void restartGroup() { edgeCount = 0; pendingPhase = 0.0; }

    bool hasPeriod() const { return periodSamples > 0.0; }

    // Per-sample phase increment (cycles/sample) at the active ratio; only valid when hasPeriod().
    double delta() const {
        return (double)activeRatio.num / ((double)activeRatio.den * periodSamples);
    }

    // Auto: the integer multiple/submultiple of the clock nearest (log-wise) to `targetDelta`
    // (the LFO's own rate, cycles/sample).
    LFOClockRatio nearestRatio(double targetDelta) const {
        const double x = targetDelta * periodSamples;   // target LFO cycles per clock pulse
        if (!(x > 0.0)) return {1, 1};
        const bool   up = x >= 1.0;
        const double y  = up ? x : 1.0 / x;            // >= 1
        double n = std::floor(y);
        if (std::log2(y / n) > std::log2((n + 1.0) / y)) n += 1.0;   // nearer in octaves
        if (n > kLFOClockMaxRatio) n = kLFOClockMaxRatio;
        const int ni = (int)n;
        return up ? LFOClockRatio{ni, 1} : LFOClockRatio{1, ni};
    }

    // Feed one clock sample. On a rising edge, updates the period, resolves the ratio (`ratio`
    // from lfoClockRatioForIndex; {0,0} = Auto, nearest to `targetDelta`) and measures how far the
    // current `phase` is from where this edge says it should be (worked off later by step()).
    // Call once per sample BEFORE rendering it, then use step() for that sample's increment.
    void edge(float clock, LFOClockRatio ratio, double targetDelta, double phase) {
        const bool rising = clock >= 0.5f && prevClock < 0.5f;
        prevClock = clock;
        if (!rising) return;
        if (edgeCount > 0 && samplesSinceEdge >= 1.0) periodSamples = samplesSinceEdge;
        samplesSinceEdge = 0.0;
        activeRatio = ratio.den != 0 ? ratio
                    : hasPeriod() ? nearestRatio(targetDelta) : LFOClockRatio{1, 1};
        const uint32_t k = edgeCount % (uint32_t)activeRatio.den;   // position in the group
        const double target = (double)((k * (uint32_t)activeRatio.num) % (uint32_t)activeRatio.den)
                            / (double)activeRatio.den;
        double err = target - phase;                                  // shortest way round
        err -= std::floor(err + 0.5);
        pendingPhase = err;
        ++edgeCount;
    }

    // This sample's phase increment (cycles/sample): the slewed clock rate plus a slice of the
    // pending phase correction. `freeDelta` is the LFO's own rate, used until a period is known.
    // Never reverses: the result stays within 10%..190% of the current rate.
    double step(double freeDelta) {
        samplesSinceEdge += 1.0;
        if (!hasPeriod()) { curDelta = freeDelta; return freeDelta; }
        if (curDelta <= 0.0) curDelta = freeDelta;
        const double target = delta();
        const double a = std::min(4.0 * std::max(target, curDelta), 1.0 / 64.0);   // 1/(¼ cycle)
        curDelta += (target - curDelta) * a;
        const double lim = 0.9 * curDelta;
        const double corr = std::clamp(pendingPhase * a, -lim, lim);
        pendingPhase -= corr;
        return curDelta + corr;
    }
};
