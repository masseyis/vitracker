#pragma once

#include "../model/SamplerParams.h"
#include "../dsp/adsr_envelope.h"
#include "../dsp/moog_filter.h"
#include <JuceHeader.h>

namespace audio {

class SamplerVoice {
public:
    SamplerVoice();

    void setSampleRate(double sampleRate);
    void setSampleData(const float* data, int numChannels, size_t numSamples, int originalSampleRate);

    void trigger(int midiNote, float velocity, const model::SamplerParams& params);
    void release();

    void render(float* leftOut, float* rightOut, int numSamples);

    bool isActive() const { return active_; }
    int getNote() const { return currentNote_; }

private:
    double sampleRate_ = 48000.0;
    const float* sampleData_ = nullptr;
    int sampleChannels_ = 0;
    size_t sampleLength_ = 0;
    int originalSampleRate_ = 44100;

    bool active_ = false;
    int currentNote_ = -1;
    int rootNote_ = 60;
    float velocity_ = 1.0f;

    double playPosition_ = 0.0;
    double playbackRate_ = 1.0;

    dsp::AdsrEnvelope ampEnvelope_;
    dsp::AdsrEnvelope filterEnvelope_;
    plaits::MoogFilter filterL_;
    plaits::MoogFilter filterR_;

    float baseCutoff_ = 1.0f;
    float filterEnvAmount_ = 0.0f;

    juce::Interpolators::Lagrange interpolatorL_;
    juce::Interpolators::Lagrange interpolatorR_;
};

} // namespace audio
