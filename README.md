# Vitracker

A vim-modal music tracker with the Mutable Instruments Plaits synthesis engine.

![Build Status](https://github.com/masseyis/vitracker/actions/workflows/ci.yml/badge.svg)

## What is Vitracker?

Vitracker is a desktop music tracker application inspired by classic trackers like LSDJ and Renoise, but with vim-style modal editing. It features:

- **Vim-style modal interface** - Navigate with hjkl-style keys, edit with dedicated modes
- **Plaits synthesis** - All 16 synthesis engines from Mutable Instruments' Plaits module
- **Pattern-based sequencing** - 16 tracks per pattern, chains, and song arrangement
- **Scale-locked transposition** - Transpose patterns by scale degrees, not semitones
- **Cross-platform** - Runs on macOS, Windows, and Linux

## Quick Start

### Installation

Download the latest release for your platform from the [Releases page](https://github.com/masseyis/vitracker/releases).

### Basic Concepts

Vitracker uses a hierarchical structure:
- **Song** - Arrangement of chains across 16 columns
- **Chain** - Sequence of patterns with optional transpose
- **Pattern** - 16 tracks × 16-64 rows of notes
- **Instrument** - Plaits synth settings

### Essential Keys

| Key | Action |
|-----|--------|
| `1-6` | Switch screens (Project, Song, Chain, Pattern, Instrument, Mixer) |
| `Arrow keys` | Navigate |
| `Space` | Play/Stop |
| `i` | Enter Edit mode |
| `Esc` | Return to Normal mode |
| `v` | Enter Visual mode (selection) |
| `:` | Enter Command mode |

### Entering Notes (Pattern Screen)

1. Navigate to a cell
2. Press `i` to enter Edit mode
3. Type note name: `C`, `D`, `E`, `F`, `G`, `A`, `B`
4. Add sharp with `#` or flat with `b`
5. Set octave with `0-9`
6. Press `Esc` to return to Normal mode

### Quick Edit (Without Changing Modes)

- `Alt+Up/Down` - Adjust values in grids
- `Left/Right` - Adjust non-grid parameters (scale lock, instrument settings)
- `+/-` - Increment/decrement values

### Saving and Loading

- `:w` - Save (opens file dialog)
- `:w myproject` - Save as `~/myproject.vit`
- `:e` - Load (opens file dialog)
- `:e myproject` - Load `~/myproject.vit`

## Documentation

For the complete manual with all keyboard shortcuts and commands, see the [User Guide](https://masseyis.github.io/vitracker/).

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

## Contributing

Contributions are welcome! Please open an issue or pull request on GitHub.
