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
    bool handleEdit(const juce::KeyPress& key) override;
    bool handleEditKey(const juce::KeyPress& key);

    std::string getTitle() const override { return "PROJECT"; }
    std::vector<HelpSection> getHelpContent() const override;

    std::function<void()> onTempoChanged;

private:
    int cursorRow_ = 0;
    bool editingName_ = false;
    std::string nameBuffer_;

    enum Field { NAME = 0, TEMPO, GROOVE, NUM_FIELDS };

    // Groove template names (must match GrooveManager order)
    static constexpr int NUM_GROOVES = 5;
    int getGrooveIndex(const std::string& name) const;
    std::string getGrooveName(int index) const;
};

} // namespace ui
