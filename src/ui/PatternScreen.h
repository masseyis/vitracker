#pragma once

#include "Screen.h"

namespace ui {

class PatternScreen : public Screen
{
public:
    PatternScreen(model::Project& project, input::ModeManager& modeManager);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void navigate(int dx, int dy) override;
    void handleEdit(const juce::KeyPress& key) override;
    void handleEditKey(const juce::KeyPress& key);

    std::string getTitle() const override { return "PATTERN"; }

    // For external triggering
    std::function<void(int note, int instrument)> onNotePreview;

    int getCurrentPatternIndex() const { return currentPattern_; }
    void setCurrentPattern(int index) { currentPattern_ = index; }

private:
    void drawHeader(juce::Graphics& g, juce::Rectangle<int> area);
    void drawTrackHeaders(juce::Graphics& g, juce::Rectangle<int> area);
    void drawGrid(juce::Graphics& g, juce::Rectangle<int> area);
    void drawStep(juce::Graphics& g, juce::Rectangle<int> area,
                  const model::Step& step, bool isCurrentRow, bool isCurrentCell);

    std::string noteToString(int8_t note) const;

    int currentPattern_ = 0;
    int cursorTrack_ = 0;
    int cursorRow_ = 0;
    int cursorColumn_ = 0;  // 0=note, 1=inst, 2=vol, 3-5=fx

    static constexpr int HEADER_HEIGHT = 40;
    static constexpr int TRACK_HEADER_HEIGHT = 24;
    static constexpr int ROW_HEIGHT = 18;
    static constexpr int COLUMN_WIDTHS[] = {36, 24, 24, 48, 48, 48};  // note, inst, vol, fx1, fx2, fx3
};

} // namespace ui
