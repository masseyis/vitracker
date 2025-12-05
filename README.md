# Vitracker

A vim-modal music tracker with multiple synthesis engines including Mutable Instruments Plaits, VA synthesis, sampling, and beat slicing.

![Build Status](https://github.com/masseyis/vitracker/actions/workflows/ci.yml/badge.svg)

## What is Vitracker?

Vitracker is a desktop music tracker application inspired by classic trackers like LSDJ and Renoise, with vim-style modal navigation. It features:

- **Multiple Synthesis Engines**
  - **Plaits** - All 16 synthesis engines from Mutable Instruments' Plaits module
  - **VA Synth** - 3-oscillator virtual analog synthesizer with filter and envelopes
  - **Sampler** - Load audio files with auto-pitch detection and repitching
  - **Slicer** - Beat slicer with multiple chop modes (lazy, divisions, transients)

- **Pattern-based Sequencing** - 16 tracks per pattern, chains, and song arrangement
- **Scale-locked Transposition** - Transpose patterns by scale degrees, not semitones
- **Master Effects** - Reverb, Delay, Chorus, Drive, Sidechain compression, DJ Filter, Limiter
- **Modulation** - Per-instrument LFO and envelope modulation
- **Preset System** - Factory and user presets for each synthesis engine
- **Cross-platform** - Runs on macOS, Windows, and Linux

## Quick Start

### Installation

Download the latest release for your platform from the [Releases page](https://github.com/masseyis/vitracker/releases).

### Basic Concepts

Vitracker uses a hierarchical structure:
- **Song** (Screen 2) - Arrangement of chains across 16 columns
- **Chain** (Screen 3) - Sequence of patterns with scale lock and transpose
- **Pattern** (Screen 4) - 16 tracks x 16-64 rows of notes, instruments, and effects
- **Instrument** (Screen 5) - Synth/sampler settings with modulation
- **Mixer** (Screen 6) - Per-instrument volume/pan/mute/solo and master effects

### Global Keys

| Key | Action |
|-----|--------|
| `1-6` | Switch screens (Project, Song, Chain, Pattern, Instrument, Mixer) |
| `Arrow keys` | Navigate |
| `Space` | Play/Stop |
| `v` | Enter Visual mode (selection) |
| `:` | Enter Command mode |
| `?` | Show help popup for current screen |
| `Esc` | Close help / Cancel |

### Pattern Screen (4)

| Key | Action |
|-----|--------|
| `n` | Add note (copies from row above) |
| `Alt+Up/Down` | Create note / adjust pitch in scale |
| `Shift+Up/Down` | Move note by octave |
| `.` | Note off |
| `0-9, a-f` | Hex input for values |
| `Tab / Shift+Tab` | Next/previous track |
| `[ ]` | Previous/Next pattern |
| `+/-` | Add/remove row |
| `Shift+N` | Create new pattern |
| `r` | Rename pattern |

### Visual Mode (Selection)

| Key | Action |
|-----|--------|
| `v` | Start selection |
| `y` | Yank (copy) |
| `d` | Delete |
| `p` | Paste |
| `< >` | Transpose selection |
| `f` | Fill following pattern |
| `s` | Randomize populated values |

### Instrument Screen (5)

| Key | Action |
|-----|--------|
| `Tab / Shift+Tab` | Cycle instrument type (Plaits/Sampler/Slicer/VA) |
| `[ ]` | Previous/Next instrument |
| `+/-` or `Left/Right` | Adjust parameter |
| `Shift+N` | Create new instrument |
| `r` | Rename instrument |
| Drag & Drop | Load audio file (Sampler/Slicer) |

### Mixer Screen (6)

| Key | Action |
|-----|--------|
| `Left/Right` | Select channel |
| `Up/Down` | Adjust volume/pan or toggle mute/solo |
| `m` | Toggle mute |
| `s` | Toggle solo |
| `Down` (from Solo row) | Enter FX section |

### Commands

| Command | Action |
|---------|--------|
| `:w` | Save project (file dialog) |
| `:w name` | Save as `name.vit` |
| `:e` | Load project (file dialog) |
| `:e name` | Load `name.vit` |
| `:new` | New empty project |
| `:q` | Quit |
| `:sampler` | Convert instrument to sampler |
| `:slicer` | Convert instrument to slicer |
| `:chop N` | Chop sample into N slices |
| `:save-preset name` | Save user preset |
| `:delete-preset name` | Delete user preset |

## Instrument Types

### Plaits (Mutable Instruments)

16 synthesis engines including virtual analog, FM, wavetable, physical modeling, noise, and more. Each engine has Harmonics, Timbre, and Morph parameters.

### VA Synth

Classic 3-oscillator virtual analog synthesizer:
- 3 oscillators with Saw/Square/Triangle/Pulse/Sine waveforms
- Per-oscillator octave, detune, and level
- Resonant lowpass filter with envelope
- Amp and filter ADSR envelopes
- Glide (portamento)
- Polyphony (1-16 voices, mono mode with legato)

### Sampler

Audio sample playback with pitch detection:
- Drag & drop audio files
- Auto-detected root note
- Auto-repitch to match pattern notes
- Full ADSR envelope
- Filter with envelope

### Slicer

Beat slicing for loops and breaks:
- Drag & drop audio files
- Multiple chop modes:
  - **Lazy** - Auto-slice on transients
  - **Divisions** - Equal time divisions
  - `:chop N` - Divide into N equal slices
- BPM detection and time-stretching
- Speed and pitch control
- Polyphony control (1 = choke mode)

## Master Effects

All instruments route through the master effects chain:
- **Reverb** - Size and Damping
- **Delay** - Sync'd time (1/16 to 1/2 dotted) and Feedback
- **Chorus** - Rate and Depth
- **Drive** - Gain and Tone
- **Sidechain** - Source instrument selection and Amount
- **DJ Filter** - Bipolar LP/HP filter
- **Limiter** - Threshold and Release

## Building from Source

### Requirements

- CMake 3.22+
- C++17 compiler
- Platform-specific audio libraries (ALSA/JACK on Linux)

### Build

```bash
git clone https://github.com/masseyis/vitracker.git
cd vitracker
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The built application will be in `build/Vitracker_artefacts/Release/`.

## License

This project includes code from:
- [JUCE](https://juce.com/) - GPL v3
- [Mutable Instruments Plaits](https://github.com/pichenettes/eurorack) - MIT License
- [RubberBand](https://breakfastquay.com/rubberband/) - GPL v2

## Support

If you find Vitracker useful, consider supporting development via [Ko-fi](https://ko-fi.com/masseyis).

## Contributing

Contributions are welcome! Please open an issue or pull request on GitHub.
