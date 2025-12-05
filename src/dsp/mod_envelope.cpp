// Modulation Envelope - AD envelope for modulation
// Part of PlaitsVST - GPL v3

#include "mod_envelope.h"
#include <algorithm>
#include <cmath>

namespace plaits {

void ModEnvelope::Init()
{
    attack_ms_ = 10;
    decay_ms_ = 200;
    stage_ = Stage::Idle;
    output_ = 0.0f;
    phase_ = 0.0f;
}

void ModEnvelope::Reset()
{
    stage_ = Stage::Idle;
    output_ = 0.0f;
    phase_ = 0.0f;
}

void ModEnvelope::Trigger()
{
    stage_ = Stage::Attack;
    phase_ = 0.0f;
    // Don't reset output_ - allows retriggering mid-envelope smoothly
}

float ModEnvelope::Process(float sample_rate, int num_samples)
{
    if (stage_ == Stage::Idle) {
        return output_;
    }

    float samples_f = static_cast<float>(num_samples);

    if (stage_ == Stage::Attack) {
        // Calculate increment: go from 0 to 1 over attack_ms_ milliseconds
        float attack_samples = (attack_ms_ / 1000.0f) * sample_rate;
        if (attack_samples < 1.0f) attack_samples = 1.0f;

        float increment = samples_f / attack_samples;
        phase_ += increment;

        if (phase_ >= 1.0f) {
            // Attack complete, move to decay
            output_ = 1.0f;
            phase_ = 0.0f;
            stage_ = Stage::Decay;
        } else {
            // Linear attack (could make exponential if desired)
            output_ = phase_;
        }
    }
    else if (stage_ == Stage::Decay) {
        // Calculate increment: go from 1 to 0 over decay_ms_ milliseconds
        float decay_samples = (decay_ms_ / 1000.0f) * sample_rate;
        if (decay_samples < 1.0f) decay_samples = 1.0f;

        float increment = samples_f / decay_samples;
        phase_ += increment;

        if (phase_ >= 1.0f) {
            // Decay complete
            output_ = 0.0f;
            phase_ = 0.0f;
            stage_ = Stage::Idle;
        } else {
            // Exponential decay for more natural sound
            // output = 1 - phase would be linear
            // Using exponential: output = e^(-k*phase) where k controls curve
            float curve = 4.0f;  // Adjust for steeper/gentler curve
            output_ = std::exp(-curve * phase_);
        }
    }

    return output_;
}

} // namespace plaits
