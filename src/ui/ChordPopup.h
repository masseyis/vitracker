#pragma once

#include <JuceHeader.h>
#include <string>
#include <vector>
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

class ChordPopup : public juce::Component,
                   public juce::Timer
{
public:
    ChordPopup();
    ~ChordPopup() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
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
    int getDefaultChordTypeForDegree(int degree) const;
    bool isChordTypeInKey(int typeIndex) const;

    ChordSelection selection_;
    std::string scaleLock_;
    int scaleRoot_ = 0;        // 0=C, 1=C#, etc.
    bool isMinorScale_ = false;
    int instrumentIndex_ = 0;
    int cursorTrack_ = 0;

    int currentColumn_ = 0;    // 0=degree, 1=type, 2=inversion
    int previewDebounce_ = 0;  // Countdown for debounced preview

    static constexpr int kPopupWidth = 520;
    static constexpr int kPopupHeight = 520;
    static constexpr int kPianoHeight = 70;
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
    static inline const juce::Colour inKeyColor{0xff66ff66};  // Green for in-key chords

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChordPopup)
};

} // namespace ui
