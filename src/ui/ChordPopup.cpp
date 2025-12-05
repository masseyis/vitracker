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

void ChordPopup::paint(juce::Graphics& g)
{
    // TODO: Implement in Task 7
    (void)g;
}

bool ChordPopup::keyPressed(const juce::KeyPress& key)
{
    // TODO: Implement in Task 8
    (void)key;
    return false;
}

} // namespace ui
