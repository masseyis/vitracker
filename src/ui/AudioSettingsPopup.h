#pragma once

#include <JuceHeader.h>

namespace ui {

class AudioSettingsPopup : public juce::Component
{
public:
    AudioSettingsPopup(juce::AudioDeviceManager& deviceManager);
    ~AudioSettingsPopup() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    bool isShowing() const { return isVisible(); }
    void show();
    void hide();
    void toggle();

private:
    juce::AudioDeviceSelectorComponent selector_;

    // Layout constants
    static constexpr int kWidth = 500;
    static constexpr int kHeight = 400;
    static constexpr int kPadding = 20;
    static constexpr int kTitleHeight = 30;

    // Colors (matching app theme)
    static inline const juce::Colour bgColor{0xff1a1a2e};
    static inline const juce::Colour panelColor{0xff2a2a4e};
    static inline const juce::Colour borderColor{0xff4a4a6e};
    static inline const juce::Colour titleColor{0xff7c7cff};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSettingsPopup)
};

} // namespace ui
