#include "ChannelScreen.h"
#include "HelpPopup.h"
#include <algorithm>
#include <cmath>

namespace ui {

ChannelScreen::ChannelScreen(model::Project& project, input::ModeManager& modeManager)
    : Screen(project, modeManager) {}

void ChannelScreen::setCurrentInstrument(int index) {
    if (index >= 0 && index < project_.getInstrumentCount()) {
        currentInstrument_ = index;
        repaint();
    }
}

void ChannelScreen::paint(juce::Graphics& g) {
    g.fillAll(bgColor);

    auto area = getLocalBounds();

    // Header with instrument tabs
    auto headerArea = area.removeFromTop(60);
    drawInstrumentTabs(g, headerArea);

    // Rows
    area.removeFromTop(10);  // Padding

    for (int row = 0; row < kNumRows; ++row) {
        auto rowArea = area.removeFromTop(kRowHeight);
        bool isSelected = (row == cursorRow_);
        drawRow(g, rowArea, row, isSelected);
    }
}

void ChannelScreen::resized() {
    // No child components to layout
}

void ChannelScreen::drawInstrumentTabs(juce::Graphics& g, juce::Rectangle<int> area) {
    g.setColour(headerColor);
    g.fillRect(area);

    // Title
    g.setColour(fgColor);
    g.setFont(16.0f);
    g.drawText("CHANNEL", area.removeFromLeft(100), juce::Justification::centredLeft);

    // Current instrument name
    if (auto* inst = project_.getInstrument(currentInstrument_)) {
        g.setColour(cursorColor);
        juce::String label = juce::String::formatted("%02d: %s",
            currentInstrument_, inst->getName().c_str());
        g.drawText(label, area.removeFromRight(200), juce::Justification::centredRight);
    }
}

void ChannelScreen::drawRow(juce::Graphics& g, juce::Rectangle<int> area, int row, bool selected) {
    auto* inst = project_.getInstrument(currentInstrument_);
    if (!inst) return;

    const auto& strip = inst->getChannelStrip();
    const auto& sends = inst->getSends();

    // Selection highlight
    if (selected) {
        g.setColour(highlightColor);
        g.fillRect(area);
    }

    // Row label and shortcut key
    g.setColour(selected ? cursorColor : fgColor);
    g.setFont(14.0f);

    static const char* rowLabels[] = {
        "[h] HPF", "[l] LOW SHELF", "[m] MID EQ", "[e] HIGH SHELF",
        "[d] DRIVE", "[p] PUNCH", "[o] OTT",
        "[v] VOLUME", "[n] PAN",
        "[r] REVERB", "[y] DELAY", "[c] CHORUS", "[s] SIDECHAIN"
    };

    auto labelArea = area.removeFromLeft(kLabelWidth);
    g.drawText(rowLabels[row], labelArea, juce::Justification::centredLeft);

    // Fixed-width columns for consistent alignment
    constexpr int kFieldWidth = 160;  // Width for each field (bar + value)
    constexpr int kFieldBarWidth = 100;
    constexpr int kFieldSpacing = 16;

    auto paramArea = area;

    // Helper lambda to draw a field with bar + value
    auto drawField = [&](int fieldIndex, float value, float min, float max,
                         const juce::String& valueText, bool bipolar = false) {
        auto fieldArea = paramArea.removeFromLeft(kFieldWidth);

        // Focus indicator if this field is selected
        bool fieldSelected = selected && (cursorField_ == fieldIndex);
        if (fieldSelected) {
            g.setColour(juce::Colour(0xff6666aa));
            g.fillRect(fieldArea.reduced(1, 2));
        }

        // Bar
        auto barArea = fieldArea.removeFromLeft(kFieldBarWidth);
        drawParamBar(g, barArea, value, min, max, bipolar);

        // Value text
        g.setColour(fieldSelected ? cursorColor : fgColor);
        g.drawText(valueText, fieldArea, juce::Justification::centredLeft);

        paramArea.removeFromLeft(kFieldSpacing);
    };

    switch (static_cast<ChannelRowType>(row)) {
        case ChannelRowType::HPF: {
            // Frequency
            juce::String freqText = juce::String(static_cast<int>(strip.hpfFreq)) + "Hz";
            drawField(0, strip.hpfFreq, 20.0f, 500.0f, freqText);

            // Slope (discrete values shown as 0-1 bar)
            static const char* slopes[] = {"OFF", "12dB", "24dB"};
            float slopeNorm = strip.hpfSlope / 2.0f;
            drawField(1, slopeNorm, 0.0f, 1.0f, slopes[strip.hpfSlope]);
            break;
        }

        case ChannelRowType::LowShelf: {
            juce::String gainText = (strip.lowShelfGain >= 0 ? "+" : "") + juce::String(strip.lowShelfGain, 1) + "dB";
            drawField(0, strip.lowShelfGain, -12.0f, 12.0f, gainText, true);

            juce::String freqText = juce::String(static_cast<int>(strip.lowShelfFreq)) + "Hz";
            drawField(1, strip.lowShelfFreq, 50.0f, 500.0f, freqText);
            break;
        }

        case ChannelRowType::MidEQ: {
            juce::String gainText = (strip.midGain >= 0 ? "+" : "") + juce::String(strip.midGain, 1) + "dB";
            drawField(0, strip.midGain, -12.0f, 12.0f, gainText, true);

            juce::String freqText = strip.midFreq >= 1000.0f
                ? juce::String(strip.midFreq / 1000.0f, 1) + "k"
                : juce::String(static_cast<int>(strip.midFreq)) + "Hz";
            drawField(1, strip.midFreq, 200.0f, 8000.0f, freqText);

            juce::String qText = "Q" + juce::String(strip.midQ, 1);
            drawField(2, strip.midQ, 0.5f, 8.0f, qText);
            break;
        }

        case ChannelRowType::HighShelf: {
            juce::String gainText = (strip.highShelfGain >= 0 ? "+" : "") + juce::String(strip.highShelfGain, 1) + "dB";
            drawField(0, strip.highShelfGain, -12.0f, 12.0f, gainText, true);

            juce::String freqText = strip.highShelfFreq >= 1000.0f
                ? juce::String(strip.highShelfFreq / 1000.0f, 1) + "k"
                : juce::String(static_cast<int>(strip.highShelfFreq)) + "Hz";
            drawField(1, strip.highShelfFreq, 2000.0f, 16000.0f, freqText);
            break;
        }

        case ChannelRowType::Drive: {
            juce::String amtText = juce::String(static_cast<int>(strip.driveAmount * 100)) + "%";
            drawField(0, strip.driveAmount, 0.0f, 1.0f, amtText);

            juce::String toneText = juce::String(static_cast<int>(strip.driveTone * 100)) + "%";
            drawField(1, strip.driveTone, 0.0f, 1.0f, toneText);
            break;
        }

        case ChannelRowType::Punch: {
            juce::String punchText = juce::String(static_cast<int>(strip.punchAmount * 100)) + "%";
            drawField(0, strip.punchAmount, 0.0f, 1.0f, punchText);
            break;
        }

        case ChannelRowType::OTT: {
            // 3-band OTT: Low, Mid, High depths + Mix
            juce::String lowText = juce::String(static_cast<int>(strip.ottLowDepth * 100)) + "%";
            drawField(0, strip.ottLowDepth, 0.0f, 1.0f, "L:" + lowText);

            juce::String midText = juce::String(static_cast<int>(strip.ottMidDepth * 100)) + "%";
            drawField(1, strip.ottMidDepth, 0.0f, 1.0f, "M:" + midText);

            juce::String highText = juce::String(static_cast<int>(strip.ottHighDepth * 100)) + "%";
            drawField(2, strip.ottHighDepth, 0.0f, 1.0f, "H:" + highText);

            juce::String mixText = juce::String(static_cast<int>(strip.ottMix * 100)) + "%";
            drawField(3, strip.ottMix, 0.0f, 1.0f, mixText);
            break;
        }

        case ChannelRowType::Volume: {
            float db = 20.0f * std::log10(std::max(0.0001f, inst->getVolume()));
            juce::String volText = juce::String(db, 1) + "dB";
            drawField(0, inst->getVolume(), 0.0f, 1.0f, volText);
            break;
        }

        case ChannelRowType::Pan: {
            float pan = inst->getPan();
            juce::String panText = (std::abs(pan) < 0.01f) ? "C"
                : (pan < 0 ? juce::String(static_cast<int>(-pan * 100)) + "L"
                           : juce::String(static_cast<int>(pan * 100)) + "R");
            drawField(0, pan, -1.0f, 1.0f, panText, true);
            break;
        }

        case ChannelRowType::Reverb: {
            juce::String text = juce::String(static_cast<int>(sends.reverb * 100)) + "%";
            drawField(0, sends.reverb, 0.0f, 1.0f, text);
            break;
        }

        case ChannelRowType::Delay: {
            juce::String text = juce::String(static_cast<int>(sends.delay * 100)) + "%";
            drawField(0, sends.delay, 0.0f, 1.0f, text);
            break;
        }

        case ChannelRowType::Chorus: {
            juce::String text = juce::String(static_cast<int>(sends.chorus * 100)) + "%";
            drawField(0, sends.chorus, 0.0f, 1.0f, text);
            break;
        }

        case ChannelRowType::Sidechain: {
            juce::String text = juce::String(static_cast<int>(sends.sidechainDuck * 100)) + "%";
            drawField(0, sends.sidechainDuck, 0.0f, 1.0f, text);
            break;
        }

        default:
            break;
    }
}

void ChannelScreen::drawParamBar(juce::Graphics& g, juce::Rectangle<int> area,
                                  float value, float min, float max, bool bipolar) {
    area = area.reduced(2, 4);

    // Background
    g.setColour(juce::Colour(0xff333355));
    g.fillRect(area);

    // Value bar
    g.setColour(cursorColor);

    if (bipolar) {
        float center = area.getX() + area.getWidth() / 2.0f;
        float normalized = (value - min) / (max - min);  // 0-1
        float pos = area.getX() + normalized * area.getWidth();

        if (value >= 0) {
            g.fillRect(juce::Rectangle<float>(center, static_cast<float>(area.getY()),
                pos - center, static_cast<float>(area.getHeight())));
        } else {
            g.fillRect(juce::Rectangle<float>(pos, static_cast<float>(area.getY()),
                center - pos, static_cast<float>(area.getHeight())));
        }
    } else {
        float normalized = (value - min) / (max - min);
        int barWidth = static_cast<int>(normalized * area.getWidth());
        g.fillRect(area.removeFromLeft(barWidth));
    }
}

void ChannelScreen::navigate(int dx, int dy) {
    // Vertical navigation
    if (dy != 0) {
        cursorRow_ = std::clamp(cursorRow_ + dy, 0, kNumRows - 1);
        cursorField_ = 0;  // Reset field on row change
    }

    // Horizontal navigation between fields
    if (dx != 0) {
        int numFields = getNumFieldsForRow(cursorRow_);
        cursorField_ = std::clamp(cursorField_ + dx, 0, numFields - 1);
    }

    repaint();
}

bool ChannelScreen::handleEdit(const juce::KeyPress& key) {
    auto* inst = project_.getInstrument(currentInstrument_);
    if (!inst) return false;

    auto keyCode = key.getKeyCode();
    char textChar = static_cast<char>(key.getTextCharacter());
    bool altHeld = key.getModifiers().isAltDown();
    bool shiftHeld = key.getModifiers().isShiftDown();

    // Jump keys
    int jumpRow = getRowForJumpKey(textChar);
    if (jumpRow >= 0) {
        cursorRow_ = jumpRow;
        cursorField_ = 0;
        repaint();
        return true;
    }

    // Instrument switching with [ and ]
    if (textChar == '[') {
        setCurrentInstrument(std::max(0, currentInstrument_ - 1));
        return true;
    }
    if (textChar == ']') {
        setCurrentInstrument(std::min(project_.getInstrumentCount() - 1, currentInstrument_ + 1));
        return true;
    }

    // Value adjustment with Alt+arrows
    if (altHeld) {
        float delta = shiftHeld ? 10.0f : 1.0f;  // Coarse adjustment with Shift

        if (keyCode == juce::KeyPress::leftKey) {
            adjustParam(cursorRow_, cursorField_, -delta);
            return true;
        }
        if (keyCode == juce::KeyPress::rightKey) {
            adjustParam(cursorRow_, cursorField_, delta);
            return true;
        }
        if (keyCode == juce::KeyPress::upKey) {
            adjustParam(cursorRow_, cursorField_, delta);
            return true;
        }
        if (keyCode == juce::KeyPress::downKey) {
            adjustParam(cursorRow_, cursorField_, -delta);
            return true;
        }
    }

    // +/- for quick adjustment (no Alt required)
    if (textChar == '+' || textChar == '=') {
        float delta = shiftHeld ? 10.0f : 1.0f;
        adjustParam(cursorRow_, cursorField_, delta);
        return true;
    }
    if (textChar == '-') {
        float delta = shiftHeld ? 10.0f : 1.0f;
        adjustParam(cursorRow_, cursorField_, -delta);
        return true;
    }

    return false;
}

int ChannelScreen::getNumFieldsForRow(int row) const {
    switch (static_cast<ChannelRowType>(row)) {
        case ChannelRowType::HPF:       return 2;  // freq, slope
        case ChannelRowType::LowShelf:  return 2;  // gain, freq
        case ChannelRowType::MidEQ:     return 3;  // gain, freq, Q
        case ChannelRowType::HighShelf: return 2;  // gain, freq
        case ChannelRowType::Drive:     return 2;  // amount, tone
        case ChannelRowType::OTT:       return 4;  // low, mid, high, mix
        default:                        return 1;
    }
}

int ChannelScreen::getRowForJumpKey(char key) const {
    switch (key) {
        case 'h': return static_cast<int>(ChannelRowType::HPF);
        case 'l': return static_cast<int>(ChannelRowType::LowShelf);
        case 'm': return static_cast<int>(ChannelRowType::MidEQ);
        case 'e': return static_cast<int>(ChannelRowType::HighShelf);
        case 'd': return static_cast<int>(ChannelRowType::Drive);
        case 'p': return static_cast<int>(ChannelRowType::Punch);
        case 'o': return static_cast<int>(ChannelRowType::OTT);
        case 'v': return static_cast<int>(ChannelRowType::Volume);
        case 'n': return static_cast<int>(ChannelRowType::Pan);
        case 'r': return static_cast<int>(ChannelRowType::Reverb);
        case 'y': return static_cast<int>(ChannelRowType::Delay);
        case 'c': return static_cast<int>(ChannelRowType::Chorus);
        case 's': return static_cast<int>(ChannelRowType::Sidechain);
        default:  return -1;
    }
}

void ChannelScreen::adjustParam(int row, int field, float delta) {
    auto* inst = project_.getInstrument(currentInstrument_);
    if (!inst) return;

    auto& strip = inst->getChannelStrip();
    auto& sends = inst->getSends();

    switch (static_cast<ChannelRowType>(row)) {
        case ChannelRowType::HPF:
            if (field == 0) {
                strip.hpfFreq = std::clamp(strip.hpfFreq + delta * 10.0f, 20.0f, 500.0f);
            } else {
                strip.hpfSlope = std::clamp(strip.hpfSlope + static_cast<int>(delta > 0 ? 1 : -1), 0, 2);
            }
            break;

        case ChannelRowType::LowShelf:
            if (field == 0) {
                strip.lowShelfGain = std::clamp(strip.lowShelfGain + delta, -12.0f, 12.0f);
            } else {
                strip.lowShelfFreq = std::clamp(strip.lowShelfFreq + delta * 10.0f, 50.0f, 500.0f);
            }
            break;

        case ChannelRowType::MidEQ:
            if (field == 0) {
                strip.midGain = std::clamp(strip.midGain + delta, -12.0f, 12.0f);
            } else if (field == 1) {
                strip.midFreq = std::clamp(strip.midFreq + delta * 100.0f, 200.0f, 8000.0f);
            } else {
                strip.midQ = std::clamp(strip.midQ + delta * 0.1f, 0.5f, 8.0f);
            }
            break;

        case ChannelRowType::HighShelf:
            if (field == 0) {
                strip.highShelfGain = std::clamp(strip.highShelfGain + delta, -12.0f, 12.0f);
            } else {
                strip.highShelfFreq = std::clamp(strip.highShelfFreq + delta * 100.0f, 2000.0f, 16000.0f);
            }
            break;

        case ChannelRowType::Drive:
            if (field == 0) {
                strip.driveAmount = std::clamp(strip.driveAmount + delta * 0.01f, 0.0f, 1.0f);
            } else {
                strip.driveTone = std::clamp(strip.driveTone + delta * 0.01f, 0.0f, 1.0f);
            }
            break;

        case ChannelRowType::Punch:
            strip.punchAmount = std::clamp(strip.punchAmount + delta * 0.01f, 0.0f, 1.0f);
            break;

        case ChannelRowType::OTT:
            if (field == 0) {
                strip.ottLowDepth = std::clamp(strip.ottLowDepth + delta * 0.01f, 0.0f, 1.0f);
            } else if (field == 1) {
                strip.ottMidDepth = std::clamp(strip.ottMidDepth + delta * 0.01f, 0.0f, 1.0f);
            } else if (field == 2) {
                strip.ottHighDepth = std::clamp(strip.ottHighDepth + delta * 0.01f, 0.0f, 1.0f);
            } else {
                strip.ottMix = std::clamp(strip.ottMix + delta * 0.01f, 0.0f, 1.0f);
            }
            break;

        case ChannelRowType::Volume:
            inst->setVolume(inst->getVolume() + delta * 0.01f);
            break;

        case ChannelRowType::Pan:
            inst->setPan(inst->getPan() + delta * 0.01f);
            break;

        case ChannelRowType::Reverb:
            sends.reverb = std::clamp(sends.reverb + delta * 0.01f, 0.0f, 1.0f);
            break;

        case ChannelRowType::Delay:
            sends.delay = std::clamp(sends.delay + delta * 0.01f, 0.0f, 1.0f);
            break;

        case ChannelRowType::Chorus:
            sends.chorus = std::clamp(sends.chorus + delta * 0.01f, 0.0f, 1.0f);
            break;

        case ChannelRowType::Sidechain:
            sends.sidechainDuck = std::clamp(sends.sidechainDuck + delta * 0.01f, 0.0f, 1.0f);
            break;

        default:
            break;
    }

    repaint();
}

std::vector<HelpSection> ChannelScreen::getHelpContent() const {
    return {
        {"Navigation", {
            {"j/k or Up/Down", "Move between rows"},
            {"h/l or Left/Right", "Move between fields"},
            {"[ / ]", "Previous/next instrument"},
        }},
        {"Editing", {
            {"Alt+h/l or Alt+Left/Right", "Adjust parameter value"},
            {"Alt+Shift+*", "Coarse adjustment"},
            {"Jump keys (h,l,m,e,d,p,o,v,n,r,y,c,s)", "Jump to row"},
        }},
        {"Sections", {
            {"HPF", "High-pass filter (low cut)"},
            {"Low/Mid/High", "3-band EQ"},
            {"Drive", "Saturation"},
            {"Punch", "Transient shaper"},
            {"OTT", "Multiband dynamics"},
        }},
    };
}

} // namespace ui
