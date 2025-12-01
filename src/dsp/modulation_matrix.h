// Modulation Matrix - manages LFOs, envelopes, and routing
// Part of PlaitsVST - GPL v3

#pragma once

#include "lfo.h"
#include "mod_envelope.h"

namespace plaits {

enum class ModDestination {
    Harmonics = 0,
    Timbre,
    Morph,
    Cutoff,
    Resonance,
    Lfo1Rate,
    Lfo1Amount,
    Lfo2Rate,
    Lfo2Amount,
    NumDestinations
};

enum class ModSource {
    Lfo1 = 0,
    Lfo2,
    Env1,
    Env2,
    NumSources
};

class ModulationMatrix {
public:
    ModulationMatrix() = default;
    ~ModulationMatrix() = default;

    void Init();
    void Reset();

    // LFO access
    Lfo& GetLfo1() { return lfo1_; }
    Lfo& GetLfo2() { return lfo2_; }
    const Lfo& GetLfo1() const { return lfo1_; }
    const Lfo& GetLfo2() const { return lfo2_; }

    // Envelope access
    ModEnvelope& GetEnv1() { return env1_; }
    ModEnvelope& GetEnv2() { return env2_; }
    const ModEnvelope& GetEnv1() const { return env1_; }
    const ModEnvelope& GetEnv2() const { return env2_; }

    // Routing
    void SetDestination(ModSource source, ModDestination dest);
    void SetAmount(ModSource source, int8_t amount);  // -64 to +63

    ModDestination GetDestination(ModSource source) const;
    int8_t GetAmount(ModSource source) const;

    // Set tempo for LFOs
    void SetTempo(double bpm);

    // Trigger envelopes (call on first note after silence)
    void TriggerEnvelopes();

    // Process all mod sources (call once per audio block)
    void Process(float sample_rate, int num_samples);

    // Get total modulation for a destination (-1 to +1 range)
    float GetModulation(ModDestination dest) const;

    // Get modulated value: applies modulation to base value (0-1 normalized)
    float GetModulatedValue(ModDestination dest, float base_value) const;

    // Get destination name for display
    static const char* GetDestinationName(ModDestination dest);

private:
    Lfo lfo1_;
    Lfo lfo2_;
    ModEnvelope env1_;
    ModEnvelope env2_;

    // Routing: each source has one destination and amount
    ModDestination destinations_[4] = {
        ModDestination::Timbre,     // LFO1 default
        ModDestination::Morph,      // LFO2 default
        ModDestination::Harmonics,  // ENV1 default
        ModDestination::Cutoff      // ENV2 default
    };
    int8_t amounts_[4] = {0, 0, 0, 0};  // All off by default

    // Current modulation values per destination (after processing)
    float mod_values_[static_cast<int>(ModDestination::NumDestinations)] = {0};

    // Cache source outputs
    float source_outputs_[4] = {0};

    double tempo_bpm_ = 120.0;
};

} // namespace plaits
