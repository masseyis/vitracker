#include "MixerScreen.h"
#include "HelpPopup.h"
#include "../input/KeyHandler.h"

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

    // Reserve space for FX section at bottom
    auto fxArea = area.removeFromBottom(kFxSectionHeight);

    area = area.reduced(10, 0);

    int numInstruments = getVisibleInstrumentCount();
    int maxVisible = getMaxVisibleStrips();

    // Adjust scroll offset if needed
    if (!inFxSection_ && cursorInstrument_ >= 0)
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
            bool isSelected = (!inFxSection_ && cursorInstrument_ == instrumentIndex);
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
    bool masterSelected = (!inFxSection_ && cursorInstrument_ < 0);
    g.setFont(14.0f);
    g.setColour(masterSelected ? cursorColor : fgColor);
    drawMasterStrip(g, masterArea);

    // FX section
    drawFxSection(g, fxArea);
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
    bool isSelected = (!inFxSection_ && cursorInstrument_ < 0);

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

void MixerScreen::drawFxSection(juce::Graphics& g, juce::Rectangle<int> area)
{
    // Separator line
    g.setColour(highlightColor);
    g.drawLine(static_cast<float>(area.getX() + 10), static_cast<float>(area.getY()),
               static_cast<float>(area.getRight() - 10), static_cast<float>(area.getY()), 1.0f);

    area.removeFromTop(5);
    area = area.reduced(10, 0);

    // FX label
    g.setFont(12.0f);
    g.setColour(inFxSection_ ? cursorColor : fgColor.darker(0.3f));
    g.drawText("FX", area.removeFromLeft(30), juce::Justification::centredLeft);

    // Six effect panels - calculate width accounting for 5 gaps between 6 panels
    int totalGaps = 5 * 8;  // 8px gap between panels
    int panelWidth = (area.getWidth() - totalGaps) / 6;
    for (int fx = 0; fx < 6; ++fx)
    {
        auto panelArea = area.removeFromLeft(panelWidth);
        bool isSelected = (inFxSection_ && cursorFx_ == fx);
        int selectedParam = isSelected ? cursorFxParam_ : -1;
        drawFxPanel(g, panelArea, fx, isSelected, selectedParam);
        if (fx < 5)  // Only add gap between panels, not after the last one
            area.removeFromLeft(8);
    }
}

void MixerScreen::drawFxPanel(juce::Graphics& g, juce::Rectangle<int> area, int fxIndex, bool isSelected, int selectedParam)
{
    auto& mixer = project_.getMixer();

    // Panel background
    if (isSelected)
    {
        g.setColour(highlightColor.brighter(0.1f));
        g.fillRoundedRectangle(area.toFloat(), 4.0f);
    }

    // Handle Sidechain panel (index 3) specially
    if (fxIndex == 3)
    {
        // Effect name
        g.setFont(11.0f);
        g.setColour(isSelected ? cursorColor : fgColor);
        g.drawText("SCHAIN", area.removeFromTop(16), juce::Justification::centred);

        area.removeFromTop(2);

        // Source instrument selector (param 0)
        auto sourceArea = area.removeFromTop(22);
        g.setFont(10.0f);
        g.setColour(selectedParam == 0 ? cursorColor : fgColor.darker(0.2f));
        g.drawText("Src", sourceArea.removeFromLeft(22), juce::Justification::centredLeft);

        // Draw source name box
        auto sourceBox = sourceArea.reduced(2, 3);
        g.setColour(highlightColor);
        g.fillRect(sourceBox);
        g.setColour(selectedParam == 0 ? cursorColor : juce::Colour(0xff4a9090));
        g.drawRect(sourceBox, 1);

        // Get source instrument name
        juce::String sourceName = "None";
        if (mixer.sidechainSource >= 0)
        {
            auto* inst = project_.getInstrument(mixer.sidechainSource);
            if (inst)
                sourceName = juce::String(mixer.sidechainSource + 1) + ":" + juce::String(inst->getName()).substring(0, 3);
            else
                sourceName = juce::String(mixer.sidechainSource + 1);
        }
        g.setColour(fgColor);
        g.setFont(9.0f);
        g.drawText(sourceName, sourceBox, juce::Justification::centred);

        area.removeFromTop(2);

        // Ratio parameter (param 1) - how much to duck
        auto ratioArea = area.removeFromTop(22);
        g.setFont(10.0f);
        g.setColour(selectedParam == 1 ? cursorColor : fgColor.darker(0.2f));
        g.drawText("Amt", ratioArea.removeFromLeft(22), juce::Justification::centredLeft);

        // Draw mini slider for ratio
        auto sliderArea = ratioArea.reduced(2, 4);
        g.setColour(highlightColor);
        g.fillRect(sliderArea);
        int fill = static_cast<int>(sliderArea.getWidth() * mixer.sidechainRatio);
        g.setColour(selectedParam == 1 ? cursorColor : juce::Colour(0xff4a9090));
        g.fillRect(sliderArea.getX(), sliderArea.getY(), fill, sliderArea.getHeight());
        g.setColour(fgColor);
        g.setFont(9.0f);
        g.drawText(juce::String(static_cast<int>(mixer.sidechainRatio * 100)), sliderArea, juce::Justification::centred);

        return;
    }

    // Handle DJ Filter panel (index 4) - bipolar slider
    if (fxIndex == 4)
    {
        g.setFont(11.0f);
        g.setColour(isSelected ? cursorColor : fgColor);
        g.drawText("FILTER", area.removeFromTop(16), juce::Justification::centred);

        area.removeFromTop(2);

        // Position parameter (param 0) - bipolar: -1 LP, 0 bypass, +1 HP
        auto posArea = area.removeFromTop(22);
        g.setFont(10.0f);
        g.setColour(selectedParam == 0 ? cursorColor : fgColor.darker(0.2f));

        // Show LP/HP indicator based on position
        juce::String typeLabel = mixer.djFilterPosition < -0.1f ? "LP" :
                                 mixer.djFilterPosition > 0.1f ? "HP" : "--";
        g.drawText(typeLabel, posArea.removeFromLeft(18), juce::Justification::centredLeft);

        // Draw bipolar slider (center = bypass)
        auto sliderArea = posArea.reduced(2, 4);
        g.setColour(highlightColor);
        g.fillRect(sliderArea);

        // Draw center line
        int centerX = sliderArea.getCentreX();
        g.setColour(fgColor.darker(0.5f));
        g.drawVerticalLine(centerX, static_cast<float>(sliderArea.getY()), static_cast<float>(sliderArea.getBottom()));

        // Draw fill from center
        g.setColour(selectedParam == 0 ? cursorColor : juce::Colour(0xff4a9090));
        if (mixer.djFilterPosition < 0) {
            int fillWidth = static_cast<int>(sliderArea.getWidth() * 0.5f * (-mixer.djFilterPosition));
            g.fillRect(centerX - fillWidth, sliderArea.getY(), fillWidth, sliderArea.getHeight());
        } else if (mixer.djFilterPosition > 0) {
            int fillWidth = static_cast<int>(sliderArea.getWidth() * 0.5f * mixer.djFilterPosition);
            g.fillRect(centerX, sliderArea.getY(), fillWidth, sliderArea.getHeight());
        }

        g.setColour(fgColor);
        g.setFont(9.0f);
        g.drawText(juce::String(static_cast<int>(mixer.djFilterPosition * 100)), sliderArea, juce::Justification::centred);

        area.removeFromTop(2);

        // Show "Master" label to indicate this affects master bus
        auto labelArea = area.removeFromTop(22);
        g.setFont(9.0f);
        g.setColour(fgColor.darker(0.4f));
        g.drawText("Master Bus", labelArea, juce::Justification::centred);

        return;
    }

    // Handle Limiter panel (index 5)
    if (fxIndex == 5)
    {
        g.setFont(11.0f);
        g.setColour(isSelected ? cursorColor : fgColor);
        g.drawText("LIMIT", area.removeFromTop(16), juce::Justification::centred);

        area.removeFromTop(2);

        // Threshold parameter (param 0)
        auto threshArea = area.removeFromTop(22);
        g.setFont(10.0f);
        g.setColour(selectedParam == 0 ? cursorColor : fgColor.darker(0.2f));
        g.drawText("Thr", threshArea.removeFromLeft(20), juce::Justification::centredLeft);

        auto slider1Area = threshArea.reduced(2, 4);
        g.setColour(highlightColor);
        g.fillRect(slider1Area);
        int fill1 = static_cast<int>(slider1Area.getWidth() * mixer.limiterThreshold);
        g.setColour(selectedParam == 0 ? cursorColor : juce::Colour(0xff4a9090));
        g.fillRect(slider1Area.getX(), slider1Area.getY(), fill1, slider1Area.getHeight());
        g.setColour(fgColor);
        g.setFont(9.0f);
        g.drawText(juce::String(static_cast<int>(mixer.limiterThreshold * 100)), slider1Area, juce::Justification::centred);

        area.removeFromTop(2);

        // Release parameter (param 1)
        auto relArea = area.removeFromTop(22);
        g.setFont(10.0f);
        g.setColour(selectedParam == 1 ? cursorColor : fgColor.darker(0.2f));
        g.drawText("Rel", relArea.removeFromLeft(20), juce::Justification::centredLeft);

        auto slider2Area = relArea.reduced(2, 4);
        g.setColour(highlightColor);
        g.fillRect(slider2Area);
        int fill2 = static_cast<int>(slider2Area.getWidth() * mixer.limiterRelease);
        g.setColour(selectedParam == 1 ? cursorColor : juce::Colour(0xff4a9090));
        g.fillRect(slider2Area.getX(), slider2Area.getY(), fill2, slider2Area.getHeight());
        g.setColour(fgColor);
        g.setFont(9.0f);
        g.drawText(juce::String(static_cast<int>(mixer.limiterRelease * 100)), slider2Area, juce::Justification::centred);

        return;
    }

    // Standard effect panels (0-2 only)
    // If we get here, fxIndex should be 0, 1, or 2
    if (fxIndex < 0 || fxIndex > 2)
    {
        // Safety check - should never happen
        g.setFont(11.0f);
        g.setColour(juce::Colours::red);
        g.drawText("ERROR", area.removeFromTop(16), juce::Justification::centred);
        return;
    }

    const char* fxNames[] = {"REVERB", "DELAY", "CHORUS"};
    const char* param1Names[] = {"Size", "Time", "Rate"};
    const char* param2Names[] = {"Damp", "Fdbk", "Depth"};

    float param1Values[] = {mixer.reverbSize, mixer.delayTime, mixer.chorusRate};
    float param2Values[] = {mixer.reverbDamping, mixer.delayFeedback, mixer.chorusDepth};

    // Effect name
    g.setFont(11.0f);
    g.setColour(isSelected ? cursorColor : fgColor);
    g.drawText(fxNames[fxIndex], area.removeFromTop(16), juce::Justification::centred);

    area.removeFromTop(2);

    // Parameter 1
    auto param1Area = area.removeFromTop(22);
    g.setFont(10.0f);
    g.setColour(selectedParam == 0 ? cursorColor : fgColor.darker(0.2f));
    g.drawText(param1Names[fxIndex], param1Area.removeFromLeft(35), juce::Justification::centredLeft);

    // Value display for param1
    juce::String val1Str;
    if (fxIndex == 1)  // Delay time shows note value
        val1Str = getDelayTimeLabel(param1Values[fxIndex]);
    else
        val1Str = juce::String(static_cast<int>(param1Values[fxIndex] * 100));

    // Draw mini slider
    auto slider1Area = param1Area.reduced(2, 4);
    g.setColour(highlightColor);
    g.fillRect(slider1Area);
    int fill1 = static_cast<int>(slider1Area.getWidth() * param1Values[fxIndex]);
    g.setColour(selectedParam == 0 ? cursorColor : juce::Colour(0xff4a9090));
    g.fillRect(slider1Area.getX(), slider1Area.getY(), fill1, slider1Area.getHeight());
    g.setColour(fgColor);
    g.setFont(9.0f);
    g.drawText(val1Str, slider1Area, juce::Justification::centred);

    area.removeFromTop(2);

    // Parameter 2
    auto param2Area = area.removeFromTop(22);
    g.setFont(10.0f);
    g.setColour(selectedParam == 1 ? cursorColor : fgColor.darker(0.2f));
    g.drawText(param2Names[fxIndex], param2Area.removeFromLeft(35), juce::Justification::centredLeft);

    juce::String val2Str = juce::String(static_cast<int>(param2Values[fxIndex] * 100));

    // Draw mini slider
    auto slider2Area = param2Area.reduced(2, 4);
    g.setColour(highlightColor);
    g.fillRect(slider2Area);
    int fill2 = static_cast<int>(slider2Area.getWidth() * param2Values[fxIndex]);
    g.setColour(selectedParam == 1 ? cursorColor : juce::Colour(0xff4a9090));
    g.fillRect(slider2Area.getX(), slider2Area.getY(), fill2, slider2Area.getHeight());
    g.setColour(fgColor);
    g.setFont(9.0f);
    g.drawText(val2Str, slider2Area, juce::Justification::centred);
}

juce::String MixerScreen::getDelayTimeLabel(float time) const
{
    // Map time value to note division label
    if (time < 0.15f) return "1/16";
    if (time < 0.3f) return "1/8";
    if (time < 0.45f) return "D1/8";
    if (time < 0.6f) return "1/4";
    if (time < 0.75f) return "D1/4";
    if (time < 0.9f) return "1/2";
    return "D1/2";
}

void MixerScreen::resized()
{
}

void MixerScreen::navigate(int dx, int dy)
{
    int numInstruments = getVisibleInstrumentCount();

    if (inFxSection_)
    {
        // Navigation within FX section
        if (dx != 0)
        {
            cursorFx_ = std::clamp(cursorFx_ + dx, 0, 5);
        }
        if (dy != 0)
        {
            int newParam = cursorFxParam_ + dy;
            if (newParam < 0)
            {
                // Move up out of FX section to mixer strips
                inFxSection_ = false;
                cursorRow_ = 3;  // Go to bottom row of strips
            }
            else
            {
                cursorFxParam_ = std::clamp(newParam, 0, 1);
            }
        }
    }
    else
    {
        // Navigation in mixer strips
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
                // Master only has volume (row 0), down goes to FX
                if (dy > 0)
                {
                    inFxSection_ = true;
                    cursorFxParam_ = 0;
                }
            }
            else
            {
                int newRow = cursorRow_ + dy;
                if (newRow > 3)
                {
                    // Move down to FX section
                    inFxSection_ = true;
                    cursorFxParam_ = 0;
                }
                else
                {
                    cursorRow_ = std::clamp(newRow, 0, 3);
                }
            }
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
    // Use centralized key translation
    auto action = input::KeyHandler::translateKey(key, getInputContext());

    // Determine value adjustment direction and coarseness from actions
    int valueDelta = 0;
    bool isCoarse = false;

    switch (action.action)
    {
        case input::KeyAction::Edit1Inc:
        case input::KeyAction::Edit2Inc:
        case input::KeyAction::ZoomIn:
            valueDelta = 1;
            break;
        case input::KeyAction::Edit1Dec:
        case input::KeyAction::Edit2Dec:
        case input::KeyAction::ZoomOut:
            valueDelta = -1;
            break;
        case input::KeyAction::ShiftEdit1Inc:
        case input::KeyAction::ShiftEdit2Inc:
            valueDelta = 1;
            isCoarse = true;
            break;
        case input::KeyAction::ShiftEdit1Dec:
        case input::KeyAction::ShiftEdit2Dec:
            valueDelta = -1;
            isCoarse = true;
            break;
        default:
            break;
    }

    float delta = isCoarse ? 0.01f : 0.05f;

    // Handle FX section
    if (inFxSection_)
    {
        // Value adjustment in FX section
        if (valueDelta != 0)
        {
            auto& mixer = project_.getMixer();

            // Sidechain source is special - increment/decrement instrument index
            if (cursorFx_ == 3 && cursorFxParam_ == 0)
            {
                int numInst = project_.getInstrumentCount();
                mixer.sidechainSource = std::clamp(mixer.sidechainSource + valueDelta, -1, numInst - 1);
                repaint();
                return false;
            }

            float fDelta = delta * valueDelta;

            // Adjust the selected FX parameter
            switch (cursorFx_)
            {
                case 0:  // Reverb
                    if (cursorFxParam_ == 0)
                        mixer.reverbSize = std::clamp(mixer.reverbSize + fDelta, 0.0f, 1.0f);
                    else
                        mixer.reverbDamping = std::clamp(mixer.reverbDamping + fDelta, 0.0f, 1.0f);
                    break;
                case 1:  // Delay
                    if (cursorFxParam_ == 0)
                        mixer.delayTime = std::clamp(mixer.delayTime + fDelta, 0.0f, 1.0f);
                    else
                        mixer.delayFeedback = std::clamp(mixer.delayFeedback + fDelta, 0.0f, 0.95f);
                    break;
                case 2:  // Chorus
                    if (cursorFxParam_ == 0)
                        mixer.chorusRate = std::clamp(mixer.chorusRate + fDelta, 0.0f, 1.0f);
                    else
                        mixer.chorusDepth = std::clamp(mixer.chorusDepth + fDelta, 0.0f, 1.0f);
                    break;
                case 3:  // Sidechain - only ratio uses slider (param 1)
                    if (cursorFxParam_ == 1)
                        mixer.sidechainRatio = std::clamp(mixer.sidechainRatio + fDelta, 0.0f, 1.0f);
                    break;
                case 4:  // DJ Filter - bipolar position (single param)
                    mixer.djFilterPosition = std::clamp(mixer.djFilterPosition + fDelta, -1.0f, 1.0f);
                    break;
                case 5:  // Limiter
                    if (cursorFxParam_ == 0)
                        mixer.limiterThreshold = std::clamp(mixer.limiterThreshold + fDelta, 0.1f, 1.0f);
                    else
                        mixer.limiterRelease = std::clamp(mixer.limiterRelease + fDelta, 0.01f, 1.0f);
                    break;
            }
            repaint();
            return false;
        }

        return false;
    }

    // Handle actions for mixer strips (not FX section)
    switch (action.action)
    {
        case input::KeyAction::NavLeft:
            navigate(-1, 0);
            return true;
        case input::KeyAction::NavRight:
            navigate(1, 0);
            return true;
        case input::KeyAction::TabNext:
            if (cursorInstrument_ < 0)
                cursorRow_ = 0;  // Master only has volume
            else
                cursorRow_ = (cursorRow_ + 1) % 4;
            repaint();
            return false;
        case input::KeyAction::TabPrev:
            if (cursorInstrument_ < 0)
                cursorRow_ = 0;
            else
                cursorRow_ = (cursorRow_ + 3) % 4;
            repaint();
            return false;
        case input::KeyAction::ToggleMute:
            if (cursorInstrument_ >= 0)
            {
                auto* instrument = project_.getInstrument(cursorInstrument_);
                if (instrument) instrument->setMuted(!instrument->isMuted());
                repaint();
            }
            return true;
        case input::KeyAction::ToggleSolo:
            if (cursorInstrument_ >= 0)
            {
                auto* instrument = project_.getInstrument(cursorInstrument_);
                if (instrument) instrument->setSoloed(!instrument->isSoloed());
                repaint();
            }
            return true;
        default:
            break;
    }

    // Handle master channel
    if (cursorInstrument_ < 0)
    {
        // Value adjustment on master
        if (valueDelta != 0)
        {
            auto& mixer = project_.getMixer();
            mixer.masterVolume = std::clamp(mixer.masterVolume + delta * valueDelta, 0.0f, 1.0f);
            repaint();
        }
        // NavDown goes to FX section
        if (action.action == input::KeyAction::NavDown)
        {
            inFxSection_ = true;
            cursorFxParam_ = 0;
            repaint();
            return true;
        }
        return false;
    }

    // Handle instrument strips
    auto* instrument = project_.getInstrument(cursorInstrument_);
    if (!instrument)
    {
        repaint();
        return false;
    }

    // NavUp/NavDown behavior depends on current row
    if (action.action == input::KeyAction::NavUp || action.action == input::KeyAction::NavDown)
    {
        bool isUp = (action.action == input::KeyAction::NavUp);

        if (cursorRow_ <= 1)  // Volume or Pan - adjust value
        {
            float adjustDelta = isUp ? delta : -delta;
            if (cursorRow_ == 0)
                instrument->setVolume(instrument->getVolume() + adjustDelta);
            else
                instrument->setPan(instrument->getPan() + adjustDelta);
            repaint();
            return false;
        }
        else if (cursorRow_ == 2)  // Mute row - toggle
        {
            instrument->setMuted(!instrument->isMuted());
            repaint();
            return false;
        }
        else if (cursorRow_ == 3)  // Solo row
        {
            if (isUp)
            {
                instrument->setSoloed(!instrument->isSoloed());
            }
            else
            {
                // NavDown from Solo goes to FX section
                inFxSection_ = true;
                cursorFxParam_ = 0;
            }
            repaint();
            return !isUp;  // Consume NavDown to FX
        }
    }

    // Value adjustment for volume/pan
    if (valueDelta != 0 && cursorRow_ <= 1)
    {
        float adjustDelta = delta * valueDelta;
        if (cursorRow_ == 0)
            instrument->setVolume(instrument->getVolume() + adjustDelta);
        else
            instrument->setPan(instrument->getPan() + adjustDelta);
        repaint();
        return false;
    }

    // Confirm action on mute/solo rows toggles
    if (action.action == input::KeyAction::Confirm)
    {
        if (cursorRow_ == 2)
            instrument->setMuted(!instrument->isMuted());
        else if (cursorRow_ == 3)
            instrument->setSoloed(!instrument->isSoloed());
        repaint();
        return false;
    }

    repaint();
    return false;
}

std::vector<HelpSection> MixerScreen::getHelpContent() const
{
    return {
        {"Channel Navigation", {
            {"Left/Right", "Select instrument channel"},
            {"Up/Down", "Move between Vol/Pan/Mute/Solo"},
            {"Down (from Solo)", "Enter FX section"},
            {"Tab", "Cycle through rows"},
        }},
        {"Volume & Pan Rows", {
            {"Up/Down", "Adjust value"},
            {"+/-", "Adjust value"},
            {"Shift", "Fine adjustment (hold)"},
        }},
        {"Mute & Solo Rows", {
            {"Up/Down", "Toggle"},
            {"m", "Toggle mute"},
            {"s", "Toggle solo"},
            {"Enter/Space", "Toggle"},
        }},
        {"FX Section", {
            {"Left/Right", "Select effect"},
            {"Up/Down", "Select parameter / exit to strips"},
            {"Alt+L/R", "Adjust FX parameter"},
            {"+/-", "Adjust FX parameter"},
            {"Tab", "Switch param 1 / param 2"},
        }},
        {"Master Effects", {
            {"Reverb", "Size, Damping"},
            {"Delay", "Time, Feedback"},
            {"Chorus", "Rate, Depth"},
            {"Sidechain", "Source, Amount"},
            {"Filter", "LP/HP position"},
            {"Limiter", "Threshold, Release"},
        }},
    };
}

} // namespace ui
