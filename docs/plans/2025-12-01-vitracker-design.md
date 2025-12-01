# Vitracker Design Document

**Date:** 2025-12-01
**Status:** Approved

## Overview

Vitracker is a keyboard-first tracker DAW inspired by the Dirtywave M8, built for desktop (macOS, Windows, Linux). It uses vim-style modal editing and the Plaits synth engine for sound generation.

### Design Goals

- Fast keyboard navigation (M8-style workflow)
- Vim-style modal editing (Normal, Edit, Visual, Command modes)
- Named chains/patterns/instruments instead of hex numbers
- 16 tracks (vs M8's 8)
- Intuitive groove templates instead of raw tick manipulation
- Per-instrument effect sends

## Modes

Four editing modes with clear separation:

| Mode | Purpose | Enter | Exit |
|------|---------|-------|------|
| Normal | Navigate, switch screens | Default / `Esc` | N/A |
| Edit | Modify cell values | `i` | `Esc` |
| Visual | Select ranges | `v` | `Esc` |
| Command | Execute operations | `:` | `Enter` or `Esc` |

### Normal Mode Keys

- Arrow keys - Navigate cells
- `Tab` / `Shift+Tab` - Next/previous track
- `1-6` - Switch screens
- `i` - Enter Edit mode
- `v` - Enter Visual mode
- `Space` - Play/stop
- `y` - Yank (copy)
- `p` - Paste
- `d` - Delete
- `u` - Undo
- `Ctrl+r` - Redo

### Visual Mode Keys

- Arrow keys - Extend selection
- `y` - Yank selection
- `d` - Delete selection
- `x` - Cut selection
- `>` / `<` - Transpose up/down semitone
- `+` / `-` - Adjust values

### Command Mode

Commands entered after `:`, executed with Enter:

```
:w              - Save project
:w mytrack      - Save as "mytrack.vit"
:e mytrack      - Load "mytrack.vit"
:new            - New project
:q              - Quit
:transpose 12   - Transpose selection
:interpolate    - Fill values between first/last
:randomize 10   - Randomize values by percentage
:double         - Double pattern length
:halve          - Halve pattern length
```

## Data Model

### Project

Top-level container saved as a single `.vit` file (compressed JSON).

- Name
- Tempo (BPM)
- Groove template
- Instruments (up to 128)
- Patterns (up to 256)
- Chains (up to 128)
- Song arrangement
- Mixer state

### Instrument

A named Plaits synth patch with effect sends.

- Name (string)
- Engine (0-15, Plaits engine type)
- Plaits parameters (harmonics, timbre, morph, etc.)
- Send levels: Reverb, Delay, Chorus, Drive, Sidechain duck amount

### Pattern

A named grid of steps.

- Name (string)
- Length (1-128 steps)
- 16 tracks of step data

### Step

One cell in the pattern grid.

| Field | Description | Format |
|-------|-------------|--------|
| Note | Pitch or OFF | C-4, D#5, OFF, empty |
| Instrument | Which instrument | Name/ID reference |
| Volume | Velocity | 00-FF (hex) |
| FX1 | Effect command | CMD + value |
| FX2 | Effect command | CMD + value |
| FX3 | Effect command | CMD + value |

### FX Commands

Initial set for v1:

- `ARP` - Arpeggio
- `POR` - Portamento
- `VIB` - Vibrato
- `VOL` - Volume slide
- `PAN` - Pan
- `DLY` - Retrigger/delay

### Chain

A named sequence of pattern references.

- Name (string)
- Scale lock (optional, e.g., C minor)
- Pattern list (ordered references)

### Song

16 tracks, each containing a sequence of chain references defining the full arrangement.

## Screens

Six dedicated full-screen views, switched with number keys 1-6:

### 1. Project Screen

- Project name (editable)
- Tempo BPM (editable)
- Groove template selector
- New / Load / Save actions
- Recent projects list

### 2. Song Screen

- 16 columns (tracks), rows are time slots
- Each cell shows chain name or empty
- High-level arrangement overview

### 3. Chain Screen

- Edit one chain at a time
- Vertical list of pattern references
- Add/remove/reorder patterns
- Chain name and scale lock settings

### 4. Pattern Screen

- Main tracker grid
- 16 tracks horizontal, steps vertical
- Columns: Note | Inst | Vol | FX1 | FX2 | FX3 per track
- Playhead visible during playback
- Pattern name and length at top

### 5. Instrument Screen

- Edit one instrument at a time
- Plaits engine selector (16 engines)
- All Plaits parameters
- 5 send levels (Reverb, Delay, Chorus, Drive, Sidechain)
- Instrument name at top

### 6. Mixer Screen

- 16 track strips: volume, pan, mute, solo
- 5 bus strips with effect parameters
- Master output level

## Audio Architecture

### Voice Pool

- 64 voices total (shared polyphony)
- Each voice wraps one Plaits DSP instance
- Voice stealing: oldest note first

### Signal Flow

```
Voice (Plaits) ──┬──> Track Bus ──> Volume/Pan ──> Master Out
                 │
                 ├──> Reverb Send ──> Reverb FX ──> Master Out
                 ├──> Delay Send ──> Delay FX ──> Master Out
                 ├──> Chorus Send ──> Chorus FX ──> Master Out
                 ├──> Drive Send ──> Drive FX ──> Master Out
                 └──> Sidechain Trigger ──> Detector
                                                │
                      (duck amount applied to other voices)
```

### Effects (v1)

Simple implementations:

- **Reverb** - Algorithmic (size, damping, mix)
- **Delay** - Tempo-synced (time, feedback, mix)
- **Chorus** - Simple modulation (rate, depth, mix)
- **Drive** - Soft saturation (gain, tone)
- **Sidechain** - Envelope follower with per-instrument duck amount

### Timing

- Sample rate: 48kHz internal
- Tempo: BPM only (groove templates handle swing)
- Per-track groove template selection

## File Format

### Project File

- Extension: `.vit`
- Format: JSON compressed with zlib
- Location: User-chosen

### Structure

```json
{
  "version": "1.0",
  "name": "My Song",
  "tempo": 120,
  "groove": "MPC 60%",
  "instruments": [
    {
      "name": "Bass",
      "engine": 0,
      "harmonics": 0.5,
      "timbre": 0.3,
      "morph": 0.5,
      "sends": [0.2, 0.0, 0.0, 0.3, 0.0]
    }
  ],
  "patterns": [
    {
      "name": "Intro-A",
      "length": 16,
      "tracks": [...]
    }
  ],
  "chains": [
    {
      "name": "Verse",
      "scaleLock": "C minor",
      "patterns": ["Intro-A", "Intro-A", "Intro-B"]
    }
  ],
  "song": [
    ["Verse", "Chorus", "Verse"],
    ["BassDrv", null, "BassDrv"]
  ],
  "mixer": {
    "trackLevels": [...],
    "busLevels": [...],
    "master": 0.8
  }
}
```

### Autosave

- Location: `~/.vitracker/autosave/`
- Interval: Every 60 seconds

## Technical Architecture

### Project Structure

```
vitracker/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── App.cpp/h
│   ├── audio/
│   │   ├── AudioEngine.cpp/h
│   │   ├── Voice.cpp/h
│   │   └── Effects.cpp/h
│   ├── model/
│   │   ├── Project.cpp/h
│   │   ├── Instrument.cpp/h
│   │   ├── Pattern.cpp/h
│   │   ├── Chain.cpp/h
│   │   └── Song.cpp/h
│   ├── ui/
│   │   ├── Screen.cpp/h
│   │   ├── ProjectScreen.cpp/h
│   │   ├── SongScreen.cpp/h
│   │   ├── ChainScreen.cpp/h
│   │   ├── PatternScreen.cpp/h
│   │   ├── InstrumentScreen.cpp/h
│   │   └── MixerScreen.cpp/h
│   ├── input/
│   │   ├── ModeManager.cpp/h
│   │   └── KeyHandler.cpp/h
│   └── dsp/
│       └── (Plaits DSP code)
└── resources/
    └── icon.png
```

### Dependencies

- JUCE 8.0.4 (audio, UI, file handling)
- Plaits DSP (embedded)

## v1 Scope

### Included

| Feature | Specification |
|---------|---------------|
| Modes | Normal, Edit, Visual, Command |
| Screens | Project, Song, Chain, Pattern, Instrument, Mixer |
| Tracks | 16 fixed |
| Step data | Note, Instrument, Volume, 3 FX columns |
| Pattern length | 1-128 steps per pattern |
| Instruments | Up to 128, Plaits engine, named |
| Patterns | Up to 256, named |
| Chains | Up to 128, named, with scale lock |
| Voices | 64 polyphony pool |
| Effects | Reverb, Delay, Chorus, Drive, Sidechain |
| Sends | Per-instrument |
| Groove | Per-track groove templates |
| Tempo | BPM only |
| File format | .vit (compressed JSON) |
| Platform | Standalone app (macOS, Windows, Linux) |

### Explicitly Not in v1

- Mouse support
- VST/AU hosting
- Plugin version (VST/AU of Vitracker itself)
- Triplets / odd time signatures
- WAV/MIDI/stem export

## Future Considerations

- Mouse support as optional layer
- VST/AU hosting for third-party instruments
- Export functionality (WAV, MIDI, stems)
- Time signature support (3/4, triplets)
- Plugin version for use inside other DAWs
