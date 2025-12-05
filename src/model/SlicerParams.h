#pragma once

#include <vector>
#include <cstddef>
#include "SamplerParams.h"  // Reuse SampleRef, AdsrParams, FilterParams

namespace model {

enum class ChopMode {
    Lazy,           // Real-time slice marking while playing
    Divisions,      // Equal divisions
    Transients      // Auto-detect transients
};

// For three-way dependency tracking (Original BPM, Target BPM, Speed)
enum class SlicerLastEdited {
    OriginalBPM,    // Also represents Bars (they're paired)
    TargetBPM,
    Speed           // Also represents Pitch when repitch=true (they're paired)
};

struct SlicerParams {
    SampleRef sample;

    // Slice points (sample positions)
    std::vector<size_t> slicePoints;  // Start position of each slice
    int currentSlice = 0;             // Currently selected slice for UI

    // Chop settings
    ChopMode chopMode = ChopMode::Divisions;
    int numDivisions = 8;             // For Divisions mode
    float transientSensitivity = 0.5f; // For Transients mode (0.0-1.0)

    // Polyphony (1 = choke/mono, higher = polyphonic)
    int polyphony = 1;                // Default to 1 (slices choke each other)

    // Time-stretch settings
    bool repitch = false;             // true = pitch changes with speed, false = use RubberBand
    float speed = 1.0f;               // Playback speed multiplier (x0.5, x1, x2, etc.)
    float targetBPM = 120.0f;         // Desired output BPM (editable)
    int pitchSemitones = 0;           // Pitch shift in semitones (calculated when repitch=true)

    // Three-way dependency tracking
    SlicerLastEdited lastEdited1 = SlicerLastEdited::OriginalBPM;
    SlicerLastEdited lastEdited2 = SlicerLastEdited::TargetBPM;

    // BPM detection and override (Original BPM / Bars pair)
    float detectedBPM = 0.0f;         // Auto-detected sample BPM, 0 = unknown
    float originalBPM = 0.0f;         // User override for original BPM (0 = use detected)
    int detectedBars = 0;             // Auto-calculated number of bars
    int bars = 0;                     // User override for bar count (0 = use detected)

    // Pitch detection (informational)
    float pitchHz = 0.0f;             // Detected pitch (optional)
    int detectedRootNote = 60;        // MIDI note, default C4

    // Helper to get effective original BPM (user override or detected)
    float getOriginalBPM() const {
        return originalBPM > 0.0f ? originalBPM : detectedBPM;
    }

    // Helper to get effective bars (user override or detected)
    int getBars() const {
        return bars > 0 ? bars : detectedBars;
    }

    // Audio processing (same as Sampler)
    FilterParams filter;
    AdsrParams ampEnvelope;
    float filterEnvAmount = 0.0f;
    AdsrParams filterEnvelope;

    // Modulation matrix (LFOs, mod envelopes, FX sends)
    SamplerModulationParams modulation;
};

} // namespace model
