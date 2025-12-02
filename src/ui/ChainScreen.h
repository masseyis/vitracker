#pragma once

#include "Screen.h"

namespace ui {

class ChainScreen : public Screen
{
public:
    ChainScreen(model::Project& project, input::ModeManager& modeManager);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void navigate(int dx, int dy) override;
    void handleEdit(const juce::KeyPress& key) override;
    void handleEditKey(const juce::KeyPress& key);

    std::string getTitle() const override { return "CHAIN"; }

private:
    void drawChainList(juce::Graphics& g, juce::Rectangle<int> area);
    void drawPatternList(juce::Graphics& g, juce::Rectangle<int> area);
    void drawScaleLock(juce::Graphics& g, juce::Rectangle<int> area);
    int getScaleIndex(const std::string& scale) const;
    std::string getScaleName(int index) const;

    int currentChain_ = 0;
    int cursorRow_ = 0;  // 0 = name, 1 = scale lock, 2+ = patterns
    int scrollOffset_ = 0;
    bool editingName_ = false;
    std::string nameBuffer_;

    static constexpr int NUM_SCALES = 25;
};

} // namespace ui
