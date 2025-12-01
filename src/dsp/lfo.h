// LFO with tempo-synced rates for modulation
// Part of PlaitsVST - GPL v3

#pragma once

#include <cstdint>
#include <cmath>

namespace plaits {

enum class LfoShape {
    Triangle = 0,
    Saw,
    Square,
    SampleAndHold,
    NumShapes
};

// Tempo-synced rate divisions
enum class LfoRateDivision {
    Div_1_16 = 0,   // 1/16 note
    Div_1_16T,      // 1/16 triplet
    Div_1_8,        // 1/8 note
    Div_1_8T,       // 1/8 triplet
    Div_1_4,        // 1/4 note
    Div_1_4T,       // 1/4 triplet
    Div_1_2,        // 1/2 note
    Div_1_1,        // 1 bar
    Div_2_1,        // 2 bars
    Div_4_1,        // 4 bars
    NumDivisions
};

class Lfo {
public:
    Lfo() = default;
    ~Lfo() = default;

    void Init();
    void Reset();

    void SetRate(LfoRateDivision division) { rate_division_ = division; }
    void SetShape(LfoShape shape) { shape_ = shape; }
    void SetTempo(double bpm) { tempo_bpm_ = bpm; }

    LfoRateDivision GetRate() const { return rate_division_; }
    LfoShape GetShape() const { return shape_; }

    // Process and return current value (-1 to +1)
    // num_samples: number of samples to advance (for block-based processing)
    float Process(float sample_rate, int num_samples = 1);

    // Get current output without advancing
    float GetOutput() const { return output_; }

    // Get rate name for display
    static const char* GetRateName(LfoRateDivision div);
    static const char* GetShapeName(LfoShape shape);

private:
    float ComputePhaseIncrement(float sample_rate) const;
    float ComputeWaveform(float phase) const;

    LfoRateDivision rate_division_ = LfoRateDivision::Div_1_4;
    LfoShape shape_ = LfoShape::Triangle;
    double tempo_bpm_ = 120.0;

    float phase_ = 0.0f;
    float output_ = 0.0f;
    float sh_value_ = 0.0f;  // Sample & hold current value
    bool sh_triggered_ = false;
};

} // namespace plaits
