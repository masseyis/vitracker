// Simple AD envelope for PlaitsVST
// PlaitsVST: MIT License

#include "envelope.h"
#include <algorithm>

void Envelope::Init(double sampleRate)
{
    sampleRate_ = sampleRate;
    stage_ = Stage::Idle;
    value_ = 0.0f;
}

void Envelope::Trigger(float attackMs, float decayMs)
{
    // Calculate increments based on sample rate
    // Attack: go from 0 to 1 in attackMs milliseconds
    float attackSamples = std::max(1.0f, static_cast<float>(sampleRate_) * attackMs / 1000.0f);
    attackIncrement_ = 1.0f / attackSamples;

    // Decay: go from 1 to 0 in decayMs milliseconds
    float decaySamples = std::max(1.0f, static_cast<float>(sampleRate_) * decayMs / 1000.0f);
    decayDecrement_ = 1.0f / decaySamples;

    stage_ = Stage::Attack;
    value_ = 0.0f;
}

float Envelope::Process()
{
    switch (stage_) {
        case Stage::Attack:
            value_ += attackIncrement_;
            if (value_ >= 1.0f) {
                value_ = 1.0f;
                stage_ = Stage::Decay;
            }
            break;

        case Stage::Decay:
            value_ -= decayDecrement_;
            if (value_ <= 0.0f) {
                value_ = 0.0f;
                stage_ = Stage::Idle;
            }
            break;

        case Stage::Idle:
        default:
            break;
    }

    return value_;
}
