#include "MixerScreen.h"

namespace ui {

MixerScreen::MixerScreen(model::Project& project, input::ModeManager& modeManager)
    : Screen(project, modeManager)
{
}

void MixerScreen::paint(juce::Graphics& g)
{
    g.fillAll(bgColor);

    auto area = getLocalBounds();

    // Header
    g.setFont(20.0f);
    g.setColour(fgColor);
    g.drawText("MIXER", area.removeFromTop(40).reduced(20, 0), juce::Justification::centredLeft);

    area.removeFromTop(10);
    area = area.reduced(10, 0);

    // Track strips
    int stripWidth = (area.getWidth() - 80) / 16;  // Reserve 80 for master

    for (int t = 0; t < 16; ++t)
    {
        auto stripArea = area.removeFromLeft(stripWidth);
        drawTrackStrip(g, stripArea, t);
    }

    // Master strip
    area.removeFromLeft(20);  // Gap
    drawMasterStrip(g, area);
}

void MixerScreen::drawTrackStrip(juce::Graphics& g, juce::Rectangle<int> area, int track)
{
    auto& mixer = project_.getMixer();
    bool isSelected = (cursorTrack_ == track);

    // Track number
    g.setFont(12.0f);
    g.setColour(isSelected ? cursorColor : fgColor);
    g.drawText(juce::String(track + 1), area.removeFromTop(20), juce::Justification::centred);

    // Volume fader
    auto faderArea = area.removeFromTop(150);
    int faderWidth = 20;
    int faderX = faderArea.getCentreX() - faderWidth / 2;

    g.setColour(highlightColor);
    g.fillRect(faderX, faderArea.getY(), faderWidth, faderArea.getHeight());

    float vol = mixer.trackVolumes[track];
    int fillHeight = static_cast<int>(faderArea.getHeight() * vol);
    g.setColour(isSelected && cursorRow_ == 0 ? cursorColor : fgColor);
    g.fillRect(faderX, faderArea.getBottom() - fillHeight, faderWidth, fillHeight);

    area.removeFromTop(5);

    // Pan knob (simplified as text)
    g.setColour(isSelected && cursorRow_ == 1 ? cursorColor : fgColor.darker(0.3f));
    float pan = mixer.trackPans[track];
    juce::String panStr = pan < -0.01f ? "L" + juce::String(static_cast<int>(-pan * 100)) :
                          pan > 0.01f ? "R" + juce::String(static_cast<int>(pan * 100)) : "C";
    g.drawText(panStr, area.removeFromTop(20), juce::Justification::centred);

    // Mute button
    g.setColour(mixer.trackMutes[track] ? juce::Colours::red :
                (isSelected && cursorRow_ == 2 ? cursorColor : fgColor.darker(0.5f)));
    g.drawText("M", area.removeFromTop(20), juce::Justification::centred);

    // Solo button
    g.setColour(mixer.trackSolos[track] ? juce::Colours::yellow :
                (isSelected && cursorRow_ == 3 ? cursorColor : fgColor.darker(0.5f)));
    g.drawText("S", area.removeFromTop(20), juce::Justification::centred);
}

void MixerScreen::drawMasterStrip(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto& mixer = project_.getMixer();
    bool isSelected = (cursorTrack_ == 16);

    g.setFont(14.0f);
    g.setColour(isSelected ? cursorColor : fgColor);
    g.drawText("MASTER", area.removeFromTop(20), juce::Justification::centred);

    // Master fader
    auto faderArea = area.removeFromTop(150);
    int faderWidth = 30;
    int faderX = faderArea.getCentreX() - faderWidth / 2;

    g.setColour(highlightColor);
    g.fillRect(faderX, faderArea.getY(), faderWidth, faderArea.getHeight());

    int fillHeight = static_cast<int>(faderArea.getHeight() * mixer.masterVolume);
    g.setColour(isSelected ? cursorColor : fgColor);
    g.fillRect(faderX, faderArea.getBottom() - fillHeight, faderWidth, fillHeight);

    // Value
    g.drawText(juce::String(static_cast<int>(mixer.masterVolume * 100)) + "%",
               area.removeFromTop(20), juce::Justification::centred);
}

void MixerScreen::resized()
{
}

void MixerScreen::navigate(int dx, int dy)
{
    cursorTrack_ = std::clamp(cursorTrack_ + dx, 0, 16);
    cursorRow_ = std::clamp(cursorRow_ + dy, 0, 3);
    repaint();
}

void MixerScreen::handleEditKey(const juce::KeyPress& key)
{
    auto& mixer = project_.getMixer();
    auto textChar = key.getTextCharacter();

    float delta = 0.0f;
    if (textChar == '+' || textChar == '=') delta = 0.05f;
    else if (textChar == '-') delta = -0.05f;

    if (cursorTrack_ < 16)
    {
        switch (cursorRow_)
        {
            case 0:  // Volume
                if (delta != 0.0f)
                    mixer.trackVolumes[cursorTrack_] = std::clamp(mixer.trackVolumes[cursorTrack_] + delta, 0.0f, 1.0f);
                break;
            case 1:  // Pan
                if (delta != 0.0f)
                    mixer.trackPans[cursorTrack_] = std::clamp(mixer.trackPans[cursorTrack_] + delta, -1.0f, 1.0f);
                break;
            case 2:  // Mute
                if (key.getKeyCode() == juce::KeyPress::returnKey || textChar == ' ')
                    mixer.trackMutes[cursorTrack_] = !mixer.trackMutes[cursorTrack_];
                break;
            case 3:  // Solo
                if (key.getKeyCode() == juce::KeyPress::returnKey || textChar == ' ')
                    mixer.trackSolos[cursorTrack_] = !mixer.trackSolos[cursorTrack_];
                break;
        }
    }
    else
    {
        // Master
        if (delta != 0.0f)
            mixer.masterVolume = std::clamp(mixer.masterVolume + delta, 0.0f, 1.0f);
    }

    repaint();
}

} // namespace ui
