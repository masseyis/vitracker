#pragma once

#include <JuceHeader.h>

class App : public juce::Component,
            public juce::KeyListener
{
public:
    App();
    ~App() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(App)
};
