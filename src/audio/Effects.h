#pragma once

#include <array>
#include <cmath>
#include <vector>

namespace audio {

// Simple reverb using Schroeder algorithm
class Reverb
{
public:
    void init(double sampleRate);
    void setParams(float size, float damping, float mix);
    void process(float& left, float& right);

private:
    static constexpr int NUM_COMBS = 4;
    static constexpr int NUM_ALLPASS = 4;  // More allpass stages for better diffusion

    std::array<std::vector<float>, NUM_COMBS> combBuffersL_;
    std::array<std::vector<float>, NUM_COMBS> combBuffersR_;
    std::array<int, NUM_COMBS> combIndices_{};
    std::array<float, NUM_COMBS> combFilters_{};

    std::array<std::vector<float>, NUM_ALLPASS> allpassBuffersL_;
    std::array<std::vector<float>, NUM_ALLPASS> allpassBuffersR_;
    std::array<int, NUM_ALLPASS> allpassIndices_{};

    float size_ = 0.5f;
    float damping_ = 0.5f;
};

// Tempo-synced delay
class Delay
{
public:
    void init(double sampleRate);
    void setParams(float time, float feedback, float mix);
    void setTempo(float bpm);
    void process(float& left, float& right);

private:
    std::vector<float> bufferL_;
    std::vector<float> bufferR_;
    int writeIndex_ = 0;
    int delaySamples_ = 0;

    double sampleRate_ = 48000.0;
    float time_ = 0.5f;  // 0-1 maps to 1/16 - 1/1
    float feedback_ = 0.4f;
    float mix_ = 0.3f;
    float bpm_ = 120.0f;
};

// Simple chorus
class Chorus
{
public:
    void init(double sampleRate);
    void setParams(float rate, float depth, float mix);
    void process(float& left, float& right);

private:
    std::vector<float> bufferL_;
    std::vector<float> bufferR_;
    int writeIndex_ = 0;
    float phase_ = 0.0f;

    double sampleRate_ = 48000.0;
    float rate_ = 0.5f;
    float depth_ = 0.5f;
};

// Tube-style saturation drive
class Drive
{
public:
    void init(double sampleRate);
    void setParams(float gain, float tone);
    void process(float& left, float& right);

private:
    double sampleRate_ = 48000.0;
    float gain_ = 0.5f;
    float tone_ = 0.5f;
    float lpStateL_ = 0.0f;
    float lpStateR_ = 0.0f;
    float hpStateL_ = 0.0f;
    float hpStateR_ = 0.0f;
};

// DJ-style bipolar filter: negative = lowpass, positive = highpass, 0 = bypass
class DJFilter
{
public:
    void init(double sampleRate);
    void setPosition(float position);  // -1.0 to +1.0
    void process(float& left, float& right);

    float getPosition() const { return position_; }

private:
    double sampleRate_ = 48000.0;
    float position_ = 0.0f;  // -1 = full LP, 0 = bypass, +1 = full HP

    // Filter state for 2-pole filters
    float lpStateL_[2] = {0, 0};
    float lpStateR_[2] = {0, 0};
    float hpStateL_[2] = {0, 0};
    float hpStateR_[2] = {0, 0};
};

// Simple brickwall limiter for master bus
class Limiter
{
public:
    void init(double sampleRate);
    void setParams(float threshold, float release);
    void process(float& left, float& right);

    float getGainReduction() const { return gainReduction_; }

private:
    double sampleRate_ = 48000.0;
    float threshold_ = 0.95f;  // Just below clipping
    float release_ = 0.1f;     // Release time in seconds
    float envelope_ = 0.0f;
    float gainReduction_ = 0.0f;  // For metering
};

// Sidechain compressor - follows audio level from source instrument
class Sidechain
{
public:
    void init(double sampleRate);
    void setParams(float attack, float release, float ratio);

    // Feed in the source signal level each sample
    void feedSource(float sourceLevel);

    // Process audio with ducking based on source envelope
    void process(float& left, float& right);

    // Get current envelope for visualization
    float getEnvelope() const { return envelope_; }

private:
    double sampleRate_ = 48000.0;
    float envelope_ = 0.0f;
    float attack_ = 0.005f;    // 5ms attack
    float release_ = 0.2f;     // 200ms release
    float ratio_ = 0.7f;       // How much to duck (0=none, 1=full)
};

// All effects combined
class EffectsProcessor
{
public:
    void init(double sampleRate);
    void setTempo(float bpm);

    Reverb reverb;
    Delay delay;
    Chorus chorus;
    Drive drive;
    Sidechain sidechain;
    DJFilter djFilter;
    Limiter limiter;

    // Process audio with send levels (sidechain handled separately)
    void process(float& left, float& right,
                 float reverbSend, float delaySend, float chorusSend,
                 float driveSend);

    // Process master bus effects (DJ filter + limiter)
    void processMaster(float& left, float& right);
};

} // namespace audio
