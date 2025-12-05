# Chord Entry Feature Design

## Overview

A chord entry popup for the Pattern screen that allows quick entry of scale-degree-based chords with visual feedback. Press `c` to open a popup showing a piano keyboard visualization and three-column selector for Degree, Type, and Inversion. Chords are placed across adjacent tracks using the same instrument.

## User Interaction

### Opening the Popup

Press `c` in the Pattern screen to open the chord popup. Initial state depends on context:

- **Empty cell**: Degree defaults to I, Type to maj, Inversion to root
- **Existing note**: That note becomes the root, Degree shows matching scale degree, Type defaults to maj
- **Existing chord** (same instrument on adjacent tracks at same row): Loads current chord's degree/type/inversion for editing

### Navigation

- **Left/Right**: Move between Degree, Type, Inversion columns
- **Up/Down**: Change value within current column
- **Space**: Trigger immediate audio preview
- **Enter**: Confirm and place chord
- **Esc**: Cancel without changes

### Audio Preview

- 200ms debounce after any navigation change
- Plays chord through the current track's instrument
- Space key triggers immediate preview without debounce

### Placing the Chord

On Enter:
- Notes fill adjacent tracks starting from cursor (track N, N+1, N+2, etc.)
- Each track gets the same instrument as the current track
- If not enough tracks available (e.g., 4-note chord starting at track 14), show "Need X tracks" warning and don't close

## Popup Layout

```
┌─────────────────────────────────────────────────┐
│  Chord: C Major 7 (1st inv)                     │
├─────────────────────────────────────────────────┤
│  ┌─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┐   │
│  │ │█│ │█│ │ │█│ │█│ │█│ │ │█│ │█│ │ │█│ │█│   │
│  │ └┬┘ └┬┘ │ └┬┘ └┬┘ └┬┘ │ └┬┘ └┬┘ │ └┬┘ └┬┘   │
│  │C │D │E │F │G │A │B │C │D │E │F │G │A │B │   │
│  └──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┘   │
│        ▲     ▲        ▲     ▲                   │
│       (E)   (G)      (B)   (C) ← chord tones    │
├─────────────────────────────────────────────────┤
│   Degree   │    Type    │   Inversion           │
│  ────────  │  ────────  │  ──────────           │
│     I      │    maj     │    root               │
│  >  ii  <  │    min     │ >  1st  <             │
│    iii     │    dim     │    2nd                │
│     IV     │ >  maj7 <  │    3rd                │
│      V     │    min7    │                       │
│     vi     │    7       │                       │
│    vii°    │    9       │                       │
│            │    sus4    │                       │
└─────────────────────────────────────────────────┘
         ←/→ columns   ↑/↓ select   Enter confirm
```

### Piano Keyboard

- Horizontal piano spanning 2 octaves, centered on the chord
- Highlighted keys show chord tones
- Scale tones could be subtly distinguished from non-scale tones

### Three-Column Selector

- **Degree column**: I, ii, iii, IV, V, vi, vii° (Roman numerals, lowercase for minor)
- **Type column**: All available chord types
- **Inversion column**: Dynamically shows correct count based on chord type (3 for triads, 4 for 7ths, etc.)

Current selection marked with `>` indicators.

## Scale Lock Behavior

- Uses the chain's scale lock setting to determine scale degrees
- If no scale lock is set, defaults to C Major
- Degree I in C Major = C, Degree ii = D minor, etc.

## Chord Definitions

### Triads (3 notes → 3 inversions: root, 1st, 2nd)

| Type | Intervals |
|------|-----------|
| maj | R, 3, 5 |
| min | R, ♭3, 5 |
| dim | R, ♭3, ♭5 |
| aug | R, 3, ♯5 |

### Sevenths (4 notes → 4 inversions: root, 1st, 2nd, 3rd)

| Type | Intervals |
|------|-----------|
| 7 | R, 3, 5, ♭7 |
| maj7 | R, 3, 5, 7 |
| min7 | R, ♭3, 5, ♭7 |
| dim7 | R, ♭3, ♭5, ♭♭7 |
| min7♭5 | R, ♭3, ♭5, ♭7 |

### Extended (5+ notes → matching inversion count)

| Type | Intervals | Notes |
|------|-----------|-------|
| 9 | R, 3, 5, ♭7, 9 | 5 |
| maj9 | R, 3, 5, 7, 9 | 5 |
| min9 | R, ♭3, 5, ♭7, 9 | 5 |
| 11 | R, 3, 5, ♭7, 9, 11 | 6 |
| 13 | R, 3, 5, ♭7, 9, 11, 13 | 7 |

### Sus/Add (3-4 notes)

| Type | Intervals | Notes |
|------|-----------|-------|
| sus2 | R, 2, 5 | 3 |
| sus4 | R, 4, 5 | 3 |
| add9 | R, 3, 5, 9 | 4 |

## Implementation

### New Files

- `src/ui/ChordPopup.h` - Chord popup component header
- `src/ui/ChordPopup.cpp` - Chord popup implementation with:
  - Piano keyboard rendering
  - Three-column selector
  - Chord calculation logic
  - Preview triggering

### Modified Files

- `src/ui/PatternScreen.cpp` - Handle `c` keypress, open popup, receive result and place notes
- `src/audio/AudioEngine.h/.cpp` - Add method to preview chord (trigger multiple notes simultaneously)
- `CMakeLists.txt` - Add new source files

### Data Structures

```cpp
// Intervals in semitones from root
struct ChordDefinition {
    std::string name;           // "maj7", "min", etc.
    std::vector<int> intervals; // e.g., {0, 4, 7, 11} for maj7
};

struct ChordSelection {
    int degree;        // 0-6 (I through vii)
    int typeIndex;     // Index into chord types
    int inversion;     // 0 = root position
    int rootNote;      // MIDI note of root
    std::vector<int> notes; // Resulting MIDI notes after inversion
};
```

### Chord Detection (for editing existing chords)

When opening popup on a note:
1. Scan adjacent tracks (N+1, N+2, etc.) at same row
2. Check if they have the same instrument
3. Collect the notes and calculate intervals from lowest note
4. Reverse-lookup intervals to find matching chord type
5. Determine inversion based on which chord tone is in the bass
6. Pre-populate popup with detected values

### Preview Implementation

```cpp
// In AudioEngine
void previewChord(const std::vector<int>& midiNotes, int instrumentIndex);
```

Triggers all notes simultaneously with a short duration (e.g., 500ms) for preview purposes.

## Edge Cases

- **Not enough tracks**: Show warning message, don't place chord
- **No scale lock**: Default to C Major
- **Chord spans past track 15**: Block with "Need X tracks" message
- **Existing notes on target tracks**: Overwrite them (user explicitly chose to place chord there)

## Help Text

Add to Pattern screen help:

| Key | Action |
|-----|--------|
| `c` | Open chord entry popup |
