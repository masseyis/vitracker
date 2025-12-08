#pragma once

#include <cstdint>
#include <array>

namespace model {

// DX7 preset parameters - just stores the preset index
// The actual patch data is managed by the DX7PresetBank and DX7Instrument
struct DXParams {
    int presetIndex = -1;  // -1 = no preset selected (uses init patch)
    int polyphony = 16;    // 1-16 voices
};

} // namespace model
