// VoiceAllocator - manages polyphonic voice allocation
// PlaitsVST: MIT License

#pragma once

#include <array>
#include <cstdint>
#include "voice.h"

class VoiceAllocator {
public:
    static constexpr size_t kMaxVoices = 16;

    VoiceAllocator() = default;
    ~VoiceAllocator() = default;

    void Init(double hostSampleRate, int polyphony);

    void NoteOn(int note, float velocity, float attackMs, float decayMs);
    void NoteOff(int note);
    void AllNotesOff();

    // Process all voices and output to stereo buffers
    void Process(float* leftOutput, float* rightOutput, size_t size);

    // Shared parameters for all voices
    void set_engine(int engine) { engine_ = engine; }
    void set_harmonics(float harmonics) { harmonics_ = harmonics; }
    void set_timbre(float timbre) { timbre_ = timbre; }
    void set_morph(float morph) { morph_ = morph; }
    void set_decay(float decay) { lpgDecay_ = decay; }
    void set_lpg_colour(float colour) { lpgColour_ = colour; }

    // Polyphony control
    void setPolyphony(int polyphony);
    int polyphony() const { return polyphony_; }

    // State queries
    int activeVoiceCount() const;

private:
    Voice* findFreeVoice();
    Voice* findVoiceForNote(int note);
    Voice* stealVoice();

    std::array<Voice, kMaxVoices> voices_;
    std::array<uint32_t, kMaxVoices> voiceAge_;
    uint32_t noteCounter_ = 0;

    int polyphony_ = 8;
    double hostSampleRate_ = 48000.0;

    // Shared parameters
    int engine_ = 0;
    float harmonics_ = 0.5f;
    float timbre_ = 0.5f;
    float morph_ = 0.5f;
    float lpgDecay_ = 0.5f;
    float lpgColour_ = 0.5f;
};
