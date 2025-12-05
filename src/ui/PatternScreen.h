#pragma once

#include "Screen.h"
#include "../model/Clipboard.h"

namespace ui {

class PatternScreen : public Screen
{
public:
    PatternScreen(model::Project& project, input::ModeManager& modeManager);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void navigate(int dx, int dy) override;
    bool handleEdit(const juce::KeyPress& key) override;
    bool handleEditKey(const juce::KeyPress& key);

    std::string getTitle() const override { return "PATTERN"; }
    std::vector<HelpSection> getHelpContent() const override;

    // For external triggering
    std::function<void(int note, int instrument)> onNotePreview;
    std::function<void(int instrumentIndex)> onJumpToInstrument;

    int getCurrentPatternIndex() const { return currentPattern_; }
    void setCurrentPattern(int index) { currentPattern_ = index; }
    int getInstrumentAtCursor() const;

    // Selection and clipboard operations
    void startSelection();
    void updateSelection();
    void yankSelection();
    void deleteSelection();
    void paste();
    void transpose(int semitones);
    void interpolate();
    void randomize(int percent);
    void doublePattern();
    void halvePattern();

    bool hasSelection() const { return hasSelection_; }
    void clearSelection() { hasSelection_ = false; repaint(); }

private:
    void drawPatternTabs(juce::Graphics& g, juce::Rectangle<int> area);
    void drawHeader(juce::Graphics& g, juce::Rectangle<int> area);
    void drawTrackHeaders(juce::Graphics& g, juce::Rectangle<int> area);
    void drawGrid(juce::Graphics& g, juce::Rectangle<int> area);
    void drawStep(juce::Graphics& g, juce::Rectangle<int> area,
                  const model::Step& step, bool isCurrentRow, bool isCurrentCell,
                  int highlightColumn = -1, bool isSelected = false);

    std::string noteToString(int8_t note) const;
    std::string getScaleLockForPattern(int patternIndex) const;
    int moveNoteInScale(int currentNote, int direction, const std::string& scaleLock) const;
    const model::Step* findLastNonEmptyRowAbove(int track, int startRow) const;

    int currentPattern_ = 0;
    int cursorTrack_ = 0;
    int cursorRow_ = 0;
    int cursorColumn_ = 0;  // 0=note, 1=inst, 2=vol, 3-5=fx

    static constexpr int HEADER_HEIGHT = 40;
    static constexpr int TRACK_HEADER_HEIGHT = 36;  // Two rows: track number + column labels
    static constexpr int ROW_HEIGHT = 18;
    static constexpr int COLUMN_WIDTHS[] = {36, 24, 24, 48, 48, 48};  // note, inst, vol, fx1, fx2, fx3

    // Selection state
    model::Selection selection_;
    bool hasSelection_ = false;

    // Name editing
    bool editingName_ = false;
    std::string nameBuffer_;
};

} // namespace ui
