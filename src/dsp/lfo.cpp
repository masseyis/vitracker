// LFO with tempo-synced rates for modulation
// Part of PlaitsVST - GPL v3

#include "lfo.h"
#include <cstdlib>

namespace plaits {

// Duration in beats for each division (quarter note = 1 beat)
static const float kDivisionBeats[] = {
    0.25f,      // 1/16
    0.25f * 2.0f / 3.0f,  // 1/16T (triplet = 2/3 duration)
    0.5f,       // 1/8
    0.5f * 2.0f / 3.0f,   // 1/8T
    1.0f,       // 1/4
    1.0f * 2.0f / 3.0f,   // 1/4T
    2.0f,       // 1/2
    4.0f,       // 1 bar
    8.0f,       // 2 bars
    16.0f,      // 4 bars
};

static const char* kRateNames[] = {
    "1/16", "1/16T", "1/8", "1/8T", "1/4", "1/4T", "1/2", "1", "2", "4"
};

static const char* kShapeNames[] = {
    "TRI", "SAW", "SQR", "S&H"
};

void Lfo::Init()
{
    phase_ = 0.0f;
    output_ = 0.0f;
    sh_value_ = 0.0f;
    sh_triggered_ = false;
    rate_division_ = LfoRateDivision::Div_1_4;
    shape_ = LfoShape::Triangle;
    tempo_bpm_ = 120.0;
}

void Lfo::Reset()
{
    phase_ = 0.0f;
    sh_triggered_ = false;
}

float Lfo::ComputePhaseIncrement(float sample_rate) const
{
    int div_index = static_cast<int>(rate_division_);
    float beats = kDivisionBeats[div_index];

    // Convert beats to seconds: beats / (bpm / 60) = beats * 60 / bpm
    float period_seconds = beats * 60.0f / static_cast<float>(tempo_bpm_);

    // Phase increment per sample
    return 1.0f / (period_seconds * sample_rate);
}

float Lfo::ComputeWaveform(float phase) const
{
    switch (shape_) {
        case LfoShape::Triangle: {
            // Triangle: 0->1->0->-1->0 over one cycle
            // phase 0-0.25: rise from 0 to 1
            // phase 0.25-0.75: fall from 1 to -1
            // phase 0.75-1: rise from -1 to 0
            if (phase < 0.25f) {
                return phase * 4.0f;
            } else if (phase < 0.75f) {
                return 1.0f - (phase - 0.25f) * 4.0f;
            } else {
                return -1.0f + (phase - 0.75f) * 4.0f;
            }
        }

        case LfoShape::Saw: {
            // Saw: ramp down from 1 to -1
            return 1.0f - phase * 2.0f;
        }

        case LfoShape::Square: {
            // Square: +1 for first half, -1 for second half
            return phase < 0.5f ? 1.0f : -1.0f;
        }

        case LfoShape::SampleAndHold: {
            // S&H handled separately in Process()
            return sh_value_;
        }

        default:
            return 0.0f;
    }
}

float Lfo::Process(float sample_rate, int num_samples)
{
    float increment = ComputePhaseIncrement(sample_rate) * static_cast<float>(num_samples);
    float old_phase = phase_;

    phase_ += increment;

    // Handle S&H: sample new random value at start of cycle
    if (shape_ == LfoShape::SampleAndHold) {
        if (phase_ >= 1.0f || (old_phase > phase_)) {
            // New cycle started - sample new value
            sh_value_ = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f;
        }
    }

    // Wrap phase
    while (phase_ >= 1.0f) {
        phase_ -= 1.0f;
    }

    output_ = ComputeWaveform(phase_);
    return output_;
}

const char* Lfo::GetRateName(LfoRateDivision div)
{
    int index = static_cast<int>(div);
    if (index >= 0 && index < static_cast<int>(LfoRateDivision::NumDivisions)) {
        return kRateNames[index];
    }
    return "???";
}

const char* Lfo::GetShapeName(LfoShape shape)
{
    int index = static_cast<int>(shape);
    if (index >= 0 && index < static_cast<int>(LfoShape::NumShapes)) {
        return kShapeNames[index];
    }
    return "???";
}

} // namespace plaits
