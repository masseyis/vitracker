#pragma once

namespace audio {

// Transient shaper using dual envelope followers
// Fast follower tracks transients, slow follower tracks sustain
// Difference = transient content, amplified by amount parameter
class TransientShaper {
public:
    void prepare(double sampleRate);
    void setAmount(float amount);  // 0-1
    void process(float& left, float& right);
    void reset();

private:
    double sampleRate_ = 44100.0;
    float amount_ = 0.0f;

    // Envelope followers (stereo)
    float fastEnvL_ = 0.0f, fastEnvR_ = 0.0f;
    float slowEnvL_ = 0.0f, slowEnvR_ = 0.0f;

    // Attack/release coefficients
    float fastAttack_ = 0.0f, fastRelease_ = 0.0f;
    float slowAttack_ = 0.0f, slowRelease_ = 0.0f;
};

} // namespace audio
