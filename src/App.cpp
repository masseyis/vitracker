#include "App.h"
#include "ui/PatternScreen.h"
#include "ui/InstrumentScreen.h"
#include "ui/MixerScreen.h"
#include "ui/SongScreen.h"
#include "ui/ChainScreen.h"
#include "model/ProjectSerializer.h"
#include "model/UndoManager.h"

// Groove names for cycling
const char* App::grooveNames_[5] = {"Straight", "Swing 50%", "Swing 66%", "MPC 60%", "Humanize"};

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
            int prev = (currentScreen_ - 1 + 5) % 5;
            switchScreen(prev);
        }
        else if (screen == -2)  // Next screen (Shift+])
        {
            int next = (currentScreen_ + 1) % 5;
            switchScreen(next);
        }
        else if (screen >= 1 && screen <= 5)
        {
            switchScreen(screen - 1);  // Number keys 1-5
        }
        // Key 6 does nothing
    };
    keyHandler_->onPlayStop = [this]() {
        if (audioEngine_.isPlaying())
        {
            audioEngine_.stop();
        }
        else
        {
            // Set play mode based on current screen
            if (currentScreen_ == 0)  // Song screen (now index 0)
                audioEngine_.setPlayMode(audio::AudioEngine::PlayMode::Song);
            else
                audioEngine_.setPlayMode(audio::AudioEngine::PlayMode::Pattern);

            // Sync current pattern from PatternScreen (now index 2)
            if (auto* patternScreen = dynamic_cast<ui::PatternScreen*>(screens_[2].get()))
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

    // Create screens (1=Song, 2=Chain, 3=Pattern, 4=Instrument, 5=Mixer)
    screens_[0] = std::make_unique<ui::SongScreen>(project_, modeManager_);
    screens_[1] = std::make_unique<ui::ChainScreen>(project_, modeManager_);
    screens_[2] = std::make_unique<ui::PatternScreen>(project_, modeManager_);
    screens_[3] = std::make_unique<ui::InstrumentScreen>(project_, modeManager_);
    screens_[4] = std::make_unique<ui::MixerScreen>(project_, modeManager_);

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

    // Initialize audio settings popup (hidden by default)
    audioSettingsPopup_ = std::make_unique<ui::AudioSettingsPopup>(deviceManager_);
    addChildComponent(audioSettingsPopup_.get());

    // Initialize Tip Me button (mouse-only, no keyboard focus)
    tipMeButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffff5e5b));
    tipMeButton_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    tipMeButton_.setWantsKeyboardFocus(false);
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

    // Wire up note preview for PatternScreen (now index 2)
    if (auto* patternScreen = dynamic_cast<ui::PatternScreen*>(screens_[2].get()))
    {
        patternScreen->onNotePreview = [this](int note, int instrument) {
            // Don't preview while track is playing - it conflicts with playback
            if (audioEngine_.isPlaying()) return;
            audioEngine_.triggerNote(0, note, instrument, 1.0f);
            previewNoteCounter_ = PREVIEW_NOTE_FRAMES;  // Start countdown to release
        };

        patternScreen->onChordPreview = [this](const std::vector<int>& notes, int instrument) {
            if (audioEngine_.isPlaying()) return;
            audioEngine_.previewChord(notes, instrument);
            previewNoteCounter_ = PREVIEW_NOTE_FRAMES;
        };
    }

    // Wire up note preview and preset manager for InstrumentScreen (now index 3)
    if (auto* instrumentScreen = dynamic_cast<ui::InstrumentScreen*>(screens_[3].get()))
    {
        instrumentScreen->onNotePreview = [this](int note, int instrument) {
            // Don't preview while track is playing - it conflicts with playback
            if (audioEngine_.isPlaying()) return;
            audioEngine_.triggerNote(0, note, instrument, 1.0f);
            previewNoteCounter_ = PREVIEW_NOTE_FRAMES;  // Start countdown to release
        };
        instrumentScreen->setPresetManager(&presetManager_);
    }

    // Wire up chain navigation from SongScreen (now index 0)
    if (auto* songScreen = dynamic_cast<ui::SongScreen*>(screens_[0].get()))
    {
        songScreen->onJumpToChain = [this](int chainIndex) {
            if (auto* chainScreen = dynamic_cast<ui::ChainScreen*>(screens_[1].get()))
            {
                chainScreen->setCurrentChain(chainIndex);
            }
            switchScreen(1);  // Switch to Chain screen
        };

        // Wire up new project callback
        songScreen->onNewProject = [this]() {
            confirmNewProject();
        };

        // Wire up project rename callback
        songScreen->onProjectRenamed = [this](const std::string& newName) {
            juce::ignoreUnused(newName);
            markDirty();
            updateWindowTitle();
        };
    }

    // Wire up pattern navigation from ChainScreen (now index 1)
    if (auto* chainScreen = dynamic_cast<ui::ChainScreen*>(screens_[1].get()))
    {
        chainScreen->onJumpToPattern = [this](int patternIndex) {
            if (auto* patternScreen = dynamic_cast<ui::PatternScreen*>(screens_[2].get()))
            {
                patternScreen->setCurrentPattern(patternIndex);
            }
            switchScreen(2);  // Switch to Pattern screen
        };
    }

    // Wire up instrument navigation from PatternScreen (now index 2)
    if (auto* patternScreen = dynamic_cast<ui::PatternScreen*>(screens_[2].get()))
    {
        patternScreen->onJumpToInstrument = [this](int instrumentIndex) {
            if (auto* instrumentScreen = dynamic_cast<ui::InstrumentScreen*>(screens_[3].get()))
            {
                instrumentScreen->setCurrentInstrument(instrumentIndex);
            }
            switchScreen(3);  // Switch to Instrument screen
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

    // Preset commands (InstrumentScreen is now index 3)
    keyHandler_->onSavePreset = [this](const std::string& name) {
        if (auto* instrumentScreen = dynamic_cast<ui::InstrumentScreen*>(screens_[3].get()))
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
        if (auto* instrumentScreen = dynamic_cast<ui::InstrumentScreen*>(screens_[3].get()))
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
        if (auto* instScreen = dynamic_cast<ui::InstrumentScreen*>(screens_[3].get()))
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
            if (auto* instScreen = dynamic_cast<ui::InstrumentScreen*>(screens_[3].get()))
            {
                instScreen->setCurrentInstrument(slot);
            }
            switchScreen(3);
        }
    };

    keyHandler_->onCreateSlicer = [this]() {
        // Use the current instrument from InstrumentScreen (or 0 if not available)
        int slot = 0;
        if (auto* instScreen = dynamic_cast<ui::InstrumentScreen*>(screens_[3].get()))
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
            if (auto* instScreen = dynamic_cast<ui::InstrumentScreen*>(screens_[3].get()))
            {
                instScreen->setCurrentInstrument(slot);
                instScreen->updateSlicerDisplay();
            }
            switchScreen(3);
        }
    };

    keyHandler_->onChop = [this](int divisions) {
        if (auto* instScreen = dynamic_cast<ui::InstrumentScreen*>(screens_[3].get())) {
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

    // Release preview notes after timeout (release all tracks used for chord preview)
    if (previewNoteCounter_ > 0)
    {
        previewNoteCounter_--;
        if (previewNoteCounter_ == 0)
        {
            // Release all tracks that could have been used for chord preview
            // Chords can use up to 7 notes (13th chord), so release tracks 0-6
            for (int track = 0; track < 7; ++track)
            {
                audioEngine_.releaseNote(track);
            }
        }
    }

    // Debounced autosave - saves after edits settle
    if (autosaveDebounce_ > 0)
    {
        autosaveDebounce_--;
        if (autosaveDebounce_ == 0 && projectDirty_)
        {
            // Save to project file path
            juce::File savePath = getProjectFilePath();
            if (model::ProjectSerializer::save(project_, savePath))
            {
                DBG("Autosaved to: " << savePath.getFullPathName());
                currentProjectFile_ = savePath;
                projectDirty_ = false;
            }
        }
    }

    // Periodic backup autosave every 60 seconds (600 frames at 10fps)
    autosaveCounter_++;
    if (autosaveCounter_ >= 600)
    {
        autosaveCounter_ = 0;
        autosave();  // Backup to ~/.vitracker/autosave/
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

        static const char* screenNames[] = {"SONG", "CHAIN", "PATTERN", "INSTRUMENT", "MIXER"};
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

    // Audio settings popup covers the whole window (centered dialog inside)
    if (audioSettingsPopup_)
        audioSettingsPopup_->setBounds(getLocalBounds());

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

    // Handle audio settings popup toggle (~)
    if (key.getTextCharacter() == '~' || key.getTextCharacter() == '`')
    {
        toggleAudioSettings();
        return true;
    }

    // If audio settings is showing, Escape or ~ closes it
    if (audioSettingsPopup_ && audioSettingsPopup_->isShowing())
    {
        if (key.getKeyCode() == juce::KeyPress::escapeKey)
        {
            hideAudioSettings();
            return true;
        }
        // Allow typing in the settings dialog - don't block all keys
        return false;
    }

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

    // Handle tempo adjust mode
    if (tempoAdjustMode_)
    {
        if (handleTempoAdjustKey(key))
            return true;
    }

    // Global tempo shortcut (t) - skip if screen is in text edit mode
    bool screenInTextEdit = screens_[currentScreen_] &&
        screens_[currentScreen_]->getInputContext() == input::InputContext::TextEdit;
    if (key.getTextCharacter() == 't' && modeManager_.getMode() == input::Mode::Normal && !screenInTextEdit)
    {
        enterTempoAdjustMode();
        return true;
    }

    // Global groove cycling (g/G) - skip if screen is in text edit mode
    if (modeManager_.getMode() == input::Mode::Normal && !screenInTextEdit)
    {
        if (key.getTextCharacter() == 'g')
        {
            cycleGroove(false);
            return true;
        }
        if (key.getTextCharacter() == 'G')
        {
            cycleGroove(true);
            return true;
        }
    }

    bool handled = keyHandler_->handleKey(key);
    if (handled) repaint();
    return handled;
}

void App::switchScreen(int screenIndex)
{
    if (screenIndex < 0 || screenIndex >= 5) return;  // Now 5 screens
    if (screenIndex == currentScreen_) return;

    // Exit tempo adjust mode when switching screens
    if (tempoAdjustMode_)
        exitTempoAdjustMode();

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
    juce::String modeStr = tempoAdjustMode_ ? "TEMPO" : modeManager_.getModeString();
    g.drawText("-- " + modeStr + " --",
               area.removeFromLeft(120), juce::Justification::centredLeft, true);

    // Screen indicator (after mode)
    static const char* screenKeys[] = {"1:SNG", "2:CHN", "3:PAT", "4:INS", "5:MIX"};
    for (int i = 0; i < 5; ++i)
    {
        g.setColour(i == currentScreen_ ? juce::Colours::yellow : juce::Colours::grey);
        g.drawText(screenKeys[i], area.removeFromLeft(50), juce::Justification::centred, true);
    }

    // Transport (center)
    bool isPlaying = audioEngine_.isPlaying();
    g.setColour(isPlaying ? juce::Colours::lightgreen : juce::Colours::grey);
    juce::String transportText = isPlaying ?
        juce::String("PLAYING Row ") + juce::String(audioEngine_.getCurrentRow()) :
        "STOPPED";
    g.drawText(transportText, area.removeFromLeft(130), juce::Justification::centred, true);

    // Reserve space for Tip Me button (right side)
    area.removeFromRight(75);

    // Tempo (highlight if in tempo adjust mode)
    g.setColour(tempoAdjustMode_ ? juce::Colours::yellow : juce::Colours::white);
    g.drawText(juce::String(project_.getTempo(), 1) + " BPM",
               area.removeFromRight(80), juce::Justification::centredRight, true);

    // Groove
    g.setColour(juce::Colours::white);
    g.drawText(project_.getGrooveTemplate(),
               area.removeFromRight(90), juce::Justification::centredRight, true);
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
    currentProjectFile_ = juce::File();  // Clear current file path
    projectDirty_ = false;
    updateWindowTitle();
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

// Audio settings popup
void App::showAudioSettings()
{
    if (audioSettingsPopup_)
    {
        audioSettingsPopup_->show();
        audioSettingsPopup_->toFront(true);
    }
}

void App::hideAudioSettings()
{
    if (audioSettingsPopup_)
        audioSettingsPopup_->hide();
}

void App::toggleAudioSettings()
{
    if (audioSettingsPopup_)
    {
        if (audioSettingsPopup_->isShowing())
            hideAudioSettings();
        else
            showAudioSettings();
    }
}

// Tempo adjust mode
void App::enterTempoAdjustMode()
{
    tempoAdjustMode_ = true;
    repaint();
}

void App::exitTempoAdjustMode()
{
    tempoAdjustMode_ = false;
    repaint();
}

bool App::handleTempoAdjustKey(const juce::KeyPress& key)
{
    auto keyCode = key.getKeyCode();
    bool shift = key.getModifiers().isShiftDown();

    if (keyCode == juce::KeyPress::returnKey || keyCode == juce::KeyPress::escapeKey)
    {
        exitTempoAdjustMode();
        return true;
    }

    float delta = 0.0f;
    if (keyCode == juce::KeyPress::upKey || keyCode == juce::KeyPress::rightKey)
        delta = shift ? 10.0f : 1.0f;
    else if (keyCode == juce::KeyPress::downKey || keyCode == juce::KeyPress::leftKey)
        delta = shift ? -10.0f : -1.0f;

    if (delta != 0.0f)
    {
        float newTempo = std::clamp(project_.getTempo() + delta, 20.0f, 300.0f);
        project_.setTempo(newTempo);
        markDirty();
        repaint();
        return true;
    }

    return false;
}

// Groove cycling
void App::cycleGroove(bool reverse)
{
    std::string current = project_.getGrooveTemplate();
    int currentIdx = 0;
    for (int i = 0; i < 5; ++i)
    {
        if (current == grooveNames_[i])
        {
            currentIdx = i;
            break;
        }
    }

    int newIdx = reverse ? (currentIdx + 4) % 5 : (currentIdx + 1) % 5;
    project_.setGrooveTemplate(grooveNames_[newIdx]);
    markDirty();
    repaint();
}

// Project dirty tracking and autosave
void App::markDirty()
{
    projectDirty_ = true;
    autosaveDebounce_ = 30;  // ~3 seconds at 10fps before autosave
}

void App::updateWindowTitle()
{
    if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
    {
        window->setName("Vitracker - " + juce::String(project_.getName()));
    }
}

juce::File App::getProjectFilePath() const
{
    std::string sanitized = sanitizeFilename(project_.getName());
    if (sanitized.empty())
        sanitized = "Untitled";

    auto dir = currentProjectFile_.exists() ?
        currentProjectFile_.getParentDirectory() :
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Vitracker Projects");

    if (!dir.exists())
        dir.createDirectory();

    return dir.getChildFile(juce::String(sanitized) + ".vit");
}

std::string App::sanitizeFilename(const std::string& name) const
{
    std::string result;
    for (char c : name)
    {
        if (c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
            result += '_';
        else
            result += c;
    }
    return result;
}

void App::confirmNewProject()
{
    if (projectDirty_)
    {
        // Show confirmation dialog
        auto options = juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::QuestionIcon)
            .withTitle("New Project")
            .withMessage("Current project has unsaved changes. Create new project anyway?")
            .withButton("Yes")
            .withButton("No");

        juce::AlertWindow::showAsync(options, [this](int result) {
            if (result == 1)  // Yes
            {
                newProject();
            }
        });
    }
    else
    {
        newProject();
    }
}
