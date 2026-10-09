//
//  ControlRamp.hpp
//
//  De-zippering for MIDI-driven control values (pitch bend, pressure, per-note controllers). Those
//  arrive as discrete events, and a step in pitch or cutoff is audible, so a change of target ramps
//  linearly over kSeconds. A fresh note snaps instead (snapNext()): there is nothing sounding to
//  glide from, and the note must start at its current expression value.
//

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

struct ControlRamp {
    static constexpr double kSeconds = 0.002;

    // Jump straight to the target on the next fill().
    void snapNext() { snap = true; }

    // Write `count` samples heading for `target` (re-targeting the ramp if it moved) into `out`.
    void fill(float target, double sampleRate, float* out, uint32_t count) {
        if (snap) {
            snap = false;
            current = rampTarget = target;
            remaining = 0;
        } else if (target != rampTarget) {
            rampTarget = target;
            remaining = std::max<uint32_t>(1, uint32_t(std::lround(sampleRate * kSeconds)));
            step = (target - current) / float(remaining);
        }

        if (remaining == 0) {
            std::fill_n(out, count, current);
            return;
        }
        for (uint32_t i = 0; i < count; ++i) {
            if (remaining > 0) current = (--remaining == 0) ? rampTarget : current + step;
            out[i] = current;
        }
    }

private:
    float    current = 0.0f;
    float    rampTarget = 0.0f;
    float    step = 0.0f;
    uint32_t remaining = 0;
    bool     snap = true;
};
