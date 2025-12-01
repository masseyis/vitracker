// Modulation Matrix - manages LFOs, envelopes, and routing
// Part of PlaitsVST - GPL v3

#include "modulation_matrix.h"
#include <algorithm>
#include <cmath>

namespace plaits {

static const char* kDestinationNames[] = {
    "HARMNIC", "TIMBRE", "MORPH", "CUTOFF", "RESONAN", "LFO1 RT", "LFO1 AM", "LFO2 RT", "LFO2 AM"
};

void ModulationMatrix::Init()
{
    lfo1_.Init();
    lfo2_.Init();
    env1_.Init();
    env2_.Init();

    // Default routing
    destinations_[0] = ModDestination::Timbre;
    destinations_[1] = ModDestination::Morph;
    destinations_[2] = ModDestination::Harmonics;
    destinations_[3] = ModDestination::Cutoff;

    for (int i = 0; i < 4; ++i) {
        amounts_[i] = 0;
        source_outputs_[i] = 0;
    }

    for (int i = 0; i < static_cast<int>(ModDestination::NumDestinations); ++i) {
        mod_values_[i] = 0;
    }

    tempo_bpm_ = 120.0;
}

void ModulationMatrix::Reset()
{
    lfo1_.Reset();
    lfo2_.Reset();
    env1_.Reset();
    env2_.Reset();

    for (int i = 0; i < static_cast<int>(ModDestination::NumDestinations); ++i) {
        mod_values_[i] = 0;
    }
}

void ModulationMatrix::SetDestination(ModSource source, ModDestination dest)
{
    int idx = static_cast<int>(source);
    if (idx >= 0 && idx < 4) {
        destinations_[idx] = dest;
    }
}

void ModulationMatrix::SetAmount(ModSource source, int8_t amount)
{
    int idx = static_cast<int>(source);
    if (idx >= 0 && idx < 4) {
        amounts_[idx] = std::clamp(amount, static_cast<int8_t>(-64), static_cast<int8_t>(63));
    }
}

ModDestination ModulationMatrix::GetDestination(ModSource source) const
{
    int idx = static_cast<int>(source);
    if (idx >= 0 && idx < 4) {
        return destinations_[idx];
    }
    return ModDestination::Timbre;
}

int8_t ModulationMatrix::GetAmount(ModSource source) const
{
    int idx = static_cast<int>(source);
    if (idx >= 0 && idx < 4) {
        return amounts_[idx];
    }
    return 0;
}

void ModulationMatrix::SetTempo(double bpm)
{
    tempo_bpm_ = bpm;
    lfo1_.SetTempo(bpm);
    lfo2_.SetTempo(bpm);
}

void ModulationMatrix::TriggerEnvelopes()
{
    env1_.Trigger();
    env2_.Trigger();
}

void ModulationMatrix::Process(float sample_rate, int num_samples)
{
    // For efficiency, we process at a reduced control rate
    // Process once per block rather than per-sample
    // This is fine for LFOs and envelopes which are low-frequency

    // Apply any LFO rate modulation before processing LFOs
    // Check if any mod source targets LFO rates
    float lfo1_rate_mod = 0.0f;
    float lfo2_rate_mod = 0.0f;

    // First pass: get envelope outputs (they don't depend on LFOs)
    env1_.Process(sample_rate, num_samples);
    env2_.Process(sample_rate, num_samples);
    source_outputs_[2] = env1_.GetOutput();  // ENV outputs 0-1 (unipolar)
    source_outputs_[3] = env2_.GetOutput();

    // Calculate rate modulation from envelopes if routed
    for (int i = 2; i < 4; ++i) {  // ENV1 and ENV2
        ModDestination dest = destinations_[i];
        float amount = amounts_[i] / 64.0f;  // Normalize to -1 to ~1
        float output = source_outputs_[i];  // Keep unipolar 0-1

        if (dest == ModDestination::Lfo1Rate) {
            lfo1_rate_mod += output * amount;
        } else if (dest == ModDestination::Lfo2Rate) {
            lfo2_rate_mod += output * amount;
        }
    }

    // TODO: Apply rate modulation to LFOs (would need rate scaling method)
    // For now, rate modulation affects the output amount instead

    // Process LFOs
    lfo1_.Process(sample_rate, num_samples);
    lfo2_.Process(sample_rate, num_samples);
    source_outputs_[0] = lfo1_.GetOutput();  // Already -1 to 1
    source_outputs_[1] = lfo2_.GetOutput();

    // Clear modulation values
    for (int i = 0; i < static_cast<int>(ModDestination::NumDestinations); ++i) {
        mod_values_[i] = 0;
    }

    // Calculate total modulation per destination
    for (int i = 0; i < 4; ++i) {
        ModDestination dest = destinations_[i];
        float amount = amounts_[i] / 64.0f;  // Normalize to -1 to ~1

        float output;
        if (i < 2) {
            // LFOs are already bipolar (-1 to 1)
            output = source_outputs_[i];
        } else {
            // Envelopes are unipolar (0 to 1)
            // Keep them unipolar - positive amount = add, negative = subtract
            // This makes +amount push the value UP when envelope is active
            output = source_outputs_[i];
        }

        // Check for amount modulation
        float effective_amount = amount;
        if (i == 0) {  // LFO1
            // Check if anything modulates LFO1 Amount
            for (int j = 0; j < 4; ++j) {
                if (j != i && destinations_[j] == ModDestination::Lfo1Amount) {
                    float mod_amt = amounts_[j] / 64.0f;
                    float mod_out = (j < 2) ? source_outputs_[j] : source_outputs_[j];
                    effective_amount += mod_out * mod_amt * 0.5f;  // Scale modulation of amount
                }
            }
        } else if (i == 1) {  // LFO2
            for (int j = 0; j < 4; ++j) {
                if (j != i && destinations_[j] == ModDestination::Lfo2Amount) {
                    float mod_amt = amounts_[j] / 64.0f;
                    float mod_out = (j < 2) ? source_outputs_[j] : source_outputs_[j];
                    effective_amount += mod_out * mod_amt * 0.5f;
                }
            }
        }

        int dest_idx = static_cast<int>(dest);
        if (dest_idx >= 0 && dest_idx < static_cast<int>(ModDestination::NumDestinations)) {
            mod_values_[dest_idx] += output * effective_amount;
        }
    }

    // Clamp final modulation values
    for (int i = 0; i < static_cast<int>(ModDestination::NumDestinations); ++i) {
        mod_values_[i] = std::clamp(mod_values_[i], -1.0f, 1.0f);
    }
}

float ModulationMatrix::GetModulation(ModDestination dest) const
{
    int idx = static_cast<int>(dest);
    if (idx >= 0 && idx < static_cast<int>(ModDestination::NumDestinations)) {
        return mod_values_[idx];
    }
    return 0.0f;
}

float ModulationMatrix::GetModulatedValue(ModDestination dest, float base_value) const
{
    float mod = GetModulation(dest);
    // Modulation is -1 to 1, scale to full range so max modulation can sweep 0-1
    float modulated = base_value + mod;
    return std::clamp(modulated, 0.0f, 1.0f);
}

const char* ModulationMatrix::GetDestinationName(ModDestination dest)
{
    int idx = static_cast<int>(dest);
    if (idx >= 0 && idx < static_cast<int>(ModDestination::NumDestinations)) {
        return kDestinationNames[idx];
    }
    return "???";
}

} // namespace plaits
