// VoiceAllocator - manages polyphonic voice allocation
// PlaitsVST: MIT License

#include "voice_allocator.h"
#include <algorithm>
#include <cstring>

void VoiceAllocator::Init(double hostSampleRate, int polyphony)
{
    hostSampleRate_ = hostSampleRate;
    polyphony_ = std::clamp(polyphony, 1, static_cast<int>(kMaxVoices));
    noteCounter_ = 0;

    for (size_t i = 0; i < kMaxVoices; ++i) {
        voices_[i].Init(hostSampleRate);
        voiceAge_[i] = 0;
    }
}

void VoiceAllocator::setPolyphony(int polyphony)
{
    polyphony_ = std::clamp(polyphony, 1, static_cast<int>(kMaxVoices));
}

void VoiceAllocator::NoteOn(int note, float velocity, float attackMs, float decayMs)
{
    // First check if this note is already playing - retrigger it
    Voice* voice = findVoiceForNote(note);

    if (!voice) {
        // Try to find a free voice
        voice = findFreeVoice();
    }

    if (!voice) {
        // No free voice - steal the oldest one
        voice = stealVoice();
    }

    if (voice) {
        voice->NoteOn(note, velocity, attackMs, decayMs);
        // Record when this voice was triggered
        size_t idx = voice - &voices_[0];
        voiceAge_[idx] = ++noteCounter_;
    }
}

void VoiceAllocator::NoteOff(int note)
{
    Voice* voice = findVoiceForNote(note);
    if (voice) {
        voice->NoteOff();
    }
}

void VoiceAllocator::AllNotesOff()
{
    for (size_t i = 0; i < kMaxVoices; ++i) {
        voices_[i].NoteOff();
    }
}

void VoiceAllocator::Process(float* leftOutput, float* rightOutput, size_t size)
{
    // Clear output buffers
    std::memset(leftOutput, 0, size * sizeof(float));
    std::memset(rightOutput, 0, size * sizeof(float));

    // Update shared parameters and process each active voice
    for (size_t i = 0; i < static_cast<size_t>(polyphony_); ++i) {
        voices_[i].set_engine(engine_);
        voices_[i].set_harmonics(harmonics_);
        voices_[i].set_timbre(timbre_);
        voices_[i].set_morph(morph_);
        voices_[i].set_decay(lpgDecay_);
        voices_[i].set_lpg_colour(lpgColour_);

        if (voices_[i].active()) {
            voices_[i].Process(leftOutput, rightOutput, size);
        }
    }
}

int VoiceAllocator::activeVoiceCount() const
{
    int count = 0;
    for (size_t i = 0; i < static_cast<size_t>(polyphony_); ++i) {
        if (voices_[i].active()) {
            ++count;
        }
    }
    return count;
}

Voice* VoiceAllocator::findFreeVoice()
{
    for (size_t i = 0; i < static_cast<size_t>(polyphony_); ++i) {
        if (!voices_[i].active()) {
            return &voices_[i];
        }
    }
    return nullptr;
}

Voice* VoiceAllocator::findVoiceForNote(int note)
{
    for (size_t i = 0; i < static_cast<size_t>(polyphony_); ++i) {
        if (voices_[i].active() && voices_[i].note() == note) {
            return &voices_[i];
        }
    }
    return nullptr;
}

Voice* VoiceAllocator::stealVoice()
{
    // Find the oldest voice (lowest age counter)
    size_t oldest = 0;
    uint32_t oldestAge = voiceAge_[0];

    for (size_t i = 1; i < static_cast<size_t>(polyphony_); ++i) {
        if (voiceAge_[i] < oldestAge) {
            oldest = i;
            oldestAge = voiceAge_[i];
        }
    }

    return &voices_[oldest];
}
