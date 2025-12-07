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
    bool handleEdit(const juce::KeyPress& key) override;
    bool handleEditKey(const juce::KeyPress& key);

    std::string getTitle() const override { return "SONG - " + project_.getName(); }

    // Key context for centralized key handling
    input::InputContext getInputContext() const override;
    std::vector<HelpSection> getHelpContent() const override;

    // Callback when Enter is pressed on a chain cell
    std::function<void(int chainIndex)> onJumpToChain;

    // Callback for new project (with confirmation)
    std::function<void()> onNewProject;

    // Callback for project rename
    std::function<void(const std::string& newName)> onProjectRenamed;

    // Get chain at current cursor position
    int getChainAtCursor() const { return getChainAt(cursorTrack_, cursorRow_); }

private:
    void drawGrid(juce::Graphics& g, juce::Rectangle<int> area);
    void drawCell(juce::Graphics& g, juce::Rectangle<int> area, int track, int row, bool isPlayheadRow = false);
    int getChainAt(int track, int row) const;
    void setChainAt(int track, int row, int chainIndex);

    int cursorTrack_ = 0;
    int cursorRow_ = 0;
    int scrollOffset_ = 0;
    static constexpr int VISIBLE_ROWS = 16;

    // Name editing
    bool editingName_ = false;
    std::string nameBuffer_;
};

} // namespace ui
