// Modulation Matrix for Sampler and Slicer instruments

#include "sampler_modulation.h"
#include <cmath>
#include <algorithm>

namespace dsp {

void SamplerModulationMatrix::init() {
    lfo1_.Init();
    lfo2_.Init();
    env1_.Init();
    env2_.Init();

    // Set default LFO params
    lfo1_.SetRate(plaits::LfoRateDivision::Div_1_4);
    lfo1_.SetShape(plaits::LfoShape::Triangle);
    lfo2_.SetRate(plaits::LfoRateDivision::Div_1_4);
    lfo2_.SetShape(plaits::LfoShape::Triangle);

    // Set default envelope params
    env1_.SetAttack(10);   // 10ms
    env1_.SetDecay(200);   // 200ms
    env2_.SetAttack(10);
    env2_.SetDecay(500);

    reset();
}

void SamplerModulationMatrix::reset() {
    lfo1_.Reset();
    lfo2_.Reset();
    env1_.Reset();
    env2_.Reset();

    for (int i = 0; i < kNumDestinations; ++i) {
        modValues_[i] = 0.0f;
    }
    for (int i = 0; i < kNumSources; ++i) {
        sourceOutputs_[i] = 0.0f;
    }
}

void SamplerModulationMatrix::setDestination(int sourceIndex, int destIndex) {
    if (sourceIndex >= 0 && sourceIndex < kNumSources) {
        destinations_[sourceIndex] = std::clamp(destIndex, 0, kNumDestinations - 1);
    }
}

void SamplerModulationMatrix::setAmount(int sourceIndex, int8_t amount) {
    if (sourceIndex >= 0 && sourceIndex < kNumSources) {
        amounts_[sourceIndex] = std::clamp(amount, static_cast<int8_t>(-64), static_cast<int8_t>(63));
    }
}

int SamplerModulationMatrix::getDestinationIndex(int sourceIndex) const {
    if (sourceIndex >= 0 && sourceIndex < kNumSources) {
        return destinations_[sourceIndex];
    }
    return 0;  // Volume
}

int8_t SamplerModulationMatrix::getAmount(int sourceIndex) const {
    if (sourceIndex >= 0 && sourceIndex < kNumSources) {
        return amounts_[sourceIndex];
    }
    return 0;
}

void SamplerModulationMatrix::setTempo(double bpm) {
    tempoBpm_ = bpm;
    lfo1_.SetTempo(bpm);
    lfo2_.SetTempo(bpm);
}

void SamplerModulationMatrix::triggerEnvelopes() {
    env1_.Trigger();
    env2_.Trigger();
}

void SamplerModulationMatrix::process(float sampleRate, int numSamples) {
    // Process all sources
    sourceOutputs_[0] = lfo1_.Process(sampleRate, numSamples);  // -1 to +1
    sourceOutputs_[1] = lfo2_.Process(sampleRate, numSamples);  // -1 to +1
    sourceOutputs_[2] = env1_.Process(sampleRate, numSamples);  // 0 to +1
    sourceOutputs_[3] = env2_.Process(sampleRate, numSamples);  // 0 to +1

    // Clear modulation values
    for (int i = 0; i < kNumDestinations; ++i) {
        modValues_[i] = 0.0f;
    }

    // Accumulate modulation from each source to its destination
    for (int src = 0; src < kNumSources; ++src) {
        int destIdx = destinations_[src];
        if (destIdx >= 0 && destIdx < kNumDestinations) {
            // Convert amount (-64 to +63) to -1 to +1 range
            float normalizedAmount = static_cast<float>(amounts_[src]) / 64.0f;

            // For envelopes (indices 2 and 3), scale 0-1 output to -1 to +1 for bipolar mod
            float sourceValue = sourceOutputs_[src];
            if (src >= 2) {
                // Envelope outputs are 0-1, convert to -0.5 to +0.5 range (centered)
                // So positive amount = envelope adds, negative = envelope subtracts
                sourceValue = sourceValue - 0.5f;
            }

            modValues_[destIdx] += sourceValue * normalizedAmount;
        }
    }

    // Clamp all mod values to -1 to +1
    for (int i = 0; i < kNumDestinations; ++i) {
        modValues_[i] = std::clamp(modValues_[i], -1.0f, 1.0f);
    }
}

float SamplerModulationMatrix::getModulation(int destIndex) const {
    if (destIndex >= 0 && destIndex < kNumDestinations) {
        return modValues_[destIndex];
    }
    return 0.0f;
}

float SamplerModulationMatrix::getModulatedValue(int destIndex, float baseValue) const {
    float mod = getModulation(destIndex);
    // Apply modulation: mod of +1 doubles the value, mod of -1 zeros it
    // This gives more musical results for most parameters
    float result = baseValue + mod * baseValue;
    return std::clamp(result, 0.0f, 1.0f);
}

} // namespace dsp
