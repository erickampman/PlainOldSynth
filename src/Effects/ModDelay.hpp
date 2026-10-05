// src/Effects/ModDelay.hpp — the post-voice-sum effect: ONE LFO-modulated stereo delay line that
// is a flanger, a chorus, or an echo depending only on the range of delay times it is run at.
//
//   Flanger     0.5–10 ms, deep sweep, feedback (negative feedback is the hollow/metallic flange)
//   Chorus      5–40 ms, a few ms of sweep, little or no feedback
//   Delay       20 ms–2 s, free-running
//   Sync Delay  a note division of the host tempo (1/32 … 1/1, dotted and triplet)
//
// The DSP core is PolygraphModularUnified's GlobalDelayModule (Pow2CircBuff + Catmull-Rom read,
// feedback into its own line) with what that module lacks for use as a synth's only effect:
//   - an LFO on the read position, with the right channel's LFO phase offset by Spread (0..180°):
//     that phase split is what makes a chorus/flanger wide; identical modulation would stay mono
//   - in the Delay modes, Spread instead blends a plain stereo delay into ping-pong (the summed
//     input enters the left line only and each line feeds the OTHER — GlobalDelay's cross-feedback),
//     which keeps every echo on the beat, unlike offsetting one side's time
//   - delay-length smoothing, so turning Time (or a host tempo change) glides instead of clicking
//   - a one-pole low-pass on the delayed signal (Tone), which damps each repeat in the loop
//   - buffers flushed on mode change / reset, so a stale tail never replays
//
// Runs on the audio thread only. setSampleRate() allocates and must be called from activate().
// The parameter mapping helpers below are shared with the GUI and the CLAP params text functions,
// so the knob readout, the host's text and the DSP can never disagree about what Time means.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "Pow2CircBuff.hpp"

namespace plainoldsynth {

// Mode indices — the Effect panel's Mode dropdown stores these. Appended, never inserted.
enum FxMode : int { kFxOff = 0, kFxFlanger, kFxChorus, kFxDelay, kFxSyncDelay, kFxModeCount };

struct FxSyncDivision { const char* label; double beats; };
inline constexpr FxSyncDivision kFxSyncDivisions[] = {
    { "1/32",   0.125 },
    { "1/16 T", 0.25 * 2.0 / 3.0 }, { "1/16", 0.25 }, { "1/16 D", 0.375 },
    { "1/8 T",  0.5  * 2.0 / 3.0 }, { "1/8",  0.5  }, { "1/8 D",  0.75  },
    { "1/4 T",  1.0  * 2.0 / 3.0 }, { "1/4",  1.0  }, { "1/4 D",  1.5   },
    { "1/2 T",  2.0  * 2.0 / 3.0 }, { "1/2",  2.0  }, { "1/2 D",  3.0   },
    { "1/1",    4.0 },
};
inline constexpr int kFxSyncDivisionCount = int(sizeof(kFxSyncDivisions) / sizeof(kFxSyncDivisions[0]));

// Time is stored as 0..1 and read through the current mode, so one knob covers each mode's range
// with full travel instead of a sliver of a 0.5 ms–2 s sweep.
inline void fxTimeRangeMs(int mode, double& lo, double& hi)
{
    switch (mode) {
    case kFxFlanger: lo = 0.5;  hi = 10.0;   break;
    case kFxChorus:  lo = 5.0;  hi = 40.0;   break;
    default:         lo = 20.0; hi = 2000.0; break;   // Delay (and Off, for readouts)
    }
}

inline int fxSyncDivisionIndex(double t)
{
    const int i = int(std::lround(std::clamp(t, 0.0, 1.0) * (kFxSyncDivisionCount - 1)));
    return std::clamp(i, 0, kFxSyncDivisionCount - 1);
}

inline double fxTimeMs(int mode, double t, double bpm)
{
    if (mode == kFxSyncDelay) {
        const double beatMs = 60000.0 / std::max(bpm, 1.0);
        return kFxSyncDivisions[fxSyncDivisionIndex(t)].beats * beatMs;
    }
    double lo, hi;
    fxTimeRangeMs(mode, lo, hi);
    return lo * std::pow(hi / lo, std::clamp(t, 0.0, 1.0));   // log law
}

// Readout for the Time control: a note division in Sync Delay, otherwise ms (or s past a second).
inline void formatFxTime(int mode, double t, char* out, size_t size)
{
    if (mode == kFxSyncDelay) {
        std::snprintf(out, size, "%s", kFxSyncDivisions[fxSyncDivisionIndex(t)].label);
        return;
    }
    const double ms = fxTimeMs(mode, t, 120.0);
    if (ms < 1000.0) std::snprintf(out, size, ms < 10.0 ? "%.2f ms" : "%.4g ms", ms);
    else             std::snprintf(out, size, "%.3g s", ms / 1000.0);
}

// Inverse of formatFxTime (CLAP text_to_value). Accepts a division label in Sync Delay, otherwise a
// number with an optional "ms"/"s" suffix (bare numbers are ms).
inline bool parseFxTime(int mode, const char* text, double& t)
{
    if (mode == kFxSyncDelay) {
        for (int i = 0; i < kFxSyncDivisionCount; ++i) {
            if (std::strcmp(kFxSyncDivisions[i].label, text) == 0) {
                t = double(i) / double(kFxSyncDivisionCount - 1);
                return true;
            }
        }
        return false;
    }
    char* end = nullptr;
    double ms = std::strtod(text, &end);
    if (end == text) return false;
    while (*end == ' ') ++end;
    if (end[0] == 's') ms *= 1000.0;
    double lo, hi;
    fxTimeRangeMs(mode, lo, hi);
    ms = std::clamp(ms, lo, hi);
    t = std::log(ms / lo) / std::log(hi / lo);
    return true;
}

class ModDelay {
public:
    void setSampleRate(double sampleRate)
    {
        mSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
        // Room for the longest Sync Delay (1/1) down to ~45 BPM, plus the modulation swing.
        int bits = 2;
        while ((1u << bits) < uint32_t(mSampleRate * 5.5) && bits < 24) ++bits;
        mBufL = std::make_unique<Pow2CircBuff>(bits);
        mBufR = std::make_unique<Pow2CircBuff>(bits);
        mMaxDelaySamples = double(mBufL->buffsize()) - 4.0;
        mDelaySmoothCoef = 1.0 - std::exp(-1.0 / (0.060 * mSampleRate));   // ~60 ms glide
        mGainSmoothCoef  = 1.0f - float(std::exp(-1.0 / (0.010 * mSampleRate)));
        mFlushPending = true;
    }

    void setMode(int mode)        { mMode = std::clamp(mode, 0, kFxModeCount - 1); }
    void setTime(float t)         { mTime = t; }
    void setFeedback(float fb)    { mFeedback = std::clamp(fb, -0.95f, 0.95f); }
    void setTone(float tone)      { mTone = std::clamp(tone, 0.0f, 1.0f); }
    void setRateHz(float hz)      { mRateHz = std::max(hz, 0.0f); }
    void setDepth(float d)        { mDepth = std::clamp(d, 0.0f, 1.0f); }
    void setMix(float mix)        { mMix = std::clamp(mix, 0.0f, 1.0f); }
    void setSpread(float s)       { mSpread = std::clamp(s, 0.0f, 1.0f); }
    void reset()                  { mFlushPending = true; }

    // In place on the summed master bus.
    void process(float* left, float* right, uint32_t count, double bpm)
    {
        if (mMode == kFxOff || !mBufL || !mBufR) {
            mLastMode = kFxOff;
            return;   // untouched pass-through
        }

        const double targetDelay = std::clamp(fxTimeMs(mMode, mTime, bpm) * 0.001 * mSampleRate,
                                              2.0, mMaxDelaySamples);
        if (mMode != mLastMode || mFlushPending) {
            // A tail recorded at a different delay range (or from before Off) must not replay, and
            // gliding from a 2 s echo down to a 1 ms flange would be a long pitch dive: start clean.
            mBufL->clear();
            mBufR->clear();
            mDelay = targetDelay;
            mToneL = mToneR = 0.0f;
            mDry = dryGain(); mWet = wetGain(); mFb = mFeedback;
            mLastMode = mMode;
            mFlushPending = false;
        }

        // Tone 1.0 = no damping (exactly a = 1); below that a one-pole low-pass from ~20 kHz down to
        // ~400 Hz, exponential in the knob so its travel is musically even.
        const float cutoff = 400.0f * std::pow(50.0f, mTone);
        const float toneA = (mTone >= 0.999f)
            ? 1.0f
            : 1.0f - std::exp(-2.0f * float(M_PI) * cutoff / float(mSampleRate));

        const double lfoInc  = double(mRateHz) / mSampleRate;
        const float  dryT = dryGain(), wetT = wetGain(), fbT = mFeedback;
        const bool   echo = (mMode == kFxDelay || mMode == kFxSyncDelay);
        // Flanger/Chorus: Spread is the right LFO's phase lead, 0.5 cycle (opposite) at full.
        const double rightPhase = echo ? 0.0 : 0.5 * double(mSpread);
        // Delay modes: Spread is the ping-pong amount (0 = two independent lines).
        const float  pp = echo ? mSpread : 0.0f;

        for (uint32_t i = 0; i < count; ++i) {
            mDelay += (targetDelay - mDelay) * mDelaySmoothCoef;
            mDry   += (dryT - mDry) * mGainSmoothCoef;
            mWet   += (wetT - mWet) * mGainSmoothCoef;
            mFb    += (fbT  - mFb)  * mGainSmoothCoef;

            const double lfoL = std::sin(2.0 * M_PI * mLfoPhase);
            const double lfoR = std::sin(2.0 * M_PI * (mLfoPhase + rightPhase));
            mLfoPhase += lfoInc;
            if (mLfoPhase >= 1.0) mLfoPhase -= 1.0;

            const double swing = mDepth * maxSwing(mDelay);
            const double dL = std::clamp(mDelay + swing * lfoL, 2.0, mMaxDelaySamples);
            const double dR = std::clamp(mDelay + swing * lfoR, 2.0, mMaxDelaySamples);

            mToneL += (mBufL->retrieveInterpolate(dL) - mToneL) * toneA;
            mToneR += (mBufR->retrieveInterpolate(dR) - mToneR) * toneA;

            const float inL = left[i], inR = right[i];
            // Ping-pong blend: at pp = 1 the mono sum enters only the left line and each line's
            // feedback goes to the other, so echoes alternate L, R, L... at the set time.
            const float mono  = 0.5f * (inL + inR);
            const float sendL = inL + (mono - inL) * pp;
            const float sendR = inR * (1.0f - pp);
            const float fbL   = mToneL + (mToneR - mToneL) * pp;
            const float fbR   = mToneR + (mToneL - mToneR) * pp;
            // The clamp is a safety net only (feedback is capped at 0.95): it keeps a pathological
            // input from building up in the loop, and flushes denormals out with the tiny offset.
            mBufL->insert(std::clamp(sendL + fbL * mFb, -4.0f, 4.0f) + 1e-20f);
            mBufR->insert(std::clamp(sendR + fbR * mFb, -4.0f, 4.0f) + 1e-20f);

            left[i]  = inL * mDry + mToneL * mWet;
            right[i] = inR * mDry + mToneR * mWet;
        }
    }

private:
    // Mix 0.5 is BOTH signals at unity (the classic chorus/flanger balance, and full comb depth);
    // either side of centre fades the other one out.
    float dryGain() const { return std::min(1.0f, 2.0f * (1.0f - mMix)); }
    float wetGain() const { return std::min(1.0f, 2.0f * mMix); }

    // How far Depth 1.0 moves the read position, in samples, around the current delay.
    double maxSwing(double delaySamples) const
    {
        switch (mMode) {
        case kFxFlanger: return 0.9 * delaySamples;   // sweeps ~0.1x..1.9x: the full comb sweep
        case kFxChorus:  return 0.5 * delaySamples;
        default:         return std::min(0.5 * delaySamples, 0.005 * mSampleRate);   // tape wobble
        }
    }

    std::unique_ptr<Pow2CircBuff> mBufL, mBufR;
    double mSampleRate = 48000.0;
    double mMaxDelaySamples = 0.0;
    double mDelaySmoothCoef = 0.0;
    float  mGainSmoothCoef = 1.0f;

    int   mMode = kFxOff, mLastMode = kFxOff;
    float mTime = 0.5f, mFeedback = 0.0f, mTone = 1.0f, mRateHz = 0.5f, mDepth = 0.5f, mMix = 0.5f,
          mSpread = 1.0f;

    double mDelay = 2.0, mLfoPhase = 0.0;
    float  mToneL = 0.0f, mToneR = 0.0f;
    float  mDry = 1.0f, mWet = 0.0f, mFb = 0.0f;
    bool   mFlushPending = true;
};

} // namespace plainoldsynth
