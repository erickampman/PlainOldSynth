//
//  ADSRHelper.mm
//
//  Created by Eric Kampman on 5/31/25.
//


#include <algorithm>
#include "pm_midi.h"
#include "Module.hpp"

#include "ADSRHelper.hpp"


ADSRHelper::ADSRHelper(float sr) : sampleRate(sr) {}

void ADSRHelper::setSampleRate(float sr) { sampleRate = sr; }

void ADSRHelper::setAttackTime(float seconds) { attackTime = seconds; }
void ADSRHelper::setDecayTime(float seconds) { decayTime = seconds; }
void ADSRHelper::setReleaseTime(float seconds) { releaseTime = seconds; }
void ADSRHelper::setSustainLevel(float level) { sustainLevel = std::clamp(level, 0.0f, 1.0f); }
void ADSRHelper::setLinearMode(bool linear) { linearMode = linear; }

//float ADSRHelper::calculateCoef(float timeSeconds, float targetLevel) {
//    if (timeSeconds <= 0.0f) return 0.0f;
//    return std::exp(1.0 - std::log(targetLevel) / (timeSeconds * sampleRate));
//}
float ADSRHelper::calculateAttackCoef(float timeSeconds, float targetLevel) {
    if (timeSeconds <= 0.0f) return 0.0f;
    return std::exp(std::log(1.0 - targetLevel) / (timeSeconds * sampleRate));
}

float ADSRHelper::calculateDecayCoef(float timeSeconds) {
    if (timeSeconds <= 0.0f) return 0.0f;
    constexpr float targetRatio = 0.001;
    return std::exp(std::log(targetRatio) / (sampleRate * timeSeconds));
}

void ADSRHelper::processMIDI(const pm::UmpMessage& message) {
    if (pm::isNoteOn(message)) {
        noteOn();
    } else if (pm::isNoteOff(message)) {
        noteOff();
    }
}

void ADSRHelper::noteOn() {
    state = State::Attack;
    value = 0.0001f;
    if (linearMode) {
        linearAttackRate = (attackTime > 0.0f) ? 1.0f / (attackTime * sampleRate) : 1.0f;
    } else {
        target = 0.98f;        // FIXME need some fn (& param) to map velocity to target.
        coef = calculateAttackCoef(attackTime, target);
    }
}

void ADSRHelper::noteOnLegato() {
    // Legato retrigger: leave value where it is and restart the attack
    // from the current level so there is no click or reset to zero.
    state = State::Attack;
    if (linearMode) {
        linearAttackRate = (attackTime > 0.0f) ? 1.0f / (attackTime * sampleRate) : 1.0f;
    } else {
        target = 0.98f;
        coef = calculateAttackCoef(attackTime, target);
    }
}

void ADSRHelper::noteOff() {
    if (state != State::Idle) {
        state = State::Release;
        if (linearMode) {
            valueAtReleaseStart = value;
            linearReleaseRate = (releaseTime > 0.0f && valueAtReleaseStart > 0.0f)
                ? valueAtReleaseStart / (releaseTime * sampleRate)
                : valueAtReleaseStart;
        } else {
            target = 0.0f;
            coef = calculateDecayCoef(releaseTime);
        }
    }
}

float ADSRHelper::nextSampleExp() {
    switch (state) {
        case State::Idle:
            return 0.0f;
        case State::Attack:
            value += (1.0f - value) * (1.0f - coef);
            if (value >= 0.999f) {
                state = State::Decay;
                target = sustainLevel;
                coef = calculateDecayCoef(decayTime);
            }
            break;
        case State::Sustain:
            value = sustainLevel;
            break;
        case State::Decay:
            value *= coef;
            if (value <= sustainLevel) {
                value = sustainLevel;
                state = State::Sustain;
            }
            break;
        case State::Release:
            value *= coef;
            if (value <= 0.001f) {
                state = State::Idle;
                value = 0.0f;
            }
            break;
    }
    return value;
}

float ADSRHelper::nextSampleLinear() {
    switch (state) {
        case State::Idle:
            return 0.0f;
        case State::Attack:
            value += linearAttackRate;
            if (value >= 1.0f) {
                value = 1.0f;
                state = State::Decay;
                linearDecayRate = (decayTime > 0.0f)
                    ? (1.0f - sustainLevel) / (decayTime * sampleRate)
                    : (1.0f - sustainLevel);
            }
            break;
        case State::Sustain:
            value = sustainLevel;
            break;
        case State::Decay:
            value -= linearDecayRate;
            if (value <= sustainLevel) {
                value = sustainLevel;
                state = State::Sustain;
            }
            break;
        case State::Release:
            value -= linearReleaseRate;
            if (value <= 0.0f) {
                state = State::Idle;
                value = 0.0f;
            }
            break;
    }
    return value;
}

float ADSRHelper::nextSample() {
    return linearMode ? nextSampleLinear() : nextSampleExp();
}

void ADSRHelper::nextValueSet(size_t count, float *buff) {
    if (linearMode) {
        for (float *p = buff, *end = buff + count; p < end; ++p)
            *p = nextSampleLinear();
    } else {
        for (float *p = buff, *end = buff + count; p < end; ++p)
            *p = nextSampleExp();
    }
}

bool ADSRHelper::isSilent(size_t count, float *buff, float threshold) const {
    float *p = buff, *opl;
    float sumSq = 0.0f;
    for (p = buff, opl = p + count; p < opl; ++p) {
        sumSq += *p * *p;
    }
    float rms = sqrtf(sumSq / float(count));
    return rms < threshold;
}

const char *ADSRHelper::getStateString() {
    switch (state) {
    case State::Attack:
        return "Attack";
    case State::Decay:
        return "Decay";
    case State::Sustain:
        return "Sustain";
    case State::Release:
        return "Release";
    default:
        return "Unknown";
    }
}

void ADSRHelper::onSampleRateChanged(float newSR) {
    if (newSR <= 0.0f) return;
    sampleRate = newSR;

    // Refresh the active stage's coefficient so timing remains correct
    switch (state) {
        case State::Attack:
            if (linearMode)
                linearAttackRate = (attackTime > 0.0f) ? 1.0f / (attackTime * sampleRate) : 1.0f;
            else
                coef = calculateAttackCoef(attackTime, target);
            break;
        case State::Decay:
            if (linearMode)
                linearDecayRate = (decayTime > 0.0f)
                    ? (1.0f - sustainLevel) / (decayTime * sampleRate)
                    : (1.0f - sustainLevel);
            else
                coef = calculateDecayCoef(decayTime);
            break;
        case State::Sustain:
            // no coef needed
            break;
        case State::Release:
            if (linearMode)
                linearReleaseRate = (releaseTime > 0.0f && valueAtReleaseStart > 0.0f)
                    ? valueAtReleaseStart / (releaseTime * sampleRate)
                    : valueAtReleaseStart;
            else
                coef = calculateDecayCoef(releaseTime);
            break;
        case State::Idle:
            // nothing to do
            break;
    }
}

