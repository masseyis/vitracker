#pragma once

#include <JuceHeader.h>
#include "model/Project.h"
#include "input/ModeManager.h"
#include "input/KeyHandler.h"
#include "ui/Screen.h"
#include <memory>
#include <array>

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
    void switchScreen(int screenIndex);
    void drawStatusBar(juce::Graphics& g, juce::Rectangle<int> area);

    model::Project project_;
    input::ModeManager modeManager_;
    std::unique_ptr<input::KeyHandler> keyHandler_;

    std::array<std::unique_ptr<ui::Screen>, 6> screens_;
    int currentScreen_ = 3;  // Start on Pattern screen (index 3, key '4')

    bool isPlaying_ = false;

    static constexpr int STATUS_BAR_HEIGHT = 28;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(App)
};
