// Modulation Envelope - AD envelope for modulation
// Part of PlaitsVST - GPL v3

#pragma once

#include <cstdint>

namespace plaits {

class ModEnvelope {
public:
    ModEnvelope() = default;
    ~ModEnvelope() = default;

    void Init();
    void Reset();

    // Set times in milliseconds
    void SetAttack(uint16_t attack_ms) { attack_ms_ = attack_ms; }
    void SetDecay(uint16_t decay_ms) { decay_ms_ = decay_ms; }

    uint16_t GetAttack() const { return attack_ms_; }
    uint16_t GetDecay() const { return decay_ms_; }

    // Trigger the envelope (call on first note)
    void Trigger();

    // Process and return current value (0 to 1)
    // num_samples: number of samples to advance (for block-based processing)
    float Process(float sample_rate, int num_samples = 1);

    // Get current output without advancing
    float GetOutput() const { return output_; }

    // Check if envelope has completed
    bool IsComplete() const { return stage_ == Stage::Idle; }

    // Check if envelope is active
    bool IsActive() const { return stage_ != Stage::Idle; }

private:
    enum class Stage {
        Idle,
        Attack,
        Decay
    };

    uint16_t attack_ms_ = 10;
    uint16_t decay_ms_ = 200;

    Stage stage_ = Stage::Idle;
    float output_ = 0.0f;
    float phase_ = 0.0f;
};

} // namespace plaits
