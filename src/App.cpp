#include "App.h"

App::App()
{
    setSize(1280, 800);
    setWantsKeyboardFocus(true);
    addKeyListener(this);
}

App::~App()
{
    removeKeyListener(this);
}

void App::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));

    g.setColour(juce::Colours::white);
    g.setFont(24.0f);
    g.drawText("VITRACKER", getLocalBounds(), juce::Justification::centred, true);

    g.setFont(14.0f);
    g.drawText("Press 1-6 to switch screens | i=Edit | v=Visual | :=Command | Space=Play",
               getLocalBounds().removeFromBottom(40), juce::Justification::centred, true);
}

void App::resized()
{
}

bool App::keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent)
{
    juce::ignoreUnused(originatingComponent);

    // Placeholder - will dispatch to ModeManager
    DBG("Key pressed: " << key.getTextDescription());
    return true;
}
