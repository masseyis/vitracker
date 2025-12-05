// VA Synth filter and envelope - Moog-style ladder filter for VASynth
// Separate from plaits::MoogFilter to avoid conflicts

#pragma once

#include <cmath>
#include <algorithm>

namespace dsp {

// Moog-style 4-pole (24dB/octave) ladder filter for VA synth
class VAMoogFilter {
public:
    VAMoogFilter() = default;
    ~VAMoogFilter() = default;

    void init(float sampleRate) {
        sampleRate_ = sampleRate;
        reset();
    }

    void reset() {
        stage_[0] = stage_[1] = stage_[2] = stage_[3] = 0.0f;
        stageTanh_[0] = stageTanh_[1] = stageTanh_[2] = stageTanh_[3] = 0.0f;
    }

    // Set cutoff frequency in Hz
    void setCutoff(float freq) {
        cutoffFreq_ = std::clamp(freq, 20.0f, sampleRate_ * 0.45f);
        updateCoefficients();
    }

    // Set cutoff from normalized value (0-1)
    void setCutoffNormalized(float normalized) {
        // Exponential mapping: 20Hz to 20kHz
        float freq = 20.0f * std::pow(1000.0f, normalized);
        setCutoff(freq);
    }

    // Set resonance (0-1, self-oscillation around 0.95+)
    void setResonance(float res) {
        resonance_ = std::clamp(res, 0.0f, 1.0f);
        updateCoefficients();
    }

    // Process a single sample
    float process(float input) {
        // Feedback with soft clipping for stability at high resonance
        float feedback = resonanceComp_ * softClip(stage_[3]);

        // Input with feedback subtracted
        float inputWithFeedback = input - feedback;

        // Soft clip input to prevent blowup
        inputWithFeedback = softClip(inputWithFeedback);

        // Four cascaded one-pole filters with tanh saturation
        stage_[0] += cutoffCoeff_ * (std::tanh(inputWithFeedback) - stageTanh_[0]);
        stageTanh_[0] = std::tanh(stage_[0]);

        stage_[1] += cutoffCoeff_ * (stageTanh_[0] - stageTanh_[1]);
        stageTanh_[1] = std::tanh(stage_[1]);

        stage_[2] += cutoffCoeff_ * (stageTanh_[1] - stageTanh_[2]);
        stageTanh_[2] = std::tanh(stage_[2]);

        stage_[3] += cutoffCoeff_ * (stageTanh_[2] - stageTanh_[3]);
        stageTanh_[3] = std::tanh(stage_[3]);

        return stage_[3];
    }

    // Get intermediate outputs for different slopes
    float get12dB() const { return stage_[1]; }  // 2-pole
    float get24dB() const { return stage_[3]; }  // 4-pole

private:
    void updateCoefficients() {
        // Compute filter coefficient using bilinear transform approximation
        float fc = cutoffFreq_ / sampleRate_;
        fc = std::clamp(fc, 0.001f, 0.45f);

        // Pre-warping
        cutoffCoeff_ = 2.0f * std::sin(kPi * fc);

        // Resonance compensation (4x for 4-pole filter, with some reduction)
        resonanceComp_ = 4.0f * resonance_ * (1.0f - 0.15f * fc);
    }

    // Soft clipping for stability
    float softClip(float x) const {
        if (x > 3.0f) return 1.0f;
        if (x < -3.0f) return -1.0f;
        return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
    }

    static constexpr float kPi = 3.14159265358979323846f;

    float sampleRate_ = 48000.0f;
    float cutoffFreq_ = 10000.0f;
    float resonance_ = 0.0f;

    float cutoffCoeff_ = 0.5f;
    float resonanceComp_ = 0.0f;

    // Filter state (4 stages)
    float stage_[4] = {0, 0, 0, 0};
    float stageTanh_[4] = {0, 0, 0, 0};
};

// Simple ADSR envelope for VA synth
class VAEnvelope {
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    VAEnvelope() = default;

    void init(float sampleRate) {
        sampleRate_ = sampleRate;
        reset();
    }

    void reset() {
        stage_ = Stage::Idle;
        output_ = 0.0f;
    }

    // Set times in seconds (0-1 normalized input, mapped exponentially)
    void setAttack(float normalized) {
        // Map 0-1 to 1ms - 5s exponentially
        attackTime_ = 0.001f + std::pow(normalized, 2.0f) * 4.999f;
        updateRates();
    }

    void setDecay(float normalized) {
        // Map 0-1 to 1ms - 10s exponentially
        decayTime_ = 0.001f + std::pow(normalized, 2.0f) * 9.999f;
        updateRates();
    }

    void setSustain(float level) {
        sustainLevel_ = std::clamp(level, 0.0f, 1.0f);
    }

    void setRelease(float normalized) {
        // Map 0-1 to 1ms - 10s exponentially
        releaseTime_ = 0.001f + std::pow(normalized, 2.0f) * 9.999f;
        updateRates();
    }

    void trigger() {
        stage_ = Stage::Attack;
    }

    void release() {
        if (stage_ != Stage::Idle) {
            stage_ = Stage::Release;
        }
    }

    void forceOff() {
        stage_ = Stage::Idle;
        output_ = 0.0f;
    }

    float process() {
        switch (stage_) {
            case Stage::Idle:
                output_ = 0.0f;
                break;

            case Stage::Attack:
                output_ += attackRate_;
                if (output_ >= 1.0f) {
                    output_ = 1.0f;
                    stage_ = Stage::Decay;
                }
                break;

            case Stage::Decay:
                output_ -= decayRate_;
                if (output_ <= sustainLevel_) {
                    output_ = sustainLevel_;
                    stage_ = Stage::Sustain;
                }
                break;

            case Stage::Sustain:
                output_ = sustainLevel_;
                break;

            case Stage::Release:
                output_ -= releaseRate_;
                if (output_ <= 0.0f) {
                    output_ = 0.0f;
                    stage_ = Stage::Idle;
                }
                break;
        }

        return output_;
    }

    float getOutput() const { return output_; }
    Stage getStage() const { return stage_; }
    bool isActive() const { return stage_ != Stage::Idle; }

private:
    void updateRates() {
        attackRate_ = 1.0f / (attackTime_ * sampleRate_);
        decayRate_ = (1.0f - sustainLevel_) / (decayTime_ * sampleRate_);
        // Release rate should always be able to reach 0 from any level,
        // not just from sustain level (fixes infinite release when sustain=0)
        releaseRate_ = 1.0f / (releaseTime_ * sampleRate_);
        // Prevent division by zero
        if (decayRate_ < 1e-9f) decayRate_ = 1e-9f;
        if (releaseRate_ < 1e-9f) releaseRate_ = 1e-9f;
    }

    float sampleRate_ = 48000.0f;

    float attackTime_ = 0.01f;
    float decayTime_ = 0.2f;
    float sustainLevel_ = 0.7f;
    float releaseTime_ = 0.3f;

    float attackRate_ = 0.0f;
    float decayRate_ = 0.0f;
    float releaseRate_ = 0.0f;

    Stage stage_ = Stage::Idle;
    float output_ = 0.0f;
};

} // namespace dsp
