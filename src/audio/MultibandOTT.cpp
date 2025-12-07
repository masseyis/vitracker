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
    setParams(depth_, mix_, smooth_);
}

void MultibandOTT::setParams(float depth, float mix, float smooth) {
    depth_ = std::clamp(depth, 0.0f, 1.0f);
    mix_ = std::clamp(mix, 0.0f, 1.0f);
    smooth_ = std::clamp(smooth, 0.0f, 1.0f);

    // Calculate attack/release based on smooth parameter
    // smooth=0 -> fast (5ms attack, 50ms release)
    // smooth=1 -> slow (50ms attack, 500ms release)
    float attackMs = 5.0f + smooth_ * 45.0f;
    float releaseMs = 50.0f + smooth_ * 450.0f;

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

float MultibandOTT::compressBand(float input, BandState& state, float upThreshold, float downThreshold) {
    float absInput = std::abs(input);

    // Update envelope followers
    float coef = (absInput > state.envUp) ? attackCoef_ : releaseCoef_;
    state.envUp += coef * (absInput - state.envUp);

    coef = (absInput > state.envDown) ? attackCoef_ : releaseCoef_;
    state.envDown += coef * (absInput - state.envDown);

    float gain = 1.0f;

    // Upward compression: boost quiet signals
    if (state.envUp < upThreshold && state.envUp > 0.0001f) {
        float ratio = upThreshold / state.envUp;
        gain *= 1.0f + (ratio - 1.0f) * depth_ * 0.5f;
        gain = std::min(gain, 4.0f);  // Limit boost to ~12dB
    }

    // Downward compression: squash loud signals
    if (state.envDown > downThreshold) {
        float ratio = downThreshold / state.envDown;
        gain *= 1.0f - (1.0f - ratio) * depth_ * 0.5f;
        gain = std::max(gain, 0.25f);  // Limit reduction to ~-12dB
    }

    return input * gain;
}

void MultibandOTT::process(float& left, float& right) {
    if (depth_ < 0.001f) return;  // Bypass when off

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

    // Thresholds per band (different for character)
    const float lowUpThresh = 0.15f, lowDownThresh = 0.6f;
    const float midUpThresh = 0.1f, midDownThresh = 0.5f;
    const float highUpThresh = 0.08f, highDownThresh = 0.4f;

    // Process each band
    lowL = compressBand(lowL, bandStatesL_[0], lowUpThresh, lowDownThresh);
    lowR = compressBand(lowR, bandStatesR_[0], lowUpThresh, lowDownThresh);

    midL = compressBand(midL, bandStatesL_[1], midUpThresh, midDownThresh);
    midR = compressBand(midR, bandStatesR_[1], midUpThresh, midDownThresh);

    highL = compressBand(highL, bandStatesL_[2], highUpThresh, highDownThresh);
    highR = compressBand(highR, bandStatesR_[2], highUpThresh, highDownThresh);

    // Sum bands back together
    float wetL = lowL + midL + highL;
    float wetR = lowR + midR + highR;

    // Mix dry/wet
    left = dryL + (wetL - dryL) * mix_;
    right = dryR + (wetR - dryR) * mix_;
}

} // namespace audio
