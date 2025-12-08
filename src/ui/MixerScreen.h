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
    std::vector<HelpSection> getHelpContent() const override;

private:
    void drawInstrumentStrip(juce::Graphics& g, juce::Rectangle<int> area, int instrumentIndex, bool isSelected);
    void drawMasterStrip(juce::Graphics& g, juce::Rectangle<int> area);
    void drawFxSection(juce::Graphics& g, juce::Rectangle<int> area);
    void drawFxPanel(juce::Graphics& g, juce::Rectangle<int> area, int fxIndex, bool isSelected, int selectedParam);

    int getVisibleInstrumentCount() const;
    int getMaxVisibleStrips() const;

    juce::String getDelayTimeLabel(float time) const;

    int cursorInstrument_ = 0;  // Index into instrument list, or -1 for master
    int cursorRow_ = 0;         // 0 = volume, 1 = pan, 2 = mute, 3 = solo
    int scrollOffset_ = 0;      // For scrolling when many instruments

    // FX section cursor state
    bool inFxSection_ = false;  // True when cursor is in FX section
    int cursorFx_ = 0;          // 0 = reverb, 1 = delay, 2 = chorus, 3 = sidechain, 4 = DJ filter, 5 = limiter
    int cursorFxParam_ = 0;     // 0 = first param, 1 = second param

    static constexpr int kStripWidth = 60;
    static constexpr int kMasterWidth = 80;
    static constexpr int kFxSectionHeight = 80;
};

} // namespace ui
