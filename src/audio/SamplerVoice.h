#pragma once

#include "../model/SamplerParams.h"
#include "../model/Step.h"
#include "../dsp/adsr_envelope.h"
#include "../dsp/moog_filter.h"
#include "TrackerFX.h"
#include <JuceHeader.h>

namespace audio {

class SamplerVoice {
public:
    SamplerVoice();

    void setSampleRate(double sampleRate);
    void setSampleData(const float* leftData, const float* rightData, size_t numSamples, int originalSampleRate);
    void setTempo(float bpm);

    void trigger(int midiNote, float velocity, const model::SamplerParams& params);
    void trigger(int midiNote, float velocity, const model::SamplerParams& params, const model::Step& step);
    void release();

    void render(float* leftOut, float* rightOut, int numSamples);

    bool isActive() const { return active_; }
    int getNote() const { return currentNote_; }
    size_t getPlayPosition() const { return static_cast<size_t>(playPosition_); }

private:
    double sampleRate_ = 48000.0;
    const float* sampleDataL_ = nullptr;
    const float* sampleDataR_ = nullptr;  // nullptr for mono
    size_t sampleLength_ = 0;
    int originalSampleRate_ = 44100;

    bool active_ = false;
    int currentNote_ = -1;
    int rootNote_ = 60;
    float velocity_ = 1.0f;

    double playPosition_ = 0.0;
    double playbackRate_ = 1.0;
    double basePlaybackRate_ = 1.0;  // Base rate without arpeggio

    dsp::AdsrEnvelope ampEnvelope_;
    dsp::AdsrEnvelope filterEnvelope_;
    plaits::MoogFilter filterL_;
    plaits::MoogFilter filterR_;

    float baseCutoff_ = 1.0f;
    float filterEnvAmount_ = 0.0f;

    juce::Interpolators::Lagrange interpolatorL_;
    juce::Interpolators::Lagrange interpolatorR_;

    // Tracker FX
    TrackerFX trackerFX_;
};

} // namespace audio
