// Voice class - wraps Plaits voice with envelope and resampling
// PlaitsVST: MIT License

#include "voice.h"
#include <algorithm>
#include <cstring>
#include "stmlib/utils/buffer_allocator.h"

Voice::Voice()
{
    voiceBuffer_ = std::make_unique<uint8_t[]>(kVoiceBufferSize);
}

Voice::~Voice() = default;

void Voice::Init(double hostSampleRate)
{
    hostSampleRate_ = hostSampleRate;

    // Initialize Plaits voice with buffer allocator
    stmlib::BufferAllocator allocator;
    allocator.Init(voiceBuffer_.get(), kVoiceBufferSize);
    plaitsVoice_.Init(&allocator);

    envelope_.Init(kInternalSampleRate);
    resamplerOut_.Init(kInternalSampleRate, hostSampleRate);
    resamplerAux_.Init(kInternalSampleRate, hostSampleRate);

    active_ = false;
    note_ = -1;
    velocity_ = 0.0f;
    triggerPending_ = false;
}

void Voice::NoteOn(int note, float velocity, float attackMs, float decayMs)
{
    note_ = note;
    velocity_ = velocity;
    active_ = true;
    triggerPending_ = true;

    // Trigger envelope
    envelope_.Trigger(attackMs, decayMs);

    // Reset resamplers for clean start
    resamplerOut_.Reset();
    resamplerAux_.Reset();
}

void Voice::NoteOff()
{
    // For AD envelope, note off doesn't do anything special
    // Voice becomes inactive when envelope finishes
}

void Voice::Process(float* leftOutput, float* rightOutput, size_t size)
{
    if (!active_) {
        return;
    }

    size_t outputWritten = 0;

    while (outputWritten < size) {
        size_t outputRemaining = size - outputWritten;

        // Generate internal samples at 48kHz
        size_t internalSamples = std::min(kInternalBlockSize,
            static_cast<size_t>(outputRemaining * resamplerOut_.ratio()) + 4);

        // Set up Plaits patch and modulations
        plaits::Patch patch;
        patch.engine = engine_;
        patch.note = 48.0f + static_cast<float>(note_ - 60);  // Center around MIDI 60
        patch.harmonics = harmonics_;
        patch.timbre = timbre_;
        patch.morph = morph_;
        patch.frequency_modulation_amount = 0.0f;
        patch.timbre_modulation_amount = 0.0f;
        patch.morph_modulation_amount = 0.0f;
        patch.decay = lpgDecay_;
        patch.lpg_colour = lpgColour_;

        plaits::Modulations modulations;
        modulations.engine = 0.0f;
        modulations.note = 0.0f;
        modulations.frequency = 0.0f;
        modulations.harmonics = 0.0f;
        modulations.timbre = 0.0f;
        modulations.morph = 0.0f;
        modulations.trigger = triggerPending_ ? 1.0f : 0.0f;
        modulations.level = 1.0f;
        modulations.frequency_patched = false;
        modulations.timbre_patched = false;
        modulations.morph_patched = false;
        modulations.trigger_patched = true;  // Use trigger for note on
        modulations.level_patched = false;

        triggerPending_ = false;

        // Render Plaits voice
        plaitsVoice_.Render(patch, modulations, internalBuffer_, internalSamples);

        // Extract samples and apply envelope
        for (size_t i = 0; i < internalSamples; ++i) {
            float envValue = envelope_.Process();

            // Apply envelope and velocity
            float outSample = static_cast<float>(internalBuffer_[i].out) / 32768.0f;
            float auxSample = static_cast<float>(internalBuffer_[i].aux) / 32768.0f;

            outSample *= envValue * velocity_;
            auxSample *= envValue * velocity_;

            outBuffer_[i] = static_cast<int16_t>(outSample * 32767.0f);
            auxBuffer_[i] = static_cast<int16_t>(auxSample * 32767.0f);
        }

        // Check if envelope finished
        if (envelope_.done()) {
            active_ = false;
        }

        // Resample to host rate
        size_t maxOutput = std::min(outputRemaining, sizeof(resampledOut_) / sizeof(float));
        size_t producedOut = resamplerOut_.Process(outBuffer_, internalSamples,
                                                    resampledOut_, maxOutput);
        resamplerAux_.Process(auxBuffer_, internalSamples, resampledAux_, maxOutput);

        // Mix into output (main out to left, aux to right for stereo width)
        for (size_t i = 0; i < producedOut; ++i) {
            // Mix main and aux for stereo spread
            leftOutput[outputWritten + i] += resampledOut_[i] * 0.7f + resampledAux_[i] * 0.3f;
            rightOutput[outputWritten + i] += resampledOut_[i] * 0.3f + resampledAux_[i] * 0.7f;
        }

        outputWritten += producedOut;

        // Safety check to prevent infinite loop
        if (producedOut == 0 && internalSamples > 0) {
            break;
        }
    }
}
