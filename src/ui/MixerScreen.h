#pragma once

#include "Screen.h"

namespace ui {

class MixerScreen : public Screen
{
public:
    MixerScreen(model::Project& project, input::ModeManager& modeManager);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void navigate(int dx, int dy) override;
    bool handleEdit(const juce::KeyPress& key) override;
    bool handleEditKey(const juce::KeyPress& key);

    std::string getTitle() const override { return "MIXER"; }

private:
    void drawInstrumentStrip(juce::Graphics& g, juce::Rectangle<int> area, int instrumentIndex, bool isSelected);
    void drawMasterStrip(juce::Graphics& g, juce::Rectangle<int> area);

    int getVisibleInstrumentCount() const;
    int getMaxVisibleStrips() const;

    int cursorInstrument_ = 0;  // Index into instrument list, or -1 for master
    int cursorRow_ = 0;         // 0 = volume, 1 = pan, 2 = mute, 3 = solo
    int scrollOffset_ = 0;      // For scrolling when many instruments

    static constexpr int kStripWidth = 60;
    static constexpr int kMasterWidth = 80;
};

} // namespace ui
