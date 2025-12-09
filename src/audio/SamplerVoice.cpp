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
    trackerFX_.setSampleRate(sampleRate);
}

void SamplerVoice::setTempo(float bpm) {
    trackerFX_.setTempo(bpm);
}

void SamplerVoice::setSampleData(const float* leftData, const float* rightData, size_t numSamples, int originalSampleRate) {
    sampleDataL_ = leftData;
    sampleDataR_ = rightData;  // nullptr for mono samples
    sampleLength_ = numSamples;
    originalSampleRate_ = originalSampleRate;
}

void SamplerVoice::trigger(int midiNote, float velocity, const model::SamplerParams& params) {
    model::Step emptyStep;
    trigger(midiNote, velocity, params, emptyStep);
}

void SamplerVoice::trigger(int midiNote, float velocity, const model::SamplerParams& params, const model::Step& step) {
    if (!sampleDataL_ || sampleLength_ == 0) return;

    // Trigger FX processor
    trackerFX_.triggerNote(midiNote, velocity, step);

    currentNote_ = midiNote;
    rootNote_ = params.rootNote;
    velocity_ = velocity;
    active_ = true;
    playPosition_ = 0.0;

    // Base playback rate for sample rate conversion
    basePlaybackRate_ = static_cast<double>(originalSampleRate_) / sampleRate_;

    // Apply auto-repitch ratio if enabled (transposes sample to nearest C)
    basePlaybackRate_ *= static_cast<double>(params.pitchRatio);

    // Apply pitch shift based on MIDI note vs root note (for keyboard playback)
    double semitones = midiNote - rootNote_;
    basePlaybackRate_ *= std::pow(2.0, semitones / 12.0);

    // Set initial playback rate (will be modulated by arpeggio in render)
    playbackRate_ = basePlaybackRate_;

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
    if (!active_ || !sampleDataL_) {
        for (int i = 0; i < numSamples; ++i) {
            leftOut[i] = 0.0f;
            rightOut[i] = 0.0f;
        }
        return;
    }

    for (int i = 0; i < numSamples; ++i) {
        // Process tracker FX (handles note delay, cut, off, retrigger)
        if (!trackerFX_.processSample()) {
            // FX says note should not play (delay or cut)
            leftOut[i] = 0.0f;
            rightOut[i] = 0.0f;
            continue;
        }

        // Check for note off trigger from FX
        if (trackerFX_.shouldReleaseNote()) {
            release();
        }

        // Check for retrigger from FX
        if (trackerFX_.shouldRetrigger()) {
            ampEnvelope_.trigger();
            filterEnvelope_.trigger();
            playPosition_ = 0.0;  // Reset sample position
        }

        // Check if we've reached end of sample
        if (playPosition_ >= static_cast<double>(sampleLength_ - 1)) {
            active_ = false;
            leftOut[i] = 0.0f;
            rightOut[i] = 0.0f;
            continue;
        }

        // Apply arpeggio offset to playback rate
        int arpOffset = trackerFX_.getArpOffset();
        if (arpOffset != 0) {
            // Adjust playback rate based on arpeggio semitone offset
            playbackRate_ = basePlaybackRate_ * std::pow(2.0, arpOffset / 12.0);
        } else {
            playbackRate_ = basePlaybackRate_;
        }

        // Linear interpolation for sample playback
        size_t pos0 = static_cast<size_t>(playPosition_);
        size_t pos1 = std::min(pos0 + 1, sampleLength_ - 1);
        float frac = static_cast<float>(playPosition_ - pos0);

        // Read from planar format (JUCE stores channels separately)
        float sampleL = sampleDataL_[pos0] * (1.0f - frac) + sampleDataL_[pos1] * frac;
        float sampleR;
        if (sampleDataR_) {
            sampleR = sampleDataR_[pos0] * (1.0f - frac) + sampleDataR_[pos1] * frac;
        } else {
            sampleR = sampleL;  // Mono: duplicate left to right
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
