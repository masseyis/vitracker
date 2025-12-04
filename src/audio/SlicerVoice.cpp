#include "SlicerVoice.h"
#include <cmath>
#include <algorithm>

namespace audio {

SlicerVoice::SlicerVoice() {
    ampEnvelope_.setSampleRate(48000.0f);
    filterEnvelope_.setSampleRate(48000.0f);
}

void SlicerVoice::setSampleRate(double sampleRate) {
    sampleRate_ = sampleRate;
    ampEnvelope_.setSampleRate(static_cast<float>(sampleRate));
    filterEnvelope_.setSampleRate(static_cast<float>(sampleRate));
    filterL_.Init(static_cast<float>(sampleRate));
    filterR_.Init(static_cast<float>(sampleRate));
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
    if (slices.empty()) {
        // No slices defined - play whole sample
        sliceStart_ = 0;
        sliceEnd_ = sampleLength_;
    } else if (sliceIndex >= 0 && sliceIndex < static_cast<int>(slices.size())) {
        sliceStart_ = slices[static_cast<size_t>(sliceIndex)];
        // End is either next slice or end of sample
        if (sliceIndex + 1 < static_cast<int>(slices.size())) {
            sliceEnd_ = slices[static_cast<size_t>(sliceIndex + 1)];
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

    // Set up envelopes
    ampEnvelope_.setAttack(params.ampEnvelope.attack);
    ampEnvelope_.setDecay(params.ampEnvelope.decay);
    ampEnvelope_.setSustain(params.ampEnvelope.sustain);
    ampEnvelope_.setRelease(params.ampEnvelope.release);
    ampEnvelope_.trigger();

    filterEnvelope_.setAttack(params.filterEnvelope.attack);
    filterEnvelope_.setDecay(params.filterEnvelope.decay);
    filterEnvelope_.setSustain(params.filterEnvelope.sustain);
    filterEnvelope_.setRelease(params.filterEnvelope.release);
    filterEnvelope_.trigger();

    // Store filter params
    baseCutoff_ = params.filter.cutoff;
    filterEnvAmount_ = params.filterEnvAmount;

    filterL_.SetResonance(params.filter.resonance);
    filterR_.SetResonance(params.filter.resonance);
}

void SlicerVoice::release() {
    ampEnvelope_.release();
    filterEnvelope_.release();
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

        // Process envelopes
        float ampEnv = ampEnvelope_.process();
        float filterEnv = filterEnvelope_.process();

        // Apply filter with envelope modulation
        float cutoff = baseCutoff_ + filterEnvAmount_ * filterEnv;
        cutoff = std::clamp(cutoff, 0.0f, 1.0f);
        // Convert normalized cutoff (0-1) to Hz (20-20000) using exponential mapping
        float cutoffHz = 20.0f * std::pow(1000.0f, cutoff);
        filterL_.SetCutoff(cutoffHz);
        filterR_.SetCutoff(cutoffHz);

        sampleL = filterL_.Process(sampleL);
        sampleR = filterR_.Process(sampleR);

        // Apply amplitude envelope and velocity
        leftOut[i] = sampleL * ampEnv * velocity_;
        rightOut[i] = sampleR * ampEnv * velocity_;

        // Advance position (sample rate conversion only, no pitch shift)
        playPosition_ += playbackRate_;

        // Check if envelope finished
        if (!ampEnvelope_.isActive()) {
            active_ = false;
        }
    }
}

} // namespace audio
