#pragma once

namespace model {

// Filter parameters
struct FilterParams
{
    float cutoff = 1.0f;      // 0.0-1.0 (maps to 20Hz-20kHz)
    float resonance = 0.0f;   // 0.0-1.0
};

} // namespace model
