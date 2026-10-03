//
//  ADSRHelper.hpp
//
//  Created by Eric Kampman on 5/31/25.
//

#ifndef ADSRHelper_hpp
#define ADSRHelper_hpp


#include "pm_midi.h"

class ADSRHelper {
public:
    enum class State {
        Idle,
        Attack,
        Sustain,
        Decay,
        Release
    };

    ADSRHelper(float sampleRate = 48000.0f);

    void setAttackTime(float seconds);
    void setDecayTime(float seconds);
    void setReleaseTime(float seconds);
    void setSustainLevel(float level); // 0.0 to 1.0
    void setSampleRate(float sr);
    void setLinearMode(bool linear);

    void processMIDI(const pm::UmpMessage& message);
    void noteOn();
    void noteOnLegato();    // legato retrigger: keep current value, restart attack
    void noteOff();

    float nextSample(); // get next envelope value (dispatches to exp or linear)
    void nextValueSet(size_t count, float *buff);

    State getState() const { return state; }
    const char *getStateString();
    
    bool isSilent(size_t count, float *buff, float threshold = 0.001f) const;
    
    void reset() {
        state = State::Idle;
        value = 0.0f;
        target = 0.0f;
        coef = 0.0f;
    }
    
    void onSampleRateChanged(float newSR);

private:
//    float calculateCoef(float timeSeconds, float target = 0.001f);
    float calculateAttackCoef(float timeSeconds, float target = 0.001f);
    float calculateDecayCoef(float timeSeconds);

    float nextSampleExp();
    float nextSampleLinear();

    void setState(State nState);
private:
    float sampleRate = 48000.0f;

    // Times in seconds
    float attackTime = 0.01f;
    float decayTime = 0.2f;
    float releaseTime = 0.3f;
    float sustainLevel = 0.7f;

    // Envelope state
    State state = State::Idle;
    float value = 0.0f;
    float target = 1.0f;
    float coef = 1.0f;

    // Linear mode
    bool linearMode = false;
    float valueAtReleaseStart = 0.0f;
    float linearAttackRate = 0.0f;
    float linearDecayRate = 0.0f;
    float linearReleaseRate = 0.0f;
};


#endif /* ADSRHelper_hpp */
