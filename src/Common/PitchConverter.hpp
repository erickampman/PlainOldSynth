//
//  PitchConverter.hpp
//
//  Created by Eric Kampman on 5/9/25.
//

#ifndef PitchConverter_h
#define PitchConverter_h

#pragma once

#include <cmath>
#include <cstdint>

inline constexpr double SEMITONES_TO_PITCH = 1.0 / 12.0;
inline constexpr double CENTS_TO_PITCH     = 1.0 / 1200.0;

// TODO integrate this:
inline float pitch725ToPitch1x(uint32_t pitch725) {
    return ((pitch725 >> 16) - 69) / 12.0f + ((pitch725 & 0xFFFF) / 65536.0f) / 12.0f;
}

class PitchConverter {
public:
    // Default base frequency: A4 = 440.0 Hz
    explicit PitchConverter(double baseFreqHz = 440.0)
        : baseFreq(baseFreqHz) {}

    // Frequency (Hz) → pitch (1.0 per octave)
    inline float frequencyToPitch(double freqHz) const {
        return static_cast<float>(std::log2(freqHz / baseFreq));
    }

    // Pitch (1.0 per octave) → frequency (Hz)
    inline double pitchToFrequency(float pitch) const {
        double ret = baseFreq * std::exp2f(pitch);
        return ret;
    }

    // MIDI note + optional fine offset → frequency
    inline double midiToFrequency(float midiNote) const {
        return 440.0 * std::pow(2.0, (midiNote - 69.0f) / 12.0f);
    }

#if 0
    // MIDI note → pitch (log2-based)
    inline float midiToPitch(float midiNote) const {
        double freq = midiToFrequency(midiNote);
        return frequencyToPitch(freq);
    }
#else
    // MIDI 1.0 note number -> octaves relative to A4
    inline float midiToPitch(uint8_t note) {
        return (float(note) - 69.0f) / 12.0f;
    }
#endif

    // ELK I think this is wrong.
//    // MIDI 2.0 Registered Per-Note Controller #3: Pitch 7.25 format
//    // Converts 32-bit unsigned int (7 MSBs = coarse, 25 LSBs = fine)
//    inline float midiPitch7_25ToFloat(uint32_t rawValue) const {
//        float semitones = static_cast<float>(rawValue) / static_cast<float>(1 << 25);
//        return midiToPitch(semitones);
//    }

    // Set a new base frequency (e.g. for microtonal tuning)
    inline void setBaseFrequency(double freqHz) {
        baseFreq = freqHz;
    }

    inline double getBaseFrequency() const {
        return baseFreq;
    }
    
#if 0
    inline double Pitch79ToOctaveOffset(uint16_t encoded)
    {
        // Pitch 7.9: upper 7 bits = integer MIDI note (0..127)
        //            lower 9 bits = fractional semitone in 1/512 steps (0..511)
        const uint16_t note = (encoded >> 9) & 0x7F;
        const uint16_t frac = encoded & 0x01FF;

        const double noteFloat = static_cast<double>(note)
                               + static_cast<double>(frac) / 512.0;

        // A440 is MIDI note 69. One octave = 12 semitones.
        // 0.0 -> A440, 1.0 -> A880, -1.0 -> A220, etc.
        return (noteFloat - 69.0) / 12.0;
    }
#else
    inline float Pitch79ToOctaveOffset(uint16_t q79) {
        return ( (float(q79) / 512.0f) - 69.0f ) / 12.0f;
    }
#endif

private:
    double baseFreq;
};

#endif /* PitchConverter_h */
