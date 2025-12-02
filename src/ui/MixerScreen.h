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
    void handleEdit(const juce::KeyPress& key) override;
    void handleEditKey(const juce::KeyPress& key);

    std::string getTitle() const override { return "MIXER"; }

private:
    void drawTrackStrip(juce::Graphics& g, juce::Rectangle<int> area, int track);
    void drawMasterStrip(juce::Graphics& g, juce::Rectangle<int> area);

    int cursorTrack_ = 0;  // 0-15 = tracks, 16 = master
    int cursorRow_ = 0;    // 0 = volume, 1 = pan, 2 = mute, 3 = solo
};

} // namespace ui
