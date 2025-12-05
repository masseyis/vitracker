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

// All available chord types
static const std::vector<ChordType> kChordTypes = {
    // Triads (3 notes)
    {"maj",     {0, 4, 7}},
    {"min",     {0, 3, 7}},
    {"dim",     {0, 3, 6}},
    {"aug",     {0, 4, 8}},
    // Sevenths (4 notes)
    {"7",       {0, 4, 7, 10}},
    {"maj7",    {0, 4, 7, 11}},
    {"min7",    {0, 3, 7, 10}},
    {"dim7",    {0, 3, 6, 9}},
    {"m7b5",    {0, 3, 6, 10}},
    // Extended (5+ notes)
    {"9",       {0, 4, 7, 10, 14}},
    {"maj9",    {0, 4, 7, 11, 14}},
    {"min9",    {0, 3, 7, 10, 14}},
    {"11",      {0, 4, 7, 10, 14, 17}},
    {"13",      {0, 4, 7, 10, 14, 17, 21}},
    // Sus/Add (3-4 notes)
    {"sus2",    {0, 2, 7}},
    {"sus4",    {0, 5, 7}},
    {"add9",    {0, 4, 7, 14}},
};

// Major scale degrees (for scale-locked chord selection)
static const std::vector<ScaleDegree> kMajorScaleDegrees = {
    {"I",    0,  false},
    {"ii",   2,  true},
    {"iii",  4,  true},
    {"IV",   5,  false},
    {"V",    7,  false},
    {"vi",   9,  true},
    {"vii",  11, true},
};

// Minor scale degrees
static const std::vector<ScaleDegree> kMinorScaleDegrees = {
    {"i",    0,  true},
    {"ii",   2,  true},
    {"III",  3,  false},
    {"iv",   5,  true},
    {"v",    7,  true},
    {"VI",   8,  false},
    {"VII",  10, false},
};

} // namespace ui
