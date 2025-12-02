#pragma once

#include "Screen.h"

namespace ui {

class InstrumentScreen : public Screen
{
public:
    InstrumentScreen(model::Project& project, input::ModeManager& modeManager);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void navigate(int dx, int dy) override;
    void handleEdit(const juce::KeyPress& key) override;
    void handleEditKey(const juce::KeyPress& key);

    std::string getTitle() const override { return "INSTRUMENT"; }

    std::function<void(int note, int instrument)> onNotePreview;

private:
    void drawEngineSelector(juce::Graphics& g, juce::Rectangle<int> area);
    void drawParameters(juce::Graphics& g, juce::Rectangle<int> area);
    void drawSends(juce::Graphics& g, juce::Rectangle<int> area);

    int currentInstrument_ = 0;
    int cursorRow_ = 0;

    static constexpr int NUM_PARAMS = 10;  // engine + 6 params + name + 3 visible at once from sends

    static const char* engineNames_[16];
};

} // namespace ui
