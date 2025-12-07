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

    // Parameter display depends on row type
    auto paramArea = area;

    switch (static_cast<ChannelRowType>(row)) {
        case ChannelRowType::HPF: {
            // Frequency bar + slope indicator
            auto barArea = paramArea.removeFromLeft(kBarWidth);
            drawParamBar(g, barArea, strip.hpfFreq, 20.0f, 500.0f);

            g.setColour(fgColor);
            juce::String freqText = juce::String(static_cast<int>(strip.hpfFreq)) + " Hz";
            g.drawText(freqText, paramArea.removeFromLeft(60), juce::Justification::centred);

            static const char* slopes[] = {"OFF", "12dB", "24dB"};
            g.drawText(slopes[strip.hpfSlope], paramArea.removeFromLeft(50), juce::Justification::centred);
            break;
        }

        case ChannelRowType::LowShelf:
        case ChannelRowType::HighShelf: {
            float gain = (row == static_cast<int>(ChannelRowType::LowShelf))
                ? strip.lowShelfGain : strip.highShelfGain;
            float freq = (row == static_cast<int>(ChannelRowType::LowShelf))
                ? strip.lowShelfFreq : strip.highShelfFreq;

            auto barArea = paramArea.removeFromLeft(kBarWidth);
            drawParamBar(g, barArea, gain, -12.0f, 12.0f, true);  // Bipolar

            g.setColour(fgColor);
            juce::String gainText = (gain >= 0 ? "+" : "") + juce::String(gain, 1) + " dB";
            g.drawText(gainText, paramArea.removeFromLeft(70), juce::Justification::centred);

            juce::String freqText = juce::String(static_cast<int>(freq)) + " Hz";
            g.drawText(freqText, paramArea.removeFromLeft(70), juce::Justification::centred);
            break;
        }

        case ChannelRowType::MidEQ: {
            auto barArea = paramArea.removeFromLeft(kBarWidth);
            drawParamBar(g, barArea, strip.midGain, -12.0f, 12.0f, true);

            g.setColour(fgColor);
            juce::String gainText = (strip.midGain >= 0 ? "+" : "") + juce::String(strip.midGain, 1) + " dB";
            g.drawText(gainText, paramArea.removeFromLeft(70), juce::Justification::centred);

            juce::String freqText = strip.midFreq >= 1000.0f
                ? juce::String(strip.midFreq / 1000.0f, 1) + "k"
                : juce::String(static_cast<int>(strip.midFreq));
            g.drawText(freqText, paramArea.removeFromLeft(50), juce::Justification::centred);

            g.drawText("Q" + juce::String(strip.midQ, 1), paramArea.removeFromLeft(50), juce::Justification::centred);
            break;
        }

        case ChannelRowType::Drive: {
            auto barArea = paramArea.removeFromLeft(kBarWidth);
            drawParamBar(g, barArea, strip.driveAmount, 0.0f, 1.0f);

            g.setColour(fgColor);
            g.drawText(juce::String(static_cast<int>(strip.driveAmount * 100)) + "%",
                paramArea.removeFromLeft(50), juce::Justification::centred);
            g.drawText("Tone:" + juce::String(static_cast<int>(strip.driveTone * 100)) + "%",
                paramArea.removeFromLeft(80), juce::Justification::centred);
            break;
        }

        case ChannelRowType::Punch: {
            auto barArea = paramArea.removeFromLeft(kBarWidth);
            drawParamBar(g, barArea, strip.punchAmount, 0.0f, 1.0f);

            g.setColour(fgColor);
            g.drawText(juce::String(static_cast<int>(strip.punchAmount * 100)) + "%",
                paramArea.removeFromLeft(50), juce::Justification::centred);
            break;
        }

        case ChannelRowType::OTT: {
            auto barArea = paramArea.removeFromLeft(kBarWidth);
            drawParamBar(g, barArea, strip.ottDepth, 0.0f, 1.0f);

            g.setColour(fgColor);
            g.drawText("Depth " + juce::String(static_cast<int>(strip.ottDepth * 100)) + "%",
                paramArea.removeFromLeft(80), juce::Justification::centred);
            g.drawText("Mix " + juce::String(static_cast<int>(strip.ottMix * 100)) + "%",
                paramArea.removeFromLeft(70), juce::Justification::centred);
            break;
        }

        case ChannelRowType::Volume: {
            auto barArea = paramArea.removeFromLeft(kBarWidth);
            drawParamBar(g, barArea, inst->getVolume(), 0.0f, 1.0f);

            g.setColour(fgColor);
            float db = 20.0f * std::log10(std::max(0.0001f, inst->getVolume()));
            g.drawText(juce::String(db, 1) + " dB", paramArea.removeFromLeft(70), juce::Justification::centred);
            break;
        }

        case ChannelRowType::Pan: {
            auto barArea = paramArea.removeFromLeft(kBarWidth);
            drawParamBar(g, barArea, inst->getPan(), -1.0f, 1.0f, true);

            g.setColour(fgColor);
            float pan = inst->getPan();
            juce::String panText = (std::abs(pan) < 0.01f) ? "C"
                : (pan < 0 ? juce::String(static_cast<int>(-pan * 100)) + "L"
                           : juce::String(static_cast<int>(pan * 100)) + "R");
            g.drawText(panText, paramArea.removeFromLeft(50), juce::Justification::centred);
            break;
        }

        case ChannelRowType::Reverb:
        case ChannelRowType::Delay:
        case ChannelRowType::Chorus:
        case ChannelRowType::Sidechain: {
            float value = 0.0f;
            switch (static_cast<ChannelRowType>(row)) {
                case ChannelRowType::Reverb: value = sends.reverb; break;
                case ChannelRowType::Delay: value = sends.delay; break;
                case ChannelRowType::Chorus: value = sends.chorus; break;
                case ChannelRowType::Sidechain: value = sends.sidechainDuck; break;
                default: break;
            }

            auto barArea = paramArea.removeFromLeft(kBarWidth);
            drawParamBar(g, barArea, value, 0.0f, 1.0f);

            g.setColour(fgColor);
            g.drawText(juce::String(static_cast<int>(value * 100)) + "%",
                paramArea.removeFromLeft(50), juce::Justification::centred);
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

    // Jump keys (in Edit mode)
    char textChar = static_cast<char>(key.getTextCharacter());
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

    // Value adjustment handled by KeyHandler via Alt+hjkl

    return false;
}

int ChannelScreen::getNumFieldsForRow(int row) const {
    switch (static_cast<ChannelRowType>(row)) {
        case ChannelRowType::HPF:       return 2;  // freq, slope
        case ChannelRowType::LowShelf:  return 2;  // gain, freq
        case ChannelRowType::MidEQ:     return 3;  // gain, freq, Q
        case ChannelRowType::HighShelf: return 2;  // gain, freq
        case ChannelRowType::Drive:     return 2;  // amount, tone
        case ChannelRowType::OTT:       return 3;  // depth, mix, smooth
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
                strip.ottDepth = std::clamp(strip.ottDepth + delta * 0.01f, 0.0f, 1.0f);
            } else if (field == 1) {
                strip.ottMix = std::clamp(strip.ottMix + delta * 0.01f, 0.0f, 1.0f);
            } else {
                strip.ottSmooth = std::clamp(strip.ottSmooth + delta * 0.01f, 0.0f, 1.0f);
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
