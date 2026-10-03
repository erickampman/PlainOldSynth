//
//  ResonanceComp.hpp — shared resonance gain compensation for the resonant filters.
//
//  A resonant biquad's peak rises ≈ Q (≈ 20·log10(Q) dB) on top of a 0 dB passband, so cranking Q
//  just makes a louder, narrower, harsher peak. This attenuates the filter OUTPUT above a knee so the
//  peak is tamed. PARTIAL (sqrt) compensation: above the knee the peak still rises with Q — but about
//  half as fast in dB — and the passband loss is gentler than a hard ceiling, so the resonance knob
//  keeps changing brightness all the way up instead of only lowering level. Below the knee it's unity,
//  so normal (musical) settings are completely untouched. Applied only to resonant modes, never a
//  passthrough. Bake it in — it's a voicing/quality choice, not a user parameter.
//
#ifndef ResonanceComp_hpp
#define ResonanceComp_hpp

#include <cmath>

static inline float resonanceComp(float q, float knee = 3.0f) {
    return q > knee ? std::sqrt(knee / q) : 1.0f;
}

#endif /* ResonanceComp_hpp */
