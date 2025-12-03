#include "SamplerVoice.h"
#include <cmath>

namespace audio {

SamplerVoice::SamplerVoice() {
    ampEnvelope_.setSampleRate(48000.0f);
    filterEnvelope_.setSampleRate(48000.0f);
}

void SamplerVoice::setSampleRate(double sampleRate) {
    sampleRate_ = sampleRate;
    ampEnvelope_.setSampleRate(static_cast<float>(sampleRate));
    filterEnvelope_.setSampleRate(static_cast<float>(sampleRate));
    filterL_.Init(static_cast<float>(sampleRate));
    filterR_.Init(static_cast<float>(sampleRate));
}

void SamplerVoice::setSampleData(const float* data, int numChannels, size_t numSamples, int originalSampleRate) {
    sampleData_ = data;
    sampleChannels_ = numChannels;
    sampleLength_ = numSamples;
    originalSampleRate_ = originalSampleRate;
}

void SamplerVoice::trigger(int midiNote, float velocity, const model::SamplerParams& params) {
    if (!sampleData_ || sampleLength_ == 0) return;

    currentNote_ = midiNote;
    rootNote_ = params.rootNote;
    velocity_ = velocity;
    active_ = true;
    playPosition_ = 0.0;

    // Calculate playback rate for pitch shifting
    double semitones = midiNote - rootNote_;
    playbackRate_ = std::pow(2.0, semitones / 12.0);

    // Adjust for sample rate difference
    playbackRate_ *= static_cast<double>(originalSampleRate_) / sampleRate_;

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

    interpolatorL_.reset();
    interpolatorR_.reset();
}

void SamplerVoice::release() {
    ampEnvelope_.release();
    filterEnvelope_.release();
}

void SamplerVoice::render(float* leftOut, float* rightOut, int numSamples) {
    if (!active_ || !sampleData_) {
        for (int i = 0; i < numSamples; ++i) {
            leftOut[i] = 0.0f;
            rightOut[i] = 0.0f;
        }
        return;
    }

    for (int i = 0; i < numSamples; ++i) {
        // Check if we've reached end of sample
        if (playPosition_ >= static_cast<double>(sampleLength_ - 1)) {
            active_ = false;
            leftOut[i] = 0.0f;
            rightOut[i] = 0.0f;
            continue;
        }

        // Linear interpolation for sample playback
        size_t pos0 = static_cast<size_t>(playPosition_);
        size_t pos1 = std::min(pos0 + 1, sampleLength_ - 1);
        float frac = static_cast<float>(playPosition_ - pos0);

        float sampleL, sampleR;
        if (sampleChannels_ == 1) {
            sampleL = sampleData_[pos0] * (1.0f - frac) + sampleData_[pos1] * frac;
            sampleR = sampleL;
        } else {
            sampleL = sampleData_[pos0 * 2] * (1.0f - frac) + sampleData_[pos1 * 2] * frac;
            sampleR = sampleData_[pos0 * 2 + 1] * (1.0f - frac) + sampleData_[pos1 * 2 + 1] * frac;
        }

        // Process envelopes
        float ampEnv = ampEnvelope_.process();
        float filterEnv = filterEnvelope_.process();

        // Apply filter with envelope modulation
        // Convert normalized cutoff (0-1) to Hz for MoogFilter
        float cutoff = baseCutoff_ + filterEnvAmount_ * filterEnv;
        cutoff = std::clamp(cutoff, 0.0f, 1.0f);
        // Map 0-1 to 20Hz-20kHz logarithmically
        float cutoffHz = 20.0f * std::pow(1000.0f, cutoff);
        filterL_.SetCutoff(cutoffHz);
        filterR_.SetCutoff(cutoffHz);

        sampleL = filterL_.Process(sampleL);
        sampleR = filterR_.Process(sampleR);

        // Apply amplitude envelope and velocity
        leftOut[i] = sampleL * ampEnv * velocity_;
        rightOut[i] = sampleR * ampEnv * velocity_;

        // Advance position
        playPosition_ += playbackRate_;

        // Check if envelope finished
        if (!ampEnvelope_.isActive()) {
            active_ = false;
        }
    }
}

} // namespace audio
