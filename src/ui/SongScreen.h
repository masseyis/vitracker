#pragma once

#include "Screen.h"

namespace ui {

class SongScreen : public Screen
{
public:
    SongScreen(model::Project& project, input::ModeManager& modeManager);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void navigate(int dx, int dy) override;
    void handleEdit(const juce::KeyPress& key) override;
    void handleEditKey(const juce::KeyPress& key);

    std::string getTitle() const override { return "SONG"; }

private:
    void drawGrid(juce::Graphics& g, juce::Rectangle<int> area);
    void drawCell(juce::Graphics& g, juce::Rectangle<int> area, int track, int row);
    int getChainAt(int track, int row) const;
    void setChainAt(int track, int row, int chainIndex);

    int cursorTrack_ = 0;
    int cursorRow_ = 0;
    int scrollOffset_ = 0;
    static constexpr int VISIBLE_ROWS = 16;
};

} // namespace ui
