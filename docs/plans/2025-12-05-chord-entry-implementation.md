# Chord Entry Feature Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a chord entry popup to the Pattern screen that allows entering scale-degree-based chords with visual piano feedback and audio preview.

**Architecture:** New ChordPopup component following HelpPopup pattern. Triggered by `c` key in PatternScreen. Uses existing AudioEngine::triggerNote for preview. Chord definitions stored as static data. Three-column selector for degree/type/inversion.

**Tech Stack:** C++17, JUCE for UI/audio, existing model/audio infrastructure.

---

## Task 1: Create Chord Data Structures

**Files:**
- Create: `src/ui/ChordPopup.h`

**Step 1: Write chord definition structures**

```cpp
#pragma once

#include <JuceHeader.h>
#include <string>
#include <vector>
#include <array>
#include <functional>

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

} // namespace ui
```

**Step 2: Run build to verify header compiles**

Run: `cmake --build build 2>&1 | head -20`
Expected: No errors related to ChordPopup.h (file not yet included in build)

**Step 3: Commit**

```bash
git add src/ui/ChordPopup.h
git commit -m "Add chord data structures"
```

---

## Task 2: Add Static Chord Definitions

**Files:**
- Modify: `src/ui/ChordPopup.h`

**Step 1: Add static chord type and scale degree definitions**

Add after the struct definitions, inside the namespace:

```cpp
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
```

**Step 2: Run build to verify**

Run: `cmake --build build 2>&1 | head -20`
Expected: No errors

**Step 3: Commit**

```bash
git add src/ui/ChordPopup.h
git commit -m "Add chord type and scale degree definitions"
```

---

## Task 3: Create ChordPopup Class Declaration

**Files:**
- Modify: `src/ui/ChordPopup.h`

**Step 1: Add ChordPopup class declaration**

Add after the static definitions:

```cpp
class ChordPopup : public juce::Component,
                   public juce::Timer
{
public:
    ChordPopup();
    ~ChordPopup() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;
    void timerCallback() override;

    // Show popup with context
    void show(int rootNote, int instrument, const std::string& scaleLock);
    void showAtCell(int cursorTrack, int cursorRow, int existingNote, int instrument,
                    const std::string& scaleLock);
    void hide();
    bool isShowing() const { return isVisible(); }

    // Result callback: vector of MIDI notes to place on adjacent tracks
    std::function<void(const std::vector<int>& notes)> onChordConfirmed;

    // Preview callback: trigger chord preview
    std::function<void(const std::vector<int>& notes, int instrument)> onChordPreview;

private:
    void updateChordNotes();
    void applyInversion();
    void triggerPreview();
    int calculateRootFromDegree() const;
    std::string getChordDisplayName() const;

    void drawPianoKeyboard(juce::Graphics& g, juce::Rectangle<int> area);
    void drawColumnSelector(juce::Graphics& g, juce::Rectangle<int> area);

    ChordSelection selection_;
    std::string scaleLock_;
    int scaleRoot_ = 0;        // 0=C, 1=C#, etc.
    bool isMinorScale_ = false;
    int instrumentIndex_ = 0;
    int cursorTrack_ = 0;

    int currentColumn_ = 0;    // 0=degree, 1=type, 2=inversion
    int previewDebounce_ = 0;  // Countdown for debounced preview

    static constexpr int kPopupWidth = 500;
    static constexpr int kPopupHeight = 400;
    static constexpr int kPianoHeight = 80;
    static constexpr int kPreviewDebounceMs = 200;

    // Colors (matching app theme)
    static inline const juce::Colour bgColor{0xff1a1a2e};
    static inline const juce::Colour panelColor{0xff2a2a4e};
    static inline const juce::Colour borderColor{0xff4a4a6e};
    static inline const juce::Colour titleColor{0xff7c7cff};
    static inline const juce::Colour highlightColor{0xffffcc66};
    static inline const juce::Colour textColor{0xffeaeaea};
    static inline const juce::Colour dimColor{0xff888888};
    static inline const juce::Colour selectedColor{0xff4a7cff};
    static inline const juce::Colour whiteKeyColor{0xfff0f0f0};
    static inline const juce::Colour blackKeyColor{0xff2a2a2a};
    static inline const juce::Colour chordNoteColor{0xffff6666};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChordPopup)
};
```

**Step 2: Run build to verify**

Run: `cmake --build build 2>&1 | head -20`
Expected: No errors (header only, not linked yet)

**Step 3: Commit**

```bash
git add src/ui/ChordPopup.h
git commit -m "Add ChordPopup class declaration"
```

---

## Task 4: Create ChordPopup Implementation - Constructor and Scale Parsing

**Files:**
- Create: `src/ui/ChordPopup.cpp`
- Modify: `CMakeLists.txt`

**Step 1: Create implementation file with constructor and scale parsing**

```cpp
#include "ChordPopup.h"
#include <algorithm>

namespace ui {

ChordPopup::ChordPopup()
{
    setVisible(false);
    setWantsKeyboardFocus(true);
}

void ChordPopup::show(int rootNote, int instrument, const std::string& scaleLock)
{
    selection_.rootNote = rootNote;
    selection_.degree = 0;
    selection_.typeIndex = 0;
    selection_.inversion = 0;
    instrumentIndex_ = instrument;
    scaleLock_ = scaleLock;
    currentColumn_ = 0;

    // Parse scale lock (e.g., "C Major", "A Minor")
    if (!scaleLock.empty())
    {
        // Extract root note
        char rootChar = scaleLock[0];
        scaleRoot_ = 0;
        if (rootChar >= 'A' && rootChar <= 'G')
        {
            static const int noteOffsets[] = {9, 11, 0, 2, 4, 5, 7}; // A=9, B=11, C=0, etc.
            scaleRoot_ = noteOffsets[rootChar - 'A'];
        }

        // Check for sharp
        if (scaleLock.length() > 1 && scaleLock[1] == '#')
            scaleRoot_ = (scaleRoot_ + 1) % 12;

        // Check for minor
        isMinorScale_ = (scaleLock.find("Minor") != std::string::npos ||
                         scaleLock.find("minor") != std::string::npos);
    }
    else
    {
        // Default to C Major
        scaleRoot_ = 0;
        isMinorScale_ = false;
    }

    updateChordNotes();
    setVisible(true);
    grabKeyboardFocus();
    startTimerHz(60); // For preview debounce
}

void ChordPopup::showAtCell(int cursorTrack, int cursorRow, int existingNote,
                            int instrument, const std::string& scaleLock)
{
    cursorTrack_ = cursorTrack;

    if (existingNote >= 0)
    {
        // Use existing note as root
        show(existingNote, instrument, scaleLock);
    }
    else
    {
        // Default to middle C
        show(60, instrument, scaleLock);
    }
}

void ChordPopup::hide()
{
    setVisible(false);
    stopTimer();
}

int ChordPopup::calculateRootFromDegree() const
{
    const auto& degrees = isMinorScale_ ? kMinorScaleDegrees : kMajorScaleDegrees;
    if (selection_.degree < 0 || selection_.degree >= static_cast<int>(degrees.size()))
        return 60;

    int baseNote = 60 + scaleRoot_; // Middle C + scale root offset
    return baseNote + degrees[selection_.degree].semitonesFromRoot;
}

void ChordPopup::updateChordNotes()
{
    selection_.rootNote = calculateRootFromDegree();

    const auto& chordType = kChordTypes[selection_.typeIndex];
    selection_.notes.clear();

    for (int interval : chordType.intervals)
    {
        selection_.notes.push_back(selection_.rootNote + interval);
    }

    applyInversion();

    // Trigger debounced preview
    previewDebounce_ = kPreviewDebounceMs / 16; // ~60fps timer ticks
}

void ChordPopup::applyInversion()
{
    if (selection_.inversion <= 0 || selection_.notes.empty())
        return;

    int inversions = std::min(selection_.inversion,
                               static_cast<int>(selection_.notes.size()) - 1);

    for (int i = 0; i < inversions; ++i)
    {
        // Move lowest note up an octave
        selection_.notes[i] += 12;
    }

    // Sort to maintain low-to-high order
    std::sort(selection_.notes.begin(), selection_.notes.end());
}

void ChordPopup::timerCallback()
{
    if (previewDebounce_ > 0)
    {
        previewDebounce_--;
        if (previewDebounce_ == 0)
        {
            triggerPreview();
        }
    }
}

void ChordPopup::triggerPreview()
{
    if (onChordPreview && !selection_.notes.empty())
    {
        onChordPreview(selection_.notes, instrumentIndex_);
    }
}

std::string ChordPopup::getChordDisplayName() const
{
    const auto& degrees = isMinorScale_ ? kMinorScaleDegrees : kMajorScaleDegrees;
    std::string name = degrees[selection_.degree].roman;
    name += " " + kChordTypes[selection_.typeIndex].name;

    if (selection_.inversion > 0)
    {
        static const char* invNames[] = {"", " (1st inv)", " (2nd inv)", " (3rd inv)",
                                          " (4th inv)", " (5th inv)", " (6th inv)"};
        if (selection_.inversion < 7)
            name += invNames[selection_.inversion];
    }

    return name;
}

void ChordPopup::resized()
{
    // Centered in parent - handled in paint
}

} // namespace ui
```

**Step 2: Add to CMakeLists.txt**

In the `target_sources(Vitracker PRIVATE` section, after `src/ui/HelpPopup.cpp`, add:

```cmake
    src/ui/ChordPopup.cpp
```

**Step 3: Run build to verify compilation**

Run: `cmake --build build 2>&1 | grep -i error | head -10`
Expected: No errors (paint and keyPressed not yet implemented but declared)

**Step 4: Commit**

```bash
git add src/ui/ChordPopup.cpp CMakeLists.txt
git commit -m "Add ChordPopup constructor and chord calculation logic"
```

---

## Task 5: Implement Piano Keyboard Drawing

**Files:**
- Modify: `src/ui/ChordPopup.cpp`

**Step 1: Add piano keyboard drawing method**

Add before the closing `} // namespace ui`:

```cpp
void ChordPopup::drawPianoKeyboard(juce::Graphics& g, juce::Rectangle<int> area)
{
    // Draw 2 octaves of piano centered on the chord
    int numWhiteKeys = 14; // 2 octaves
    int whiteKeyWidth = area.getWidth() / numWhiteKeys;
    int blackKeyWidth = whiteKeyWidth * 2 / 3;
    int blackKeyHeight = area.getHeight() * 2 / 3;

    // Find the lowest note to display (center chord in view)
    int lowestChordNote = selection_.notes.empty() ? 60 : selection_.notes.front();
    int startOctave = (lowestChordNote / 12) - 1;
    int startNote = startOctave * 12; // Start at C of that octave

    // Pattern of white keys: C D E F G A B (0, 2, 4, 5, 7, 9, 11 in octave)
    static const int whiteKeyNotes[] = {0, 2, 4, 5, 7, 9, 11};
    static const bool hasBlackKeyAfter[] = {true, true, false, true, true, true, false};

    // Draw white keys first
    int x = area.getX();
    for (int i = 0; i < numWhiteKeys; ++i)
    {
        int octave = startOctave + (i / 7);
        int noteInOctave = whiteKeyNotes[i % 7];
        int midiNote = octave * 12 + noteInOctave;

        bool isChordNote = std::find(selection_.notes.begin(), selection_.notes.end(),
                                      midiNote) != selection_.notes.end();

        juce::Rectangle<int> keyRect(x, area.getY(), whiteKeyWidth - 1, area.getHeight());

        g.setColour(isChordNote ? chordNoteColor : whiteKeyColor);
        g.fillRect(keyRect);
        g.setColour(borderColor);
        g.drawRect(keyRect);

        // Draw note name on C keys
        if (noteInOctave == 0)
        {
            g.setColour(dimColor);
            g.setFont(10.0f);
            g.drawText("C" + juce::String(octave), keyRect.removeFromBottom(15),
                       juce::Justification::centred);
        }

        x += whiteKeyWidth;
    }

    // Draw black keys on top
    x = area.getX();
    for (int i = 0; i < numWhiteKeys; ++i)
    {
        int octave = startOctave + (i / 7);
        int noteInOctave = whiteKeyNotes[i % 7];

        if (hasBlackKeyAfter[i % 7])
        {
            int blackNote = octave * 12 + noteInOctave + 1;
            bool isChordNote = std::find(selection_.notes.begin(), selection_.notes.end(),
                                          blackNote) != selection_.notes.end();

            int blackX = x + whiteKeyWidth - blackKeyWidth / 2;
            juce::Rectangle<int> keyRect(blackX, area.getY(), blackKeyWidth, blackKeyHeight);

            g.setColour(isChordNote ? chordNoteColor : blackKeyColor);
            g.fillRect(keyRect);
            g.setColour(borderColor);
            g.drawRect(keyRect);
        }

        x += whiteKeyWidth;
    }
}
```

**Step 2: Run build to verify**

Run: `cmake --build build 2>&1 | grep -i error | head -10`
Expected: No errors

**Step 3: Commit**

```bash
git add src/ui/ChordPopup.cpp
git commit -m "Add piano keyboard drawing"
```

---

## Task 6: Implement Column Selector Drawing

**Files:**
- Modify: `src/ui/ChordPopup.cpp`

**Step 1: Add column selector drawing method**

Add before `drawPianoKeyboard`:

```cpp
void ChordPopup::drawColumnSelector(juce::Graphics& g, juce::Rectangle<int> area)
{
    int columnWidth = area.getWidth() / 3;
    int padding = 10;
    int rowHeight = 20;

    const auto& degrees = isMinorScale_ ? kMinorScaleDegrees : kMajorScaleDegrees;
    int maxInversions = static_cast<int>(selection_.notes.size());

    // Column headers
    g.setColour(dimColor);
    g.setFont(juce::Font(13.0f).boldened());

    auto col1 = area.removeFromLeft(columnWidth);
    auto col2 = area.removeFromLeft(columnWidth);
    auto col3 = area;

    g.drawText("Degree", col1.removeFromTop(rowHeight), juce::Justification::centred);
    g.drawText("Type", col2.removeFromTop(rowHeight), juce::Justification::centred);
    g.drawText("Inversion", col3.removeFromTop(rowHeight), juce::Justification::centred);

    g.setFont(13.0f);

    // Degree column
    for (int i = 0; i < static_cast<int>(degrees.size()); ++i)
    {
        auto rowArea = col1.removeFromTop(rowHeight);
        bool isSelected = (i == selection_.degree);
        bool isCurrentColumn = (currentColumn_ == 0);

        if (isSelected)
        {
            g.setColour(isCurrentColumn ? selectedColor.withAlpha(0.3f) : dimColor.withAlpha(0.2f));
            g.fillRect(rowArea.reduced(padding, 2));
        }

        g.setColour(isSelected ? highlightColor : textColor);
        std::string display = isCurrentColumn && isSelected ? "> " + degrees[i].roman + " <" : degrees[i].roman;
        g.drawText(display, rowArea, juce::Justification::centred);
    }

    // Type column
    for (int i = 0; i < static_cast<int>(kChordTypes.size()); ++i)
    {
        auto rowArea = col2.removeFromTop(rowHeight);
        bool isSelected = (i == selection_.typeIndex);
        bool isCurrentColumn = (currentColumn_ == 1);

        if (isSelected)
        {
            g.setColour(isCurrentColumn ? selectedColor.withAlpha(0.3f) : dimColor.withAlpha(0.2f));
            g.fillRect(rowArea.reduced(padding, 2));
        }

        g.setColour(isSelected ? highlightColor : textColor);
        std::string display = isCurrentColumn && isSelected ? "> " + kChordTypes[i].name + " <" : kChordTypes[i].name;
        g.drawText(display, rowArea, juce::Justification::centred);
    }

    // Inversion column (dynamic based on chord size)
    static const char* inversionNames[] = {"root", "1st", "2nd", "3rd", "4th", "5th", "6th"};
    for (int i = 0; i < maxInversions && i < 7; ++i)
    {
        auto rowArea = col3.removeFromTop(rowHeight);
        bool isSelected = (i == selection_.inversion);
        bool isCurrentColumn = (currentColumn_ == 2);

        if (isSelected)
        {
            g.setColour(isCurrentColumn ? selectedColor.withAlpha(0.3f) : dimColor.withAlpha(0.2f));
            g.fillRect(rowArea.reduced(padding, 2));
        }

        g.setColour(isSelected ? highlightColor : textColor);
        std::string display = isCurrentColumn && isSelected ?
            "> " + std::string(inversionNames[i]) + " <" : inversionNames[i];
        g.drawText(display, rowArea, juce::Justification::centred);
    }
}
```

**Step 2: Run build to verify**

Run: `cmake --build build 2>&1 | grep -i error | head -10`
Expected: No errors

**Step 3: Commit**

```bash
git add src/ui/ChordPopup.cpp
git commit -m "Add column selector drawing"
```

---

## Task 7: Implement paint() Method

**Files:**
- Modify: `src/ui/ChordPopup.cpp`

**Step 1: Add paint method**

Add after `resized()`:

```cpp
void ChordPopup::paint(juce::Graphics& g)
{
    // Semi-transparent overlay
    g.fillAll(juce::Colours::black.withAlpha(0.7f));

    // Calculate centered panel position
    int panelX = (getWidth() - kPopupWidth) / 2;
    int panelY = (getHeight() - kPopupHeight) / 2;
    auto panelBounds = juce::Rectangle<int>(panelX, panelY, kPopupWidth, kPopupHeight);

    // Panel background
    g.setColour(panelColor);
    g.fillRoundedRectangle(panelBounds.toFloat(), 8.0f);
    g.setColour(borderColor);
    g.drawRoundedRectangle(panelBounds.toFloat(), 8.0f, 2.0f);

    auto contentArea = panelBounds.reduced(20);

    // Title
    g.setColour(titleColor);
    g.setFont(juce::Font(18.0f).boldened());
    g.drawText("Chord: " + getChordDisplayName(),
               contentArea.removeFromTop(30), juce::Justification::centred);

    contentArea.removeFromTop(10);

    // Piano keyboard
    drawPianoKeyboard(g, contentArea.removeFromTop(kPianoHeight));

    contentArea.removeFromTop(15);

    // Column selector
    drawColumnSelector(g, contentArea.removeFromTop(200));

    // Footer with instructions
    g.setColour(dimColor);
    g.setFont(12.0f);
    g.drawText("Left/Right: columns  Up/Down: select  Space: preview  Enter: confirm  Esc: cancel",
               contentArea, juce::Justification::centred);
}
```

**Step 2: Run build to verify**

Run: `cmake --build build 2>&1 | grep -i error | head -10`
Expected: No errors

**Step 3: Commit**

```bash
git add src/ui/ChordPopup.cpp
git commit -m "Add ChordPopup paint method"
```

---

## Task 8: Implement Key Handling

**Files:**
- Modify: `src/ui/ChordPopup.cpp`

**Step 1: Add keyPressed method**

Add after `paint()`:

```cpp
bool ChordPopup::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    int keyCode = key.getKeyCode();

    const auto& degrees = isMinorScale_ ? kMinorScaleDegrees : kMajorScaleDegrees;
    int maxInversions = static_cast<int>(kChordTypes[selection_.typeIndex].intervals.size());

    if (keyCode == juce::KeyPress::escapeKey)
    {
        hide();
        return true;
    }

    if (keyCode == juce::KeyPress::returnKey)
    {
        // Check if we have enough tracks
        int numNotesNeeded = static_cast<int>(selection_.notes.size());
        int tracksAvailable = 16 - cursorTrack_;

        if (numNotesNeeded > tracksAvailable)
        {
            // Could show warning, for now just don't confirm
            return true;
        }

        if (onChordConfirmed && !selection_.notes.empty())
        {
            onChordConfirmed(selection_.notes);
        }
        hide();
        return true;
    }

    if (keyCode == juce::KeyPress::spaceKey)
    {
        triggerPreview();
        return true;
    }

    if (keyCode == juce::KeyPress::leftKey)
    {
        currentColumn_ = std::max(0, currentColumn_ - 1);
        repaint();
        return true;
    }

    if (keyCode == juce::KeyPress::rightKey)
    {
        currentColumn_ = std::min(2, currentColumn_ + 1);
        repaint();
        return true;
    }

    if (keyCode == juce::KeyPress::upKey)
    {
        if (currentColumn_ == 0)
        {
            selection_.degree = std::max(0, selection_.degree - 1);
        }
        else if (currentColumn_ == 1)
        {
            selection_.typeIndex = std::max(0, selection_.typeIndex - 1);
            // Reset inversion if it exceeds new chord's note count
            int newMax = static_cast<int>(kChordTypes[selection_.typeIndex].intervals.size());
            selection_.inversion = std::min(selection_.inversion, newMax - 1);
        }
        else
        {
            selection_.inversion = std::max(0, selection_.inversion - 1);
        }
        updateChordNotes();
        repaint();
        return true;
    }

    if (keyCode == juce::KeyPress::downKey)
    {
        if (currentColumn_ == 0)
        {
            selection_.degree = std::min(static_cast<int>(degrees.size()) - 1,
                                          selection_.degree + 1);
        }
        else if (currentColumn_ == 1)
        {
            selection_.typeIndex = std::min(static_cast<int>(kChordTypes.size()) - 1,
                                             selection_.typeIndex + 1);
            // Reset inversion if it exceeds new chord's note count
            int newMax = static_cast<int>(kChordTypes[selection_.typeIndex].intervals.size());
            selection_.inversion = std::min(selection_.inversion, newMax - 1);
        }
        else
        {
            selection_.inversion = std::min(maxInversions - 1, selection_.inversion + 1);
        }
        updateChordNotes();
        repaint();
        return true;
    }

    return false;
}
```

**Step 2: Run build to verify full compilation**

Run: `cmake --build build 2>&1 | tail -5`
Expected: Build succeeds

**Step 3: Commit**

```bash
git add src/ui/ChordPopup.cpp
git commit -m "Add ChordPopup key handling"
```

---

## Task 9: Add Chord Preview to AudioEngine

**Files:**
- Modify: `src/audio/AudioEngine.h`
- Modify: `src/audio/AudioEngine.cpp`

**Step 1: Add preview method declaration to header**

In `src/audio/AudioEngine.h`, after `void releaseNote(int track);` (around line 52), add:

```cpp
    // Preview a chord (multiple notes at once)
    void previewChord(const std::vector<int>& notes, int instrumentIndex);
```

**Step 2: Add implementation**

In `src/audio/AudioEngine.cpp`, after the `releaseNote` method, add:

```cpp
void AudioEngine::previewChord(const std::vector<int>& notes, int instrumentIndex)
{
    // Use tracks 0, 1, 2, etc. for chord preview
    int track = 0;
    for (int note : notes)
    {
        if (track >= NUM_TRACKS) break;
        triggerNote(track, note, instrumentIndex, 0.8f);
        track++;
    }
}
```

**Step 3: Run build to verify**

Run: `cmake --build build 2>&1 | tail -5`
Expected: Build succeeds

**Step 4: Commit**

```bash
git add src/audio/AudioEngine.h src/audio/AudioEngine.cpp
git commit -m "Add chord preview method to AudioEngine"
```

---

## Task 10: Integrate ChordPopup into PatternScreen

**Files:**
- Modify: `src/ui/PatternScreen.h`
- Modify: `src/ui/PatternScreen.cpp`

**Step 1: Add include and member to header**

In `src/ui/PatternScreen.h`, add include after other includes:

```cpp
#include "ChordPopup.h"
```

Add member variable in private section (after `std::string nameBuffer_;`):

```cpp
    // Chord popup
    std::unique_ptr<ChordPopup> chordPopup_;
    void showChordPopup();
```

**Step 2: Add callback declaration to header**

In public section, after `std::function<void(int instrumentIndex)> onJumpToInstrument;`:

```cpp
    std::function<void(const std::vector<int>& notes, int instrument)> onChordPreview;
```

**Step 3: Initialize in constructor**

In `src/ui/PatternScreen.cpp`, in the constructor after `Screen(project, modeManager)`, add:

```cpp
    chordPopup_ = std::make_unique<ChordPopup>();
    addChildComponent(chordPopup_.get());
```

**Step 4: Add showChordPopup method**

Add after `halvePattern()` method:

```cpp
void PatternScreen::showChordPopup()
{
    auto* pattern = project_.getPattern(currentPattern_);
    if (!pattern) return;

    const auto& step = pattern->getStep(cursorTrack_, cursorRow_);
    int existingNote = step.note >= 0 ? step.note : -1;
    int instrument = step.instrument >= 0 ? step.instrument : 0;

    // Get scale lock from chain (if pattern is in a chain)
    std::string scaleLock = getScaleLockForPattern(currentPattern_);

    chordPopup_->setBounds(getLocalBounds());
    chordPopup_->showAtCell(cursorTrack_, cursorRow_, existingNote, instrument, scaleLock);

    // Set up callbacks
    chordPopup_->onChordConfirmed = [this](const std::vector<int>& notes) {
        auto* pat = project_.getPattern(currentPattern_);
        if (!pat) return;

        const auto& currentStep = pat->getStep(cursorTrack_, cursorRow_);
        int instrument = currentStep.instrument >= 0 ? currentStep.instrument : 0;

        // Place notes across adjacent tracks
        for (size_t i = 0; i < notes.size() && cursorTrack_ + i < 16; ++i)
        {
            auto& step = pat->getStep(cursorTrack_ + static_cast<int>(i), cursorRow_);
            step.note = static_cast<int8_t>(notes[i]);
            step.instrument = static_cast<int16_t>(instrument);
        }
        repaint();
    };

    chordPopup_->onChordPreview = [this](const std::vector<int>& notes, int instrument) {
        if (onChordPreview)
            onChordPreview(notes, instrument);
    };
}
```

**Step 5: Handle 'c' key in handleEditKey**

In `handleEditKey`, find a suitable place (e.g., after checking for other single-key commands) and add:

```cpp
    // Chord popup
    if (textChar == 'c' && !shiftHeld)
    {
        showChordPopup();
        return true;
    }
```

**Step 6: Add to help content**

In `getHelpContent()`, in the "Editing" section, add:

```cpp
        {"c", "Open chord entry popup"},
```

**Step 7: Run build to verify**

Run: `cmake --build build 2>&1 | tail -10`
Expected: Build succeeds

**Step 8: Commit**

```bash
git add src/ui/PatternScreen.h src/ui/PatternScreen.cpp
git commit -m "Integrate ChordPopup into PatternScreen"
```

---

## Task 11: Wire Up Chord Preview in App

**Files:**
- Modify: `src/App.cpp`

**Step 1: Add chord preview callback setup**

In `App.cpp`, find where `patternScreen->onNotePreview` is set up (around line 125). After that block, add:

```cpp
        patternScreen->onChordPreview = [this](const std::vector<int>& notes, int instrument) {
            if (audioEngine_.isPlaying()) return;
            audioEngine_.previewChord(notes, instrument);
            previewNoteCounter_ = PREVIEW_NOTE_FRAMES;
        };
```

**Step 2: Run build to verify**

Run: `cmake --build build 2>&1 | tail -5`
Expected: Build succeeds

**Step 3: Commit**

```bash
git add src/App.cpp
git commit -m "Wire up chord preview callback in App"
```

---

## Task 12: Test the Feature Manually

**Step 1: Build and run**

Run: `cmake --build build && ./build/Vitracker_artefacts/Release/Vitracker.app/Contents/MacOS/Vitracker`

**Step 2: Test chord popup**

1. Go to Pattern screen (press 3)
2. Press `c` to open chord popup
3. Verify piano keyboard displays
4. Use arrow keys to navigate columns and change values
5. Verify preview plays when values change
6. Press Enter to place chord
7. Verify notes appear on adjacent tracks
8. Press `?` to verify help shows chord entry

**Step 3: Commit test confirmation**

```bash
git add -A
git commit -m "Complete chord entry feature implementation"
```

---

## Task 13: Push and Create Pull Request

**Step 1: Push feature branch**

```bash
git push -u origin feature/chord-entry
```

**Step 2: Create pull request**

```bash
gh pr create --title "Add chord entry feature" --body "$(cat <<'EOF'
## Summary
- Adds chord entry popup to Pattern screen (press `c`)
- Three-column selector for Degree, Type, and Inversion
- Piano keyboard visualization showing chord notes
- Audio preview with debounce as you navigate
- Places notes across adjacent tracks on confirm
- Supports scale-degree-based chords using chain's scale lock

## Chord Types Supported
- Triads: maj, min, dim, aug
- Sevenths: 7, maj7, min7, dim7, m7b5
- Extended: 9, maj9, min9, 11, 13
- Sus/Add: sus2, sus4, add9

## Test Plan
- [x] Tested on macOS
- [ ] Tested on Windows
- [ ] Tested on Linux

## Screenshots
(Add screenshots of chord popup)

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

---

## Summary

This plan implements the chord entry feature in 13 tasks:

1. Create chord data structures
2. Add static chord definitions
3. Create ChordPopup class declaration
4. Implement constructor and scale parsing
5. Implement piano keyboard drawing
6. Implement column selector drawing
7. Implement paint() method
8. Implement key handling
9. Add chord preview to AudioEngine
10. Integrate ChordPopup into PatternScreen
11. Wire up chord preview in App
12. Manual testing
13. Push and create PR

Each task is a small, focused change with clear verification steps.
