#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "FilterParams.h"

namespace model {

struct SampleRef {
    std::string path;
    bool embedded = false;
    std::vector<float> embeddedData;
    int numChannels = 0;
    int sampleRate = 0;
    size_t numSamples = 0;
};

struct AdsrParams {
    float attack = 0.01f;   // seconds (0.001 - 10.0)
    float decay = 0.1f;     // seconds (0.001 - 10.0)
    float sustain = 0.8f;   // level (0.0 - 1.0)
    float release = 0.25f;  // seconds (0.001 - 10.0)
};

struct SamplerParams {
    SampleRef sample;
    int rootNote = 60;                  // MIDI note for original pitch
    float detectedPitchHz = 0.0f;       // Auto-detected pitch, 0 = unknown
    float detectedPitchCents = 0.0f;    // Cents deviation from nearest note
    int detectedMidiNote = -1;          // -1 = unknown
    int targetRootNote = 60;            // Note to repitch to (nearest C)
    float pitchRatio = 1.0f;            // Playback ratio for auto-repitch
    bool autoRepitchEnabled = true;     // Enable auto-repitch to C

    FilterParams filter;
    AdsrParams ampEnvelope;
    float filterEnvAmount = 0.0f;       // -1.0 to +1.0
    AdsrParams filterEnvelope;
};

} // namespace model
