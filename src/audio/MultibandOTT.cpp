#include "MultibandOTT.h"
#include <cmath>
#include <algorithm>

namespace audio {

void MultibandOTT::prepare(double sampleRate, int /*samplesPerBlock*/) {
    sampleRate_ = sampleRate;

    // Set up crossover filters
    const float lowXover = 100.0f;
    const float highXover = 3000.0f;
    const float q = 0.707f;  // Butterworth

    // Low band LP filters (cascaded for LR4 = 24dB/oct)
    lowpassL1_.setSampleRate(sampleRate);
    lowpassL2_.setSampleRate(sampleRate);
    lowpassR1_.setSampleRate(sampleRate);
    lowpassR2_.setSampleRate(sampleRate);
    lowpassL1_.setLowpass(lowXover, q);
    lowpassL2_.setLowpass(lowXover, q);
    lowpassR1_.setLowpass(lowXover, q);
    lowpassR2_.setLowpass(lowXover, q);

    // High band HP filters (cascaded for LR4)
    highpassL1_.setSampleRate(sampleRate);
    highpassL2_.setSampleRate(sampleRate);
    highpassR1_.setSampleRate(sampleRate);
    highpassR2_.setSampleRate(sampleRate);
    highpassL1_.setHighpass(highXover, q);
    highpassL2_.setHighpass(highXover, q);
    highpassR1_.setHighpass(highXover, q);
    highpassR2_.setHighpass(highXover, q);

    reset();
    setParams(lowDepth_, midDepth_, highDepth_, mix_);
}

void MultibandOTT::setParams(float lowDepth, float midDepth, float highDepth, float mix) {
    lowDepth_ = std::clamp(lowDepth, 0.0f, 1.0f);
    midDepth_ = std::clamp(midDepth, 0.0f, 1.0f);
    highDepth_ = std::clamp(highDepth, 0.0f, 1.0f);
    mix_ = std::clamp(mix, 0.0f, 1.0f);

    // Fixed attack/release times for classic OTT character (medium-fast)
    float attackMs = 15.0f;
    float releaseMs = 150.0f;

    attackCoef_ = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate_) * attackMs * 0.001f));
    releaseCoef_ = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate_) * releaseMs * 0.001f));
}

void MultibandOTT::reset() {
    for (auto& state : bandStatesL_) {
        state.envUp = 0.0f;
        state.envDown = 0.0f;
    }
    for (auto& state : bandStatesR_) {
        state.envUp = 0.0f;
        state.envDown = 0.0f;
    }

    lowpassL1_.reset(); lowpassL2_.reset();
    lowpassR1_.reset(); lowpassR2_.reset();
    highpassL1_.reset(); highpassL2_.reset();
    highpassR1_.reset(); highpassR2_.reset();
}

float MultibandOTT::compressBand(float input, BandState& state, float upThreshold, float downThreshold, float depth) {
    float absInput = std::abs(input);

    // Update envelope followers
    float coef = (absInput > state.envUp) ? attackCoef_ : releaseCoef_;
    state.envUp += coef * (absInput - state.envUp);

    coef = (absInput > state.envDown) ? attackCoef_ : releaseCoef_;
    state.envDown += coef * (absInput - state.envDown);

    float gain = 1.0f;

    // Upward compression: boost quiet signals aggressively
    if (state.envUp < upThreshold && state.envUp > 0.00001f) {
        float ratio = upThreshold / state.envUp;
        // Full depth scaling - no 0.5f damping
        gain *= 1.0f + (ratio - 1.0f) * depth;
        gain = std::min(gain, 10.0f);  // Limit boost to ~20dB for extreme effect
    }

    // Downward compression: squash loud signals hard
    if (state.envDown > downThreshold) {
        float ratio = downThreshold / state.envDown;
        // Full depth scaling - more aggressive squashing
        gain *= 1.0f - (1.0f - ratio) * depth;
        gain = std::max(gain, 0.1f);  // Limit reduction to ~-20dB
    }

    return input * gain;
}

void MultibandOTT::process(float& left, float& right) {
    // Bypass when all bands are off
    if (lowDepth_ < 0.001f && midDepth_ < 0.001f && highDepth_ < 0.001f) return;

    float dryL = left, dryR = right;

    // Split into 3 bands using crossover filters
    // Low band: cascaded lowpass at 100Hz
    float lowL = lowpassL1_.process(left);
    lowL = lowpassL2_.process(lowL);
    float lowR = lowpassR1_.process(right);
    lowR = lowpassR2_.process(lowR);

    // High band: cascaded highpass at 3kHz
    float highL = highpassL1_.process(left);
    highL = highpassL2_.process(highL);
    float highR = highpassR1_.process(right);
    highR = highpassR2_.process(highR);

    // Mid band: what's left (input - low - high)
    float midL = left - lowL - highL;
    float midR = right - lowR - highR;

    // Thresholds per band - more aggressive for pronounced OTT effect
    // Lower up thresholds = more upward boost, lower down thresholds = more squashing
    const float lowUpThresh = 0.25f, lowDownThresh = 0.4f;
    const float midUpThresh = 0.2f, midDownThresh = 0.35f;
    const float highUpThresh = 0.15f, highDownThresh = 0.3f;

    // Process each band with per-band depth control
    lowL = compressBand(lowL, bandStatesL_[0], lowUpThresh, lowDownThresh, lowDepth_);
    lowR = compressBand(lowR, bandStatesR_[0], lowUpThresh, lowDownThresh, lowDepth_);

    midL = compressBand(midL, bandStatesL_[1], midUpThresh, midDownThresh, midDepth_);
    midR = compressBand(midR, bandStatesR_[1], midUpThresh, midDownThresh, midDepth_);

    highL = compressBand(highL, bandStatesL_[2], highUpThresh, highDownThresh, highDepth_);
    highR = compressBand(highR, bandStatesR_[2], highUpThresh, highDownThresh, highDepth_);

    // Sum bands back together
    float wetL = lowL + midL + highL;
    float wetR = lowR + midR + highR;

    // Mix dry/wet
    left = dryL + (wetL - dryL) * mix_;
    right = dryR + (wetR - dryR) * mix_;
}

} // namespace audio
