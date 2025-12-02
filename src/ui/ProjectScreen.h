#pragma once

#include "Screen.h"

namespace ui {

class ProjectScreen : public Screen
{
public:
    ProjectScreen(model::Project& project, input::ModeManager& modeManager);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void navigate(int dx, int dy) override;
    void handleEditKey(const juce::KeyPress& key);

    std::string getTitle() const override { return "PROJECT"; }

    std::function<void()> onTempoChanged;

private:
    int cursorRow_ = 0;

    enum Field { NAME = 0, TEMPO, GROOVE, NUM_FIELDS };
};

} // namespace ui
