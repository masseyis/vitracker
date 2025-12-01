// Simple AD envelope for PlaitsVST
// PlaitsVST: MIT License

#pragma once

#include <cstdint>

class Envelope {
public:
    Envelope() = default;
    ~Envelope() = default;

    void Init(double sampleRate);
    void Trigger(float attackMs, float decayMs);
    float Process();  // Returns envelope value 0.0-1.0

    bool active() const { return stage_ != Stage::Idle; }
    bool done() const { return stage_ == Stage::Idle; }

private:
    enum class Stage { Idle, Attack, Decay };

    Stage stage_ = Stage::Idle;
    float value_ = 0.0f;
    float attackIncrement_ = 0.0f;
    float decayDecrement_ = 0.0f;
    double sampleRate_ = 48000.0;
};
