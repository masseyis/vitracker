#pragma once

#include <cstdint>

namespace model {

// Modulation destinations for Sampler/Slicer
enum class SamplerModDest {
    Volume = 0,     // Output volume
    Pitch,          // Pitch shift
    Cutoff,         // Filter cutoff
    Resonance,      // Filter resonance
    Pan,            // Pan position
    Lfo1Rate,       // LFO1 rate cross-mod
    Lfo1Amount,     // LFO1 amount cross-mod
    Lfo2Rate,       // LFO2 rate cross-mod
    Lfo2Amount,     // LFO2 amount cross-mod
    NumDestinations
};

// LFO parameters for Sampler/Slicer modulation
struct SamplerLfoParams {
    int rateIndex = 4;      // LfoRateDivision index (default 1/4 note)
    int shapeIndex = 0;     // LfoShape index (default triangle)
    int destIndex = 2;      // SamplerModDest index (default cutoff)
    int8_t amount = 0;      // -64 to +63
};

// Modulation envelope parameters (AD envelope) for Sampler/Slicer
struct SamplerModEnvParams {
    float attack = 0.01f;   // seconds (0.001 - 2.0)
    float decay = 0.2f;     // seconds (0.001 - 10.0)
    int destIndex = 2;      // SamplerModDest index (default cutoff)
    int8_t amount = 0;      // -64 to +63
};

// FX send levels (per-instrument) for Sampler/Slicer
struct SamplerFxSendParams {
    float reverb = 0.0f;    // 0.0-1.0
    float delay = 0.0f;     // 0.0-1.0
    float chorus = 0.0f;    // 0.0-1.0
    float drive = 0.0f;     // 0.0-1.0
};

// Combined modulation parameters for Sampler and Slicer
struct SamplerModulationParams {
    SamplerLfoParams lfo1;
    SamplerLfoParams lfo2;
    SamplerModEnvParams env1;
    SamplerModEnvParams env2;
    SamplerFxSendParams fxSends;
};

// Helper to get destination name
inline const char* getModDestName(SamplerModDest dest) {
    switch (dest) {
        case SamplerModDest::Volume: return "VOLUME";
        case SamplerModDest::Pitch: return "PITCH";
        case SamplerModDest::Cutoff: return "CUTOFF";
        case SamplerModDest::Resonance: return "RESON";
        case SamplerModDest::Pan: return "PAN";
        case SamplerModDest::Lfo1Rate: return "LFO1 RT";
        case SamplerModDest::Lfo1Amount: return "LFO1 AM";
        case SamplerModDest::Lfo2Rate: return "LFO2 RT";
        case SamplerModDest::Lfo2Amount: return "LFO2 AM";
        default: return "???";
    }
}

} // namespace model
