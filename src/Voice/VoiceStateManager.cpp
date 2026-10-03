//
//  VoiceStateManager.cpp
//
//  Created by Eric Kampman on 5/15/25.
//

#include "pm_midi.h"

#include "Voice.hpp"
#include "VoiceStateManager.hpp"


void addInToOut(uint32_t frameCount, float *inLeft, float *inRight, float *outLeft, float *outRight)
{
    float *ilp = inLeft;
    float *irp = inRight;
    float *lopl = ilp + frameCount;
    float *olp = outLeft;
    float *orp = outRight;
    
    for (; ilp < lopl; ++ilp, ++irp, ++olp, ++orp) {
        *olp += *ilp;
        *orp += *irp;
    }
}

bool isSilent(float threshold, uint32_t frameCount, float *left, float *right) {
    float sumSqL = 0.0f;
    float sumSqR = 0.0f;
    float *lp, *rp, *opl;
    
    for (lp = left, rp = right, opl = left + frameCount;
         lp < opl; ++lp, ++rp)
    {
        sumSqL += *lp * *lp;
        sumSqR += *rp * *rp;
    }
    float rmsL = sqrtf(sumSqL / frameCount);
    float rmsR = sqrtf(sumSqR / frameCount);
    return (rmsL + rmsR) < threshold;   // is adding kosher?
}
    
float calcDuckPerSample(double sampleRate)
{
    const double duckTime = 0.005;
    const double startValue = 1.0;
    
    return startValue/(sampleRate * duckTime);
}

inline const char* VoiceStateManager::stateString() const {
    switch (state) {
        case VoiceState::Inactive:     return "Inactive";
        case VoiceState::Active:       return "Active";
        case VoiceState::Deactivating: return "Deactivating";
        case VoiceState::Stealing:     return "Stealing";
        default:                       return "Unknown";
    }
}

bool VoiceStateManager::wantsMIDIMessage(const pm::UmpMessage& message, uint32_t voiceID) const
{
    LOG_VOICE("VoiceStateManager::handleMIDIMessage entrypoint id(%d)", voiceID);
    if (!currentNoteOn) {
        LOG_VOICE("VoiceStateManager::handleMIDIMessage currentNoteOn is empty");
        return true;
    }
    
    uint8_t msgNote;
    if (pm::containsNoteNumber(message, msgNote)) {
        uint8_t currentNote = currentNoteOn.value().note;
        if (currentNote == msgNote) {
            LOG_VOICE("VoiceStateManager::wantsMIDIMessage currentNote == msgNote");
            return true;
        }
        if (deferredNoteOff) {
            uint8_t defNoteOff = deferredNoteOff.value().note;
            if (defNoteOff == msgNote) {
                LOG_VOICE("VoiceStateManager::wantsMIDIMessage defNoteOff == msgNote");
                return true;
            }
        }
        if (deferredNoteOn) {
            uint8_t defNoteOn = deferredNoteOn.value().note;
            if (defNoteOn == msgNote) {
                LOG_VOICE("VoiceStateManager::wantsMIDIMessage defNoteOn == msgNote");
                return true;
            }
        }
        // Also accept messages for any physically-held note in the stack.
        // This is necessary so that e.g. a Note Off for a prior held note
        // (not currently sounding) can be tracked and removed from the stack.
        for (const auto& held : heldNoteStack) {
            if (held.note == msgNote) {
                LOG_VOICE("VoiceStateManager::wantsMIDIMessage note found in heldNoteStack");
                return true;
            }
        }
        LOG_VOICE("VoiceStateManager::wantsMIDIMessage Note Number doesn't match");
        return false;
    }

    LOG_VOICE("VoiceStateManager::wantsMIDIMessage default returns true");
    return true;
}

// returns true if voice should propagate the message
bool VoiceStateManager::handleMIDIMessage(const pm::UmpMessage& message, uint32_t voiceID)
{
    // Do we care about anything but Note messages? I think not
    bool isNoteOn = pm::isNoteOn(message);
    bool isNoteOff = pm::isNoteOff(message);
    bool wantsPropagate = true;
    
    if (!(isNoteOn || isNoteOff)) {  // Later remove this check
        return wantsPropagate;
    }
    
    // pm::isNoteOn checks if velocity is 0. If it is, it's not a note on.
    LOG_VOICE("VoiceStateManager::handleMIDIMessage VOICE-ID(%d) VSM State: %{public}s - Is on: %{public}s Is off: %{public}s", voiceID, stateString(), isNoteOn ? "true" : "false", isNoteOff ? "true" : "false" );

    switch (state) {
    case VoiceState::Inactive:
        if (isNoteOn) {
            handleMIDINoteMessageInInactiveState(message, voiceID);
            break;
        }
        if (Voice::loggingEnabled) {
            LOG_VOICE("VoiceStateManager::handleMIDIMessage non-Note-On received in Inactive state VOICE-ID(%d)", voiceID);
        }
        break;
    case VoiceState::Active:
        wantsPropagate = handleMIDINoteMessageInActiveState(message, voiceID);
        break;
    case VoiceState::Deactivating:
        handleMIDINoteMessageInDeactivatingState(message, voiceID);
        break;
   case VoiceState::Stealing:
        wantsPropagate = handleMIDINoteMessageInStealingState(message, voiceID);
        break;
    }
    
    return wantsPropagate;
}


void VoiceStateManager::process(uint32_t frameCount, Voice *voice, float *outLeft, float *outRight) {
    // If a prior held note needs to be re-triggered (because the note that
    // took over was released while the prior note was still held), send its
    // Note On to the voice's modules now.  This happens after any Note Off
    // that was already dispatched in handleMIDIMessage, so the modules see
    // the correct order: silence the outgoing note, then restart the prior one.
    if (deferredRetrigger) {
        pm::UmpMessage retrigMsg = *deferredRetrigger;
        deferredRetrigger = std::nullopt;
        voice->handleMIDIMessage(retrigMsg);
    }

    Module* finalModule = voice->getFinalModule();
    float *inLeft = finalModule->output.first->data;
    float *inRight = finalModule->output.second->data;

    switch (state) {
    case VoiceState::Inactive:
        processInInactiveState(frameCount, inLeft, inRight, outLeft, outRight);
        break;
    case VoiceState::Active:
        processInActiveState(frameCount, inLeft, inRight, outLeft, outRight);
        break;
    case VoiceState::Deactivating:
        processInDeactivatingState(frameCount, inLeft, inRight, outLeft, outRight);
        break;
   case VoiceState::Stealing:
        LOG_VOICE("VoiceStateManager::process - stealing");
        processInStealingState(frameCount, voice, inLeft, inRight, outLeft, outRight);
        break;
    }
}

void VoiceStateManager::handleMIDINoteMessageInInactiveState(const pm::UmpMessage& message, uint32_t voiceID)
{
    LOG_VOICE("VoiceStateManager::handleMIDINoteMessageInInactiveState id(%d)", voiceID);
    state = VoiceState::Active;

    if (pm::isNoteOn(message)) {
        copyNoteOnMessageToCurrentNoteOn(message);
        heldNoteStack.clear();  // defensive — should already be empty
        auto noteOn = pm::makeNoteOnMsgFromOnMsg(message);
        if (noteOn) heldNoteStack.push_back(*noteOn);
    }
    // else ??? -- ignore note off
}
// Returns true if the message should be propagated to the voice's modules.
bool VoiceStateManager::handleMIDINoteMessageInActiveState(const pm::UmpMessage& message, uint32_t voiceID)
{
    LOG_VOICE("VoiceStateManager::handleMIDINoteMessageInActiveState id(%d)", voiceID);

    if (pm::isNoteOff(message)) {
        uint8_t releasedNote = message.note;

        // Remove the released note from the held stack.
        auto it = std::find_if(heldNoteStack.begin(), heldNoteStack.end(),
            [releasedNote](const pm::UmpMessage& m) {
                return m.note == releasedNote;
            });
        if (it != heldNoteStack.end()) heldNoteStack.erase(it);

        bool isCurrentNote = currentNoteOn &&
            (currentNoteOn->note == releasedNote);

        if (isCurrentNote) {
            if (!heldNoteStack.empty()) {
                // A prior note is still physically held.  Schedule a Note On
                // for it; it will be dispatched to the modules at the top of
                // the next process() call, after the Note Off has already
                // been suppressed (return false below).
                deferredRetrigger = heldNoteStack.back();
                copyNoteOnMessageToCurrentNoteOn(heldNoteStack.back());
                // Stay in Active state — do NOT transition to Deactivating.
                return false;  // suppress Note Off propagation to modules
            } else {
                // No more held notes — begin normal release.
                state = VoiceState::Deactivating;
                silentFrames = 0;
                return true;
            }
        } else {
            // Note Off for a held-but-not-currently-sounding note.
            // Already removed from the stack above; don't send to modules.
            return false;
        }

    } else if (pm::isNoteOn(message)) {
        // Legato Note On: update pitch attribution so wantsMIDIMessage()
        // tracks the new note. Push onto the held stack only if not already
        // present (guards against spurious duplicates during retrigger path).
        copyNoteOnMessageToCurrentNoteOn(message);
        uint8_t noteNum = message.note;
        bool inStack = std::any_of(heldNoteStack.begin(), heldNoteStack.end(),
            [noteNum](const pm::UmpMessage& m) {
                return m.note == noteNum;
            });
        if (!inStack) {
            auto noteOn = pm::makeNoteOnMsgFromOnMsg(message);
            if (noteOn) heldNoteStack.push_back(*noteOn);
        }
        return true;
    }

    return true;
}
void VoiceStateManager::handleMIDINoteMessageInDeactivatingState(const pm::UmpMessage& message, uint32_t voiceID)
{
    LOG_VOICE("VoiceStateManager::handleMIDINoteMessageInDeactivatingState id(%d)", voiceID);
    if (pm::isNoteOn(message)) {
        state = VoiceState::Active;
        copyNoteOnMessageToCurrentNoteOn(message);  // update attribution so NoteOff can match
        // Stack was empty when we entered Deactivating; start fresh.
        heldNoteStack.clear();
        auto noteOn = pm::makeNoteOnMsgFromOnMsg(message);
        if (noteOn) heldNoteStack.push_back(*noteOn);
    }
}
bool VoiceStateManager::handleMIDINoteMessageInStealingState(const pm::UmpMessage& message, uint32_t voiceID)
{
    LOG_VOICE("VoiceStateManager::handleMIDINoteMessageInStealingState id(%d)", voiceID);
    // unlikely but possible?
    if (pm::isNoteOff(message)) {
        deferredNoteOff = pm::makeNoteOffMsgFromOffMsg(message);
    }
    return false;
}

void VoiceStateManager::processInInactiveState(uint32_t frameCount, float *inLeft, float *inRight, float *outLeft, float *outRight)
{
    // this should not happen.
}

void VoiceStateManager::processInActiveState(uint32_t frameCount, float *inLeft, float *inRight, float *outLeft, float *outRight)
{
    addInToOut(frameCount, inLeft, inRight, outLeft, outRight);
}

void VoiceStateManager::processInDeactivatingState(uint32_t frameCount, float *inLeft, float *inRight, float *outLeft, float *outRight)
{
    if (!isSilent(1e-5, frameCount, inLeft, inRight)) {
        silentFrames = 0;
    } else if ((silentFrames += frameCount) >= kMinSilentFrames) {
        silentFrames = 0;
        LOG_VOICE("VoiceStateManager::processInDeactivatingState clearing state info");
        state = VoiceState::Inactive;
        clearCurrentNoteOn();
        heldNoteStack.clear();
        deferredRetrigger = std::nullopt;
        deferredNoteOn = std::nullopt;
        deferredNoteOff = std::nullopt;
        // no point in copying the data, I guess.
        return;
    }
    addInToOut(frameCount, inLeft, inRight, outLeft, outRight);
}

void VoiceStateManager::processInStealingState(uint32_t frameCount, Voice *voice, float *inLeft, float *inRight, float *outLeft, float *outRight)
{
    float *ilp = inLeft;
    float *irp = inRight;
    float *opl = irp + frameCount;
    float *olp = outLeft;
    float *orp = outRight;
    
    for(; irp < opl; ++ilp, ++irp, ++olp, ++orp) {
        duckMultiplier -= duckPerSample;
        if (duckMultiplier <= 0) {
            duckMultiplier = 0;
            break;
        }
        *olp += *ilp * duckMultiplier;
        *orp += *irp * duckMultiplier;
    }
    
    if (currentNoteOn == std::nullopt || deferredNoteOn == std::nullopt) {
        LOG_VOICE("VoiceStateManager::processInStealingState -- insufficient info for new note.");
        state = VoiceState::Inactive;
        return;
    }

    // Still ducking — come back next buffer.
    if (duckMultiplier > 0) {
        return;
    }

    // Duck is complete — silence the old note, start the new one.
    state = VoiceState::Inactive;  // start over so NoteOn is received cleanly
    if (deferredNoteOff) {
        voice->handleMIDIMessage(*deferredNoteOff);
    } else {
        std::optional<pm::UmpMessage> noteOff = pm::makeNoteOffMsgFromOnMsg(*currentNoteOn);
        voice->handleMIDIMessage(*noteOff);
    }
    voice->handleMIDIMessage(*deferredNoteOn);

    LOG_VOICE("VoiceStateManager::processInStealingState -- back to active with new note.");
    copyNoteOnMessageToCurrentNoteOn(deferredNoteOn.value());
    deferredNoteOn = std::nullopt;
    deferredNoteOff = std::nullopt;
}

const char *stateString(VoiceState state) {
    switch (state) {
    case VoiceState::Inactive:
        return "Inactive";
    case VoiceState::Active:
        return "Active";
    case VoiceState::Deactivating:
        return "Deactivating";
    case VoiceState::Stealing:
        return "Stealing";
    default:
        return "Unknown";
    }
}

bool VoiceStateManager::canBeStolen() const {
    return state != VoiceState::Inactive && state != VoiceState::Stealing;
}

bool VoiceStateManager::startSteal(const pm::UmpMessage& deferredMessage) {
    if (!canBeStolen()) {
        return false;
    }
    if (!pm::isNoteOn(deferredMessage)) {
        return false;
    }
    deferredNoteOn = pm::makeNoteOnMsgFromOnMsg(deferredMessage);
    deferredNoteOff = std::nullopt;

    // Stealing transitions the voice to a new note context; the held-note
    // stack (used only in mono/legato mode) is no longer meaningful.
    heldNoteStack.clear();
    deferredRetrigger = std::nullopt;
    pedalSustained = false;   // cancel any deferred NoteOff — steal takes precedence

    duckPerSample = calcDuckPerSample(sampleRate);
    duckMultiplier = 1.0;  // for note stealing

    state = VoiceState::Stealing;

    return true;
}

void  VoiceStateManager::copyNoteOnMessageToCurrentNoteOn(const pm::UmpMessage& message)
{
    LOG_VOICE("VoiceStateManager::copyNoteOnMessageToCurrentNoteOn");
    std::optional<pm::UmpMessage> noteOn = pm::makeNoteOnMsgFromOnMsg(message);
    if (Voice::loggingEnabled) {
        if (!noteOn) {
            LOG_VOICE("VoiceStateManager::copyNoteOnMessageToCurrentNoteOn copy failed");
        }
    }
    currentNoteOn = noteOn;
}
void  VoiceStateManager::clearCurrentNoteOn()
{
    LOG_VOICE("VoiceStateManager::clearCurrentNoteOn");
    currentNoteOn = std::nullopt;
}

