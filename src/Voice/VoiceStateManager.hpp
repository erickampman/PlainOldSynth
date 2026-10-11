//
//  VoiceStateManager.hpp
//
//  Created by Eric Kampman on 5/15/25.
//

#ifndef VoiceStateManager_hpp
#define VoiceStateManager_hpp

#include "pm_midi.h"

#include <optional>
#include <vector>

#include "Module.hpp"

enum class VoiceState {
    Inactive,
    Active,
    Deactivating,
    Stealing
};

class VoiceStateManager {
public:
    VoiceStateManager(double inSampleRate) :
        sampleRate(inSampleRate) { heldNoteStack.reserve(32); }
    
    bool isActive() const { return state == VoiceState::Active; };
    bool isDeactivating() const { return state == VoiceState::Deactivating; };
    bool isInactive() const { return state == VoiceState::Inactive; };
    bool canBeStolen() const;
    bool startSteal(const pm::UmpMessage& deferredMessage);   // returns false if can't do it.

    bool isPedalSustained() const { return pedalSustained; }
    void setPedalSustained(bool sustained) { pedalSustained = sustained; }
    // The note has been let go: note-off received (ringing out) or held only by the sustain pedal.
    bool isReleased() const { return state == VoiceState::Deactivating || pedalSustained; }
        
    // returns true if voice should propagate the message
    bool handleMIDIMessage(const pm::UmpMessage& message, uint32_t voiceID);
    bool wantsMIDIMessage(const pm::UmpMessage& deferredMessage, uint32_t voiceID) const;
    
    void process(uint32_t frameCount, Voice *voice, float *outLeft, float *outRight);

    VoiceState getState() { return state; }

    // The note this voice is sounding (nullopt when idle), and the mono note stack in press order.
    // Used to build note-offs that match on channel and group, not just note number.
    const std::optional<pm::UmpMessage>& currentNote() const { return currentNoteOn; }
    const std::vector<pm::UmpMessage>& heldNotes() const { return heldNoteStack; }
    // Whether this voice still holds `noteOn` in any form: sounding (held or ringing out), arriving
    // through a steal, or waiting in the mono stack. False once the voice has rung down to silence.
    bool holdsNote(const pm::UmpMessage& noteOn) const;
    // Whether any note this voice holds (as holdsNote) was played on this group/channel.
    bool holdsNoteOnChannel(uint8_t group, uint8_t channel) const;
    // All Sound Off: drop every note at once and go Inactive. The caller silences the modules.
    void silence();
    inline const char* stateString() const;
    
private:
    VoiceState state = VoiceState::Inactive;
    double sampleRate = 44100.0;
    bool pedalSustained = false;
    
    float duckPerSample = 1.0;  // for note stealing
    float duckMultiplier = 1.0;
    
    void handleMIDINoteMessageInInactiveState(const pm::UmpMessage& message, uint32_t voiceID);
    bool handleMIDINoteMessageInActiveState(const pm::UmpMessage& message, uint32_t voiceID);
    void handleMIDINoteMessageInDeactivatingState(const pm::UmpMessage& message, uint32_t voiceID);
    bool handleMIDINoteMessageInStealingState(const pm::UmpMessage& message, uint32_t voiceID);

    void processInInactiveState(uint32_t frameCount, float *inLeft, float *inRight, float *outLeft, float *outRight);
    void processInActiveState(uint32_t frameCount, float *inLeft, float *inRight, float *outLeft, float *outRight);
    void processInDeactivatingState(uint32_t frameCount, float *inLeft, float *inRight, float *outLeft, float *outRight);
    void processInStealingState(uint32_t frameCount, Voice *voice, float *inLeft, float *inRight, float *outLeft, float *outRight);
    
    void copyNoteOnMessageToCurrentNoteOn(const pm::UmpMessage& message);
    void clearCurrentNoteOn();
    
    std::optional<pm::UmpMessage> currentNoteOn = std::nullopt;
    std::optional<pm::UmpMessage> deferredNoteOn = std::nullopt;
    std::optional<pm::UmpMessage> deferredNoteOff = std::nullopt;

    // Stack of physically-held notes in mono/legato mode, in press order.
    // When the currently-playing note is released, the previous note at the
    // back of the stack (if any) gets a new Note On so it can resume.
    std::vector<pm::UmpMessage> heldNoteStack;

    // Set when the active note is released but a prior held note needs to be
    // re-triggered.  Consumed at the top of process() so the Note On reaches
    // the modules in the correct order (after any Note Off already dispatched).
    std::optional<pm::UmpMessage> deferredRetrigger = std::nullopt;

    // Consecutive below-threshold frames seen while Deactivating. The plugin splits rendering at
    // MIDI event offsets, so a process() call can be only a few frames long; judging silence on
    // one such sliver (e.g. a single sample at a zero crossing) would free a still-ringing voice.
    // Require kMinSilentFrames of accumulated silence instead.
    static constexpr uint32_t kMinSilentFrames = 256;
    uint32_t silentFrames = 0;
};

#endif /* VoiceStateManager_hpp */
