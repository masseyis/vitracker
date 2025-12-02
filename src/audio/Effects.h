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
    static constexpr int NUM_ALLPASS = 2;

    std::array<std::vector<float>, NUM_COMBS> combBuffersL_;
    std::array<std::vector<float>, NUM_COMBS> combBuffersR_;
    std::array<int, NUM_COMBS> combIndices_{};
    std::array<float, NUM_COMBS> combFilters_{};

    std::array<std::vector<float>, NUM_ALLPASS> allpassBuffersL_;
    std::array<std::vector<float>, NUM_ALLPASS> allpassBuffersR_;
    std::array<int, NUM_ALLPASS> allpassIndices_{};

    float size_ = 0.5f;
    float damping_ = 0.5f;
    float mix_ = 0.3f;
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
    float mix_ = 0.3f;
};

// Soft saturation drive
class Drive
{
public:
    void setParams(float gain, float tone);
    void process(float& left, float& right);

private:
    float gain_ = 0.5f;
    float tone_ = 0.5f;
    float lpState_ = 0.0f;
};

// Sidechain compressor
class Sidechain
{
public:
    void init(double sampleRate);
    void trigger();  // Called when kick/trigger instrument plays
    void process(float& left, float& right, float duckAmount);

private:
    double sampleRate_ = 48000.0;
    float envelope_ = 0.0f;
    float attack_ = 0.001f;
    float release_ = 0.2f;
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

    // Process audio with send levels
    void process(float& left, float& right,
                 float reverbSend, float delaySend, float chorusSend,
                 float driveSend, float sidechainDuck);
};

} // namespace audio
