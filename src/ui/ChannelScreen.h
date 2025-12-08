#pragma once

#include "Screen.h"

namespace ui {

// Row types for ChannelScreen
enum class ChannelRowType {
    HPF,           // hpfFreq, hpfSlope
    LowShelf,      // lowShelfGain, lowShelfFreq
    MidEQ,         // midGain, midFreq, midQ
    HighShelf,     // highShelfGain, highShelfFreq
    Drive,         // driveAmount, driveTone
    Punch,         // punchAmount
    OTT,           // ottDepth, ottMix, ottSmooth
    Volume,        // volume (from Instrument)
    Pan,           // pan (from Instrument)
    Reverb,        // reverb send
    Delay,         // delay send
    Chorus,        // chorus send
    Sidechain,     // sidechain duck
    NumRows
};

class ChannelScreen : public Screen {
public:
    ChannelScreen(model::Project& project, input::ModeManager& modeManager);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void navigate(int dx, int dy) override;
    bool handleEdit(const juce::KeyPress& key) override;

    std::string getTitle() const override { return "CHANNEL"; }
    std::vector<HelpSection> getHelpContent() const override;

    int getCurrentInstrument() const { return currentInstrument_; }
    void setCurrentInstrument(int index);

    // Allow KeyHandler to adjust parameters
    void adjustParam(int row, int field, float delta);
    int getCursorRow() const { return cursorRow_; }
    int getCursorField() const { return cursorField_; }

private:
    void drawInstrumentTabs(juce::Graphics& g, juce::Rectangle<int> area);
    void drawRow(juce::Graphics& g, juce::Rectangle<int> area, int row, bool selected);
    void drawParamBar(juce::Graphics& g, juce::Rectangle<int> area, float value,
                      float min, float max, bool bipolar = false);

    int getNumFieldsForRow(int row) const;

    // Jump key handling
    int getRowForJumpKey(char key) const;

    int currentInstrument_ = 0;
    int cursorRow_ = 0;
    int cursorField_ = 0;  // For multi-field rows

    static constexpr int kNumRows = static_cast<int>(ChannelRowType::NumRows);
    static constexpr int kRowHeight = 24;
    static constexpr int kLabelWidth = 100;
    static constexpr int kBarWidth = 160;
};

} // namespace ui
