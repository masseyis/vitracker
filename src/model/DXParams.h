#pragma once

#include <cstdint>
#include <array>

namespace model {

// DX7 preset parameters
// Stores both the preset index (for UI display) and the actual patch data (for audio engine)
struct DXParams {
    int presetIndex = -1;           // -1 = no preset selected (uses init patch)
    int polyphony = 16;             // 1-16 voices
    std::array<uint8_t, 128> packedPatch = {};  // 128-byte DX7 packed patch data
};

} // namespace model
