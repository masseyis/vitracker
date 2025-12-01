// Voice class - wraps Plaits voice with envelope and resampling
// PlaitsVST: MIT License

#pragma once

#include <cstdint>
#include <memory>
#include "plaits/dsp/voice.h"
#include "envelope.h"
#include "resampler.h"

class Voice {
public:
    static constexpr double kInternalSampleRate = 48000.0;
    static constexpr size_t kInternalBlockSize = 24;

    Voice();
    ~Voice();

    void Init(double hostSampleRate);

    void NoteOn(int note, float velocity, float attackMs, float decayMs);
    void NoteOff();

    // Process and mix into output buffers (adds to existing content)
    void Process(float* leftOutput, float* rightOutput, size_t size);

    // Setters for parameters
    // Maps UI engine index (0-15) to internal Plaits engine index
    void set_engine(int engine) { engine_ = mapEngineIndex(engine); }
    void set_harmonics(float harmonics) { harmonics_ = harmonics; }
    void set_timbre(float timbre) { timbre_ = timbre; }
    void set_morph(float morph) { morph_ = morph; }
    void set_decay(float decay) { lpgDecay_ = decay; }
    void set_lpg_colour(float colour) { lpgColour_ = colour; }

    // State queries
    bool active() const { return active_; }
    int note() const { return note_; }

private:
    // Maps UI engine selection (0-15) to actual Plaits engine index
    // Plaits has 24 engines, we expose the classic 16 (indices 8-23)
    static int mapEngineIndex(int uiEngine) {
        // Classic Plaits engines start at index 8 in the engine registry
        return uiEngine + 8;
    }

    plaits::Voice plaitsVoice_;
    Envelope envelope_;
    Resampler resamplerOut_;
    Resampler resamplerAux_;

    // Memory buffer for Plaits voice
    std::unique_ptr<uint8_t[]> voiceBuffer_;
    static constexpr size_t kVoiceBufferSize = 32768;

    // Voice state
    bool active_ = false;
    int note_ = -1;
    float velocity_ = 0.0f;
    bool triggerPending_ = false;

    // Parameters
    int engine_ = 0;
    float harmonics_ = 0.5f;
    float timbre_ = 0.5f;
    float morph_ = 0.5f;
    float lpgDecay_ = 0.5f;
    float lpgColour_ = 0.5f;

    // Internal buffers
    plaits::Voice::Frame internalBuffer_[kInternalBlockSize];
    int16_t outBuffer_[kInternalBlockSize];
    int16_t auxBuffer_[kInternalBlockSize];
    float resampledOut_[256];
    float resampledAux_[256];

    double hostSampleRate_ = 48000.0;
};
