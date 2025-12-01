// Resampler for converting 48kHz Plaits output to host sample rate
// PlaitsVST: MIT License

#include "resampler.h"

void Resampler::Init(double sourceSampleRate, double targetSampleRate)
{
    ratio_ = sourceSampleRate / targetSampleRate;
    phase_ = 0.0;
    lastSample_ = 0;
}

void Resampler::Reset()
{
    phase_ = 0.0;
    lastSample_ = 0;
}

size_t Resampler::Process(const int16_t* input, size_t inputSize,
                          float* output, size_t maxOutputSize)
{
    size_t outputWritten = 0;

    while (outputWritten < maxOutputSize) {
        // Calculate which input samples we're interpolating between
        size_t idx0 = static_cast<size_t>(phase_);

        if (idx0 >= inputSize) {
            // Consumed all input
            break;
        }

        // Get samples for interpolation
        int16_t s0 = (idx0 == 0 && phase_ < 1.0) ? lastSample_ : input[idx0];
        int16_t s1 = (idx0 + 1 < inputSize) ? input[idx0 + 1] : input[idx0];

        // Linear interpolation
        double frac = phase_ - static_cast<double>(idx0);
        double interpolated = s0 + (s1 - s0) * frac;

        // Convert to float (-1.0 to 1.0)
        output[outputWritten++] = static_cast<float>(interpolated / 32768.0);

        // Advance phase by ratio (consuming ratio_ input samples per output sample)
        phase_ += ratio_;
    }

    // Save last sample for next block's interpolation
    if (inputSize > 0) {
        lastSample_ = input[inputSize - 1];
    }

    // Adjust phase to be relative to next block
    phase_ -= static_cast<double>(inputSize);
    if (phase_ < 0.0) {
        phase_ = 0.0;
    }

    return outputWritten;
}
