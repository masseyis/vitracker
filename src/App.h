#pragma once

#include <JuceHeader.h>
#include "model/Project.h"
#include "model/PresetManager.h"
#include "input/ModeManager.h"
#include "input/KeyHandler.h"
#include "audio/AudioEngine.h"
#include "ui/Screen.h"
#include "ui/HelpPopup.h"
#include <memory>
#include <array>
#include <functional>

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
    model::PresetManager presetManager_;
    input::ModeManager modeManager_;
    std::unique_ptr<input::KeyHandler> keyHandler_;

    audio::AudioEngine audioEngine_;
    juce::AudioDeviceManager deviceManager_;
    juce::AudioSourcePlayer audioSourcePlayer_;

    std::array<std::unique_ptr<ui::Screen>, 6> screens_;
    int currentScreen_ = 2;  // Pattern screen (was 3, now 2 after removing Project)
    int autosaveCounter_ = 0;
    int previewNoteCounter_ = 0;  // Countdown to release preview note
    int autosaveDebounce_ = 0;    // Debounce counter for autosave
    bool projectDirty_ = false;   // Track if project has unsaved changes

    // Tempo adjust mode
    bool tempoAdjustMode_ = false;
    void enterTempoAdjustMode();
    void exitTempoAdjustMode();
    bool handleTempoAdjustKey(const juce::KeyPress& key);

    // Groove cycling
    void cycleGroove(bool reverse);
    static const char* grooveNames_[5];

    // Project name/file management
    juce::File currentProjectFile_;
    void updateWindowTitle();
    juce::File getProjectFilePath() const;
    std::string sanitizeFilename(const std::string& name) const;
    void markDirty();

    // New project with confirmation
    void confirmNewProject();

    // Help popup overlay
    ui::HelpPopup helpPopup_;
    void showHelp();
    void hideHelp();
    void toggleHelp();

    // Tip Me button
    juce::TextButton tipMeButton_{"Tip Me"};

    static constexpr int STATUS_BAR_HEIGHT = 28;
    static constexpr int PREVIEW_NOTE_FRAMES = 6;  // ~200ms at 30fps

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(App)
};
