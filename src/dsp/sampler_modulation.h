// Modulation Matrix for Sampler and Slicer instruments
// Handles LFOs, mod envelopes, and routing to Sampler-specific destinations

#pragma once

#include "lfo.h"
#include "mod_envelope.h"

namespace dsp {

class SamplerModulationMatrix {
public:
    SamplerModulationMatrix() = default;
    ~SamplerModulationMatrix() = default;

    void init();
    void reset();

    // LFO access
    plaits::Lfo& getLfo1() { return lfo1_; }
    plaits::Lfo& getLfo2() { return lfo2_; }
    const plaits::Lfo& getLfo1() const { return lfo1_; }
    const plaits::Lfo& getLfo2() const { return lfo2_; }

    // Envelope access
    plaits::ModEnvelope& getEnv1() { return env1_; }
    plaits::ModEnvelope& getEnv2() { return env2_; }
    const plaits::ModEnvelope& getEnv1() const { return env1_; }
    const plaits::ModEnvelope& getEnv2() const { return env2_; }

    // Routing
    void setDestination(int sourceIndex, int destIndex);
    void setAmount(int sourceIndex, int8_t amount);  // -64 to +63

    int getDestinationIndex(int sourceIndex) const;
    int8_t getAmount(int sourceIndex) const;

    // Set tempo for LFOs
    void setTempo(double bpm);

    // Trigger envelopes (call on first note after silence)
    void triggerEnvelopes();

    // Process all mod sources (call once per audio block)
    void process(float sampleRate, int numSamples);

    // Get total modulation for a destination (-1 to +1 range)
    float getModulation(int destIndex) const;

    // Get modulated value: applies modulation to base value (0-1 normalized)
    float getModulatedValue(int destIndex, float baseValue) const;

private:
    static constexpr int kNumSources = 4;  // LFO1, LFO2, ENV1, ENV2
    static constexpr int kNumDestinations = 9;  // SamplerModDest::NumDestinations

    plaits::Lfo lfo1_;
    plaits::Lfo lfo2_;
    plaits::ModEnvelope env1_;
    plaits::ModEnvelope env2_;

    // Routing: each source has one destination and amount
    int destinations_[kNumSources] = {2, 0, 2, 0};  // Cutoff, Volume, Cutoff, Volume
    int8_t amounts_[kNumSources] = {0, 0, 0, 0};    // All off by default

    // Current modulation values per destination (after processing)
    float modValues_[kNumDestinations] = {0};

    // Cache source outputs
    float sourceOutputs_[kNumSources] = {0};

    double tempoBpm_ = 120.0;
};

} // namespace dsp
