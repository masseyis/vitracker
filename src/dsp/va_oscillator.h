// Virtual Analog Oscillator with PolyBLEP antialiasing
// Generates saw, square, triangle, pulse, and sine waveforms

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace dsp {

enum class VAOscWaveform {
    Saw = 0,
    Square,
    Triangle,
    Pulse,
    Sine,
    NumWaveforms
};

class VAOscillator {
public:
    VAOscillator() = default;
    ~VAOscillator() = default;

    void init(float sampleRate) {
        sampleRate_ = sampleRate;
        phase_ = 0.0f;
        phaseIncrement_ = 0.0f;
        frequency_ = 440.0f;
        pulseWidth_ = 0.5f;
        waveform_ = VAOscWaveform::Saw;
    }

    void setFrequency(float freq) {
        frequency_ = freq;
        phaseIncrement_ = freq / sampleRate_;
    }

    void setWaveform(VAOscWaveform wf) {
        waveform_ = wf;
    }

    void setPulseWidth(float pw) {
        pulseWidth_ = std::clamp(pw, 0.01f, 0.99f);
    }

    void reset() {
        phase_ = 0.0f;
    }

    // Process a single sample
    float process() {
        float output = 0.0f;

        switch (waveform_) {
            case VAOscWaveform::Saw:
                output = processSaw();
                break;
            case VAOscWaveform::Square:
                output = processSquare();
                break;
            case VAOscWaveform::Triangle:
                output = processTriangle();
                break;
            case VAOscWaveform::Pulse:
                output = processPulse();
                break;
            case VAOscWaveform::Sine:
                output = processSine();
                break;
            default:
                output = 0.0f;
        }

        // Advance phase
        phase_ += phaseIncrement_;
        if (phase_ >= 1.0f) {
            phase_ -= 1.0f;
        }

        return output;
    }

    // Get current phase (0-1)
    float getPhase() const { return phase_; }

    // Set phase directly (for sync)
    void setPhase(float p) { phase_ = p - std::floor(p); }

private:
    // PolyBLEP correction for discontinuities
    float polyBlep(float t) const {
        float dt = phaseIncrement_;
        if (t < dt) {
            t /= dt;
            return t + t - t * t - 1.0f;
        } else if (t > 1.0f - dt) {
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }
        return 0.0f;
    }

    float processSaw() {
        // Naive saw: 2 * phase - 1 (range -1 to 1)
        float output = 2.0f * phase_ - 1.0f;
        // Apply PolyBLEP at the discontinuity
        output -= polyBlep(phase_);
        return output;
    }

    float processSquare() {
        // Naive square: +1 for first half, -1 for second half
        float output = (phase_ < 0.5f) ? 1.0f : -1.0f;
        // Apply PolyBLEP at both transitions
        output += polyBlep(phase_);
        output -= polyBlep(std::fmod(phase_ + 0.5f, 1.0f));
        return output;
    }

    float processTriangle() {
        // Triangle from integrated square (using leaky integrator approach)
        // More efficient: direct formula with PolyBLAMP (polynomial band-limited ramp)
        // Simplified version using phase
        float output;
        if (phase_ < 0.5f) {
            output = 4.0f * phase_ - 1.0f;
        } else {
            output = 3.0f - 4.0f * phase_;
        }
        return output;
    }

    float processPulse() {
        // Pulse wave with variable width
        float output = (phase_ < pulseWidth_) ? 1.0f : -1.0f;
        // Apply PolyBLEP at both transitions
        output += polyBlep(phase_);
        output -= polyBlep(std::fmod(phase_ + (1.0f - pulseWidth_), 1.0f));
        return output;
    }

    float processSine() {
        return std::sin(phase_ * 2.0f * kPi);
    }

    static constexpr float kPi = 3.14159265358979323846f;

    float sampleRate_ = 48000.0f;
    float phase_ = 0.0f;
    float phaseIncrement_ = 0.0f;
    float frequency_ = 440.0f;
    float pulseWidth_ = 0.5f;
    VAOscWaveform waveform_ = VAOscWaveform::Saw;
};

// White noise generator using xorshift
class NoiseGenerator {
public:
    NoiseGenerator() : state_(12345) {}

    void reset() { state_ = 12345; }

    float process() {
        // xorshift32
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        // Convert to float in range -1 to 1
        return static_cast<float>(static_cast<int32_t>(state_)) / 2147483648.0f;
    }

private:
    uint32_t state_;
};

} // namespace dsp
