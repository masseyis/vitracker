#include "AudioSettingsPopup.h"

namespace ui {

AudioSettingsPopup::AudioSettingsPopup(juce::AudioDeviceManager& deviceManager)
    : selector_(deviceManager,
                0,      // minAudioInputChannels
                2,      // maxAudioInputChannels
                0,      // minAudioOutputChannels
                2,      // maxAudioOutputChannels
                true,   // showMidiInputOptions
                true,   // showMidiOutputSelector
                false,  // showChannelsAsStereoPairs
                false)  // hideAdvancedOptionsWithButton
{
    addAndMakeVisible(selector_);
    setVisible(false);
}

void AudioSettingsPopup::paint(juce::Graphics& g)
{
    // Draw semi-transparent background overlay
    g.fillAll(juce::Colour(0x80000000));

    // Calculate panel bounds
    auto bounds = getLocalBounds();
    int panelWidth = kWidth + kPadding * 2;
    int panelHeight = kHeight + kPadding * 2 + kTitleHeight;
    auto panelBounds = bounds.withSizeKeepingCentre(panelWidth, panelHeight);

    // Draw panel background
    g.setColour(panelColor);
    g.fillRoundedRectangle(panelBounds.toFloat(), 8.0f);

    // Draw panel border
    g.setColour(borderColor);
    g.drawRoundedRectangle(panelBounds.toFloat(), 8.0f, 2.0f);

    // Draw title
    g.setColour(titleColor);
    g.setFont(juce::Font(18.0f).boldened());
    auto titleBounds = panelBounds.removeFromTop(kTitleHeight + kPadding);
    g.drawText("Audio/MIDI Settings", titleBounds.reduced(kPadding, 0),
               juce::Justification::centredLeft);

    // Draw hint at the bottom
    g.setColour(juce::Colour(0xff888888));
    g.setFont(juce::Font(12.0f));
    g.drawText("Press ~ or Escape to close",
               panelBounds.removeFromBottom(20).reduced(kPadding, 0),
               juce::Justification::centredRight);
}

void AudioSettingsPopup::resized()
{
    auto bounds = getLocalBounds();
    int panelWidth = kWidth + kPadding * 2;
    int panelHeight = kHeight + kPadding * 2 + kTitleHeight;
    auto panelBounds = bounds.withSizeKeepingCentre(panelWidth, panelHeight);

    // Position selector inside panel (below title, above hint)
    auto selectorBounds = panelBounds.reduced(kPadding);
    selectorBounds.removeFromTop(kTitleHeight);
    selectorBounds.removeFromBottom(20);  // Space for hint text
    selector_.setBounds(selectorBounds);
}

void AudioSettingsPopup::show()
{
    setVisible(true);
    toFront(true);
}

void AudioSettingsPopup::hide()
{
    setVisible(false);
}

void AudioSettingsPopup::toggle()
{
    if (isVisible())
        hide();
    else
        show();
}

} // namespace ui
