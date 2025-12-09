/*
 * Stub header for libMTSClient (MTS-ESP microtuning)
 * We don't need microtuning support, so this provides stub implementations
 */

#pragma once

#include <cmath>

// Stub client type
typedef void MTSClient;

// Stub functions - always use standard 12-TET tuning
inline bool MTS_HasMaster(MTSClient*) { return false; }

inline double MTS_NoteToFrequency(MTSClient*, int note, int /*channel*/) {
    // Standard 12-TET: A4 = 440Hz
    return 440.0 * std::pow(2.0, (note - 69) / 12.0);
}
