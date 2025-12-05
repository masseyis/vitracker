#include "App.h"
#include "ui/PatternScreen.h"
#include "ui/ProjectScreen.h"
#include "ui/InstrumentScreen.h"
#include "ui/MixerScreen.h"
#include "ui/SongScreen.h"
#include "ui/ChainScreen.h"
#include "model/ProjectSerializer.h"
#include "model/UndoManager.h"

App::App()
{
    setSize(1280, 800);
    setWantsKeyboardFocus(true);
    addKeyListener(this);

    // Initialize preset manager
    presetManager_.initialize();

    // Set up audio
    audioEngine_.setProject(&project_);

    deviceManager_.initialiseWithDefaultDevices(0, 2);
    audioSourcePlayer_.setSource(&audioEngine_);
    deviceManager_.addAudioCallback(&audioSourcePlayer_);

    // Create key handler
    keyHandler_ = std::make_unique<input::KeyHandler>(modeManager_);

    // Set up key handler callbacks
    keyHandler_->onScreenSwitch = [this](int screen) {
        if (screen == -1)  // Previous screen (Shift+[)
        {
            int prev = (currentScreen_ - 1 + 6) % 6;
            switchScreen(prev);
        }
        else if (screen == -2)  // Next screen (Shift+])
        {
            int next = (currentScreen_ + 1) % 6;
            switchScreen(next);
        }
        else
        {
            switchScreen(screen - 1);  // Number keys 1-6
        }
    };
    keyHandler_->onPlayStop = [this]() {
        if (audioEngine_.isPlaying())
        {
            audioEngine_.stop();
        }
        else
        {
            // Set play mode based on current screen
            if (currentScreen_ == 1)  // Song screen
                audioEngine_.setPlayMode(audio::AudioEngine::PlayMode::Song);
            else
                audioEngine_.setPlayMode(audio::AudioEngine::PlayMode::Pattern);

            // Sync current pattern from PatternScreen
            if (auto* patternScreen = dynamic_cast<ui::PatternScreen*>(screens_[3].get()))
            {
                audioEngine_.setCurrentPattern(patternScreen->getCurrentPatternIndex());
            }

            audioEngine_.play();
        }
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

    // Give screens access to audio engine for playhead display
    for (auto& screen : screens_)
    {
        if (screen)
            screen->setAudioEngine(&audioEngine_);
    }

    // Add active screen as child
    if (screens_[currentScreen_])
    {
        addAndMakeVisible(screens_[currentScreen_].get());
    }

    // Initialize help popup (hidden by default, always on top)
    addChildComponent(helpPopup_);

    // Initialize Tip Me button
    tipMeButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffff5e5b));
    tipMeButton_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    tipMeButton_.onClick = []() {
        juce::URL("https://ko-fi.com/masseyis").launchInDefaultBrowser();
    };
    addAndMakeVisible(tipMeButton_);

    // Edit mode key forwarding - use virtual handleEdit() on all screens
    keyHandler_->onEditKey = [this](const juce::KeyPress& key) -> bool {
        if (screens_[currentScreen_])
        {
            return screens_[currentScreen_]->handleEdit(key);
        }
        return false;
    };

    // Wire up note preview for PatternScreen
    if (auto* patternScreen = dynamic_cast<ui::PatternScreen*>(screens_[3].get()))
    {
        patternScreen->onNotePreview = [this](int note, int instrument) {
            // Don't preview while track is playing - it conflicts with playback
            if (audioEngine_.isPlaying()) return;
            audioEngine_.triggerNote(0, note, instrument, 1.0f);
            previewNoteCounter_ = PREVIEW_NOTE_FRAMES;  // Start countdown to release
        };
    }

    // Wire up note preview and preset manager for InstrumentScreen
    if (auto* instrumentScreen = dynamic_cast<ui::InstrumentScreen*>(screens_[4].get()))
    {
        instrumentScreen->onNotePreview = [this](int note, int instrument) {
            // Don't preview while track is playing - it conflicts with playback
            if (audioEngine_.isPlaying()) return;
            audioEngine_.triggerNote(0, note, instrument, 1.0f);
            previewNoteCounter_ = PREVIEW_NOTE_FRAMES;  // Start countdown to release
        };
        instrumentScreen->setPresetManager(&presetManager_);
    }

    // Wire up chain navigation from SongScreen
    if (auto* songScreen = dynamic_cast<ui::SongScreen*>(screens_[1].get()))
    {
        songScreen->onJumpToChain = [this](int chainIndex) {
            if (auto* chainScreen = dynamic_cast<ui::ChainScreen*>(screens_[2].get()))
            {
                chainScreen->setCurrentChain(chainIndex);
            }
            switchScreen(2);  // Switch to Chain screen
        };
    }

    // Wire up pattern navigation from ChainScreen
    if (auto* chainScreen = dynamic_cast<ui::ChainScreen*>(screens_[2].get()))
    {
        chainScreen->onJumpToPattern = [this](int patternIndex) {
            if (auto* patternScreen = dynamic_cast<ui::PatternScreen*>(screens_[3].get()))
            {
                patternScreen->setCurrentPattern(patternIndex);
            }
            switchScreen(3);  // Switch to Pattern screen
        };
    }

    // Wire up instrument navigation from PatternScreen
    if (auto* patternScreen = dynamic_cast<ui::PatternScreen*>(screens_[3].get()))
    {
        patternScreen->onJumpToInstrument = [this](int instrumentIndex) {
            if (auto* instrumentScreen = dynamic_cast<ui::InstrumentScreen*>(screens_[4].get()))
            {
                instrumentScreen->setCurrentInstrument(instrumentIndex);
            }
            switchScreen(4);  // Switch to Instrument screen
        };
    }

    // Enter key confirms/activates (e.g., jump to chain from song screen, pattern from chain screen)
    keyHandler_->onConfirm = [this]() {
        // Song screen -> Chain screen
        if (auto* songScreen = dynamic_cast<ui::SongScreen*>(screens_[currentScreen_].get()))
        {
            if (songScreen->onJumpToChain)
            {
                int chainIdx = songScreen->getChainAtCursor();
                if (chainIdx >= 0)
                    songScreen->onJumpToChain(chainIdx);
            }
        }
        // Chain screen -> Pattern screen
        else if (auto* chainScreen = dynamic_cast<ui::ChainScreen*>(screens_[currentScreen_].get()))
        {
            if (chainScreen->onJumpToPattern)
            {
                int patternIdx = chainScreen->getPatternAtCursor();
                if (patternIdx >= 0)
                    chainScreen->onJumpToPattern(patternIdx);
            }
        }
        // Pattern screen -> Instrument screen
        else if (auto* patternScreen = dynamic_cast<ui::PatternScreen*>(screens_[currentScreen_].get()))
        {
            if (patternScreen->onJumpToInstrument)
            {
                int instIdx = patternScreen->getInstrumentAtCursor();
                if (instIdx >= 0)
                    patternScreen->onJumpToInstrument(instIdx);
            }
        }
    };

    // Mode change callback - clear selection when leaving Visual mode
    modeManager_.onModeChanged = [this](input::Mode newMode) {
        if (newMode != input::Mode::Visual) {
            if (auto* ps = dynamic_cast<ui::PatternScreen*>(screens_[currentScreen_].get()))
                ps->clearSelection();
        }
        repaint();
    };

    // File operations
    keyHandler_->onSave = [this](const std::string& filename) { saveProject(filename); };
    keyHandler_->onLoad = [this](const std::string& filename) { loadProject(filename); };
    keyHandler_->onNew = [this]() { newProject(); };

    // Selection and clipboard operations
    keyHandler_->onStartSelection = [this]() {
        if (auto* ps = dynamic_cast<ui::PatternScreen*>(screens_[currentScreen_].get()))
            ps->startSelection();
    };

    keyHandler_->onUpdateSelection = [this]() {
        if (auto* ps = dynamic_cast<ui::PatternScreen*>(screens_[currentScreen_].get()))
            ps->updateSelection();
    };

    keyHandler_->onYank = [this]() {
        if (auto* ps = dynamic_cast<ui::PatternScreen*>(screens_[currentScreen_].get()))
            ps->yankSelection();
    };

    keyHandler_->onPaste = [this]() {
        if (auto* ps = dynamic_cast<ui::PatternScreen*>(screens_[currentScreen_].get()))
            ps->paste();
    };

    keyHandler_->onDelete = [this]() {
        if (auto* ps = dynamic_cast<ui::PatternScreen*>(screens_[currentScreen_].get()))
            ps->deleteSelection();
    };

    keyHandler_->onTranspose = [this](int semitones) {
        if (auto* ps = dynamic_cast<ui::PatternScreen*>(screens_[currentScreen_].get()))
            ps->transpose(semitones);
    };

    keyHandler_->onInterpolate = [this]() {
        if (auto* ps = dynamic_cast<ui::PatternScreen*>(screens_[currentScreen_].get()))
            ps->interpolate();
    };

    keyHandler_->onRandomize = [this](int percent) {
        if (auto* ps = dynamic_cast<ui::PatternScreen*>(screens_[currentScreen_].get()))
            ps->randomize(percent);
    };

    keyHandler_->onDouble = [this]() {
        if (auto* ps = dynamic_cast<ui::PatternScreen*>(screens_[currentScreen_].get()))
            ps->doublePattern();
    };

    keyHandler_->onHalve = [this]() {
        if (auto* ps = dynamic_cast<ui::PatternScreen*>(screens_[currentScreen_].get()))
            ps->halvePattern();
    };

    // Undo/Redo
    keyHandler_->onUndo = [this]() {
        model::UndoManager::instance().undo();
        repaint();
    };

    keyHandler_->onRedo = [this]() {
        model::UndoManager::instance().redo();
        repaint();
    };

    // Preset commands
    keyHandler_->onSavePreset = [this](const std::string& name) {
        if (auto* instrumentScreen = dynamic_cast<ui::InstrumentScreen*>(screens_[4].get()))
        {
            int instIdx = instrumentScreen->getCurrentInstrument();
            auto* instrument = project_.getInstrument(instIdx);
            if (instrument)
            {
                int engine = instrument->getParams().engine;
                if (presetManager_.saveUserPreset(name, engine, instrument->getParams()))
                {
                    DBG("Preset saved: " << name);
                    repaint();
                }
                else
                {
                    DBG("Failed to save preset (invalid name?)");
                }
            }
        }
    };

    keyHandler_->onDeletePreset = [this](const std::string& name) {
        if (auto* instrumentScreen = dynamic_cast<ui::InstrumentScreen*>(screens_[4].get()))
        {
            int instIdx = instrumentScreen->getCurrentInstrument();
            auto* instrument = project_.getInstrument(instIdx);
            if (instrument)
            {
                int engine = instrument->getParams().engine;
                // Check if it's a factory preset
                int presetIdx = presetManager_.findPresetIndex(engine, name);
                if (presetIdx >= 0 && presetManager_.isFactoryPreset(engine, presetIdx))
                {
                    DBG("Cannot delete factory preset");
                }
                else if (presetManager_.deleteUserPreset(name, engine))
                {
                    DBG("Preset deleted: " << name);
                    repaint();
                }
                else
                {
                    DBG("Preset not found: " << name);
                }
            }
        }
    };

    keyHandler_->onCreateSampler = [this]() {
        // Use the current instrument from InstrumentScreen (or 0 if not available)
        int slot = 0;
        if (auto* instScreen = dynamic_cast<ui::InstrumentScreen*>(screens_[4].get()))
        {
            slot = instScreen->getCurrentInstrument();
        }

        // Set instrument type (keep existing name unless it's a default)
        auto* instrument = project_.getInstrument(slot);
        if (instrument)
        {
            instrument->setType(model::InstrumentType::Sampler);
            // Only rename if it has a default name
            std::string name = instrument->getName();
            if (name.find("Instrument") == 0 || name.find("Slicer") == 0 || name.find("Plaits") == 0)
            {
                instrument->setName("Sampler " + std::to_string(slot));
            }

            // Switch to instrument screen showing this instrument
            if (auto* instScreen = dynamic_cast<ui::InstrumentScreen*>(screens_[4].get()))
            {
                instScreen->setCurrentInstrument(slot);
            }
            switchScreen(4);
        }
    };

    keyHandler_->onCreateSlicer = [this]() {
        // Use the current instrument from InstrumentScreen (or 0 if not available)
        int slot = 0;
        if (auto* instScreen = dynamic_cast<ui::InstrumentScreen*>(screens_[4].get()))
        {
            slot = instScreen->getCurrentInstrument();
        }

        // Set instrument type (keep existing name unless it's a default)
        auto* instrument = project_.getInstrument(slot);
        if (instrument)
        {
            instrument->setType(model::InstrumentType::Slicer);
            // Only rename if it has a default name
            std::string name = instrument->getName();
            if (name.find("Instrument") == 0 || name.find("Sampler") == 0 || name.find("Plaits") == 0)
            {
                instrument->setName("Slicer " + std::to_string(slot));
            }

            // Switch to instrument screen showing this instrument
            if (auto* instScreen = dynamic_cast<ui::InstrumentScreen*>(screens_[4].get()))
            {
                instScreen->setCurrentInstrument(slot);
                instScreen->updateSlicerDisplay();
            }
            switchScreen(4);
        }
    };

    keyHandler_->onChop = [this](int divisions) {
        if (auto* instScreen = dynamic_cast<ui::InstrumentScreen*>(screens_[4].get())) {
            int currentInst = instScreen->getCurrentInstrument();
            auto* inst = project_.getInstrument(currentInst);

            if (inst && inst->getType() == model::InstrumentType::Slicer) {
                if (auto* slicer = audioEngine_.getSlicerProcessor(currentInst)) {
                    slicer->chopIntoDivisions(divisions);
                    instScreen->updateSlicerDisplay();  // Sync waveform display with new slices
                }
            }
        }
    };

    // Start timer for UI updates (playhead position)
    // 10fps is sufficient for playhead display and reduces CPU load
    startTimerHz(10);
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
        // Only repaint the active screen, not the entire app
        // This dramatically reduces CPU usage during playback
        if (screens_[currentScreen_])
            screens_[currentScreen_]->repaint();
    }

    // Release preview note after timeout
    if (previewNoteCounter_ > 0)
    {
        previewNoteCounter_--;
        if (previewNoteCounter_ == 0)
        {
            audioEngine_.releaseNote(0);  // Release on track 0 (preview track)
        }
    }

    // Autosave every 60 seconds (1800 frames at 30fps)
    autosaveCounter_++;
    if (autosaveCounter_ >= 1800)
    {
        autosaveCounter_ = 0;
        autosave();
    }
}

juce::File App::getAutosavePath() const
{
    auto dir = juce::File::getSpecialLocation(juce::File::userHomeDirectory)
        .getChildFile(".vitracker")
        .getChildFile("autosave");

    if (!dir.exists())
        dir.createDirectory();

    return dir.getChildFile("autosave.vit");
}

void App::autosave()
{
    if (model::ProjectSerializer::save(project_, getAutosavePath()))
    {
        DBG("Autosaved to: " << getAutosavePath().getFullPathName());
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
    auto statusBarArea = area.removeFromBottom(STATUS_BAR_HEIGHT);

    for (auto& screen : screens_)
    {
        if (screen)
            screen->setBounds(area);
    }

    // Help popup covers the whole window
    helpPopup_.setBounds(getLocalBounds());

    // Position Tip Me button in status bar (right side)
    tipMeButton_.setBounds(statusBarArea.removeFromRight(70).reduced(4, 3));
}

void App::visibilityChanged()
{
    if (isVisible())
    {
        // Delay focus grab and repaint - component may not be ready immediately
        juce::Timer::callAfterDelay(100, [this]() {
            if (isVisible())
            {
                grabKeyboardFocus();
                resized();  // Ensure screen bounds are set
                repaint();
            }
        });
    }
}

bool App::keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent)
{
    juce::ignoreUnused(originatingComponent);

    // Handle help popup toggle
    if (key.getTextCharacter() == '?')
    {
        toggleHelp();
        return true;
    }

    // If help is showing, Escape closes it
    if (helpPopup_.isShowing())
    {
        if (key.getKeyCode() == juce::KeyPress::escapeKey)
        {
            hideHelp();
            return true;
        }
        // Block other keys while help is showing
        return true;
    }

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

    // Reserve space for Tip Me button (right side)
    area.removeFromRight(75);

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

void App::showHelp()
{
    if (screens_[currentScreen_])
    {
        helpPopup_.setScreenTitle(screens_[currentScreen_]->getTitle());
        helpPopup_.setContent(screens_[currentScreen_]->getHelpContent());
    }
    helpPopup_.show();
    helpPopup_.toFront(true);
}

void App::hideHelp()
{
    helpPopup_.hide();
}

void App::toggleHelp()
{
    if (helpPopup_.isShowing())
        hideHelp();
    else
        showHelp();
}
