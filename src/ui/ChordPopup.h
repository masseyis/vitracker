#pragma once

#include <JuceHeader.h>
#include <string>
#include <vector>

namespace ui {

// Chord type definition with intervals from root
struct ChordType {
    std::string name;           // Display name: "maj", "min7", etc.
    std::vector<int> intervals; // Semitones from root: {0, 4, 7} for major
};

// Current chord selection state
struct ChordSelection {
    int degree = 0;         // 0-6 (I through vii)
    int typeIndex = 0;      // Index into chord types
    int inversion = 0;      // 0 = root position, 1 = 1st inversion, etc.
    int rootNote = 60;      // MIDI note of root (middle C default)
    std::vector<int> notes; // Resulting MIDI notes after inversion
};

// Scale degree info for display
struct ScaleDegree {
    std::string roman;      // "I", "ii", "iii", etc.
    int semitonesFromRoot;  // 0, 2, 4, 5, 7, 9, 11 for major
    bool isMinor;           // true for ii, iii, vi, vii°
};

} // namespace ui
