//
//  VoiceManager.mm
//
//  Created by Eric Kampman on 4/13/25.
//

#include "pm_midi.h"
#include "pm_addresses.h"
#include "VoiceManager.hpp"
#include <algorithm>
#include <cstdio>
#include "PlainOldSynthParams.h"

bool VoiceManager::loggingEnabled = false;

VoiceManager::VoiceManager(double sampleRate, uint32_t blockSize)
{
	moduleFactory = std::make_unique<ModuleFactory>(sampleRate, blockSize);
	initialize(sampleRate);
}

void VoiceManager::initialize(double sampleRate) {
//    LOG_VM("VoiceManager::initialize entrypoint");
    referenceVoice = std::make_unique<Voice>(sampleRate);
	voices.reserve(MaxVoices);
	for (size_t i = 0; i < MaxVoices; ++i) {
		auto voice = std::make_unique<Voice>(sampleRate);
		voice->id = static_cast<uint32_t>(voiceIdCounter++);
		// Optional: attach oscillator/envelope here via ModuleFactory
		voices.push_back(std::move(voice));
	}
	initializeVoices(&pitchConverter);
}

void VoiceManager::initializeVoices(PitchConverter *pitchConverter)
{
    referenceVoice->initializeModules(moduleFactory.get(), pitchConverter, &sharedStateRegistry);
    buildDefaultPatch(*referenceVoice);

	for (auto& voice : voices) {
		voice->initializeModules(moduleFactory.get(), pitchConverter, &sharedStateRegistry);
		buildDefaultPatch(*voice);
	}

    wireHostTransport();
}

void VoiceManager::wireHostTransport() {
    // The module pool is fully pre-created in initializeModules (node-add only connects
    // existing pooled modules), so wiring the pointer once here covers every module that
    // will ever exist. Per-block updates just mutate mTransport via setHostTransport().
    referenceVoice->setHostTransport(&mTransport);
    for (auto& voice : voices) {
        voice->setHostTransport(&mTransport);
    }
}

void VoiceManager::reset() {
    sustainPedalActive = false;
    pedalSustainedVoiceIDs.clear();
    allNotesOff();
    for (auto& voice : voices) {
        voice->reset();
    }
    sharedStateRegistry.reset();   // soft reset: clears state, keeps objects alive
}

// This occurs at a bad time in initialization. I want to defer this until
// later, with default patches.
#define HAS_DEFAULT_PATCH   1
// The voice graph. See plugin/PlainOldSynthParams.h for the full diagram; in short:
//
//   PitchConverter ─┬─► Slew ─┬─► TriSaw L (pitchInput)      ─┐
//                   │         └─► TriSaw R (pitchInput)      │  raw path
//                   └─► LPF (freqModInput = key tracking)    │
//   TriSaw L/R ─► Comparator L/R (pulse)                     │  pulse path
//   raw/pulse per channel ─► Level crossfade ─► Combine(Add) ─┘
//   ─► Pan L (−1) / Pan R (+1) ─► Combine(Add) ─► LPF ─► Combine(Multiply, VCA) ─► Final
//                                                        ▲
//   ADSR2 (amp env) ─────────────────────────────────────┘
//   ADSR1 (filter env) ─► LPF freqModInput2      LFO1 ─► osc modPitch      LFO2 ─► comparator threshCV
//
// Every module is instantiated from this voice's pool up front (pos_module_specs.cpp); this function
// only wires them. The modulation inputs are connected here and their depths stay at zero until the
// plugin pushes its parameters (see src/plugin.cpp's initPatchConstants / applyParam), so the graph
// is complete and sample-accurate from the first block.
void VoiceManager::buildDefaultPatch(Voice& voice)
{
    // The graph's addresses and the parameter table live in PlainOldSynthParams.h.
    using namespace plainoldsynth;

    Module* pitchConverter = voice.getModule(posPitchConverter1);
    Module* slew           = voice.getModule(posSlew1);
    Module* oscL           = voice.getModule(posTriSaw1);
    Module* oscR           = voice.getModule(posTriSaw2);
    Module* cmpL           = voice.getModule(posComparator1);
    Module* cmpR           = voice.getModule(posComparator2);
    Module* rawL           = voice.getModule(posLevelRaw1);
    Module* pulseL         = voice.getModule(posLevelPulse1);
    Module* rawR           = voice.getModule(posLevelRaw2);
    Module* pulseR         = voice.getModule(posLevelPulse2);
    Module* oscMixL        = voice.getModule(posCombineOsc1);
    Module* oscMixR        = voice.getModule(posCombineOsc2);
    Module* mix            = voice.getModule(posCombineMix);
    Module* vca            = voice.getModule(posCombineVca);
    Module* panL           = voice.getModule(posPan1);
    Module* panR           = voice.getModule(posPan2);
    Module* filterEnv      = voice.getModule(posAdsr1);
    Module* ampEnv         = voice.getModule(posAdsr2);
    Module* lfo1           = voice.getModule(posLfo1);
    Module* lfo2           = voice.getModule(posLfo2);
    Module* lpf            = voice.getModule(posLpf1);
    Module* finalModule    = voice.getModule(posFinal1);
    Module* ccConverter    = voice.getModule(posCcConverter1);
    Module* pitchBend      = voice.getModule(posPitchBend1);
    Module* timbre         = voice.getModule(posCcConverter2);
    Module* pressure       = voice.getModule(posChannelPressure1);
    Module* velocityAmp    = voice.getModule(posVelocityAmp);
    Module* velocitySource = voice.getModule(posVelocitySource);

    // A null here means this function and pos_module_specs.cpp have drifted apart — wire nothing
    // rather than dereference it, and say so once so it is not mistaken for a silent synth.
    const Module* const required[] = {
        pitchConverter, slew, oscL, oscR, cmpL, cmpR, rawL, pulseL, rawR, pulseR,
        oscMixL, oscMixR, mix, vca, panL, panR, filterEnv, ampEnv, lfo1, lfo2, lpf, finalModule,
        ccConverter, pitchBend, timbre, pressure, velocityAmp, velocitySource };
    for (const Module* m : required) {
        if (m == nullptr) {
            std::fprintf(stderr, "[PlainOldSynth] voice graph module missing from the registry; "
                                 "check pos_module_specs.cpp against PlainOldSynthParams.h\n");
            return;
        }
    }

    // Final is the graph's sink. Bumping its activeCount is what makes it receive MIDI at all —
    // Voice::handleMIDIMessage only feeds modules with a non-zero activeCount (and a MIDI proc).
    ++(finalModule->activeCount);
    voice.connectModules(vca,      finalModule, POS_ITEM(posFinal1, FinalOutputParamItemInput));

    voice.connectModules(lpf,      vca,   POS_ITEM(posCombineVca, CombineParamItemInput1));
    voice.connectModules(ampEnv,   vca,   POS_ITEM(posCombineVca, CombineParamItemInput2));  // VCA
    // Velocity → loudness: the VCA's third factor. At the Velocity dropdown's Off it is a constant 1.
    voice.connectModules(velocityAmp, vca, POS_ITEM(posCombineVca, CombineParamItemInput3));
    voice.connectModules(mix,      lpf,   POS_ITEM(posLpf1, LPFParamItemInput));

    voice.connectModules(panL,     mix,   POS_ITEM(posCombineMix, CombineParamItemInput1));
    voice.connectModules(panR,     mix,   POS_ITEM(posCombineMix, CombineParamItemInput2));

    voice.connectModules(oscMixL,  panL,  POS_ITEM(posPan1, PanParamItemInput));
    voice.connectModules(oscMixR,  panR,  POS_ITEM(posPan2, PanParamItemInput));

    voice.connectModules(rawL,     oscMixL, POS_ITEM(posCombineOsc1, CombineParamItemInput1));
    voice.connectModules(pulseL,   oscMixL, POS_ITEM(posCombineOsc1, CombineParamItemInput2));
    voice.connectModules(rawR,     oscMixR, POS_ITEM(posCombineOsc2, CombineParamItemInput1));
    voice.connectModules(pulseR,   oscMixR, POS_ITEM(posCombineOsc2, CombineParamItemInput2));

    voice.connectModules(oscL,     rawL,  POS_ITEM(posLevelRaw1, LevelParamItemInput));
    voice.connectModules(cmpL,     pulseL, POS_ITEM(posLevelPulse1, LevelParamItemInput));
    voice.connectModules(oscR,     rawR,  POS_ITEM(posLevelRaw2, LevelParamItemInput));
    voice.connectModules(cmpR,     pulseR, POS_ITEM(posLevelPulse2, LevelParamItemInput));

    voice.connectModules(oscL,     cmpL,  POS_ITEM(posComparator1, ComparatorParamItemInput));
    voice.connectModules(oscR,     cmpR,  POS_ITEM(posComparator2, ComparatorParamItemInput));

    // Pitch: MIDI → octaves → glide → both oscillators. PitchConverter is what carries a MIDI 2.0
    // 7.9-pitch attribute through to the oscillators (see its Pitch79ToOctaveOffset path).
    voice.connectModules(pitchConverter, slew, POS_ITEM(posSlew1, SlewParamItemInput));
    voice.connectModules(slew,     oscL,  POS_ITEM(posTriSaw1, TriSawParamItemPitchInput));
    voice.connectModules(slew,     oscR,  POS_ITEM(posTriSaw2, TriSawParamItemPitchInput));

    // Pitch bend (channel + MIDI 2.0 per-note) into the oscillators' SECOND pitch-mod input, after the
    // glide rather than through it: a bend must track the controller, not the portamento time. The
    // first pitch-mod input belongs to the modulation sections. Depth is pinned at 1 (octaves in,
    // octaves out) in plugin.cpp's initPatchConstants.
    voice.connectModules(pitchBend, oscL, POS_ITEM(posTriSaw1, TriSawParamItemModPitch2));
    voice.connectModules(pitchBend, oscR, POS_ITEM(posTriSaw2, TriSawParamItemModPitch2));

    // Filter key tracking: the raw pitch CV into the cutoff. This is kParamKeyTrack's own input and
    // stays hard-wired. The filter's SECOND mod input (the envelope sweep) is deliberately NOT wired
    // here — a modulation section owns it now, and plugin.cpp's applyModRouting points it at whichever
    // section selects the Cutoff destination.
    voice.connectModules(pitchConverter, lpf, POS_ITEM(posLpf1, LPFParamItemModFreqInput));

    // LFO 1 / LFO 2 / ADSR 1 used to be hard-wired to oscillator pitch, comparator threshold and the
    // filter cutoff, with the LFO Depth and Env Amount host params as their depths. All three of
    // those connections are gone: they are exactly what the modulation sections route, at levels the
    // sections own. The destination modules' own mod levels stay pinned at full scale in
    // initPatchConstants, so a section at level 1.0 reaches the range the old depth knob did.

    // The MIDI sources (CC #1, Timbre, Pressure, Velocity) have to keep receiving MIDI even while no section has
    // them selected — their activeCount is what gates MIDI dispatch (see Voice::handleMIDIMessage), and
    // they would otherwise sit at 0 and miss a mod-wheel move, or the pressure and timbre a note was
    // played with. Same trick the output stage uses.
    ++(ccConverter->activeCount);
    ++(timbre->activeCount);
    ++(pressure->activeCount);
    ++(velocitySource->activeCount);
}

void VoiceManager::handleMIDIMessage(const pm::UmpMessage& message) {
	// Note on (and maybe off) require special handling, for
	// Voice allocation.
    if (VoiceManager::loggingEnabled) {
        std::string descStr = pm::toString(message);
        LOG_VM("VoiceManager::handleMIDIMessage %s ", descStr.c_str());
    }
    
    if (pm::isChannelMessage(message)) {
        uint32_t channel = uint32_t(voiceManagerParamByEnum(MIDIParamItemChannelFilterValue));
        if (channel) {
            channel -= 1;
            if (channel != message.channel) {
                return;
            }
        }
    }
    
    uint32_t group = uint32_t(voiceManagerParamByEnum(MIDIParamItemGroupFilterValue));
    if (group) {
        group -= 1;
        if (message.group != group) {
            return;
        }
    }
    
	if (message.messageType == pm::kUmpTypeChannelVoice2) {
		switch (message.status) {
		case pm::kStatusNoteOn:
			noteOn(message);
            return;
        case pm::kStatusNoteOff:
            noteOff(message);
            return;
        case pm::kStatusControlChange:
            // Intercept CC 64 (sustain pedal) for voice-lifecycle management.
            // Fall through so CCConverter modules also receive it.
            if (message.ccIndex == 64) {
                bool newState = message.ccData >= (UINT32_MAX / 2u + 1u);
                handleSustainPedalChange(newState);
            }
            // Channel mode messages: handled here, not passed on to the modules.
            if (message.ccIndex == 123) { allNotesOffOnChannel(message.group, message.channel); return; }
            if (message.ccIndex == 120) { allSoundOffOnChannel(message.group, message.channel); return; }
            break;
		default:
			break;
		}
	}

	for (auto& voice : voices) {
		if (voice->wantsMIDIMessage(message, voice->id)) {
			voice->handleMIDIMessage(message);
		}
	}
}

bool VoiceManager::freeRunning() {
    AUValue freeRun = voiceManagerParamByEnum(MIDIParamItemFreeRun);
    AUValue voiceCount = voiceManagerParamByEnum(MIDIParamItemVoiceCount);
    return (freeRun >= 0.5f && voiceCount == 1);
}

void VoiceManager::noteOn(const pm::UmpMessage& msg) {
    LOG_VM("VoiceManager::noteOn entrypoint");

    // needed?
    if (freeRunning()) {
        Voice* voice = voices[0].get();
        // Optionally: voice->copyParamsFrom(*referenceVoice); if needed
        voice->handleMIDIMessage(msg);  // retune / retrigger as you see fit
        return;
    }

    // Legato mode: when in mono (1 voice), skip the steal path and route
    // the NoteOn directly to the voice that is still SOUNDING — Active (a note
    // is held) OR Deactivating (the previous note was released but its envelope
    // is still ringing out).  Routing a detached-but-still-ringing note here
    // (instead of letting it fall through to the steal path) is what keeps the
    // oscillator on the same voice so it can retune without a phase-reset click
    // (see Voice::handleMIDIMessage / Module::legatoContinuation). The ADSR/ADR/
    // IFS Env modules decide independently (via their own legato param) whether
    // to retrigger the envelope or let it continue. A fully Inactive voice is
    // silent, so a fresh note there resets phase harmlessly — fall through to
    // normal allocation for that case.
    uint32_t legatoMode = uint32_t(voiceManagerParamByEnum(MIDIParamItemLegato));
    if (legatoMode > 0) {
        for (auto& v : voices) {
            if (v && (v->voiceState() == VoiceState::Active
                   || v->voiceState() == VoiceState::Deactivating)) {
                v->handleMIDIMessage(msg);
                return;
            }
        }
        // No sounding voice: fall through to normal allocation below.
    }

    Voice* voice = findVoiceForMessage(msg);
    bool stolen = false;
    if (!voice) {
        voice = findFreeVoice();
    }
	if (!voice) {
		voice = stealVoice(msg); // implement envelope-aware stealing
		if (!voice) return;   // out of voices
        stolen = true;
	}
    if (stolen) {
        // If this voice was pedal-sustained, cancel the deferred NoteOff —
        // startSteal() also clears pedalSustained on the VoiceStateManager.
        auto it = std::find(pedalSustainedVoiceIDs.begin(), pedalSustainedVoiceIDs.end(), voice->id);
        if (it != pedalSustainedVoiceIDs.end()) {
            pedalSustainedVoiceIDs.erase(it);
        }
        voice->startSteal(msg);
    }
    voice->copyParamsFrom(*referenceVoice);
	voice->handleMIDIMessage(msg);
}

void VoiceManager::noteOff(const pm::UmpMessage& msg) {
    LOG_VM("VoiceManager::noteOff entrypoint");

    // FreeRun and legato modes bypass sustain pedal logic:
    // FreeRun ignores note lifecycle entirely; legato has its own held-note stack.
    bool bypass = freeRunning() ||
                  (uint32_t(voiceManagerParamByEnum(MIDIParamItemLegato)) > 0);

    for (auto& voice : voices) {
        if (!voice->wantsMIDIMessage(msg, voice->id)) continue;

        if (!bypass && sustainPedalActive) {
            // Pedal held — defer the NoteOff. Only add to list if not already there
            // (guards against duplicate entries if multiple buffers send the same NoteOff).
            if (!voice->voiceStateManager.isPedalSustained()) {
                voice->voiceStateManager.setPedalSustained(true);
                pedalSustainedVoiceIDs.push_back(voice->id);
            }
        } else {
            voice->handleMIDIMessage(msg);
        }
    }
}

void VoiceManager::handleSustainPedalChange(bool newState) {
    if (newState == sustainPedalActive) return;
    sustainPedalActive = newState;

    if (!newState && !pedalSustainedVoiceIDs.empty()) {
        // Pedal released — dispatch deferred NoteOffs to all sustained voices.
        for (uint32_t voiceID : pedalSustainedVoiceIDs) {
            for (auto& voice : voices) {
                if (voice->id == voiceID && voice->voiceStateManager.isPedalSustained()) {
                    voice->voiceStateManager.setPedalSustained(false);
                    // Built from the voice's own note-on so it carries that note's channel and group,
                    // which note-off matching requires.
                    const auto& current = voice->voiceStateManager.currentNote();
                    if (auto noteOff = current ? pm::makeNoteOffMsgFromOnMsg(*current) : std::nullopt) {
                        voice->handleMIDIMessage(*noteOff);
                    }
                }
            }
        }
        pedalSustainedVoiceIDs.clear();
    }
}

// Release every note a voice holds: the mono stack's silent notes first, then the sounding one, so
// releasing the sounding note never retriggers a stacked note. Note-offs are built from the
// note-ons themselves because note-off matching includes channel and group.
void VoiceManager::releaseAllNotes(Voice& voice) {
    std::vector<pm::UmpMessage> notes = voice.voiceStateManager.heldNotes();   // copy: mutated below
    const std::optional<pm::UmpMessage> current = voice.voiceStateManager.currentNote();
    if (current) {
        notes.erase(std::remove_if(notes.begin(), notes.end(),
                        [&current](const pm::UmpMessage& m) { return pm::isSameNote(m, *current); }),
                    notes.end());
        notes.push_back(*current);
    }
    for (const pm::UmpMessage& on : notes) {
        if (auto noteOff = pm::makeNoteOffMsgFromOnMsg(on)) voice.handleMIDIMessage(*noteOff);
    }
}

int VoiceManager::keyForNoteId(int32_t noteId) {
    if (noteId < 0) return -1;
    for (auto& voice : voices) {
        const auto& current = voice->voiceStateManager.currentNote();
        if (current && current->noteId == noteId) return current->note;
    }
    return -1;
}

// Take a voice out of the sustain pedal's deferred list, so pedal-up does not send it a note-off for
// a note it no longer has.
void VoiceManager::dropPedalSustain(Voice& voice) {
    if (!voice.voiceStateManager.isPedalSustained()) return;
    voice.voiceStateManager.setPedalSustained(false);
    pedalSustainedVoiceIDs.erase(std::remove(pedalSustainedVoiceIDs.begin(), pedalSustainedVoiceIDs.end(), voice.id),
                                 pedalSustainedVoiceIDs.end());
}

// releaseAllNotes, limited to the notes played on one group/channel. A mono voice can hold notes
// from several channels; only this channel's leave the stack, and the sounding note is released
// last so it does not retrigger one of them.
void VoiceManager::releaseNotesOnChannel(Voice& voice, uint8_t group, uint8_t channel) {
    auto onChannel = [group, channel](const pm::UmpMessage& m) { return m.group == group && m.channel == channel; };
    std::vector<pm::UmpMessage> notes;
    const std::optional<pm::UmpMessage> current = voice.voiceStateManager.currentNote();
    for (const pm::UmpMessage& held : voice.voiceStateManager.heldNotes())
        if (onChannel(held) && !(current && pm::isSameNote(held, *current))) notes.push_back(held);
    if (current && onChannel(*current)) {
        dropPedalSustain(voice);   // the pedal was holding this very note
        notes.push_back(*current);
    }
    for (const pm::UmpMessage& on : notes) {
        if (auto noteOff = pm::makeNoteOffMsgFromOnMsg(on)) voice.handleMIDIMessage(*noteOff);
    }
}

// CC 123. Releases the channel's notes, the sustain pedal's included (hosts and senders use it to
// clear everything). The notes go into their release, as after any note-off.
void VoiceManager::allNotesOffOnChannel(uint8_t group, uint8_t channel) {
    for (auto& voice : voices) {
        if (voice->voiceStateManager.holdsNoteOnChannel(group, channel))
            releaseNotesOnChannel(*voice, group, channel);
    }
}

// CC 120. Silences the channel's voices at once, release tails included: the voice goes Inactive
// (an Inactive voice outputs nothing) and its envelopes, oscillators and comparators are reset so the
// next note starts clean. A full Voice::reset() would also clear the per-channel bend, pressure and CC
// memory, which belongs to every channel, not just this one. The global effect's tail (delay/chorus)
// is shared by all channels and keeps going.
void VoiceManager::allSoundOffOnChannel(uint8_t group, uint8_t channel) {
    using namespace plainoldsynth;
    static constexpr AUParameterAddress kSoundModules[] = {
        posAdsr1, posAdsr2, posTriSaw1, posTriSaw2, posComparator1, posComparator2 };
    for (auto& voice : voices) {
        if (!voice->voiceStateManager.holdsNoteOnChannel(group, channel)) continue;
        dropPedalSustain(*voice);
        voice->voiceStateManager.silence();
        for (const AUParameterAddress address : kSoundModules)
            if (Module* module = voice->getModule(address)) module->reset();
    }
}

void VoiceManager::allNotesOff() {
    // Clear pedal state so no deferred NoteOffs linger after all-notes-off.
    sustainPedalActive = false;
    pedalSustainedVoiceIDs.clear();

	for (auto& voice : voices) {
        voice->voiceStateManager.setPedalSustained(false);
		if (voice->isActive()) {
            releaseAllNotes(*voice);
		}
	}
}

void VoiceManager::panic() {
    // Every voice, whatever the Group/Channel filter says: handleMIDIMessage()'s filter is bypassed
    // by talking to the voices directly.
    sustainPedalActive = false;
    pedalSustainedVoiceIDs.clear();
    for (auto& voice : voices) {
        voice->voiceStateManager.setPedalSustained(false);
        releaseAllNotes(*voice);
    }
}

void VoiceManager::drainCommandQueue()
{
    VoiceCommand cmd;
    while (commandQueue.pop(cmd)) {
        switch (cmd.type) {

        case VoiceCommandType::Connect: {
            for (auto& voice : voices) {
                Module *up = voice->getModule(cmd.upstreamID);
                Module *dn = voice->getModule(cmd.downstreamID);
                voice->connectModules(up, dn, cmd.inputAddress);
            }
            break;
        }

        case VoiceCommandType::ClearInput: {
            uint64_t baseAddr = MODULE_BASE_ADDRESS(cmd.inputAddress);
            uint8_t inputID   = static_cast<uint8_t>(MODULE_ITEM_FROM_ADDRESS(cmd.inputAddress));
            for (auto& voice : voices) {
                Module *mod = voice->getModule(baseAddr);
                if (mod) {
                    // Decrement upstream's activeCount before clearing the slot,
                    // completing the compound op that connectModules started.
                    Module* upstream = mod->inputs[inputID].upstream;
                    if (upstream && upstream->activeCount > 0) {
                        --(upstream->activeCount);
                    }
                    mod->removeInput(inputID);
                }
            }
            break;
        }
        }
    }
}

void VoiceManager::process(uint32_t frameCount, float *left, float *right)
{
    // Apply any pending connection changes before processing voices.
    drainCommandQueue();

    // Advance any cross-voice per-chunk shared state exactly once, before the voices read it (all
    // MIDI events for this block were already applied upstream, so activeCount() here reflects this
    // chunk's note-ons). The shared LFO uses this to advance its one shared phase / reset on idle.
    sharedStateRegistry.beginBlock(activeCount(), frameCount);

    // necessary?
    std::fill_n(left, frameCount, 0.f);
    std::fill_n(right, frameCount, 0.f);

    
    for (size_t i = 0; i < voices.size(); ++i) {
        auto& v = voices[i];
        if (!v) continue;
        
        bool active = false;
        
        if (freeRunning() && i == 0) {  // only voice[0] potentially is free-runnning
            active = true;
        } else {
            if (v->isActive()) { active = true; }
        }

        if (active) {
            v->process(frameCount, left, right);
        }
	}
}

//uint32_t VoiceManager::activeCount() {
//    uint32_t activeCount = 0;
//    for (auto& voice : voices) {
//        if (voice->isActive()) {
//            activeCount++;
//        }
//    }
//    return activeCount;
//}
uint32_t VoiceManager::activeCount()
{
    uint32_t activeCount = 0;
    bool freeRun = freeRunning();

    for (size_t i = 0; i < voices.size(); ++i) {
        const auto& v = voices[i];
        if (!v) {
            continue;
        }

        if (v->isActive()) {
            ++activeCount;
        } else if (freeRun && i == 0) {
            // Voice 0 is considered logically active in free-run mode
            ++activeCount;
        }
    }

    return activeCount;
}


Voice* VoiceManager::findFreeVoice() {
    uint32_t max = voiceManagerParamByEnum(MIDIParamItemVoiceCount);
    if (activeCount() < max) {
        for (auto& voice : voices) {
            if (!voice->isActive())
                return voice.get();
        }
    }
    return nullptr;
}

// TODO: implement more advanced stealing (e.g. quietest, oldest)
Voice* VoiceManager::stealVoice(const pm::UmpMessage& msg) {
    Voice *stolen = nullptr;
    
//    for (auto& voice : voices) {
//        if (voice->voiceStateManager.canBeStolen()) {
//            stolen = voice.get(); // safe to reuse
//            LOG_VM("VoiceManager::stealVoice canBeStolen id(%d)", voice->id);
//            break;
//        }
//    }
//    if (!stolen) {
        uint8_t algorithm = uint8_t(voiceManagerParamByEnum(MIDIParamItemVoiceStealAlgorithm));
        switch (algorithm) {
        case VoiceStealAlgorithmOldest:
            stolen = oldestVoice();
            break;
        case VoiceStealAlgorithmLowest:
            stolen = lowestVoice();
            break;
        case VoiceStealAlgorithmHighest:
            stolen = highestVoice();
            break;
        case VoiceStealAlgorithmClosest:
            stolen = closest(msg);
            break;
        case VoiceStealAlgorithmFurthest:
            stolen = furthest(msg);
            break;
        }
        if (stolen) {
            LOG_VM("VoiceManager::stealVoice voice id(%d)", stolen->id);
        }
//    }
    return stolen;
}


Voice* VoiceManager::oldestVoice() {
    Voice* oldestVoice = nullptr;
    TimePoint oldestTime = Clock::now();

    for (auto& voice : voices) {
        if (!voice->voiceStateManager.canBeStolen()) continue;

        if (voice->lastUsed < oldestTime) {
            oldestTime = voice->lastUsed;
            oldestVoice = voice.get();
        }
    }
    return oldestVoice;
}
Voice* VoiceManager::lowestVoice() {
    Voice* ret = nullptr;

    for (auto& voice : voices) {
        if (!voice->voiceStateManager.canBeStolen()) continue;

        if (!ret) {
            ret = voice.get();
        } else {
            uint8_t currentNote = voice->currentNoteNumber;
            if (currentNote) {
                if (voice->currentNoteNumber < ret->currentNoteNumber) {
                    ret = voice.get();
                }
            }
        }
    }
    return ret;
}
Voice* VoiceManager::highestVoice() {
    Voice* ret = nullptr;

    for (auto& voice : voices) {
        if (!voice->voiceStateManager.canBeStolen()) continue;

        if (!ret) {
            ret = voice.get();
        } else {
            uint8_t currentNote = voice->currentNoteNumber;
            if (currentNote) {
                if (voice->currentNoteNumber > ret->currentNoteNumber) {
                    ret = voice.get();
                }
            }
        }
    }
    return ret;
}

Voice* VoiceManager::closest(const pm::UmpMessage& msg) {
    Voice* ret = nullptr;

    if (!pm::isNoteOn(msg)) {
        return ret;
    }
    uint8_t msgNote = msg.note;
    
    for (auto& voice : voices) {
        if (!voice->voiceStateManager.canBeStolen()) continue;

        if (!ret) {
            ret = voice.get();
        } else {
            uint8_t currentNote = voice->currentNoteNumber;
            if (currentNote) {
                uint8_t newDist = abs(voice->currentNoteNumber - msgNote);
                uint8_t oldDist = abs(ret->currentNoteNumber - msgNote);
                
                if (newDist < oldDist) {
                    ret = voice.get();
                }
            }
        }
    }
    return ret;
}

Voice* VoiceManager::furthest(const pm::UmpMessage& msg) {
    Voice* ret = nullptr;

    if (!pm::isNoteOn(msg)) {
        return ret;
    }
    uint8_t msgNote = msg.note;
    
    for (auto& voice : voices) {
        if (!voice->voiceStateManager.canBeStolen()) continue;

        if (!ret) {
            ret = voice.get();
        } else {
            uint8_t currentNote = voice->currentNoteNumber;
            if (currentNote) {
                uint8_t newDist = abs(voice->currentNoteNumber - msgNote);
                uint8_t oldDist = abs(ret->currentNoteNumber - msgNote);
                
                if (newDist > oldDist) {
                    ret = voice.get();
                }
            }
        }
    }
    return ret;
}


// called from VoiceManagerBridge.
Module *VoiceManager::getModule(AUParameterAddress paramAddress)
{
    // referenceVoice mirrors every voice's module set (same pool) — use it as the
    // canonical lookup for "does this instance exist in the pool?" (main thread).
    return referenceVoice->getModule(paramAddress);
}

void VoiceManager::connectModules(uint64_t upstreamID, uint64_t downstreamID, AUParameterAddress inputAddress)
{
    // referenceVoice is only ever accessed from the main thread, so it is safe
    // to mutate its Module::inputs directly here.
    Module *upstreamModule = referenceVoice->getModule(upstreamID);
    Module *downstreamModule = referenceVoice->getModule(downstreamID);
    referenceVoice->connectModules(upstreamModule, downstreamModule, inputAddress);

    // Polyphonic voices are processed on the render thread, so we must not
    // mutate their Module::inputs here.  Post a command to the SPSC queue;
    // the render thread will drain it at the top of process().
    VoiceCommand cmd;
    cmd.type         = VoiceCommandType::Connect;
    cmd.upstreamID   = upstreamID;
    cmd.downstreamID = downstreamID;
    cmd.inputAddress = inputAddress;
    commandQueue.push(cmd);
}

Voice* VoiceManager::findVoiceForMessage(const pm::UmpMessage& msg) {
    
    if (pm::isNoteOn(msg)) {
        uint32_t max = voiceManagerParamByEnum(MIDIParamItemVoiceCount);
        if (activeCount() >= max) {
            return nullptr;
        }
    }
	for (auto& voice : voices) {
        if (voice->wantsMIDIMessage(msg, 9999)) {
            return voice.get();
        }
	}
	return nullptr;
}

// Parameters

AUValue VoiceManager::voiceManagerParamAtIndex(size_t index)
{
    return params[index];
//        return params_[index];
}

AUValue VoiceManager::voiceManagerParamByEnum(NSUInteger paramEnum) {
    size_t index = PARAM_ENUM_TO_INDEX(paramEnum);
    return voiceManagerParamAtIndex(index);
}

void VoiceManager::setVoiceManagerParameter(AUParameterAddress address, AUValue value) {
//    if (!isVoiceManagerModule(address)) {
//        return;
//    }
    uint8_t item = MODULE_ITEM_FROM_ADDRESS(address);
    params[item - 1] = value;
}

/*
    The below are params distributed to modules
 */

void VoiceManager::setParameter(AUParameterAddress address, AUValue value)
{
    referenceVoice->setParameter(address, value);
    for (auto& vp : voices) {
        vp->setParameter(address, value);
    }

}
AUValue VoiceManager::getParameter(AUParameterAddress address) {
    return referenceVoice->getParameter(address);
}

void VoiceManager::setModSource(AUParameterAddress destInput, AUParameterAddress source)
{
    clearInput(destInput);                 // disconnect current source (safe if none)
    if (source != 0)                        // 0 = None
        connectModules(source, MODULE_BASE_ADDRESS(destInput), destInput);
}

void VoiceManager::clearInput(AUParameterAddress address)
{
    uint64_t baseAddr = MODULE_BASE_ADDRESS(address);
    uint8_t inputID = static_cast<uint8_t>(MODULE_ITEM_FROM_ADDRESS(address));

    // referenceVoice is main-thread-only; mutate directly.
    Module *module = referenceVoice->getModule(baseAddr);
    if (module) {
        // Decrement the upstream module's activeCount before the slot is
        // cleared, mirroring what Voice::connectModules did on connect.
        // (Module::removeInput has this commented out as "Done in Voice".)
        Module* upstream = module->inputs[inputID].upstream;
        if (upstream && upstream->activeCount > 0) {
            --(upstream->activeCount);
        }
        module->removeInput(inputID);
    }

    // Zero the AUParameter value so the state is persisted correctly.
    setParameter(address, 0);

    // Defer the per-voice removeInput to the render thread via the command queue.
    VoiceCommand cmd;
    cmd.type         = VoiceCommandType::ClearInput;
    cmd.upstreamID   = 0;
    cmd.downstreamID = 0;
    cmd.inputAddress = address;
    commandQueue.push(cmd);
}

void VoiceManager::updateSampleRate(double newSampleRate) {
    moduleFactory->updateSampleRate(newSampleRate); // if needed

    referenceVoice->updateSampleRate(newSampleRate);
    for (auto& voice : voices) {
        voice->updateSampleRate(newSampleRate);
    }
}


//bool VoiceManager::isVoiceManagerModule(AUParameterAddress address) {
//    uint8_t moduleKind = MODULE_KIND_FROM_ADDRESS(address);
//    switch (moduleKind) {
//    case ModuleParamKindMIDI:
//        return true;
//    default:
//        return false;
//    }
//}
//
