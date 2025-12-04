#pragma once

#include "../model/SlicerParams.h"
#include "../dsp/adsr_envelope.h"
#include "../dsp/moog_filter.h"

namespace audio {

class SlicerVoice {
public:
    SlicerVoice();

    void setSampleRate(double sampleRate);
    void setSampleData(const float* data, int numChannels, size_t numSamples, int originalSampleRate);

    // Trigger a specific slice (sliceIndex maps from MIDI note)
    void trigger(int sliceIndex, float velocity, const model::SlicerParams& params);
    void release();

    void render(float* leftOut, float* rightOut, int numSamples);

    bool isActive() const { return active_; }
    int getSliceIndex() const { return currentSlice_; }

private:
    double sampleRate_ = 48000.0;
    const float* sampleData_ = nullptr;
    int sampleChannels_ = 0;
    size_t sampleLength_ = 0;
    int originalSampleRate_ = 44100;

    bool active_ = false;
    int currentSlice_ = -1;
    float velocity_ = 1.0f;

    // Slice boundaries
    size_t sliceStart_ = 0;
    size_t sliceEnd_ = 0;
    double playPosition_ = 0.0;  // Fractional position for interpolation

    // Playback rate for sample rate conversion only (no pitch shift)
    double playbackRate_ = 1.0;

    dsp::AdsrEnvelope ampEnvelope_;
    dsp::AdsrEnvelope filterEnvelope_;
    plaits::MoogFilter filterL_;
    plaits::MoogFilter filterR_;

    float baseCutoff_ = 1.0f;
    float filterEnvAmount_ = 0.0f;
};

} // namespace audio
