#pragma once

#include <vector>
#include <cstddef>
#include "SamplerParams.h"  // Reuse SampleRef, AdsrParams, FilterParams

namespace model {

enum class ChopMode {
    Divisions,      // Equal divisions
    Transients,     // Auto-detect transients (Phase 3)
    Manual          // User-placed slices
};

struct SlicerParams {
    SampleRef sample;

    // Slice points (sample positions)
    std::vector<size_t> slicePoints;  // Start position of each slice
    int currentSlice = 0;             // Currently selected slice for UI

    // Chop settings
    ChopMode chopMode = ChopMode::Divisions;
    int numDivisions = 8;             // For Divisions mode

    // BPM detection and override
    float detectedBPM = 0.0f;         // Auto-detected sample BPM, 0 = unknown
    float originalBPM = 0.0f;         // User override for original BPM (0 = use detected)
    float stretchedBPM = 0.0f;        // BPM at current playback rate
    int detectedBars = 0;             // Auto-calculated number of bars
    int bars = 0;                     // User override for bar count (0 = use detected)
    float pitchHz = 0.0f;             // Detected pitch (optional)
    int detectedRootNote = 60;        // MIDI note, default C4

    // Audio processing (same as Sampler)
    FilterParams filter;
    AdsrParams ampEnvelope;
    float filterEnvAmount = 0.0f;
    AdsrParams filterEnvelope;
};

} // namespace model
