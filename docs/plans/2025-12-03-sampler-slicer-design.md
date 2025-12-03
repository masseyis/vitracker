# Sampler & Slicer Design

A keyboard-centric sample playback system for Vitracker, adding two new instrument types alongside Plaits.

---

## Overview

Two new instrument types:
- **Sampler** - Chromatic pitched playback of samples (one-shots, melodic)
- **Slicer** - Chopped loops/breaks with slice-per-key triggering

Both share: sample loading, waveform display, filter, ADSR envelope, sends.

### Design Goals

- Clear BPM/pitch display (no confusion about what's happening)
- Keyboard-centric workflow (vim-modal, like M8)
- Independent time-stretch and pitch-shift (offline rendering)
- Auto-detection with manual override options

---

## Data Model

### Instrument Types

```cpp
enum class InstrumentType { Plaits, Sampler, Slicer };
```

### Sample Reference

```cpp
struct SampleRef {
    std::string path;           // File path or library-relative path
    bool embedded = false;      // If true, data stored in project
    std::vector<float> data;    // Embedded sample data (if embedded)
};
```

### Sampler Parameters

```cpp
struct SamplerParams {
    SampleRef sample;
    int rootNote = 60;              // MIDI note that plays at original pitch
    float detectedPitch = 0.0f;     // Auto-detected pitch (Hz), 0 = unknown
    float detectedPitchCents = 0.0f;
    FilterParams filter;
    AdsrParams ampEnvelope;
    float filterEnvAmount = 0.0f;
};
```

### Slicer Parameters

```cpp
struct SlicerParams {
    SampleRef sample;
    float originalBpm = 0.0f;       // Auto-detected or manual
    float targetBpm = 0.0f;         // 0 = sync to project
    bool syncToProject = true;
    int originalBars = 0;           // Manual override for bar count
    std::vector<size_t> slicePoints; // Sample positions
    std::vector<float> warpedData;  // Stretched sample buffer
    FilterParams filter;
    AdsrParams ampEnvelope;
    float filterEnvAmount = 0.0f;
};
```

### ADSR Envelope

```cpp
struct AdsrParams {
    float attack = 0.01f;   // 0.0-1.0
    float decay = 0.5f;     // 0.0-1.0
    float sustain = 0.8f;   // 0.0-1.0
    float release = 0.25f;  // 0.0-1.0
};
```

---

## Sample Storage

### Hybrid Approach

- **Default**: References (file paths) - small project files
- **Optional**: Embed samples in project for portability
- **Library**: `~/.vitracker/samples/` indexed with cached metadata

### Metadata Cache

File: `~/.vitracker/samples/.vitracker-cache.json`

```json
{
  "kick_808.wav": {
    "bpm": null,
    "detectedPitch": 32.70,
    "detectedNote": "C1",
    "cents": 0,
    "duration": 0.4,
    "sampleRate": 44100,
    "channels": 2,
    "analyzedAt": "2024-01-15T10:30:00Z"
  }
}
```

---

## UI Layout

### Sampler Screen

```
┌─────────────────────────────────────────────────────────────┐
│  ██████████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  │  Waveform
│  ██████████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  │  (split view)
│  Zoom: 100%                              Detected: C3 +12c  │
├─────────────────────────────────────────────────────────────┤
│  Sample      kick_808.wav                         [1.2 MB]  │
│  Root Note   C3                                             │
│  ─────────────────────────────────────────────────────────  │
│  Filter      Cutoff      ████████████░░░░  0.75             │
│              Resonance   ████░░░░░░░░░░░░  0.25             │
│  ─────────────────────────────────────────────────────────  │
│  Amp Env     Attack      ██░░░░░░░░░░░░░░  0.01             │
│              Decay       ████████░░░░░░░░  0.50             │
│              Sustain     ██████████████░░  0.80             │
│              Release     ████░░░░░░░░░░░░  0.25             │
│  ─────────────────────────────────────────────────────────  │
│  Sends       Reverb 0.2   Delay 0.0   Chorus 0.0   Drive 0  │
└─────────────────────────────────────────────────────────────┘
```

### Slicer Screen

```
┌─────────────────────────────────────────────────────────────┐
│  ▌0 ████▌1 ████▌2 ████▌3 ████▌4 ████▌5 ████▌6 ████▌7 ████  │  Waveform with
│  ▌  ████▌  ████▌  ████▌  ████▌  ████▌  ████▌  ████▌  ████  │  slice markers
│  Zoom: 50%    Slice: 3/8         120 → 140 BPM  (1.17x)     │
├─────────────────────────────────────────────────────────────┤
│  Sample      break_amen.wav                       [2.4 MB]  │
│  BPM         120 [auto]    Bars  4 [auto]    Sync [ON]      │
│  Chop        Divisions: 8         ──or──  Transients  Lazy  │
│  ─────────────────────────────────────────────────────────  │
│  Filter      Cutoff      ████████████████  1.00             │
│              Resonance   ░░░░░░░░░░░░░░░░  0.00             │
│  ─────────────────────────────────────────────────────────  │
│  Amp Env     Attack      ░░░░░░░░░░░░░░░░  0.00             │
│              Decay       ████████████████  1.00             │
│              Sustain     ████████████████  1.00             │
│              Release     ██░░░░░░░░░░░░░░  0.10             │
│  ─────────────────────────────────────────────────────────  │
│  Sends       Reverb 0.3   Delay 0.1   Chorus 0.0   Drive 0  │
└─────────────────────────────────────────────────────────────┘
```

### Waveform Interaction

Waveform is always visible, controlled by hotkeys (cursor stays in params):

| Key | Action |
|-----|--------|
| `h/l` | Jump between slice points (Slicer) |
| `Shift+h/l` | Smooth scroll waveform |
| `+/-` | Zoom in/out |
| `s` | Add slice at cursor position (Slicer) |
| `d` | Delete current slice (Slicer) |

### Slice Visualization

- Alternating colored regions between slice points
- Numbered markers at boundaries (0, 1, 2...)
- Current slice highlighted

---

## Browser Screen (Screen 7)

```
┌─────────────────────────────────────────────────────────────┐
│  BROWSER                                    ~/Music/Samples │
├─────────────────────────────────────────────────────────────┤
│  [..] Parent Directory                                      │
│  [D] Drums/                                                 │
│  [D] Breaks/                                                │
│  [D] Bass/                                                  │
│  [F] kick_808.wav              120 BPM    C1     0:00.4     │
│► [F] snare_crack.wav           ---        D#2    0:00.2     │
│  [F] hihat_open.wav            ---        ---    0:00.8     │
│  [F] amen_break.wav            136 BPM    4 bar  0:07.2     │
│  [F] think_break.wav           98 BPM     2 bar  0:04.9     │
│                                                             │
├─────────────────────────────────────────────────────────────┤
│  ██████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  Preview      │
│  snare_crack.wav   D#2 +3c   0:00.2   44.1kHz   Stereo      │
└─────────────────────────────────────────────────────────────┘
```

### Browser Keys

| Key | Action |
|-----|--------|
| `Up/Down` | Navigate list |
| `Enter` | Open folder / Load sample |
| `Space` | Preview (audition) sample |
| `Backspace` | Parent directory |
| `/` | Search/filter |
| `~` | Jump to library |
| `Esc` | Return to previous screen |

---

## Commands

### Sample Loading

| Command | Action |
|---------|--------|
| `:load` | Open file browser dialog |
| `:load filename` | Load specific file |

### Slicing (Slicer only)

| Command | Action |
|---------|--------|
| `:chop N` | Chop into N equal divisions |
| `:transients` | Auto-detect and chop on transients |
| `:clear-slices` | Remove all slice points |

### Time Manipulation (Slicer only)

| Command | Action |
|---------|--------|
| `:warp` | Apply time-stretch (render to buffer) |
| `:bpm N` | Override detected BPM |
| `:bars N` | Override detected bar count |

---

## Audio Processing

### Sampler Voice

```cpp
class SamplerVoice {
    const float* sampleData;
    size_t sampleLength;
    double playPosition;
    double playbackRate;    // pow(2.0, (note - rootNote) / 12.0)

    MoogFilter filter;
    AdsrEnvelope ampEnvelope;
    AdsrEnvelope filterEnvelope;
};
```

Pitch shifting via playback rate with JUCE Lagrange interpolation.

### Slicer Voice

```cpp
class SlicerVoice {
    const float* warpedData;    // Pre-stretched sample
    size_t sliceStart;
    size_t sliceEnd;
    size_t playPosition;

    MoogFilter filter;
    AdsrEnvelope ampEnvelope;
    AdsrEnvelope filterEnvelope;
};
```

No pitch shifting - plays slice at original pitch (post-warp).

### Time Stretch

Offline processing using Rubber Band Library (BSD license):

```cpp
class TimeStretchProcessor {
    void process(
        const float* input, size_t inputLength,
        float* output, size_t& outputLength,
        double stretchRatio,
        double pitchRatio = 1.0
    );
};
```

---

## Analysis & Detection

### BPM Detection

- Onset detection → inter-onset intervals → histogram clustering
- Works best on rhythmic material
- Returns primary BPM, confidence, alternative (half/double), estimated bars
- User can override BPM or specify bar count

### Pitch Detection

- Autocorrelation-based (YIN algorithm)
- Returns frequency, MIDI note, cents deviation, confidence
- Works best on monophonic pitched material
- Drums typically return no pitch ("---")

### Transient Detection

- Spectral flux or energy envelope derivative
- Sensitivity parameter (0.0-1.0)
- Always snaps to zero-crossings

---

## Restrictions & Constraints

### Technical

- **Rubber Band dependency**: Adds ~2MB, requires linking
- **Memory usage**: Long samples use significant RAM (no streaming v1)
- **BPM detection**: May return half/double tempo; works best on 4/4 rhythmic material
- **Transient detection**: Best for drums, less reliable on legato content

### Scope (Deferred)

- Multi-sample support (velocity layers, key zones) - future feature
- Real-time warp - offline only for v1
- Disk streaming - load full sample to RAM for v1

### Warnings

- Large file warning for samples > 50MB
- Analysis confidence displayed to user

---

## Implementation Phases

### Phase 1 - Foundation (Sampler)

**New Files:**
- `src/model/SampleRef.h`
- `src/model/SamplerParams.h`
- `src/audio/SamplerVoice.h/cpp`
- `src/audio/SamplerInstrument.h/cpp`
- `src/ui/SamplerScreen.h/cpp`

**Deliverables:**
- Load WAV/AIFF files
- Waveform display (split view)
- Chromatic playback with interpolation
- Filter + ADSR
- Sends integration

### Phase 2 - Slicer

**New Files:**
- `src/model/SlicerParams.h`
- `src/audio/SlicerVoice.h/cpp`
- `src/audio/SlicerInstrument.h/cpp`
- `src/audio/TransientDetector.h/cpp`
- `src/ui/SlicerScreen.h/cpp`

**Deliverables:**
- Slice markers (colored regions, numbers)
- Chop modes: divisions, transients, lazy
- Zero-crossing snap (always on)
- h/l navigation, s/d for add/delete
- Slice-per-key triggering

### Phase 3 - Time Manipulation

**New Files:**
- `src/audio/TimeStretch.h/cpp` (Rubber Band wrapper)
- `src/audio/BpmDetector.h/cpp`
- `src/audio/PitchDetector.h/cpp`

**Deliverables:**
- Auto BPM detection
- Auto pitch detection
- Sync to project tempo
- Manual BPM/bar override
- `:warp` command
- Clear stretch ratio display

### Phase 4 - Browser & Polish

**New Files:**
- `src/ui/BrowserScreen.h/cpp`
- `src/model/SampleLibrary.h/cpp`

**Deliverables:**
- Browser screen (Screen 7)
- Preview with Space
- Metadata columns
- Library at `~/.vitracker/samples/`
- Metadata cache
- Large file warning

---

## Summary

| Feature | Sampler | Slicer |
|---------|---------|--------|
| Playback | Chromatic pitch | Slice-per-key |
| Waveform | Display + zoom/scroll | Display + slice markers |
| Time stretch | No | Yes (offline) |
| BPM detection | No | Yes |
| Pitch detection | Yes | No |
| Chop modes | No | Divisions, transients, lazy |
| Filter | Yes | Yes |
| ADSR | Yes | Yes |
| Sends | Same as Plaits | Same as Plaits |
