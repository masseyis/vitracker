// Moog-style 4-pole ladder filter
// Part of PlaitsVST - GPL v3

#pragma once

#include <cmath>

namespace plaits {

class MoogFilter {
public:
    MoogFilter() = default;
    ~MoogFilter() = default;

    void Init(float sample_rate);
    void Reset();

    // Set cutoff frequency in Hz (20-20000)
    void SetCutoff(float cutoff_hz);

    // Set resonance (0-1, where 1 is self-oscillation)
    void SetResonance(float resonance);

    // Set sample rate (call if it changes)
    void SetSampleRate(float sample_rate) { sample_rate_ = sample_rate; }

    // Process a single sample
    float Process(float input);

    // Get current settings
    float GetCutoff() const { return cutoff_hz_; }
    float GetResonance() const { return resonance_; }

private:
    void UpdateCoefficients();

    float sample_rate_ = 44100.0f;
    float cutoff_hz_ = 10000.0f;
    float resonance_ = 0.0f;

    // Filter state - 4 cascaded one-pole sections
    float stage_[4] = {0, 0, 0, 0};
    float stage_tanh_[3] = {0, 0, 0};

    // Coefficients
    float g_ = 0.0f;      // Cutoff coefficient
    float k_ = 0.0f;      // Resonance coefficient (feedback)
};

} // namespace plaits
