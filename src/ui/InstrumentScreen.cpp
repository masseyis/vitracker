#include "InstrumentScreen.h"

namespace ui {

const char* InstrumentScreen::engineNames_[16] = {
    "Virtual Analog", "Waveshaping", "FM", "Grain",
    "Additive", "Wavetable", "Chords", "Vowel/Speech",
    "Swarm", "Noise", "Particle", "String",
    "Modal", "Bass Drum", "Snare Drum", "Hi-Hat"
};

InstrumentScreen::InstrumentScreen(model::Project& project, input::ModeManager& modeManager)
    : Screen(project, modeManager)
{
}

void InstrumentScreen::paint(juce::Graphics& g)
{
    g.fillAll(bgColor);

    auto* instrument = project_.getInstrument(currentInstrument_);
    if (!instrument) return;

    auto area = getLocalBounds().reduced(20);

    // Header
    g.setFont(20.0f);
    g.setColour(fgColor);
    juce::String title = "INSTRUMENT " + juce::String(currentInstrument_ + 1) + ": " + instrument->getName();
    g.drawText(title, area.removeFromTop(40), juce::Justification::centredLeft);

    area.removeFromTop(10);

    // Engine selector
    drawEngineSelector(g, area.removeFromTop(50));

    area.removeFromTop(20);

    // Parameters
    drawParameters(g, area.removeFromTop(200));

    area.removeFromTop(20);

    // Sends
    drawSends(g, area.removeFromTop(150));
}

void InstrumentScreen::drawEngineSelector(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto* instrument = project_.getInstrument(currentInstrument_);
    if (!instrument) return;

    g.setFont(16.0f);
    g.setColour(cursorRow_ == 0 ? cursorColor : fgColor);

    int engine = instrument->getParams().engine;
    g.drawText("Engine: " + juce::String(engineNames_[engine]), area, juce::Justification::centredLeft);
}

void InstrumentScreen::drawParameters(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto* instrument = project_.getInstrument(currentInstrument_);
    if (!instrument) return;

    const auto& params = instrument->getParams();

    struct ParamInfo { const char* name; float value; };
    ParamInfo paramList[] = {
        {"Harmonics", params.harmonics},
        {"Timbre", params.timbre},
        {"Morph", params.morph},
        {"Attack", params.attack},
        {"Decay", params.decay},
        {"LPG Colour", params.lpgColour}
    };

    g.setFont(14.0f);
    int y = area.getY();
    int rowHeight = 30;

    for (int i = 0; i < 6; ++i)
    {
        bool selected = (cursorRow_ == i + 1);
        g.setColour(selected ? cursorColor : fgColor);

        g.drawText(paramList[i].name, area.getX(), y, 100, rowHeight, juce::Justification::centredLeft);

        // Draw slider bar
        int barX = area.getX() + 110;
        int barWidth = 200;
        g.setColour(highlightColor);
        g.fillRect(barX, y + 10, barWidth, 10);
        g.setColour(selected ? cursorColor : fgColor);
        g.fillRect(barX, y + 10, static_cast<int>(barWidth * paramList[i].value), 10);

        // Value text
        g.drawText(juce::String(paramList[i].value, 2), barX + barWidth + 10, y, 50, rowHeight, juce::Justification::centredLeft);

        y += rowHeight;
    }
}

void InstrumentScreen::drawSends(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto* instrument = project_.getInstrument(currentInstrument_);
    if (!instrument) return;

    const auto& sends = instrument->getSends();

    g.setFont(14.0f);
    g.setColour(fgColor);
    g.drawText("SENDS", area.removeFromTop(25), juce::Justification::centredLeft);

    struct SendInfo { const char* name; float value; };
    SendInfo sendList[] = {
        {"Reverb", sends.reverb},
        {"Delay", sends.delay},
        {"Chorus", sends.chorus},
        {"Drive", sends.drive},
        {"Sidechain", sends.sidechainDuck}
    };

    int y = area.getY();
    int rowHeight = 24;

    for (int i = 0; i < 5; ++i)
    {
        bool selected = (cursorRow_ == i + 7);
        g.setColour(selected ? cursorColor : fgColor);

        g.drawText(sendList[i].name, area.getX(), y, 80, rowHeight, juce::Justification::centredLeft);

        int barX = area.getX() + 90;
        int barWidth = 150;
        g.setColour(highlightColor);
        g.fillRect(barX, y + 6, barWidth, 10);
        g.setColour(selected ? cursorColor : fgColor);
        g.fillRect(barX, y + 6, static_cast<int>(barWidth * sendList[i].value), 10);

        y += rowHeight;
    }
}

void InstrumentScreen::resized()
{
}

void InstrumentScreen::navigate(int dx, int dy)
{
    if (dx != 0)
    {
        // Switch instruments
        int numInstruments = project_.getInstrumentCount();
        currentInstrument_ = (currentInstrument_ + dx + numInstruments) % numInstruments;
    }

    if (dy != 0)
    {
        cursorRow_ = std::clamp(cursorRow_ + dy, 0, 11);  // 0=engine, 1-6=params, 7-11=sends
    }

    repaint();
}

void InstrumentScreen::handleEdit(const juce::KeyPress& key)
{
    handleEditKey(key);
}

void InstrumentScreen::handleEditKey(const juce::KeyPress& key)
{
    auto* instrument = project_.getInstrument(currentInstrument_);
    if (!instrument) return;

    auto textChar = key.getTextCharacter();
    float delta = 0.0f;

    if (textChar == '+' || textChar == '=' || key.getKeyCode() == juce::KeyPress::rightKey)
        delta = 0.05f;
    else if (textChar == '-' || key.getKeyCode() == juce::KeyPress::leftKey)
        delta = -0.05f;

    if (delta == 0.0f) return;

    auto& params = instrument->getParams();
    auto& sends = instrument->getSends();

    auto clamp01 = [](float& v, float d) { v = std::clamp(v + d, 0.0f, 1.0f); };

    switch (cursorRow_)
    {
        case 0: params.engine = std::clamp(params.engine + (delta > 0 ? 1 : -1), 0, 15); break;
        case 1: clamp01(params.harmonics, delta); break;
        case 2: clamp01(params.timbre, delta); break;
        case 3: clamp01(params.morph, delta); break;
        case 4: clamp01(params.attack, delta); break;
        case 5: clamp01(params.decay, delta); break;
        case 6: clamp01(params.lpgColour, delta); break;
        case 7: clamp01(sends.reverb, delta); break;
        case 8: clamp01(sends.delay, delta); break;
        case 9: clamp01(sends.chorus, delta); break;
        case 10: clamp01(sends.drive, delta); break;
        case 11: clamp01(sends.sidechainDuck, delta); break;
    }

    // Preview note when changing parameters
    if (onNotePreview) onNotePreview(60, currentInstrument_);

    repaint();
}

} // namespace ui
