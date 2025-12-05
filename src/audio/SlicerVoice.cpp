#include "SlicerVoice.h"
#include <cmath>
#include <algorithm>

namespace audio {

SlicerVoice::SlicerVoice() = default;

void SlicerVoice::setSampleRate(double sampleRate) {
    sampleRate_ = sampleRate;
}

void SlicerVoice::setSampleData(const float* leftData, const float* rightData, size_t numSamples, int originalSampleRate) {
    sampleDataL_ = leftData;
    sampleDataR_ = rightData;  // nullptr for mono samples
    sampleLength_ = numSamples;
    originalSampleRate_ = originalSampleRate;
    // Sample rate conversion: play at correct speed regardless of output sample rate
    playbackRate_ = static_cast<double>(originalSampleRate) / sampleRate_;
}

void SlicerVoice::trigger(int sliceIndex, float velocity, const model::SlicerParams& params) {
    if (!sampleDataL_ || sampleLength_ == 0) return;

    const auto& slices = params.slicePoints;

    // Calculate scale factor if using stretched buffer
    // Slice points are stored in original sample coordinates
    double scaleFactor = 1.0;
    if (params.sample.numSamples > 0 && sampleLength_ != params.sample.numSamples) {
        scaleFactor = static_cast<double>(sampleLength_) / static_cast<double>(params.sample.numSamples);
    }

    if (slices.empty()) {
        // No slices defined - play whole sample
        sliceStart_ = 0;
        sliceEnd_ = sampleLength_;
    } else if (sliceIndex >= 0 && sliceIndex < static_cast<int>(slices.size())) {
        // Scale slice positions to match the buffer being played
        sliceStart_ = static_cast<size_t>(slices[static_cast<size_t>(sliceIndex)] * scaleFactor);
        // End is either next slice or end of sample
        if (sliceIndex + 1 < static_cast<int>(slices.size())) {
            sliceEnd_ = static_cast<size_t>(slices[static_cast<size_t>(sliceIndex + 1)] * scaleFactor);
        } else {
            sliceEnd_ = sampleLength_;
        }
    } else {
        // Invalid slice index
        return;
    }

    currentSlice_ = sliceIndex;
    velocity_ = velocity;
    active_ = true;
    playPosition_ = static_cast<double>(sliceStart_);
}

void SlicerVoice::release() {
    // Slicer voices are one-shot - they play to the end of the slice
    // Don't trigger envelope release, let the slice play out naturally
    // The voice will deactivate when it reaches sliceEnd_
}

void SlicerVoice::render(float* leftOut, float* rightOut, int numSamples) {
    if (!active_ || !sampleDataL_) {
        for (int i = 0; i < numSamples; ++i) {
            leftOut[i] = 0.0f;
            rightOut[i] = 0.0f;
        }
        return;
    }

    for (int i = 0; i < numSamples; ++i) {
        // Check if we've reached end of slice
        if (playPosition_ >= static_cast<double>(sliceEnd_)) {
            active_ = false;
            leftOut[i] = 0.0f;
            rightOut[i] = 0.0f;
            continue;
        }

        // Linear interpolation for sample playback
        size_t pos0 = static_cast<size_t>(playPosition_);
        size_t pos1 = std::min(pos0 + 1, sampleLength_ - 1);
        double frac = playPosition_ - static_cast<double>(pos0);

        // Read from planar format (JUCE stores channels separately)
        float sampleL = static_cast<float>(sampleDataL_[pos0] * (1.0 - frac) + sampleDataL_[pos1] * frac);
        float sampleR;
        if (sampleDataR_) {
            sampleR = static_cast<float>(sampleDataR_[pos0] * (1.0 - frac) + sampleDataR_[pos1] * frac);
        } else {
            sampleR = sampleL;  // Mono: duplicate left to right
        }

        // Apply velocity
        leftOut[i] = sampleL * velocity_;
        rightOut[i] = sampleR * velocity_;

        // Advance position (sample rate conversion + speed adjustment)
        playPosition_ += playbackRate_ * static_cast<double>(speed_);
    }
}

} // namespace audio
