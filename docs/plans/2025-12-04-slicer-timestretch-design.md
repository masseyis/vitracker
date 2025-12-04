# Slicer Time-Stretch and Chop Modes Design

**Date:** 2025-12-04
**Status:** Approved

## Overview

Enhance the Slicer instrument with:
1. Time-stretching with BPM/speed control and optional repitch
2. Three chop modes: Lazy, Divisions, Transients
3. Fix visual mode selection highlight in PatternScreen

## Data Model

### SlicerParams additions

```cpp
// Time-stretch settings
bool repitch = false;              // true = pitch changes with speed, false = RubberBand
float speed = 1.0f;                // Playback speed multiplier
float targetBPM = 120.0f;          // Desired output BPM (editable)
int pitchSemitones = 0;            // Pitch shift in semitones

// Track which values were last edited for 3-way dependency
enum class LastEdited { OriginalBPM, TargetBPM, Speed };
LastEdited lastEdited1 = LastEdited::OriginalBPM;
LastEdited lastEdited2 = LastEdited::TargetBPM;
```

### Three-Way Dependency Model

Three control groups with paired internal dependencies:

1. **Original BPM / Original Bars** - Edit one, other recalculates
2. **Target BPM** - Independent, editable
3. **Speed / Pitch** - Paired when repitch=true

**Relationships:**
- `Original BPM = 60.0 * sampleRate * bars * 4 / numSamples`
- `Speed = targetBPM / originalBPM`
- `Pitch (semitones) = 12 * log2(speed)` (when repitch=true)

**Last-two-edits rule:** Track which two groups were most recently edited. The third group's value is calculated from the other two.

### Buffer Rendering

- **repitch=true:** No buffer rendering. Voice plays at different sample rate.
- **repitch=false:** Pre-render time-stretched buffer using RubberBand library.

Buffer is regenerated (not saved) on project load.

## UI Layout

### SlicerRowType enum

```cpp
enum class SlicerRowType {
    ChopMode,       // Lazy / Divisions / Transients
    OriginalBars,   // Editable
    OriginalBPM,    // Editable
    TargetBPM,      // Editable
    Speed,          // Editable (x0.5, x1, x2, etc.)
    Pitch,          // Editable when repitch=true
    Repitch,        // Toggle ON/OFF
    Polyphony,      // 1-8 voices
    Volume,
    Pan,
    NumSlicerRows
};
```

### Row Behaviors

- **ChopMode:** Left/right cycles modes. Sub-controls shown based on mode:
  - Divisions: shows division count (editable)
  - Transients: shows sensitivity 0-100% (editable)
  - Lazy: no sub-control
- **Speed:** Display as "x0.67", "x1.5", etc.
- **Pitch:** Display as "+5st", "-7st". Greyed when repitch=false.

## Chop Modes

### Divisions (existing)
Equal slices based on `numDivisions`. Triggered by `:chop N` command or editing division count in UI.

### Transients (new)
Auto-detect percussive hits using onset detection:
1. Calculate smoothed envelope
2. Find peaks in envelope derivative
3. Apply threshold based on sensitivity
4. Enforce minimum distance between transients (50ms)
5. Snap to zero-crossings

```cpp
// dsp/AudioAnalysis.h
static std::vector<size_t> detectTransients(
    const float* samples,
    size_t numSamples,
    int sampleRate,
    float sensitivity = 0.5f
);
```

### Lazy Chop (new)
Real-time slice marking while sample plays:

**Keys:**
- `Enter` - Play current slice (whole sample initially)
- `c` - Add slice point at current playhead position
- `Shift+C` or `:clearslices` - Clear all slices (with confirmation)

**Flow:**
1. Select Lazy mode in ChopMode row
2. Press Enter to play
3. Press `c` at each point where you want a slice
4. Slice points added in real-time, snapped to zero-crossings

## Visual Mode Fix

### Problem
Selection in PatternScreen works functionally but isn't visible.

### Solution
Draw selection highlight in `PatternScreen::paint()`:

```cpp
if (hasSelection_) {
    int minT = selection_.minTrack();
    int maxT = selection_.maxTrack();
    int minR = selection_.minRow();
    int maxR = selection_.maxRow();

    // Calculate pixel bounds
    // Draw semi-transparent highlight
    g.setColour(juce::Colours::cyan.withAlpha(0.3f));
    g.fillRect(selectionBounds);
    g.setColour(juce::Colours::cyan);
    g.drawRect(selectionBounds, 1);
}
```

## Implementation Order

1. Visual mode selection highlight (quick fix)
2. SlicerParams data model updates
3. Three-way dependency calculation logic
4. UI rows for new parameters
5. Lazy chop mode (playhead tracking + 'c' key)
6. Transient detection algorithm
7. RubberBand integration for pitch-preserved stretching
8. Persistence (save/load new params, regenerate buffers on load)

## Dependencies

- **RubberBand library** - For pitch-preserved time-stretching
  - Add via CMake FetchContent or system package
  - BSD-style license, compatible with project
