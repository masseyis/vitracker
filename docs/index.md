# Vitracker User Manual

A vim-modal music tracker with multiple synthesis engines including Mutable Instruments Plaits, VA synthesis, sampling, and beat slicing.

<p align="center">
  <a href="https://github.com/masseyis/vitracker"><img src="https://img.shields.io/badge/GitHub-View%20Source-181717?logo=github" alt="GitHub" /></a>
  <a href="https://github.com/masseyis/vitracker/releases"><img src="https://img.shields.io/github/v/release/masseyis/vitracker" alt="Download" /></a>
  <a href="https://ko-fi.com/masseyis"><img src="https://img.shields.io/badge/Ko--fi-Support%20Development-ff5e5b?logo=ko-fi&logoColor=white" alt="Ko-fi" /></a>
</p>

<p align="center">
  <strong><a href="https://github.com/masseyis/vitracker">Source Code</a></strong> ·
  <strong><a href="https://github.com/masseyis/vitracker/releases">Download</a></strong> ·
  <strong><a href="https://github.com/masseyis/vitracker/discussions">Discussions</a></strong> ·
  <strong><a href="https://github.com/masseyis/vitracker/issues">Report Bug</a></strong>
</p>

---

## Table of Contents

1. [Overview](#overview)
2. [Screens](#screens)
3. [Modes](#modes)
4. [Global Keys](#global-keys)
5. [Pattern Screen](#pattern-screen)
6. [Chain Screen](#chain-screen)
7. [Song Screen](#song-screen)
8. [Instrument Screen](#instrument-screen)
9. [Mixer Screen](#mixer-screen)
10. [Commands](#commands)
11. [Plaits Engines](#plaits-engines)

---

## Overview

Vitracker is a music tracker with vim-style modal editing. Like vim, it has distinct modes for navigation and editing, allowing fast, keyboard-driven workflow.

### Structure Hierarchy

```
Song
 └── Chains (16 columns)
      └── Patterns (sequence of patterns per chain)
           └── Tracks (16 tracks per pattern)
                └── Steps (notes, instruments, effects)
```

### Workflow

1. Create **Instruments** with different Plaits engine sounds
2. Compose **Patterns** with notes across 16 tracks
3. Organize patterns into **Chains** with optional transpose
4. Arrange chains in the **Song** across 16 columns
5. Mix levels in the **Mixer**

---

## Screens

Switch between screens using number keys `1-6` or `{`/`}` for previous/next.

| Key | Screen | Description |
|-----|--------|-------------|
| `1` | Project | Project settings, tempo, groove |
| `2` | Song | Arrange chains into a song |
| `3` | Chain | Sequence patterns with transpose |
| `4` | Pattern | Edit notes and steps |
| `5` | Instrument | Configure Plaits synth parameters |
| `6` | Mixer | Volume, pan, mute, solo per instrument |

---

## Modes

Vitracker has four modes, shown in the status bar at the bottom.

### Normal Mode

Default mode for navigation and triggering actions.

- Arrow keys navigate
- Single-key commands (y, p, d, etc.)
- Press `i` to enter Edit mode
- Press `v` to enter Visual mode
- Press `:` to enter Command mode

### Edit Mode

For entering and modifying data.

- Arrow keys adjust values (not navigate)
- Type notes directly (C, D, E, F, G, A, B)
- Press `Esc` to return to Normal mode

### Visual Mode

For selecting regions.

- Arrow keys extend selection
- `y` to yank (copy)
- `d` to delete
- `<` / `>` to transpose selection
- Press `Esc` to cancel selection

### Command Mode

For executing commands.

- Type command after `:`
- Press `Enter` to execute
- Press `Esc` to cancel

---

## Global Keys

These keys work in Normal mode across all screens.

### Navigation

| Key | Action |
|-----|--------|
| `Arrow keys` | Navigate cursor |
| `1-6` | Switch to screen 1-6 |
| `{` | Previous screen |
| `}` | Next screen |
| `[` | Previous pattern/chain/item |
| `]` | Next pattern/chain/item |
| `Enter` | Drill down (Song→Chain→Pattern→Instrument) |

### Transport

| Key | Action |
|-----|--------|
| `Space` | Play/Stop |

### Mode Switching

| Key | Action |
|-----|--------|
| `i` | Enter Edit mode |
| `v` | Enter Visual mode |
| `:` | Enter Command mode |
| `Esc` | Return to Normal mode |

### Editing (Normal Mode)

| Key | Action |
|-----|--------|
| `Alt+Up/Down` | Adjust value in grids |
| `Alt+Left/Right` | Adjust value in grids |
| `Left/Right` | Adjust non-grid parameters |
| `+` / `=` | Increment value |
| `-` | Decrement value |
| `Shift+N` | Create new item |
| `r` | Rename current item |
| `d` | Delete |
| `Backspace` | Clear cell |

### Clipboard

| Key | Action |
|-----|--------|
| `y` | Yank (copy) |
| `p` | Paste |

### Undo/Redo

| Key | Action |
|-----|--------|
| `u` | Undo |
| `Ctrl+R` | Redo |

---

## Pattern Screen

The Pattern screen (`4`) is where you compose music. Each pattern has 16 tracks and 16-64 rows.

### Layout

```
Track:  0    1    2    3   ...  15
      ┌────┬────┬────┬────┬───┬────┐
Row 0 │NOTE│NOTE│NOTE│NOTE│...│NOTE│
Row 1 │ .. │ .. │ .. │ .. │...│ .. │
 ...  │    │    │    │    │   │    │
      └────┴────┴────┴────┴───┴────┘
```

Each step contains:
- **Note** (C-0 to B-9, or `---` for empty, `OFF` for note-off)
- **Instrument** (00-7F)
- **Volume** (00-FF, or `..` for default)
- **FX1, FX2, FX3** (effect commands)

### Entering Notes (Edit Mode)

1. Press `i` to enter Edit mode
2. Type note letter: `C`, `D`, `E`, `F`, `G`, `A`, `B`
3. Optional: `#` for sharp, `b` for flat
4. Type octave: `0-9`
5. Arrow keys move to next field (instrument, volume, FX)
6. Press `Esc` when done

### Quick Note Entry (Normal Mode)

- Type `A-G` directly to enter note at cursor
- Instrument is auto-filled from last used

### Pattern Navigation

| Key | Action |
|-----|--------|
| `[` | Previous pattern |
| `]` | Next pattern |
| `Shift+N` | Create new pattern |
| `r` | Rename pattern |

### Visual Selection

1. Press `v` to start selection
2. Move cursor to extend
3. Actions on selection:

| Key | Action |
|-----|--------|
| `y` | Copy selection |
| `d` | Delete selection |
| `<` | Transpose down 1 semitone |
| `>` | Transpose up 1 semitone |
| `-` | Transpose down 1 octave |
| `+` | Transpose up 1 octave |

---

## Chain Screen

The Chain screen (`3`) sequences patterns with optional transpose.

### Layout

```
Chain: 00 "Lead"
Scale Lock: C Major

 Pat  Trans
┌────┬─────┐
│ 00 │  0  │
│ 01 │ +2  │
│ 00 │ +4  │
│ 02 │  0  │
└────┴─────┘
```

### Fields

- **Pattern** - Which pattern to play (00-FF)
- **Transpose** - Scale degrees to transpose (e.g., +2 = up 2 scale degrees)
- **Scale Lock** - Scale for transpose calculation (top of screen)

### Editing

| Key | Action |
|-----|--------|
| `Alt+Up/Down` | Change pattern number |
| `Alt+Left/Right` | Change transpose |
| `Left/Right` | Change scale lock (when on scale row) |
| `[` / `]` | Previous/next chain |
| `Shift+N` | Create new chain |
| `r` | Rename chain |
| `Enter` | Jump to selected pattern |

### Scale Lock Options

- None, C Major, C Minor, C# Major, C# Minor, ... (all keys)
- Transpose is in scale degrees, not semitones

---

## Song Screen

The Song screen (`2`) arranges chains into a complete song.

### Layout

```
     Col 0  Col 1  Col 2  ...  Col 15
    ┌─────┬─────┬─────┬─────┬─────┐
Row 0│  00 │  01 │  -- │ ... │  -- │
Row 1│  00 │  02 │  -- │ ... │  -- │
Row 2│  -- │  -- │  -- │ ... │  -- │
    └─────┴─────┴─────┴─────┴─────┘
```

- Each column plays independently
- All columns in a row play simultaneously
- `--` indicates empty slot

### Editing

| Key | Action |
|-----|--------|
| `Alt+Up/Down` | Change chain number |
| `d` or `Backspace` | Clear slot |
| `Enter` | Jump to selected chain |

### Playback

When playing in Song mode:
- All columns play their chains simultaneously
- Each column's pattern track 0 plays on its corresponding audio track
- When a chain finishes, it advances to the next pattern in the chain
- When all chains finish, the song advances to the next row

---

## Instrument Screen

The Instrument screen (`5`) configures Plaits synthesis parameters.

### Presets

The Instrument screen includes factory and user presets for quick sound selection.

#### Preset Row

At the top of the screen, a **Preset** row shows the current preset:
- `[Engine] PresetName` - Current preset
- `[Engine] PresetName*` - Preset modified (asterisk indicates you've tweaked parameters)
- `[Engine] --` - No preset loaded

#### Factory Presets

Each engine includes 2-7 factory presets:
- All engines have an **Init** preset (neutral starting point)
- FM engine includes classic sounds: DX7 E-Piano, M1 Piano, M1 Organ, Lately Bass
- Drum engines include variations like Punchy, 808, Tight, etc.

#### User Presets

Save your own presets with `:save-preset name`. User presets are stored in `~/.vitracker/presets/`.

### Parameters

#### Main Parameters (Top)
- **Preset** - Current preset (use Left/Right to browse)
- **Engine** - Plaits synthesis engine (0-15)
- **Harmonics** - Harmonic content
- **Timbre** - Timbral character
- **Morph** - Morphing between variations

#### Envelope
- **Attack** - Amplitude attack time
- **Decay** - Amplitude decay time

#### Filter
- **Cutoff** - Filter cutoff frequency
- **Resonance** - Filter resonance

#### LFO 1 & 2
- **Rate** - LFO speed (0-15, tempo-synced)
- **Shape** - Waveform (Sine, Triangle, Saw, Square, Random)
- **Dest** - Modulation destination
- **Amount** - Modulation depth (-64 to +63)

#### ENV 1 & 2
- **Attack** - Envelope attack
- **Decay** - Envelope decay
- **Dest** - Modulation destination
- **Amount** - Modulation depth

#### Sends
- **Reverb** - Reverb send level
- **Delay** - Delay send level
- **Chorus** - Chorus send level
- **Drive** - Distortion amount
- **Sidechain** - Sidechain duck amount

### Editing

| Key | Action |
|-----|--------|
| `Up/Down` | Navigate parameters |
| `Left/Right` | Adjust value (non-mod parameters) |
| `Alt+Left/Right` | Adjust value (mod matrix rows) |
| `Alt+Up/Down` | Adjust value (mod matrix rows) |
| `[` / `]` | Previous/next instrument |
| `Shift+N` | Create new instrument |
| `r` | Rename instrument |

---

## Mixer Screen

The Mixer screen (`6`) controls per-instrument levels.

### Per-Instrument Controls

- **Volume** - Output level (0.0 - 1.0)
- **Pan** - Stereo position (-1.0 left to +1.0 right)
- **Mute** - Silence this instrument
- **Solo** - Solo this instrument (mute all others)

### Master Controls

- **Master Volume** - Final output level

### Keys

| Key | Action |
|-----|--------|
| `Arrow keys` | Navigate |
| `Alt+Up/Down` | Adjust value |
| `m` | Toggle mute |
| `s` | Toggle solo |

---

## Commands

Enter Command mode with `:`, type command, press `Enter`.

### File Commands

| Command | Action |
|---------|--------|
| `:w` | Save (file dialog) |
| `:w filename` | Save as ~/filename.vit |
| `:e` | Load (file dialog) |
| `:e filename` | Load ~/filename.vit |
| `:new` | New project |
| `:q` | Quit |

### Pattern Commands

| Command | Action |
|---------|--------|
| `:transpose N` | Transpose selection by N semitones |
| `:interpolate` | Interpolate values between first and last in selection |
| `:randomize N` | Randomize selection by N percent |
| `:double` | Double pattern length |
| `:halve` | Halve pattern length |

### Preset Commands

| Command | Action |
|---------|--------|
| `:save-preset name` | Save current instrument as user preset |
| `:delete-preset name` | Delete a user preset (factory presets cannot be deleted) |

---

## Plaits Engines

Vitracker uses all 16 Plaits synthesis engines:

| # | Engine | Description |
|---|--------|-------------|
| 0 | Virtual Analog | Classic VA oscillator with variable waveshape |
| 1 | Waveshaping | Waveshaper with fold/drive |
| 2 | FM | 2-operator FM synthesis |
| 3 | Grain | Granular synthesis |
| 4 | Additive | Additive synthesis with harmonic control |
| 5 | Wavetable | Wavetable morphing |
| 6 | Chord | Chord generator |
| 7 | Speech | Formant/speech synthesis |
| 8 | Swarm | Swarm of detuned oscillators |
| 9 | Noise | Filtered noise |
| 10 | Particle | Dust/particle synthesis |
| 11 | String | Physical modeling string |
| 12 | Modal | Physical modeling resonator |
| 13 | Bass Drum | Analog bass drum |
| 14 | Snare Drum | Analog snare drum |
| 15 | Hi-Hat | Analog hi-hat |

Each engine responds differently to the Harmonics, Timbre, and Morph parameters.

---

## Tips & Tricks

### Fast Workflow

- Use `[`/`]` to quickly switch patterns/chains/instruments
- `Enter` drills down: Song → Chain → Pattern → Instrument
- `Alt+arrows` for quick edits without leaving Normal mode

### Live Performance

- Mute/solo instruments in Mixer (keys `m`/`s`)
- Use Song mode for arrangement, Pattern mode for loops
- Scale lock ensures transpose stays musical

### Sound Design

- Start with an engine that matches your sound goal
- Harmonics controls complexity
- Timbre controls brightness
- Use LFOs for movement
- Use ENVs for plucks and swells

---

## Keyboard Reference Card

### Modes
```
Normal ──i──> Edit ──Esc──> Normal
Normal ──v──> Visual ──Esc──> Normal
Normal ──:──> Command ──Enter/Esc──> Normal
```

### Quick Reference
```
Navigation:     ↑↓←→      Mode Switch:    i v : Esc
Screens:        1-6 { }   Transport:      Space
Edit Values:    Alt+↑↓←→  Clipboard:      y p d
Undo/Redo:      u Ctrl+R  New/Rename:     Shift+N r
Next/Prev:      [ ]       Drill Down:     Enter
```

### Commands
```
:w :e :new :q
:transpose N :interpolate :randomize N :double :halve
:save-preset name :delete-preset name
```

---

## Contributing

Vitracker is open source under the GPL v3 license. Contributions are welcome!

- **[View Source on GitHub](https://github.com/masseyis/vitracker)** - Browse the code, fork, and submit PRs
- **[Report Bugs](https://github.com/masseyis/vitracker/issues/new?template=bug_report.md)** - Help us fix issues
- **[Request Features](https://github.com/masseyis/vitracker/issues/new?template=feature_request.md)** - Suggest new features
- **[Join Discussions](https://github.com/masseyis/vitracker/discussions)** - Ask questions and share ideas
- **[Contributing Guide](https://github.com/masseyis/vitracker/blob/main/CONTRIBUTING.md)** - Read the contribution guidelines

---

## Support Development

If you find Vitracker useful, please consider supporting its development!

<a href="https://ko-fi.com/masseyis"><img src="https://ko-fi.com/img/githubbutton_sm.svg" alt="Support on Ko-fi" /></a>

---

*Vitracker - Made with [JUCE](https://juce.com/) and [Mutable Instruments Plaits](https://github.com/pichenettes/eurorack)*
