#pragma once

#include <cmath>
#include <algorithm>

namespace dsp {

class AdsrEnvelope {
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void setSampleRate(float sampleRate) {
        sampleRate_ = sampleRate;
        recalculate();
    }

    void setAttack(float seconds) {
        attackTime_ = std::max(0.001f, seconds);
        recalculate();
    }

    void setDecay(float seconds) {
        decayTime_ = std::max(0.001f, seconds);
        recalculate();
    }

    void setSustain(float level) {
        sustainLevel_ = std::clamp(level, 0.0f, 1.0f);
    }

    void setRelease(float seconds) {
        releaseTime_ = std::max(0.001f, seconds);
        recalculate();
    }

    void trigger() {
        stage_ = Stage::Attack;
        currentValue_ = 0.0f;
    }

    void release() {
        if (stage_ != Stage::Idle) {
            stage_ = Stage::Release;
            releaseStartValue_ = currentValue_;
        }
    }

    float process() {
        switch (stage_) {
            case Stage::Idle:
                currentValue_ = 0.0f;
                break;

            case Stage::Attack:
                currentValue_ += attackRate_;
                if (currentValue_ >= 1.0f) {
                    currentValue_ = 1.0f;
                    stage_ = Stage::Decay;
                }
                break;

            case Stage::Decay:
                currentValue_ -= decayRate_;
                if (currentValue_ <= sustainLevel_) {
                    currentValue_ = sustainLevel_;
                    stage_ = Stage::Sustain;
                }
                break;

            case Stage::Sustain:
                currentValue_ = sustainLevel_;
                break;

            case Stage::Release:
                currentValue_ -= releaseRate_ * releaseStartValue_;
                if (currentValue_ <= 0.0f) {
                    currentValue_ = 0.0f;
                    stage_ = Stage::Idle;
                }
                break;
        }
        return currentValue_;
    }

    bool isActive() const { return stage_ != Stage::Idle; }
    Stage getStage() const { return stage_; }
    float getValue() const { return currentValue_; }

private:
    void recalculate() {
        if (sampleRate_ <= 0.0f) return;
        attackRate_ = 1.0f / (attackTime_ * sampleRate_);
        decayRate_ = (1.0f - sustainLevel_) / (decayTime_ * sampleRate_);
        releaseRate_ = 1.0f / (releaseTime_ * sampleRate_);
    }

    float sampleRate_ = 48000.0f;
    float attackTime_ = 0.01f;
    float decayTime_ = 0.1f;
    float sustainLevel_ = 0.8f;
    float releaseTime_ = 0.25f;

    float attackRate_ = 0.0f;
    float decayRate_ = 0.0f;
    float releaseRate_ = 0.0f;

    Stage stage_ = Stage::Idle;
    float currentValue_ = 0.0f;
    float releaseStartValue_ = 1.0f;
};

} // namespace dsp
