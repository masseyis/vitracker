// Moog-style 4-pole ladder filter
// Based on Antti Huovilainen's non-linear digital model
// Part of PlaitsVST - GPL v3

#include "moog_filter.h"
#include <algorithm>

namespace plaits {

void MoogFilter::Init(float sample_rate)
{
    sample_rate_ = sample_rate;
    cutoff_hz_ = 10000.0f;
    resonance_ = 0.0f;
    Reset();
    UpdateCoefficients();
}

void MoogFilter::Reset()
{
    for (int i = 0; i < 4; ++i) {
        stage_[i] = 0.0f;
    }
    for (int i = 0; i < 3; ++i) {
        stage_tanh_[i] = 0.0f;
    }
}

void MoogFilter::SetCutoff(float cutoff_hz)
{
    cutoff_hz_ = std::clamp(cutoff_hz, 20.0f, 20000.0f);
    UpdateCoefficients();
}

void MoogFilter::SetResonance(float resonance)
{
    resonance_ = std::clamp(resonance, 0.0f, 1.0f);
    UpdateCoefficients();
}

void MoogFilter::UpdateCoefficients()
{
    // Calculate normalized frequency
    float fc = cutoff_hz_ / sample_rate_;

    // Attempt to match analog response using bilinear pre-warping
    float wc = 2.0f * std::tan(3.14159265f * fc);

    // One-pole coefficient (simplified for efficiency)
    g_ = wc / (1.0f + wc);

    // Resonance feedback - scale to 0-4 range (4 = self-oscillation)
    // Slightly reduce max to prevent instability
    k_ = resonance_ * 3.8f;
}

float MoogFilter::Process(float input)
{
    // Soft-clip the input to prevent harsh distortion at high resonance
    float x = input;

    // Feedback from output with resonance
    float feedback = k_ * stage_[3];

    // Soft-clip the feedback to tame self-oscillation
    feedback = std::tanh(feedback);

    // Input minus feedback
    float u = x - feedback;

    // Soft-clip the input to the ladder
    u = std::tanh(u);

    // 4 cascaded one-pole lowpass filters
    // Each stage: y = y + g * (tanh(x) - tanh(y))
    // Using tanh for nonlinear saturation like analog Moog

    stage_[0] += g_ * (u - std::tanh(stage_[0]));
    stage_[1] += g_ * (std::tanh(stage_[0]) - std::tanh(stage_[1]));
    stage_[2] += g_ * (std::tanh(stage_[1]) - std::tanh(stage_[2]));
    stage_[3] += g_ * (std::tanh(stage_[2]) - std::tanh(stage_[3]));

    return stage_[3];
}

} // namespace plaits
