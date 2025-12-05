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

bool ChordPopup::keyPressed(const juce::KeyPress& key)
{
    // TODO: Implement in Task 8
    (void)key;
    return false;
}

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

} // namespace ui
