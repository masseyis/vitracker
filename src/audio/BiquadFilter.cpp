#include "BiquadFilter.h"
#include <cmath>

namespace audio {

void BiquadFilter::reset() {
    z1_ = z2_ = 0.0f;
}

void BiquadFilter::setSampleRate(double sampleRate) {
    sampleRate_ = sampleRate;
}

void BiquadFilter::setHighpass(float freq, float q) {
    calculateCoefficients(Type::Highpass, freq, 0.0f, q);
}

void BiquadFilter::setLowpass(float freq, float q) {
    calculateCoefficients(Type::Lowpass, freq, 0.0f, q);
}

void BiquadFilter::setLowShelf(float freq, float gainDb) {
    calculateCoefficients(Type::LowShelf, freq, gainDb, 0.707f);
}

void BiquadFilter::setHighShelf(float freq, float gainDb) {
    calculateCoefficients(Type::HighShelf, freq, gainDb, 0.707f);
}

void BiquadFilter::setPeak(float freq, float gainDb, float q) {
    calculateCoefficients(Type::Peak, freq, gainDb, q);
}

float BiquadFilter::process(float input) {
    float output = b0_ * input + z1_;
    z1_ = b1_ * input - a1_ * output + z2_;
    z2_ = b2_ * input - a2_ * output;
    return output;
}

void BiquadFilter::calculateCoefficients(Type type, float freq, float gainDb, float q) {
    const float w0 = 2.0f * static_cast<float>(M_PI) * freq / static_cast<float>(sampleRate_);
    const float cosW0 = std::cos(w0);
    const float sinW0 = std::sin(w0);
    const float alpha = sinW0 / (2.0f * q);
    const float A = std::pow(10.0f, gainDb / 40.0f);  // sqrt of linear gain

    float b0, b1, b2, a0, a1, a2;

    switch (type) {
        case Type::Highpass:
            b0 = (1.0f + cosW0) / 2.0f;
            b1 = -(1.0f + cosW0);
            b2 = (1.0f + cosW0) / 2.0f;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cosW0;
            a2 = 1.0f - alpha;
            break;

        case Type::Lowpass:
            b0 = (1.0f - cosW0) / 2.0f;
            b1 = 1.0f - cosW0;
            b2 = (1.0f - cosW0) / 2.0f;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cosW0;
            a2 = 1.0f - alpha;
            break;

        case Type::LowShelf: {
            const float sqrtA = std::sqrt(A);
            const float alphaShelf = sinW0 / 2.0f * std::sqrt((A + 1.0f/A) * (1.0f/0.707f - 1.0f) + 2.0f);
            b0 = A * ((A + 1.0f) - (A - 1.0f) * cosW0 + 2.0f * sqrtA * alphaShelf);
            b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosW0);
            b2 = A * ((A + 1.0f) - (A - 1.0f) * cosW0 - 2.0f * sqrtA * alphaShelf);
            a0 = (A + 1.0f) + (A - 1.0f) * cosW0 + 2.0f * sqrtA * alphaShelf;
            a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosW0);
            a2 = (A + 1.0f) + (A - 1.0f) * cosW0 - 2.0f * sqrtA * alphaShelf;
            break;
        }

        case Type::HighShelf: {
            const float sqrtA = std::sqrt(A);
            const float alphaShelf = sinW0 / 2.0f * std::sqrt((A + 1.0f/A) * (1.0f/0.707f - 1.0f) + 2.0f);
            b0 = A * ((A + 1.0f) + (A - 1.0f) * cosW0 + 2.0f * sqrtA * alphaShelf);
            b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosW0);
            b2 = A * ((A + 1.0f) + (A - 1.0f) * cosW0 - 2.0f * sqrtA * alphaShelf);
            a0 = (A + 1.0f) - (A - 1.0f) * cosW0 + 2.0f * sqrtA * alphaShelf;
            a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosW0);
            a2 = (A + 1.0f) - (A - 1.0f) * cosW0 - 2.0f * sqrtA * alphaShelf;
            break;
        }

        case Type::Peak:
            b0 = 1.0f + alpha * A;
            b1 = -2.0f * cosW0;
            b2 = 1.0f - alpha * A;
            a0 = 1.0f + alpha / A;
            a1 = -2.0f * cosW0;
            a2 = 1.0f - alpha / A;
            break;
    }

    // Normalize coefficients
    b0_ = b0 / a0;
    b1_ = b1 / a0;
    b2_ = b2 / a0;
    a1_ = a1 / a0;
    a2_ = a2 / a0;
}

} // namespace audio
