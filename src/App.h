#pragma once

#include <JuceHeader.h>
#include "model/Project.h"
#include "input/ModeManager.h"
#include "input/KeyHandler.h"
#include "audio/AudioEngine.h"
#include "ui/Screen.h"
#include <memory>
#include <array>

class App : public juce::Component,
            public juce::KeyListener,
            public juce::Timer
{
public:
    App();
    ~App() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;

    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;

    void timerCallback() override;

private:
    void switchScreen(int screenIndex);
    void drawStatusBar(juce::Graphics& g, juce::Rectangle<int> area);

    void saveProject(const std::string& filename = "");
    void loadProject(const std::string& filename);
    void newProject();
    void autosave();
    juce::File getAutosavePath() const;

    model::Project project_;
    input::ModeManager modeManager_;
    std::unique_ptr<input::KeyHandler> keyHandler_;

    audio::AudioEngine audioEngine_;
    juce::AudioDeviceManager deviceManager_;
    juce::AudioSourcePlayer audioSourcePlayer_;

    std::array<std::unique_ptr<ui::Screen>, 6> screens_;
    int currentScreen_ = 3;
    int autosaveCounter_ = 0;
    int previewNoteCounter_ = 0;  // Countdown to release preview note

    static constexpr int STATUS_BAR_HEIGHT = 28;
    static constexpr int PREVIEW_NOTE_FRAMES = 6;  // ~200ms at 30fps

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(App)
};
