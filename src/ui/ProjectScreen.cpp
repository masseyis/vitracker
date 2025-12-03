#include "ProjectScreen.h"

namespace ui {

static const char* grooveNames[] = {"Straight", "Swing 50%", "Swing 66%", "MPC 60%", "Humanize"};

ProjectScreen::ProjectScreen(model::Project& project, input::ModeManager& modeManager)
    : Screen(project, modeManager)
{
}

int ProjectScreen::getGrooveIndex(const std::string& name) const
{
    for (int i = 0; i < NUM_GROOVES; ++i)
    {
        if (name == grooveNames[i])
            return i;
    }
    return 0;
}

std::string ProjectScreen::getGrooveName(int index) const
{
    if (index >= 0 && index < NUM_GROOVES)
        return grooveNames[index];
    return grooveNames[0];
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
    if (editingName_ && cursorRow_ == NAME)
        g.drawText(juce::String(nameBuffer_) + "_", 150, y, 400, rowHeight, juce::Justification::centredLeft);
    else
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
    g.drawText("Press 'i' to edit, arrows to adjust values", 40, y, 400, 30, juce::Justification::centredLeft);
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

bool ProjectScreen::handleEdit(const juce::KeyPress& key)
{
    return handleEditKey(key);
}

bool ProjectScreen::handleEditKey(const juce::KeyPress& key)
{
    auto keyCode = key.getKeyCode();
    auto textChar = key.getTextCharacter();

    // Up/Down navigate between fields (unless editing name)
    if (!editingName_)
    {
        if (keyCode == juce::KeyPress::upKey)
        {
            navigate(0, -1);
            return false;
        }
        if (keyCode == juce::KeyPress::downKey)
        {
            navigate(0, 1);
            return false;
        }
    }

    // Handle each field
    if (cursorRow_ == NAME)
    {
        // Start editing if not already
        if (!editingName_)
        {
            editingName_ = true;
            nameBuffer_ = project_.getName();
        }

        if (keyCode == juce::KeyPress::returnKey || keyCode == juce::KeyPress::escapeKey)
        {
            if (keyCode == juce::KeyPress::returnKey)
                project_.setName(nameBuffer_);
            editingName_ = false;
            repaint();
            return true;  // Consume Enter/Escape when in name editing mode
        }
        else if (keyCode == juce::KeyPress::backspaceKey && !nameBuffer_.empty())
        {
            nameBuffer_.pop_back();
        }
        else if (textChar >= ' ' && textChar <= '~' && nameBuffer_.length() < 32)
        {
            nameBuffer_ += static_cast<char>(textChar);
        }
        repaint();
        return editingName_;  // Consume keys when editing name
    }
    else if (cursorRow_ == TEMPO)
    {
        if (textChar == '+' || textChar == '=' || keyCode == juce::KeyPress::rightKey)
        {
            project_.setTempo(std::min(project_.getTempo() + 1.0f, 300.0f));
            if (onTempoChanged) onTempoChanged();
            repaint();
        }
        else if (textChar == '-' || keyCode == juce::KeyPress::leftKey)
        {
            project_.setTempo(std::max(project_.getTempo() - 1.0f, 20.0f));
            if (onTempoChanged) onTempoChanged();
            repaint();
        }
    }
    else if (cursorRow_ == GROOVE)
    {
        int currentGroove = getGrooveIndex(project_.getGrooveTemplate());
        if (textChar == '+' || textChar == '=' || keyCode == juce::KeyPress::rightKey)
        {
            int newGroove = (currentGroove + 1) % NUM_GROOVES;
            project_.setGrooveTemplate(getGrooveName(newGroove));
            repaint();
        }
        else if (textChar == '-' || keyCode == juce::KeyPress::leftKey)
        {
            int newGroove = (currentGroove - 1 + NUM_GROOVES) % NUM_GROOVES;
            project_.setGrooveTemplate(getGrooveName(newGroove));
            repaint();
        }
    }
    return false;
}

} // namespace ui
