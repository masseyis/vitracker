// Resampler for converting 48kHz Plaits output to host sample rate
// PlaitsVST: MIT License

#pragma once

#include <cstdint>
#include <cstddef>

class Resampler {
public:
    Resampler() = default;
    ~Resampler() = default;

    void Init(double sourceSampleRate, double targetSampleRate);
    void Reset();

    // Process input samples (int16) and produce output samples (float)
    // Returns number of output samples written
    size_t Process(const int16_t* input, size_t inputSize,
                   float* output, size_t maxOutputSize);

    double ratio() const { return ratio_; }

private:
    double ratio_ = 1.0;           // source/target ratio
    double phase_ = 0.0;           // Fractional position in input
    int16_t lastSample_ = 0;       // Previous sample for interpolation
};
