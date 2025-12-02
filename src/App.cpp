#include "App.h"
#include "ui/PatternScreen.h"
#include "ui/ProjectScreen.h"
#include "ui/InstrumentScreen.h"
#include "ui/MixerScreen.h"
#include "ui/SongScreen.h"
#include "ui/ChainScreen.h"
#include "model/ProjectSerializer.h"

App::App()
{
    setSize(1280, 800);
    setWantsKeyboardFocus(true);
    addKeyListener(this);

    // Set up audio
    audioEngine_.setProject(&project_);

    deviceManager_.initialiseWithDefaultDevices(0, 2);
    audioSourcePlayer_.setSource(&audioEngine_);
    deviceManager_.addAudioCallback(&audioSourcePlayer_);

    // Create key handler
    keyHandler_ = std::make_unique<input::KeyHandler>(modeManager_);

    // Set up key handler callbacks
    keyHandler_->onScreenSwitch = [this](int screen) { switchScreen(screen - 1); };
    keyHandler_->onPlayStop = [this]() {
        if (audioEngine_.isPlaying())
            audioEngine_.stop();
        else
            audioEngine_.play();
        repaint();
    };
    keyHandler_->onNavigate = [this](int dx, int dy) {
        if (screens_[currentScreen_])
            screens_[currentScreen_]->navigate(dx, dy);
    };

    // Create screens
    screens_[0] = std::make_unique<ui::ProjectScreen>(project_, modeManager_);
    screens_[1] = std::make_unique<ui::SongScreen>(project_, modeManager_);
    screens_[2] = std::make_unique<ui::ChainScreen>(project_, modeManager_);
    screens_[3] = std::make_unique<ui::PatternScreen>(project_, modeManager_);
    screens_[4] = std::make_unique<ui::InstrumentScreen>(project_, modeManager_);
    screens_[5] = std::make_unique<ui::MixerScreen>(project_, modeManager_);

    // Add active screen as child
    if (screens_[currentScreen_])
    {
        addAndMakeVisible(screens_[currentScreen_].get());
    }

    // Edit mode key forwarding
    keyHandler_->onEditKey = [this](const juce::KeyPress& key) {
        if (auto* patternScreen = dynamic_cast<ui::PatternScreen*>(screens_[currentScreen_].get()))
        {
            patternScreen->handleEditKey(key);
        }
        else if (auto* instrumentScreen = dynamic_cast<ui::InstrumentScreen*>(screens_[currentScreen_].get()))
        {
            instrumentScreen->handleEditKey(key);
        }
        else if (auto* mixerScreen = dynamic_cast<ui::MixerScreen*>(screens_[currentScreen_].get()))
        {
            mixerScreen->handleEditKey(key);
        }
        else if (auto* songScreen = dynamic_cast<ui::SongScreen*>(screens_[currentScreen_].get()))
        {
            songScreen->handleEditKey(key);
        }
        else if (auto* chainScreen = dynamic_cast<ui::ChainScreen*>(screens_[currentScreen_].get()))
        {
            chainScreen->handleEditKey(key);
        }
    };

    // Wire up note preview for PatternScreen
    if (auto* patternScreen = dynamic_cast<ui::PatternScreen*>(screens_[3].get()))
    {
        patternScreen->onNotePreview = [this](int note, int instrument) {
            audioEngine_.triggerNote(0, note, instrument, 1.0f);
        };
    }

    // Wire up note preview for InstrumentScreen
    if (auto* instrumentScreen = dynamic_cast<ui::InstrumentScreen*>(screens_[4].get()))
    {
        instrumentScreen->onNotePreview = [this](int note, int instrument) {
            audioEngine_.triggerNote(0, note, instrument, 1.0f);
        };
    }

    // Mode change callback
    modeManager_.onModeChanged = [this](input::Mode) { repaint(); };

    // File operations
    keyHandler_->onSave = [this](const std::string& filename) { saveProject(filename); };
    keyHandler_->onLoad = [this](const std::string& filename) { loadProject(filename); };
    keyHandler_->onNew = [this]() { newProject(); };

    // Start timer for UI updates (playhead position)
    startTimerHz(30);
}

App::~App()
{
    stopTimer();
    deviceManager_.removeAudioCallback(&audioSourcePlayer_);
    audioSourcePlayer_.setSource(nullptr);
    removeKeyListener(this);
}

void App::timerCallback()
{
    if (audioEngine_.isPlaying())
    {
        repaint();  // Update playhead display
    }
}

void App::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));

    // Draw placeholder for screens not yet implemented
    if (!screens_[currentScreen_])
    {
        g.setColour(juce::Colours::white);
        g.setFont(24.0f);

        static const char* screenNames[] = {"PROJECT", "SONG", "CHAIN", "PATTERN", "INSTRUMENT", "MIXER"};
        g.drawText(juce::String(screenNames[currentScreen_]) + " (Coming Soon)",
                   getLocalBounds().reduced(0, STATUS_BAR_HEIGHT),
                   juce::Justification::centred, true);
    }

    // Status bar at bottom
    drawStatusBar(g, getLocalBounds().removeFromBottom(STATUS_BAR_HEIGHT));
}

void App::resized()
{
    auto area = getLocalBounds();
    area.removeFromBottom(STATUS_BAR_HEIGHT);

    for (auto& screen : screens_)
    {
        if (screen)
            screen->setBounds(area);
    }
}

bool App::keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent)
{
    juce::ignoreUnused(originatingComponent);

    bool handled = keyHandler_->handleKey(key);
    if (handled) repaint();
    return handled;
}

void App::switchScreen(int screenIndex)
{
    if (screenIndex < 0 || screenIndex >= 6) return;
    if (screenIndex == currentScreen_) return;

    // Hide old screen
    if (screens_[currentScreen_])
    {
        screens_[currentScreen_]->onExit();
        screens_[currentScreen_]->setVisible(false);
    }

    currentScreen_ = screenIndex;

    // Show new screen
    if (screens_[currentScreen_])
    {
        screens_[currentScreen_]->onEnter();
        screens_[currentScreen_]->setVisible(true);
        addAndMakeVisible(screens_[currentScreen_].get());
    }

    resized();
    repaint();
}

void App::drawStatusBar(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(juce::Colour(0xff2a2a4e));
    g.fillRect(area);

    g.setFont(14.0f);

    // Mode indicator (left)
    g.setColour(juce::Colours::white);
    g.drawText("-- " + juce::String(modeManager_.getModeString()) + " --",
               area.removeFromLeft(200), juce::Justification::centredLeft, true);

    // Transport (center-left)
    bool isPlaying = audioEngine_.isPlaying();
    g.setColour(isPlaying ? juce::Colours::lightgreen : juce::Colours::grey);
    juce::String transportText = isPlaying ?
        juce::String("PLAYING Row ") + juce::String(audioEngine_.getCurrentRow()) :
        "STOPPED";
    g.drawText(transportText, area.removeFromLeft(150), juce::Justification::centred, true);

    // Tempo (right)
    g.setColour(juce::Colours::white);
    g.drawText(juce::String(project_.getTempo(), 1) + " BPM",
               area.removeFromRight(100), juce::Justification::centredRight, true);

    // Screen indicator
    static const char* screenKeys[] = {"1:PRJ", "2:SNG", "3:CHN", "4:PAT", "5:INS", "6:MIX"};
    int x = area.getX();
    for (int i = 0; i < 6; ++i)
    {
        g.setColour(i == currentScreen_ ? juce::Colours::yellow : juce::Colours::grey);
        g.drawText(screenKeys[i], x, area.getY(), 50, area.getHeight(),
                   juce::Justification::centred, true);
        x += 55;
    }
}

void App::saveProject(const std::string& filename)
{
    if (filename.empty())
    {
        auto chooser = std::make_shared<juce::FileChooser>("Save Project",
            juce::File::getSpecialLocation(juce::File::userHomeDirectory),
            "*.vit");

        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc)
            {
                auto results = fc.getResults();
                if (!results.isEmpty())
                {
                    if (model::ProjectSerializer::save(project_, results[0]))
                        DBG("Project saved to: " << results[0].getFullPathName());
                    else
                        DBG("Failed to save project");
                }
            });
    }
    else
    {
        auto file = juce::File::getSpecialLocation(juce::File::userHomeDirectory)
            .getChildFile(juce::String(filename) + ".vit");

        if (model::ProjectSerializer::save(project_, file))
            DBG("Project saved to: " << file.getFullPathName());
        else
            DBG("Failed to save project");
    }
}

void App::loadProject(const std::string& filename)
{
    if (filename.empty())
    {
        auto chooser = std::make_shared<juce::FileChooser>("Load Project",
            juce::File::getSpecialLocation(juce::File::userHomeDirectory),
            "*.vit");

        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc)
            {
                auto results = fc.getResults();
                if (!results.isEmpty())
                {
                    if (model::ProjectSerializer::load(project_, results[0]))
                    {
                        DBG("Project loaded from: " << results[0].getFullPathName());
                        audioEngine_.stop();
                        repaint();
                    }
                    else
                        DBG("Failed to load project");
                }
            });
    }
    else
    {
        auto file = juce::File::getSpecialLocation(juce::File::userHomeDirectory)
            .getChildFile(juce::String(filename) + ".vit");

        if (model::ProjectSerializer::load(project_, file))
        {
            DBG("Project loaded from: " << file.getFullPathName());
            audioEngine_.stop();
            repaint();
        }
        else
            DBG("Failed to load project");
    }
}

void App::newProject()
{
    audioEngine_.stop();
    project_ = model::Project("Untitled");
    repaint();
}
