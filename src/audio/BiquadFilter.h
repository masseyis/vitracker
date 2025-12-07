#pragma once

namespace audio {

// Direct Form II Transposed biquad filter
// Implements RBJ Audio-EQ-Cookbook filters
class BiquadFilter {
public:
    enum class Type {
        Highpass,
        LowShelf,
        HighShelf,
        Peak
    };

    void reset();
    void setSampleRate(double sampleRate);

    // Configure filter type and parameters
    void setHighpass(float freq, float q = 0.707f);
    void setLowShelf(float freq, float gainDb);
    void setHighShelf(float freq, float gainDb);
    void setPeak(float freq, float gainDb, float q);

    // Process single sample (call for each channel)
    float process(float input);

private:
    void calculateCoefficients(Type type, float freq, float gainDb, float q);

    double sampleRate_ = 44100.0;

    // Coefficients
    float b0_ = 1.0f, b1_ = 0.0f, b2_ = 0.0f;
    float a1_ = 0.0f, a2_ = 0.0f;

    // State (Direct Form II Transposed)
    float z1_ = 0.0f, z2_ = 0.0f;
};

} // namespace audio
