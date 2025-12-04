# Phase 3: Time Manipulation & UI Polish

## Overview

This plan covers:
1. BPM detection for samples
2. Pitch detection for auto-repitching sampler to C
3. Proper sample rate conversion (fixing fast playback)
4. UI improvements: consistent color scheme, instrument type selector, BPM display

## Architecture Decisions

### BPM Detection
- Use onset detection via energy difference between frames
- Apply autocorrelation to find tempo period
- Common BPM range: 60-180 BPM
- Store detected BPM in SlicerParams and SamplerParams

### Pitch Detection (YIN Algorithm)
- Well-documented algorithm, no external dependencies
- Detects fundamental frequency of audio
- Convert to MIDI note, calculate offset from C
- Apply pitch offset during playback for sampler

### Sample Rate Conversion
- Calculate ratio: sourceSampleRate / outputSampleRate
- Apply during playback to maintain original pitch
- Use linear interpolation (already in place, just needs correct rate)

### UI Color Scheme (from Screen.h)
- bgColor: 0xff1a1a2e (dark blue)
- fgColor: 0xffeaeaea (light gray)
- highlightColor: 0xff4a4a6e (muted purple)
- cursorColor: 0xff7c7cff (bright purple)
- headerColor: 0xff2a2a4e (darker blue)

---

## Task 1: Add DSP Analysis Utilities

**Files:**
- Create: `src/dsp/AudioAnalysis.h`
- Create: `src/dsp/AudioAnalysis.cpp`

Implement:
- `detectBPM(const float* data, int numSamples, int sampleRate)` - returns BPM
- `detectPitch(const float* data, int numSamples, int sampleRate)` - returns frequency in Hz
- `frequencyToMidiNote(float frequency)` - returns MIDI note number
- `midiNoteToFrequency(int note)` - returns frequency

---

## Task 2: Update SamplerParams and SlicerParams

**Files:**
- Modify: `src/model/SamplerParams.h`
- Modify: `src/model/SlicerParams.h`

Add to both:
- `float detectedBPM = 0.0f`
- `float detectedPitchHz = 0.0f`
- `int detectedRootNote = 60` (MIDI note, default C4)

Add to SlicerParams:
- `float stretchedBPM = 0.0f` (for display)

---

## Task 3: Integrate Analysis into SamplerInstrument

**Files:**
- Modify: `src/audio/SamplerInstrument.cpp`

In `loadSample()`:
- After loading, call pitch detection
- Store detected root note
- Calculate pitch offset from C (note 60, 48, 36, etc.)
- Apply offset to playback rate

---

## Task 4: Integrate Analysis into SlicerInstrument

**Files:**
- Modify: `src/audio/SlicerInstrument.cpp`

In `loadSample()`:
- After loading, call BPM detection
- Store detected BPM
- Calculate stretched BPM based on current playback context

---

## Task 5: Fix Sample Rate Conversion

**Files:**
- Modify: `src/audio/SamplerVoice.cpp`
- Modify: `src/audio/SlicerVoice.cpp`

Fix playback rate calculation:
- `playbackRate = originalSampleRate / outputSampleRate`
- For sampler, also apply pitch offset: `playbackRate *= pitchRatio`

---

## Task 6: Add Instrument Type Selector to InstrumentScreen

**Files:**
- Modify: `src/ui/InstrumentScreen.h`
- Modify: `src/ui/InstrumentScreen.cpp`

Add:
- Type selector row at top: [Plaits] [Sampler] [Slicer]
- Tab key or arrow keys to switch between types
- When type changes, update instrument and UI

---

## Task 7: Update Slicer UI to Match Color Scheme

**Files:**
- Modify: `src/ui/SliceWaveformDisplay.cpp`
- Modify: `src/ui/InstrumentScreen.cpp` (paintSlicerUI)

Changes:
- Use Screen color constants
- Match header style with other screens
- Add BPM display (original and stretched)

---

## Task 8: Update Sampler UI

**Files:**
- Modify: `src/ui/InstrumentScreen.cpp` (sampler section)

Add:
- Show detected pitch and root note
- Show pitch offset being applied
- Match styling with rest of UI

---

## Task 9: Test End-to-End

Test:
1. Load sample in Sampler - verify pitch detection and auto-repitch
2. Load sample in Slicer - verify BPM detection and display
3. Verify playback speed is correct (not fast)
4. Verify type selector works
5. Verify UI colors are consistent

---

## Summary

This phase delivers:
- BPM detection for slicers
- Pitch detection and auto-repitch for samplers
- Correct playback speed via proper sample rate conversion
- Unified UI with instrument type selector
- BPM display in slicer UI
