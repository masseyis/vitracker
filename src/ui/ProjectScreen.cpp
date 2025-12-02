#include "ProjectScreen.h"

namespace ui {

ProjectScreen::ProjectScreen(model::Project& project, input::ModeManager& modeManager)
    : Screen(project, modeManager)
{
}

void ProjectScreen::paint(juce::Graphics& g)
{
    g.fillAll(bgColor);

    g.setFont(24.0f);
    g.setColour(fgColor);
    g.drawText("PROJECT", 20, 20, 200, 30, juce::Justification::centredLeft);

    g.setFont(16.0f);

    int y = 80;
    int rowHeight = 40;

    // Name field
    g.setColour(cursorRow_ == NAME ? cursorColor : fgColor);
    g.drawText("Name:", 40, y, 100, rowHeight, juce::Justification::centredLeft);
    g.drawText(project_.getName(), 150, y, 400, rowHeight, juce::Justification::centredLeft);
    y += rowHeight;

    // Tempo field
    g.setColour(cursorRow_ == TEMPO ? cursorColor : fgColor);
    g.drawText("Tempo:", 40, y, 100, rowHeight, juce::Justification::centredLeft);
    g.drawText(juce::String(project_.getTempo(), 1) + " BPM", 150, y, 400, rowHeight, juce::Justification::centredLeft);
    y += rowHeight;

    // Groove field
    g.setColour(cursorRow_ == GROOVE ? cursorColor : fgColor);
    g.drawText("Groove:", 40, y, 100, rowHeight, juce::Justification::centredLeft);
    g.drawText(project_.getGrooveTemplate(), 150, y, 400, rowHeight, juce::Justification::centredLeft);
    y += rowHeight;

    // Instructions
    y += 40;
    g.setColour(fgColor.darker(0.3f));
    g.setFont(14.0f);
    g.drawText("Press 'i' to edit, +/- to adjust tempo", 40, y, 400, 30, juce::Justification::centredLeft);
}

void ProjectScreen::resized()
{
}

void ProjectScreen::navigate(int dx, int dy)
{
    juce::ignoreUnused(dx);
    cursorRow_ = std::clamp(cursorRow_ + dy, 0, NUM_FIELDS - 1);
    repaint();
}

void ProjectScreen::handleEditKey(const juce::KeyPress& key)
{
    auto textChar = key.getTextCharacter();

    if (cursorRow_ == TEMPO)
    {
        if (textChar == '+' || textChar == '=')
        {
            project_.setTempo(std::min(project_.getTempo() + 1.0f, 300.0f));
            if (onTempoChanged) onTempoChanged();
            repaint();
        }
        else if (textChar == '-')
        {
            project_.setTempo(std::max(project_.getTempo() - 1.0f, 20.0f));
            if (onTempoChanged) onTempoChanged();
            repaint();
        }
    }
}

} // namespace ui
