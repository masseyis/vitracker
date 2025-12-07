#pragma once

#include "BiquadFilter.h"
#include <array>

namespace audio {

// 3-band OTT (Over The Top) multiband dynamics processor
// Linkwitz-Riley crossovers at ~100Hz and ~3kHz
// Each band has upward + downward compression
class MultibandOTT {
public:
    void prepare(double sampleRate, int samplesPerBlock);
    void setParams(float depth, float mix, float smooth);  // All 0-1
    void process(float& left, float& right);
    void reset();

private:
    struct BandState {
        float envUp = 0.0f;    // Upward compressor envelope
        float envDown = 0.0f;  // Downward compressor envelope
    };

    float compressBand(float input, BandState& state, float upThreshold, float downThreshold);

    double sampleRate_ = 44100.0;
    float depth_ = 0.0f;
    float mix_ = 1.0f;
    float smooth_ = 0.5f;

    // Crossover filters (Linkwitz-Riley = 2x Butterworth cascaded)
    // Low band: below ~100Hz
    BiquadFilter lowpassL1_, lowpassL2_;   // LP for low band extraction
    BiquadFilter lowpassR1_, lowpassR2_;

    // High band: above ~3kHz
    BiquadFilter highpassL1_, highpassL2_; // HP for high band extraction
    BiquadFilter highpassR1_, highpassR2_;

    // Mid band is derived: input - low - high

    // Per-band envelope states (stereo linked)
    std::array<BandState, 3> bandStatesL_;  // Low, Mid, High
    std::array<BandState, 3> bandStatesR_;

    // Attack/release coefficients (recalculated when smooth changes)
    float attackCoef_ = 0.0f, releaseCoef_ = 0.0f;
};

} // namespace audio
