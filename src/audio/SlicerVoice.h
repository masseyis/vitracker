#pragma once

#include "../model/SlicerParams.h"
#include "../model/Step.h"
#include "TrackerFX.h"

namespace audio {

class SlicerVoice {
public:
    SlicerVoice();

    void setSampleRate(double sampleRate);
    void setSampleData(const float* leftData, const float* rightData, size_t numSamples, int originalSampleRate);
    void setPlaybackSpeed(float speed) { speed_ = speed; }
    void setTempo(float bpm);

    // Trigger a specific slice (sliceIndex maps from MIDI note)
    void trigger(int sliceIndex, float velocity, const model::SlicerParams& params);
    void trigger(int sliceIndex, float velocity, const model::SlicerParams& params, const model::Step& step);
    void release();

    void render(float* leftOut, float* rightOut, int numSamples);

    bool isActive() const { return active_; }
    int getSliceIndex() const { return currentSlice_; }
    size_t getPlayPosition() const { return static_cast<size_t>(playPosition_); }

private:
    double sampleRate_ = 48000.0;
    const float* sampleDataL_ = nullptr;
    const float* sampleDataR_ = nullptr;  // nullptr for mono
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
    double basePlaybackRate_ = 1.0;  // Base rate without arpeggio
    float speed_ = 1.0f;  // Speed multiplier from time-stretch params

    // Tracker FX
    TrackerFX trackerFX_;
};

} // namespace audio
