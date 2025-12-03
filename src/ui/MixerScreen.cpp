#include "MixerScreen.h"

namespace ui {

MixerScreen::MixerScreen(model::Project& project, input::ModeManager& modeManager)
    : Screen(project, modeManager)
{
}

int MixerScreen::getVisibleInstrumentCount() const
{
    return project_.getInstrumentCount();
}

int MixerScreen::getMaxVisibleStrips() const
{
    int availableWidth = getWidth() - kMasterWidth - 40;  // 40 for margins
    return std::max(1, availableWidth / kStripWidth);
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

    int numInstruments = getVisibleInstrumentCount();
    int maxVisible = getMaxVisibleStrips();

    // Adjust scroll offset if needed
    if (cursorInstrument_ >= 0)
    {
        if (cursorInstrument_ < scrollOffset_)
            scrollOffset_ = cursorInstrument_;
        else if (cursorInstrument_ >= scrollOffset_ + maxVisible)
            scrollOffset_ = cursorInstrument_ - maxVisible + 1;
    }

    // Reserve space for master strip on the right
    auto masterArea = area.removeFromRight(kMasterWidth);
    area.removeFromRight(20);  // Gap before master

    // Draw instrument strips
    if (numInstruments == 0)
    {
        // No instruments - show message
        g.setFont(14.0f);
        g.setColour(fgColor.darker(0.3f));
        g.drawText("No instruments created", area, juce::Justification::centred);
    }
    else
    {
        int visibleCount = std::min(maxVisible, numInstruments - scrollOffset_);

        for (int i = 0; i < visibleCount; ++i)
        {
            int instrumentIndex = scrollOffset_ + i;
            auto stripArea = area.removeFromLeft(kStripWidth);
            bool isSelected = (cursorInstrument_ == instrumentIndex);
            drawInstrumentStrip(g, stripArea, instrumentIndex, isSelected);
        }

        // Show scroll indicators if needed
        if (scrollOffset_ > 0)
        {
            g.setColour(fgColor);
            g.drawText("<", area.getX() - 20, area.getY(), 20, 20, juce::Justification::centred);
        }
        if (scrollOffset_ + maxVisible < numInstruments)
        {
            g.setColour(fgColor);
            g.drawText(">", area.getX(), area.getY(), 20, 20, juce::Justification::centred);
        }
    }

    // Master strip (always visible)
    drawMasterStrip(g, masterArea);
}

void MixerScreen::drawInstrumentStrip(juce::Graphics& g, juce::Rectangle<int> area, int instrumentIndex, bool isSelected)
{
    auto* instrument = project_.getInstrument(instrumentIndex);
    if (!instrument) return;

    // Instrument number and name
    g.setFont(11.0f);
    g.setColour(isSelected ? cursorColor : fgColor);

    juce::String label = juce::String(instrumentIndex + 1) + ":" +
                         juce::String(instrument->getName()).substring(0, 5);
    g.drawText(label, area.removeFromTop(20), juce::Justification::centred);

    // Volume fader
    auto faderArea = area.removeFromTop(120);
    int faderWidth = 20;
    int faderX = faderArea.getCentreX() - faderWidth / 2;

    // Fader background
    g.setColour(highlightColor);
    g.fillRect(faderX, faderArea.getY(), faderWidth, faderArea.getHeight());

    // Volume fill
    float vol = instrument->getVolume();
    int fillHeight = static_cast<int>(faderArea.getHeight() * vol);
    g.setColour(isSelected && cursorRow_ == 0 ? cursorColor : juce::Colour(0xff4a9090));
    g.fillRect(faderX, faderArea.getBottom() - fillHeight, faderWidth, fillHeight);

    // Volume value
    g.setFont(10.0f);
    g.setColour(isSelected && cursorRow_ == 0 ? cursorColor : fgColor);
    g.drawText(juce::String(static_cast<int>(vol * 100)), area.removeFromTop(15), juce::Justification::centred);

    area.removeFromTop(3);

    // Pan display
    float pan = instrument->getPan();
    juce::String panStr = pan < -0.01f ? "L" + juce::String(static_cast<int>(-pan * 100)) :
                          pan > 0.01f ? "R" + juce::String(static_cast<int>(pan * 100)) : "C";
    g.setColour(isSelected && cursorRow_ == 1 ? cursorColor : fgColor.darker(0.3f));
    g.drawText(panStr, area.removeFromTop(18), juce::Justification::centred);

    area.removeFromTop(3);

    // Mute button
    bool muted = instrument->isMuted();
    g.setColour(muted ? juce::Colours::red :
                (isSelected && cursorRow_ == 2 ? cursorColor : fgColor.darker(0.5f)));
    auto muteArea = area.removeFromTop(18);
    if (muted)
    {
        g.fillRoundedRectangle(muteArea.toFloat().reduced(8, 2), 3.0f);
        g.setColour(juce::Colours::white);
    }
    g.drawText("M", muteArea, juce::Justification::centred);

    area.removeFromTop(2);

    // Solo button
    bool soloed = instrument->isSoloed();
    g.setColour(soloed ? juce::Colours::yellow :
                (isSelected && cursorRow_ == 3 ? cursorColor : fgColor.darker(0.5f)));
    auto soloArea = area.removeFromTop(18);
    if (soloed)
    {
        g.fillRoundedRectangle(soloArea.toFloat().reduced(8, 2), 3.0f);
        g.setColour(juce::Colours::black);
    }
    g.drawText("S", soloArea, juce::Justification::centred);
}

void MixerScreen::drawMasterStrip(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto& mixer = project_.getMixer();
    bool isSelected = (cursorInstrument_ < 0);

    g.setFont(14.0f);
    g.setColour(isSelected ? cursorColor : fgColor);
    g.drawText("MASTER", area.removeFromTop(20), juce::Justification::centred);

    // Master fader
    auto faderArea = area.removeFromTop(120);
    int faderWidth = 30;
    int faderX = faderArea.getCentreX() - faderWidth / 2;

    g.setColour(highlightColor);
    g.fillRect(faderX, faderArea.getY(), faderWidth, faderArea.getHeight());

    int fillHeight = static_cast<int>(faderArea.getHeight() * mixer.masterVolume);
    g.setColour(isSelected ? cursorColor : fgColor);
    g.fillRect(faderX, faderArea.getBottom() - fillHeight, faderWidth, fillHeight);

    // Value
    g.setFont(12.0f);
    g.drawText(juce::String(static_cast<int>(mixer.masterVolume * 100)) + "%",
               area.removeFromTop(20), juce::Justification::centred);
}

void MixerScreen::resized()
{
}

void MixerScreen::navigate(int dx, int dy)
{
    int numInstruments = getVisibleInstrumentCount();

    if (dx != 0)
    {
        if (cursorInstrument_ < 0)
        {
            // On master, can only go left to instruments
            if (dx < 0 && numInstruments > 0)
                cursorInstrument_ = numInstruments - 1;
        }
        else
        {
            cursorInstrument_ += dx;
            if (cursorInstrument_ < 0)
                cursorInstrument_ = 0;
            else if (cursorInstrument_ >= numInstruments)
                cursorInstrument_ = -1;  // Go to master
        }
    }

    if (dy != 0)
    {
        if (cursorInstrument_ < 0)
        {
            // Master only has volume (row 0)
            cursorRow_ = 0;
        }
        else
        {
            cursorRow_ = std::clamp(cursorRow_ + dy, 0, 3);
        }
    }

    repaint();
}

bool MixerScreen::handleEdit(const juce::KeyPress& key)
{
    return handleEditKey(key);
}

bool MixerScreen::handleEditKey(const juce::KeyPress& key)
{
    auto keyCode = key.getKeyCode();
    auto textChar = key.getTextCharacter();
    bool shiftHeld = key.getModifiers().isShiftDown();

    // Left/Right navigate between instruments
    if (keyCode == juce::KeyPress::leftKey)
    {
        navigate(-1, 0);
        return false;
    }
    if (keyCode == juce::KeyPress::rightKey)
    {
        navigate(1, 0);
        return false;
    }

    // Tab cycles through parameters (vol -> pan -> mute -> solo)
    if (keyCode == juce::KeyPress::tabKey)
    {
        if (cursorInstrument_ < 0)
        {
            // Master only has volume
            cursorRow_ = 0;
        }
        else
        {
            cursorRow_ = (cursorRow_ + 1) % 4;
        }
        repaint();
        return false;
    }

    // Delta for value adjustments (smaller with shift)
    float delta = shiftHeld ? 0.01f : 0.05f;
    bool isAdjust = false;

    // Up/Down adjust values for volume/pan rows, or toggle mute/solo
    if (keyCode == juce::KeyPress::upKey)
    {
        if (cursorRow_ <= 1)  // Volume or Pan
        {
            isAdjust = true;
        }
        else if (cursorRow_ == 2)  // Mute - toggle
        {
            auto* instrument = project_.getInstrument(cursorInstrument_);
            if (instrument) instrument->setMuted(!instrument->isMuted());
            repaint();
            return false;
        }
        else if (cursorRow_ == 3)  // Solo - toggle
        {
            auto* instrument = project_.getInstrument(cursorInstrument_);
            if (instrument) instrument->setSoloed(!instrument->isSoloed());
            repaint();
            return false;
        }
    }
    else if (keyCode == juce::KeyPress::downKey)
    {
        if (cursorRow_ <= 1)  // Volume or Pan
        {
            delta = -delta;
            isAdjust = true;
        }
        else if (cursorRow_ == 2)  // Mute - toggle
        {
            auto* instrument = project_.getInstrument(cursorInstrument_);
            if (instrument) instrument->setMuted(!instrument->isMuted());
            repaint();
            return false;
        }
        else if (cursorRow_ == 3)  // Solo - toggle
        {
            auto* instrument = project_.getInstrument(cursorInstrument_);
            if (instrument) instrument->setSoloed(!instrument->isSoloed());
            repaint();
            return false;
        }
    }

    if (textChar == '+' || textChar == '=')
    {
        isAdjust = true;
    }
    else if (textChar == '-')
    {
        delta = -delta;
        isAdjust = true;
    }

    // Handle master
    if (cursorInstrument_ < 0)
    {
        if (isAdjust)
        {
            auto& mixer = project_.getMixer();
            mixer.masterVolume = std::clamp(mixer.masterVolume + delta, 0.0f, 1.0f);
        }
        repaint();
        return false;
    }

    // Handle instrument
    auto* instrument = project_.getInstrument(cursorInstrument_);
    if (!instrument)
    {
        repaint();
        return false;
    }

    switch (cursorRow_)
    {
        case 0:  // Volume
            if (isAdjust)
                instrument->setVolume(instrument->getVolume() + delta);
            break;

        case 1:  // Pan
            if (isAdjust)
                instrument->setPan(instrument->getPan() + delta);
            break;

        case 2:  // Mute
            if (keyCode == juce::KeyPress::returnKey || textChar == ' ' || textChar == 'm' || textChar == 'M')
                instrument->setMuted(!instrument->isMuted());
            break;

        case 3:  // Solo
            if (keyCode == juce::KeyPress::returnKey || textChar == ' ' || textChar == 's' || textChar == 'S')
                instrument->setSoloed(!instrument->isSoloed());
            break;
    }

    repaint();
    return false;
}

} // namespace ui
