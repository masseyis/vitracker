# Remove Project Screen Design

## Overview

Eliminate the standalone Project screen by moving its functionality to more appropriate locations:
- Tempo and groove display → Status bar (always visible)
- Project name → Song screen header and window title
- Autosave replaces manual save workflow

## Changes

### 1. Screen Renumbering

Remove Project screen. New mapping:

| Key | Screen |
|-----|--------|
| 1 | Song |
| 2 | Chain |
| 3 | Pattern |
| 4 | Instrument |
| 5 | Mixer |

Key `6` does nothing.

### 2. Status Bar Layout

Left to right:
- Mode indicator (NORMAL/EDIT/VISUAL/COMMAND)
- Current screen name
- **Tempo** (e.g., "120.0") - highlights in tempo-adjust mode
- **Groove** (e.g., "Swing 50%")
- Play state and position
- Tip Me button (far right)

### 3. Global Tempo/Groove Shortcuts

Work from any screen:

**Tempo (`t`):**
- Press `t` to enter tempo-adjust mode
- Status bar highlights tempo value
- Arrow keys: ±1 BPM
- Shift+Arrow keys: ±10 BPM
- Enter or Escape: exit tempo-adjust mode

**Groove (`g`):**
- Press `g` to cycle groove forward
- Press `G` (Shift+g) to cycle groove reverse
- No focus mode needed (only 5 options)

### 4. Song Screen Changes

**Header:**
- Shows "SONG - {project name}" (e.g., "SONG - My Track")
- Matches pattern of other screens

**Rename (`r`):**
- Press `r` on Song screen to rename project
- Enters text edit mode (same as pattern/instrument renaming)
- Enter confirms, Escape cancels

**New Song (`n`):**
- Press `n` on Song screen to create new song
- Shows confirmation if current project has unsaved changes
- Resets to fresh "Untitled" project

**Window Title:**
- Updates to "Vitracker - {project name}"

### 5. Autosave Behavior

**Trigger:**
- Autosave on any edit after ~1-2 second debounce
- No manual save needed for normal workflow

**Filename:**
- Derived from project name: "My Song" → "My Song.vit"
- Invalid characters (`/\:*?"<>|`) replaced with `_`
- New projects default to "Untitled.vit"

**Location:**
- Saves to last-used directory
- New projects use default "Vitracker Projects" folder

**Name Changes:**
- Saves to new filename
- Old file remains (user deletes manually if desired)

### 6. Commands

**`:new`** - Works globally, creates fresh project (same as `n` on Song screen)

**`:w` and `:e`** - Still available for explicit save/load to specific locations

## Implementation Tasks

1. Remove ProjectScreen class and references
2. Update screen array and key mappings (1-5 instead of 1-6)
3. Add tempo/groove display to status bar
4. Implement tempo-adjust mode in App (focus grab pattern)
5. Add `g`/`G` groove cycling to KeyHandler
6. Update SongScreen header to show project name
7. Add `r` rename handling to SongScreen
8. Add `n` new song handling to SongScreen (with confirmation)
9. Update window title on project name change
10. Implement autosave with debounce
11. Implement filename derivation from project name
12. Update all help content
13. Remove screen 6 references from documentation
