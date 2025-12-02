# Vitracker Phase 2: Audio Engine & Screens

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add audio playback with Plaits synth, pattern editing, and remaining UI screens.

**Architecture:** AudioEngine manages a pool of Voice objects (each wrapping Plaits DSP). Transport controls playback position. PatternScreen gains edit mode for note entry. Remaining screens follow the established Screen pattern.

**Tech Stack:** JUCE audio, Plaits DSP (already in src/dsp/), C++17

---

## Phase 6: Audio Engine

### Task 1: Create Voice class wrapping Plaits

**Files:**
- Create: `src/audio/Voice.h`
- Create: `src/audio/Voice.cpp`

**Step 1: Write Voice.h**

```cpp
#pragma once

#include "../dsp/plaits/dsp/voice.h"
#include "../model/Instrument.h"
#include <memory>

namespace audio {

class Voice
{
public:
    Voice();
    ~Voice();

    void init();
    void noteOn(int note, float velocity, const model::Instrument& instrument);
    void noteOff();

    void render(float* outL, float* outR, int numSamples);

    bool isActive() const { return active_; }
    int getNote() const { return currentNote_; }
    uint64_t getStartTime() const { return startTime_; }

    // Send levels for effects (read by AudioEngine)
    float getReverbSend() const { return reverbSend_; }
    float getDelaySend() const { return delaySend_; }
    float getChorusSend() const { return chorusSend_; }
    float getDriveSend() const { return driveSend_; }
    float getSidechainSend() const { return sidechainSend_; }

private:
    plaits::Voice plaitsVoice_;
    plaits::Patch patch_;
    plaits::Modulations modulations_;

    float outBuffer_[24];
    float auxBuffer_[24];

    char sharedBuffer_[16384];

    bool active_ = false;
    int currentNote_ = -1;
    uint64_t startTime_ = 0;

    float reverbSend_ = 0.0f;
    float delaySend_ = 0.0f;
    float chorusSend_ = 0.0f;
    float driveSend_ = 0.0f;
    float sidechainSend_ = 0.0f;

    static uint64_t globalTime_;
};

} // namespace audio
```

**Step 2: Write Voice.cpp**

```cpp
#include "Voice.h"
#include <cstring>
#include <algorithm>

namespace audio {

uint64_t Voice::globalTime_ = 0;

Voice::Voice()
{
    std::memset(&patch_, 0, sizeof(patch_));
    std::memset(&modulations_, 0, sizeof(modulations_));
    std::memset(outBuffer_, 0, sizeof(outBuffer_));
    std::memset(auxBuffer_, 0, sizeof(auxBuffer_));
    std::memset(sharedBuffer_, 0, sizeof(sharedBuffer_));
}

Voice::~Voice() = default;

void Voice::init()
{
    stmlib::BufferAllocator allocator(sharedBuffer_, sizeof(sharedBuffer_));
    plaitsVoice_.Init(&allocator);
}

void Voice::noteOn(int note, float velocity, const model::Instrument& instrument)
{
    active_ = true;
    currentNote_ = note;
    startTime_ = ++globalTime_;

    // Set up patch from instrument
    const auto& params = instrument.getParams();
    patch_.engine = params.engine;
    patch_.note = static_cast<float>(note);
    patch_.harmonics = params.harmonics;
    patch_.timbre = params.timbre;
    patch_.morph = params.morph;
    patch_.lpg_colour = params.lpgColour;
    patch_.decay = params.decay;

    // Modulations
    modulations_.engine = 0.0f;
    modulations_.note = 0.0f;
    modulations_.frequency = 0.0f;
    modulations_.harmonics = 0.0f;
    modulations_.timbre = 0.0f;
    modulations_.morph = 0.0f;
    modulations_.trigger = velocity;
    modulations_.level = velocity;

    // Store send levels
    const auto& sends = instrument.getSends();
    reverbSend_ = sends.reverb;
    delaySend_ = sends.delay;
    chorusSend_ = sends.chorus;
    driveSend_ = sends.drive;
    sidechainSend_ = sends.sidechainDuck;
}

void Voice::noteOff()
{
    // For now, just deactivate (Plaits uses decay envelope)
    // In future, could trigger release phase
    active_ = false;
}

void Voice::render(float* outL, float* outR, int numSamples)
{
    if (!active_) return;

    // Plaits renders in blocks of 24 samples
    int processed = 0;
    while (processed < numSamples)
    {
        int blockSize = std::min(24, numSamples - processed);

        plaits::Voice::Frame frames[24];
        plaitsVoice_.Render(patch_, modulations_, frames, blockSize);

        // Clear trigger after first block
        modulations_.trigger = 0.0f;

        for (int i = 0; i < blockSize; ++i)
        {
            outL[processed + i] += frames[i].out / 32768.0f;
            outR[processed + i] += frames[i].aux / 32768.0f;
        }

        processed += blockSize;
    }
}

} // namespace audio
```

**Step 3: Add to CMakeLists.txt**

Add to target_sources after existing sources:
```cmake
    src/audio/Voice.cpp
```

**Step 4: Build and verify**

Run: `cmake --build build 2>&1 | tail -20`
Expected: Build succeeds (may have warnings, but no errors)

**Step 5: Commit**

```bash
git add src/audio/Voice.h src/audio/Voice.cpp CMakeLists.txt
git commit -m "feat: add Voice class wrapping Plaits DSP"
```

---

### Task 2: Create AudioEngine with voice pool

**Files:**
- Create: `src/audio/AudioEngine.h`
- Create: `src/audio/AudioEngine.cpp`

**Step 1: Write AudioEngine.h**

```cpp
#pragma once

#include "Voice.h"
#include "../model/Project.h"
#include <JuceHeader.h>
#include <array>
#include <mutex>

namespace audio {

class AudioEngine : public juce::AudioSource
{
public:
    static constexpr int NUM_VOICES = 64;
    static constexpr int NUM_TRACKS = 16;

    AudioEngine();
    ~AudioEngine() override;

    void setProject(model::Project* project) { project_ = project; }

    // Transport
    void play();
    void stop();
    bool isPlaying() const { return playing_; }

    int getCurrentRow() const { return currentRow_; }
    int getCurrentPattern() const { return currentPattern_; }

    // Trigger a note
    void triggerNote(int track, int note, int instrumentIndex, float velocity);
    void releaseNote(int track);

    // AudioSource interface
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

private:
    Voice* allocateVoice(int note);
    void advancePlayhead();

    model::Project* project_ = nullptr;

    std::array<Voice, NUM_VOICES> voices_;
    std::array<Voice*, NUM_TRACKS> trackVoices_;  // Current voice per track

    double sampleRate_ = 48000.0;
    int samplesPerBlock_ = 512;

    bool playing_ = false;
    int currentRow_ = 0;
    int currentPattern_ = 0;
    double samplesUntilNextRow_ = 0.0;

    std::mutex mutex_;
};

} // namespace audio
```

**Step 2: Write AudioEngine.cpp**

```cpp
#include "AudioEngine.h"

namespace audio {

AudioEngine::AudioEngine()
{
    trackVoices_.fill(nullptr);
}

AudioEngine::~AudioEngine() = default;

void AudioEngine::play()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!playing_)
    {
        playing_ = true;
        currentRow_ = 0;
        samplesUntilNextRow_ = 0.0;
    }
}

void AudioEngine::stop()
{
    std::lock_guard<std::mutex> lock(mutex_);
    playing_ = false;

    // Release all voices
    for (auto& voice : voices_)
    {
        if (voice.isActive())
            voice.noteOff();
    }
    trackVoices_.fill(nullptr);
}

void AudioEngine::triggerNote(int track, int note, int instrumentIndex, float velocity)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!project_) return;
    auto* instrument = project_->getInstrument(instrumentIndex);
    if (!instrument) return;

    // Release current voice on this track
    if (trackVoices_[track])
    {
        trackVoices_[track]->noteOff();
    }

    // Allocate new voice
    Voice* voice = allocateVoice(note);
    if (voice)
    {
        voice->noteOn(note, velocity, *instrument);
        trackVoices_[track] = voice;
    }
}

void AudioEngine::releaseNote(int track)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (trackVoices_[track])
    {
        trackVoices_[track]->noteOff();
        trackVoices_[track] = nullptr;
    }
}

Voice* AudioEngine::allocateVoice(int note)
{
    // First, look for inactive voice
    for (auto& voice : voices_)
    {
        if (!voice.isActive())
            return &voice;
    }

    // Voice stealing: find oldest voice
    Voice* oldest = &voices_[0];
    for (auto& voice : voices_)
    {
        if (voice.getStartTime() < oldest->getStartTime())
            oldest = &voice;
    }

    oldest->noteOff();
    return oldest;
}

void AudioEngine::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    sampleRate_ = sampleRate;
    samplesPerBlock_ = samplesPerBlockExpected;

    // Initialize all voices
    for (auto& voice : voices_)
    {
        voice.init();
    }
}

void AudioEngine::releaseResources()
{
    stop();
}

void AudioEngine::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();

    std::lock_guard<std::mutex> lock(mutex_);

    float* outL = bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
    float* outR = bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample);

    int numSamples = bufferToFill.numSamples;

    // If playing, process pattern
    if (playing_ && project_)
    {
        auto* pattern = project_->getPattern(currentPattern_);
        if (pattern)
        {
            double samplesPerBeat = sampleRate_ * 60.0 / project_->getTempo();
            double samplesPerRow = samplesPerBeat / 4.0;  // 4 rows per beat

            int processed = 0;
            while (processed < numSamples)
            {
                if (samplesUntilNextRow_ <= 0.0)
                {
                    // Trigger notes on current row
                    for (int track = 0; track < NUM_TRACKS; ++track)
                    {
                        const auto& step = pattern->getStep(track, currentRow_);

                        if (step.note == model::Step::NOTE_OFF)
                        {
                            releaseNote(track);
                        }
                        else if (step.note >= 0 && step.instrument >= 0)
                        {
                            float vel = step.volume < 0xFF ? step.volume / 255.0f : 1.0f;
                            triggerNote(track, step.note, step.instrument, vel);
                        }
                    }

                    // Advance row
                    currentRow_ = (currentRow_ + 1) % pattern->getLength();
                    samplesUntilNextRow_ = samplesPerRow;
                }

                int blockSamples = std::min(numSamples - processed,
                                            static_cast<int>(samplesUntilNextRow_));

                samplesUntilNextRow_ -= blockSamples;
                processed += blockSamples;
            }
        }
    }

    // Render all active voices
    for (auto& voice : voices_)
    {
        if (voice.isActive())
        {
            voice.render(outL, outR, numSamples);
        }
    }

    // Apply master volume from mixer
    if (project_)
    {
        float masterVol = project_->getMixer().masterVolume;
        for (int i = 0; i < numSamples; ++i)
        {
            outL[i] *= masterVol;
            outR[i] *= masterVol;
        }
    }
}

void AudioEngine::advancePlayhead()
{
    // Called from audio thread - advance to next row
    if (!project_) return;

    auto* pattern = project_->getPattern(currentPattern_);
    if (pattern)
    {
        currentRow_ = (currentRow_ + 1) % pattern->getLength();
    }
}

} // namespace audio
```

**Step 3: Add to CMakeLists.txt**

Add to target_sources:
```cmake
    src/audio/AudioEngine.cpp
```

**Step 4: Build and verify**

Run: `cmake --build build 2>&1 | tail -10`
Expected: Build succeeds

**Step 5: Commit**

```bash
git add src/audio/AudioEngine.h src/audio/AudioEngine.cpp CMakeLists.txt
git commit -m "feat: add AudioEngine with voice pool and playback"
```

---

### Task 3: Integrate AudioEngine into App

**Files:**
- Modify: `src/App.h`
- Modify: `src/App.cpp`

**Step 1: Update App.h**

Add includes and member:
```cpp
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

    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;

    void timerCallback() override;

private:
    void switchScreen(int screenIndex);
    void drawStatusBar(juce::Graphics& g, juce::Rectangle<int> area);

    model::Project project_;
    input::ModeManager modeManager_;
    std::unique_ptr<input::KeyHandler> keyHandler_;

    audio::AudioEngine audioEngine_;
    juce::AudioDeviceManager deviceManager_;
    juce::AudioSourcePlayer audioSourcePlayer_;

    std::array<std::unique_ptr<ui::Screen>, 6> screens_;
    int currentScreen_ = 3;

    static constexpr int STATUS_BAR_HEIGHT = 28;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(App)
};
```

**Step 2: Update App.cpp constructor and destructor**

Replace the constructor and destructor:
```cpp
#include "App.h"
#include "ui/PatternScreen.h"

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
    screens_[3] = std::make_unique<ui::PatternScreen>(project_, modeManager_);

    // Add active screen as child
    if (screens_[currentScreen_])
    {
        addAndMakeVisible(screens_[currentScreen_].get());
    }

    // Mode change callback
    modeManager_.onModeChanged = [this](input::Mode) { repaint(); };

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
```

**Step 3: Add timerCallback and update drawStatusBar**

Add timerCallback:
```cpp
void App::timerCallback()
{
    if (audioEngine_.isPlaying())
    {
        repaint();  // Update playhead display
    }
}
```

Update drawStatusBar to show playing state:
```cpp
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
```

**Step 4: Build and test**

Run: `cmake --build build && ./build/Vitracker_artefacts/Debug/Vitracker.app/Contents/MacOS/Vitracker`
Expected: App opens, pressing Space toggles play/stop

**Step 5: Commit**

```bash
git add src/App.h src/App.cpp
git commit -m "feat: integrate AudioEngine with audio device output"
```

---

### Task 4: Add note entry in Edit mode

**Files:**
- Modify: `src/ui/PatternScreen.h`
- Modify: `src/ui/PatternScreen.cpp`
- Modify: `src/input/KeyHandler.cpp`

**Step 1: Add edit handling to PatternScreen.h**

Add to PatternScreen class:
```cpp
    void handleEditKey(const juce::KeyPress& key);

    // For external triggering
    std::function<void(int note, int instrument)> onNotePreview;
```

**Step 2: Update PatternScreen.cpp with note entry**

Add the handleEditKey method:
```cpp
void PatternScreen::handleEditKey(const juce::KeyPress& key)
{
    auto* pattern = project_.getPattern(currentPattern_);
    if (!pattern) return;

    auto& step = pattern->getStep(cursorTrack_, cursorRow_);
    auto textChar = key.getTextCharacter();

    // Note entry using keyboard layout (like a piano)
    // Lower row: Z=C, X=D, C=E, V=F, B=G, N=A, M=B
    // Upper row: Q=C, W=D, E=E, R=F, T=G, Y=A, U=B (one octave up)
    // S=C#, D=D#, G=F#, H=G#, J=A#, 2=C#, 3=D#, 5=F#, 6=G#, 7=A#

    static const std::map<char, int> noteMap = {
        // Lower octave (octave 4)
        {'z', 48}, {'s', 49}, {'x', 50}, {'d', 51}, {'c', 52},
        {'v', 53}, {'g', 54}, {'b', 55}, {'h', 56}, {'n', 57},
        {'j', 58}, {'m', 59}, {',', 60},
        // Upper octave (octave 5)
        {'q', 60}, {'2', 61}, {'w', 62}, {'3', 63}, {'e', 64},
        {'r', 65}, {'5', 66}, {'t', 67}, {'6', 68}, {'y', 69},
        {'7', 70}, {'u', 71}, {'i', 72},
    };

    char lowerChar = static_cast<char>(std::tolower(textChar));

    auto it = noteMap.find(lowerChar);
    if (it != noteMap.end())
    {
        step.note = it->second;
        if (step.instrument < 0) step.instrument = 0;  // Default instrument

        // Preview the note
        if (onNotePreview) onNotePreview(step.note, step.instrument);

        // Move down to next row
        cursorRow_ = std::min(cursorRow_ + 1, pattern->getLength() - 1);
        repaint();
        return;
    }

    // Backspace/Delete clears cell
    if (key.getKeyCode() == juce::KeyPress::backspaceKey ||
        key.getKeyCode() == juce::KeyPress::deleteKey)
    {
        step.clear();
        repaint();
        return;
    }

    // Period for note off
    if (textChar == '.')
    {
        step.note = model::Step::NOTE_OFF;
        cursorRow_ = std::min(cursorRow_ + 1, pattern->getLength() - 1);
        repaint();
        return;
    }

    // +/- to change octave of current note
    if (textChar == '+' || textChar == '=')
    {
        if (step.note >= 0 && step.note < 120) step.note += 12;
        repaint();
        return;
    }
    if (textChar == '-')
    {
        if (step.note >= 12) step.note -= 12;
        repaint();
        return;
    }
}
```

**Step 3: Update KeyHandler to call handleEditKey**

In handleEditMode, add:
```cpp
bool KeyHandler::handleEditMode(const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        modeManager_.setMode(Mode::Normal);
        return true;
    }

    // Navigation also works in edit mode
    auto keyCode = key.getKeyCode();
    if (keyCode == juce::KeyPress::upKey || keyCode == juce::KeyPress::downKey ||
        keyCode == juce::KeyPress::leftKey || keyCode == juce::KeyPress::rightKey)
    {
        return handleNormalMode(key);
    }

    // Forward to edit handler callback
    if (onEditKey)
    {
        onEditKey(key);
        return true;
    }

    return false;
}
```

Add to KeyHandler.h:
```cpp
    std::function<void(const juce::KeyPress&)> onEditKey;
```

**Step 4: Wire up in App.cpp**

In App constructor, add:
```cpp
    keyHandler_->onEditKey = [this](const juce::KeyPress& key) {
        if (auto* patternScreen = dynamic_cast<ui::PatternScreen*>(screens_[3].get()))
        {
            patternScreen->handleEditKey(key);
        }
    };

    // Wire up note preview
    if (auto* patternScreen = dynamic_cast<ui::PatternScreen*>(screens_[3].get()))
    {
        patternScreen->onNotePreview = [this](int note, int instrument) {
            audioEngine_.triggerNote(0, note, instrument, 1.0f);
        };
    }
```

**Step 5: Build and test**

Run: `cmake --build build && ./build/Vitracker_artefacts/Debug/Vitracker.app/Contents/MacOS/Vitracker`
Expected: Press 'i' to enter Edit mode, type QWERTY keys to enter notes, hear preview

**Step 6: Commit**

```bash
git add src/ui/PatternScreen.h src/ui/PatternScreen.cpp src/input/KeyHandler.h src/input/KeyHandler.cpp src/App.cpp
git commit -m "feat: add note entry in Edit mode with preview"
```

---

## Phase 7: Remaining Screens

### Task 5: Create ProjectScreen

**Files:**
- Create: `src/ui/ProjectScreen.h`
- Create: `src/ui/ProjectScreen.cpp`

**Step 1: Write ProjectScreen.h**

```cpp
#pragma once

#include "Screen.h"

namespace ui {

class ProjectScreen : public Screen
{
public:
    ProjectScreen(model::Project& project, input::ModeManager& modeManager);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void navigate(int dx, int dy) override;
    void handleEditKey(const juce::KeyPress& key);

    std::string getTitle() const override { return "PROJECT"; }

    std::function<void()> onTempoChanged;

private:
    int cursorRow_ = 0;

    enum Field { NAME = 0, TEMPO, GROOVE, NUM_FIELDS };
};

} // namespace ui
```

**Step 2: Write ProjectScreen.cpp**

```cpp
#include "ProjectScreen.h"

namespace ui {

ProjectScreen::ProjectScreen(model::Project& project, input::ModeManager& modeManager)
    : Screen(project, modeManager)
{
}

void ProjectScreen::paint(juce::Graphics& g)
{
    g.fillAll(bgColor);

    g.setFont(24.0f);
    g.setColour(fgColor);
    g.drawText("PROJECT", 20, 20, 200, 30, juce::Justification::centredLeft);

    g.setFont(16.0f);

    int y = 80;
    int rowHeight = 40;

    // Name field
    g.setColour(cursorRow_ == NAME ? cursorColor : fgColor);
    g.drawText("Name:", 40, y, 100, rowHeight, juce::Justification::centredLeft);
    g.drawText(project_.getName(), 150, y, 400, rowHeight, juce::Justification::centredLeft);
    y += rowHeight;

    // Tempo field
    g.setColour(cursorRow_ == TEMPO ? cursorColor : fgColor);
    g.drawText("Tempo:", 40, y, 100, rowHeight, juce::Justification::centredLeft);
    g.drawText(juce::String(project_.getTempo(), 1) + " BPM", 150, y, 400, rowHeight, juce::Justification::centredLeft);
    y += rowHeight;

    // Groove field
    g.setColour(cursorRow_ == GROOVE ? cursorColor : fgColor);
    g.drawText("Groove:", 40, y, 100, rowHeight, juce::Justification::centredLeft);
    g.drawText(project_.getGrooveTemplate(), 150, y, 400, rowHeight, juce::Justification::centredLeft);
    y += rowHeight;

    // Instructions
    y += 40;
    g.setColour(fgColor.darker(0.3f));
    g.setFont(14.0f);
    g.drawText("Press 'i' to edit, +/- to adjust tempo", 40, y, 400, 30, juce::Justification::centredLeft);
}

void ProjectScreen::resized()
{
}

void ProjectScreen::navigate(int dx, int dy)
{
    juce::ignoreUnused(dx);
    cursorRow_ = std::clamp(cursorRow_ + dy, 0, NUM_FIELDS - 1);
    repaint();
}

void ProjectScreen::handleEditKey(const juce::KeyPress& key)
{
    auto textChar = key.getTextCharacter();

    if (cursorRow_ == TEMPO)
    {
        if (textChar == '+' || textChar == '=')
        {
            project_.setTempo(std::min(project_.getTempo() + 1.0f, 300.0f));
            if (onTempoChanged) onTempoChanged();
            repaint();
        }
        else if (textChar == '-')
        {
            project_.setTempo(std::max(project_.getTempo() - 1.0f, 20.0f));
            if (onTempoChanged) onTempoChanged();
            repaint();
        }
    }
}

} // namespace ui
```

**Step 3: Add to CMakeLists.txt**

Add to target_sources:
```cmake
    src/ui/ProjectScreen.cpp
```

**Step 4: Commit**

```bash
git add src/ui/ProjectScreen.h src/ui/ProjectScreen.cpp CMakeLists.txt
git commit -m "feat: add ProjectScreen with tempo control"
```

---

### Task 6: Create InstrumentScreen

**Files:**
- Create: `src/ui/InstrumentScreen.h`
- Create: `src/ui/InstrumentScreen.cpp`

**Step 1: Write InstrumentScreen.h**

```cpp
#pragma once

#include "Screen.h"

namespace ui {

class InstrumentScreen : public Screen
{
public:
    InstrumentScreen(model::Project& project, input::ModeManager& modeManager);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void navigate(int dx, int dy) override;
    void handleEditKey(const juce::KeyPress& key);

    std::string getTitle() const override { return "INSTRUMENT"; }

    std::function<void(int note, int instrument)> onNotePreview;

private:
    void drawEngineSelector(juce::Graphics& g, juce::Rectangle<int> area);
    void drawParameters(juce::Graphics& g, juce::Rectangle<int> area);
    void drawSends(juce::Graphics& g, juce::Rectangle<int> area);

    int currentInstrument_ = 0;
    int cursorRow_ = 0;

    static constexpr int NUM_PARAMS = 10;  // engine + 6 params + name + 3 visible at once from sends

    static const char* engineNames_[16];
};

} // namespace ui
```

**Step 2: Write InstrumentScreen.cpp**

```cpp
#include "InstrumentScreen.h"

namespace ui {

const char* InstrumentScreen::engineNames_[16] = {
    "Virtual Analog", "Waveshaping", "FM", "Grain",
    "Additive", "Wavetable", "Chords", "Vowel/Speech",
    "Swarm", "Noise", "Particle", "String",
    "Modal", "Bass Drum", "Snare Drum", "Hi-Hat"
};

InstrumentScreen::InstrumentScreen(model::Project& project, input::ModeManager& modeManager)
    : Screen(project, modeManager)
{
}

void InstrumentScreen::paint(juce::Graphics& g)
{
    g.fillAll(bgColor);

    auto* instrument = project_.getInstrument(currentInstrument_);
    if (!instrument) return;

    auto area = getLocalBounds().reduced(20);

    // Header
    g.setFont(20.0f);
    g.setColour(fgColor);
    juce::String title = "INSTRUMENT " + juce::String(currentInstrument_ + 1) + ": " + instrument->getName();
    g.drawText(title, area.removeFromTop(40), juce::Justification::centredLeft);

    area.removeFromTop(10);

    // Engine selector
    drawEngineSelector(g, area.removeFromTop(50));

    area.removeFromTop(20);

    // Parameters
    drawParameters(g, area.removeFromTop(200));

    area.removeFromTop(20);

    // Sends
    drawSends(g, area.removeFromTop(150));
}

void InstrumentScreen::drawEngineSelector(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto* instrument = project_.getInstrument(currentInstrument_);
    if (!instrument) return;

    g.setFont(16.0f);
    g.setColour(cursorRow_ == 0 ? cursorColor : fgColor);

    int engine = instrument->getParams().engine;
    g.drawText("Engine: " + juce::String(engineNames_[engine]), area, juce::Justification::centredLeft);
}

void InstrumentScreen::drawParameters(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto* instrument = project_.getInstrument(currentInstrument_);
    if (!instrument) return;

    const auto& params = instrument->getParams();

    struct ParamInfo { const char* name; float value; };
    ParamInfo paramList[] = {
        {"Harmonics", params.harmonics},
        {"Timbre", params.timbre},
        {"Morph", params.morph},
        {"Attack", params.attack},
        {"Decay", params.decay},
        {"LPG Colour", params.lpgColour}
    };

    g.setFont(14.0f);
    int y = area.getY();
    int rowHeight = 30;

    for (int i = 0; i < 6; ++i)
    {
        bool selected = (cursorRow_ == i + 1);
        g.setColour(selected ? cursorColor : fgColor);

        g.drawText(paramList[i].name, area.getX(), y, 100, rowHeight, juce::Justification::centredLeft);

        // Draw slider bar
        int barX = area.getX() + 110;
        int barWidth = 200;
        g.setColour(highlightColor);
        g.fillRect(barX, y + 10, barWidth, 10);
        g.setColour(selected ? cursorColor : fgColor);
        g.fillRect(barX, y + 10, static_cast<int>(barWidth * paramList[i].value), 10);

        // Value text
        g.drawText(juce::String(paramList[i].value, 2), barX + barWidth + 10, y, 50, rowHeight, juce::Justification::centredLeft);

        y += rowHeight;
    }
}

void InstrumentScreen::drawSends(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto* instrument = project_.getInstrument(currentInstrument_);
    if (!instrument) return;

    const auto& sends = instrument->getSends();

    g.setFont(14.0f);
    g.setColour(fgColor);
    g.drawText("SENDS", area.removeFromTop(25), juce::Justification::centredLeft);

    struct SendInfo { const char* name; float value; };
    SendInfo sendList[] = {
        {"Reverb", sends.reverb},
        {"Delay", sends.delay},
        {"Chorus", sends.chorus},
        {"Drive", sends.drive},
        {"Sidechain", sends.sidechainDuck}
    };

    int y = area.getY();
    int rowHeight = 24;

    for (int i = 0; i < 5; ++i)
    {
        bool selected = (cursorRow_ == i + 7);
        g.setColour(selected ? cursorColor : fgColor);

        g.drawText(sendList[i].name, area.getX(), y, 80, rowHeight, juce::Justification::centredLeft);

        int barX = area.getX() + 90;
        int barWidth = 150;
        g.setColour(highlightColor);
        g.fillRect(barX, y + 6, barWidth, 10);
        g.setColour(selected ? cursorColor : fgColor);
        g.fillRect(barX, y + 6, static_cast<int>(barWidth * sendList[i].value), 10);

        y += rowHeight;
    }
}

void InstrumentScreen::resized()
{
}

void InstrumentScreen::navigate(int dx, int dy)
{
    if (dx != 0)
    {
        // Switch instruments
        int numInstruments = project_.getInstrumentCount();
        currentInstrument_ = (currentInstrument_ + dx + numInstruments) % numInstruments;
    }

    if (dy != 0)
    {
        cursorRow_ = std::clamp(cursorRow_ + dy, 0, 11);  // 0=engine, 1-6=params, 7-11=sends
    }

    repaint();
}

void InstrumentScreen::handleEditKey(const juce::KeyPress& key)
{
    auto* instrument = project_.getInstrument(currentInstrument_);
    if (!instrument) return;

    auto textChar = key.getTextCharacter();
    float delta = 0.0f;

    if (textChar == '+' || textChar == '=' || key.getKeyCode() == juce::KeyPress::rightKey)
        delta = 0.05f;
    else if (textChar == '-' || key.getKeyCode() == juce::KeyPress::leftKey)
        delta = -0.05f;

    if (delta == 0.0f) return;

    auto& params = instrument->getParams();
    auto& sends = instrument->getSends();

    auto clamp01 = [](float& v, float d) { v = std::clamp(v + d, 0.0f, 1.0f); };

    switch (cursorRow_)
    {
        case 0: params.engine = std::clamp(params.engine + (delta > 0 ? 1 : -1), 0, 15); break;
        case 1: clamp01(params.harmonics, delta); break;
        case 2: clamp01(params.timbre, delta); break;
        case 3: clamp01(params.morph, delta); break;
        case 4: clamp01(params.attack, delta); break;
        case 5: clamp01(params.decay, delta); break;
        case 6: clamp01(params.lpgColour, delta); break;
        case 7: clamp01(sends.reverb, delta); break;
        case 8: clamp01(sends.delay, delta); break;
        case 9: clamp01(sends.chorus, delta); break;
        case 10: clamp01(sends.drive, delta); break;
        case 11: clamp01(sends.sidechainDuck, delta); break;
    }

    // Preview note when changing parameters
    if (onNotePreview) onNotePreview(60, currentInstrument_);

    repaint();
}

} // namespace ui
```

**Step 3: Add to CMakeLists.txt**

Add to target_sources:
```cmake
    src/ui/InstrumentScreen.cpp
```

**Step 4: Commit**

```bash
git add src/ui/InstrumentScreen.h src/ui/InstrumentScreen.cpp CMakeLists.txt
git commit -m "feat: add InstrumentScreen with Plaits parameter editing"
```

---

### Task 7: Create MixerScreen

**Files:**
- Create: `src/ui/MixerScreen.h`
- Create: `src/ui/MixerScreen.cpp`

**Step 1: Write MixerScreen.h**

```cpp
#pragma once

#include "Screen.h"

namespace ui {

class MixerScreen : public Screen
{
public:
    MixerScreen(model::Project& project, input::ModeManager& modeManager);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void navigate(int dx, int dy) override;
    void handleEditKey(const juce::KeyPress& key);

    std::string getTitle() const override { return "MIXER"; }

private:
    void drawTrackStrip(juce::Graphics& g, juce::Rectangle<int> area, int track);
    void drawMasterStrip(juce::Graphics& g, juce::Rectangle<int> area);

    int cursorTrack_ = 0;  // 0-15 = tracks, 16 = master
    int cursorRow_ = 0;    // 0 = volume, 1 = pan, 2 = mute, 3 = solo
};

} // namespace ui
```

**Step 2: Write MixerScreen.cpp**

```cpp
#include "MixerScreen.h"

namespace ui {

MixerScreen::MixerScreen(model::Project& project, input::ModeManager& modeManager)
    : Screen(project, modeManager)
{
}

void MixerScreen::paint(juce::Graphics& g)
{
    g.fillAll(bgColor);

    auto area = getLocalBounds();

    // Header
    g.setFont(20.0f);
    g.setColour(fgColor);
    g.drawText("MIXER", area.removeFromTop(40).reduced(20, 0), juce::Justification::centredLeft);

    area.removeFromTop(10);
    area = area.reduced(10, 0);

    // Track strips
    int stripWidth = (area.getWidth() - 80) / 16;  // Reserve 80 for master

    for (int t = 0; t < 16; ++t)
    {
        auto stripArea = area.removeFromLeft(stripWidth);
        drawTrackStrip(g, stripArea, t);
    }

    // Master strip
    area.removeFromLeft(20);  // Gap
    drawMasterStrip(g, area);
}

void MixerScreen::drawTrackStrip(juce::Graphics& g, juce::Rectangle<int> area, int track)
{
    auto& mixer = project_.getMixer();
    bool isSelected = (cursorTrack_ == track);

    // Track number
    g.setFont(12.0f);
    g.setColour(isSelected ? cursorColor : fgColor);
    g.drawText(juce::String(track + 1), area.removeFromTop(20), juce::Justification::centred);

    // Volume fader
    auto faderArea = area.removeFromTop(150);
    int faderWidth = 20;
    int faderX = faderArea.getCentreX() - faderWidth / 2;

    g.setColour(highlightColor);
    g.fillRect(faderX, faderArea.getY(), faderWidth, faderArea.getHeight());

    float vol = mixer.trackVolumes[track];
    int fillHeight = static_cast<int>(faderArea.getHeight() * vol);
    g.setColour(isSelected && cursorRow_ == 0 ? cursorColor : fgColor);
    g.fillRect(faderX, faderArea.getBottom() - fillHeight, faderWidth, fillHeight);

    area.removeFromTop(5);

    // Pan knob (simplified as text)
    g.setColour(isSelected && cursorRow_ == 1 ? cursorColor : fgColor.darker(0.3f));
    float pan = mixer.trackPans[track];
    juce::String panStr = pan < -0.01f ? "L" + juce::String(static_cast<int>(-pan * 100)) :
                          pan > 0.01f ? "R" + juce::String(static_cast<int>(pan * 100)) : "C";
    g.drawText(panStr, area.removeFromTop(20), juce::Justification::centred);

    // Mute button
    g.setColour(mixer.trackMutes[track] ? juce::Colours::red :
                (isSelected && cursorRow_ == 2 ? cursorColor : fgColor.darker(0.5f)));
    g.drawText("M", area.removeFromTop(20), juce::Justification::centred);

    // Solo button
    g.setColour(mixer.trackSolos[track] ? juce::Colours::yellow :
                (isSelected && cursorRow_ == 3 ? cursorColor : fgColor.darker(0.5f)));
    g.drawText("S", area.removeFromTop(20), juce::Justification::centred);
}

void MixerScreen::drawMasterStrip(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto& mixer = project_.getMixer();
    bool isSelected = (cursorTrack_ == 16);

    g.setFont(14.0f);
    g.setColour(isSelected ? cursorColor : fgColor);
    g.drawText("MASTER", area.removeFromTop(20), juce::Justification::centred);

    // Master fader
    auto faderArea = area.removeFromTop(150);
    int faderWidth = 30;
    int faderX = faderArea.getCentreX() - faderWidth / 2;

    g.setColour(highlightColor);
    g.fillRect(faderX, faderArea.getY(), faderWidth, faderArea.getHeight());

    int fillHeight = static_cast<int>(faderArea.getHeight() * mixer.masterVolume);
    g.setColour(isSelected ? cursorColor : fgColor);
    g.fillRect(faderX, faderArea.getBottom() - fillHeight, faderWidth, fillHeight);

    // Value
    g.drawText(juce::String(static_cast<int>(mixer.masterVolume * 100)) + "%",
               area.removeFromTop(20), juce::Justification::centred);
}

void MixerScreen::resized()
{
}

void MixerScreen::navigate(int dx, int dy)
{
    cursorTrack_ = std::clamp(cursorTrack_ + dx, 0, 16);
    cursorRow_ = std::clamp(cursorRow_ + dy, 0, 3);
    repaint();
}

void MixerScreen::handleEditKey(const juce::KeyPress& key)
{
    auto& mixer = project_.getMixer();
    auto textChar = key.getTextCharacter();

    float delta = 0.0f;
    if (textChar == '+' || textChar == '=') delta = 0.05f;
    else if (textChar == '-') delta = -0.05f;

    if (cursorTrack_ < 16)
    {
        switch (cursorRow_)
        {
            case 0:  // Volume
                if (delta != 0.0f)
                    mixer.trackVolumes[cursorTrack_] = std::clamp(mixer.trackVolumes[cursorTrack_] + delta, 0.0f, 1.0f);
                break;
            case 1:  // Pan
                if (delta != 0.0f)
                    mixer.trackPans[cursorTrack_] = std::clamp(mixer.trackPans[cursorTrack_] + delta, -1.0f, 1.0f);
                break;
            case 2:  // Mute
                if (key.getKeyCode() == juce::KeyPress::returnKey || textChar == ' ')
                    mixer.trackMutes[cursorTrack_] = !mixer.trackMutes[cursorTrack_];
                break;
            case 3:  // Solo
                if (key.getKeyCode() == juce::KeyPress::returnKey || textChar == ' ')
                    mixer.trackSolos[cursorTrack_] = !mixer.trackSolos[cursorTrack_];
                break;
        }
    }
    else
    {
        // Master
        if (delta != 0.0f)
            mixer.masterVolume = std::clamp(mixer.masterVolume + delta, 0.0f, 1.0f);
    }

    repaint();
}

} // namespace ui
```

**Step 3: Add to CMakeLists.txt**

Add to target_sources:
```cmake
    src/ui/MixerScreen.cpp
```

**Step 4: Commit**

```bash
git add src/ui/MixerScreen.h src/ui/MixerScreen.cpp CMakeLists.txt
git commit -m "feat: add MixerScreen with track and master faders"
```

---

### Task 8: Wire up all screens in App

**Files:**
- Modify: `src/App.cpp`

**Step 1: Add includes**

```cpp
#include "ui/ProjectScreen.h"
#include "ui/InstrumentScreen.h"
#include "ui/MixerScreen.h"
```

**Step 2: Create all screens in constructor**

Replace the screens creation:
```cpp
    // Create screens
    screens_[0] = std::make_unique<ui::ProjectScreen>(project_, modeManager_);
    screens_[3] = std::make_unique<ui::PatternScreen>(project_, modeManager_);
    screens_[4] = std::make_unique<ui::InstrumentScreen>(project_, modeManager_);
    screens_[5] = std::make_unique<ui::MixerScreen>(project_, modeManager_);
    // screens_[1] = SongScreen (TODO)
    // screens_[2] = ChainScreen (TODO)
```

**Step 3: Update onEditKey to handle all screens**

```cpp
    keyHandler_->onEditKey = [this](const juce::KeyPress& key) {
        if (auto* patternScreen = dynamic_cast<ui::PatternScreen*>(screens_[currentScreen_].get()))
            patternScreen->handleEditKey(key);
        else if (auto* projectScreen = dynamic_cast<ui::ProjectScreen*>(screens_[currentScreen_].get()))
            projectScreen->handleEditKey(key);
        else if (auto* instrumentScreen = dynamic_cast<ui::InstrumentScreen*>(screens_[currentScreen_].get()))
            instrumentScreen->handleEditKey(key);
        else if (auto* mixerScreen = dynamic_cast<ui::MixerScreen*>(screens_[currentScreen_].get()))
            mixerScreen->handleEditKey(key);
    };
```

**Step 4: Wire up note preview for InstrumentScreen**

```cpp
    if (auto* instrumentScreen = dynamic_cast<ui::InstrumentScreen*>(screens_[4].get()))
    {
        instrumentScreen->onNotePreview = [this](int note, int instrument) {
            audioEngine_.triggerNote(0, note, instrument, 1.0f);
        };
    }
```

**Step 5: Build and test**

Run: `cmake --build build && ./build/Vitracker_artefacts/Debug/Vitracker.app/Contents/MacOS/Vitracker`
Expected: All screens work (1=Project, 4=Pattern, 5=Instrument, 6=Mixer)

**Step 6: Commit**

```bash
git add src/App.cpp
git commit -m "feat: wire up Project, Instrument, and Mixer screens"
```

---

## Phase 8: File I/O

### Task 9: Add JSON save/load to Project

**Files:**
- Create: `src/model/ProjectSerializer.h`
- Create: `src/model/ProjectSerializer.cpp`

**Step 1: Write ProjectSerializer.h**

```cpp
#pragma once

#include "Project.h"
#include <JuceHeader.h>

namespace model {

class ProjectSerializer
{
public:
    static bool save(const Project& project, const juce::File& file);
    static bool load(Project& project, const juce::File& file);

    static juce::String toJson(const Project& project);
    static bool fromJson(Project& project, const juce::String& json);

private:
    static juce::var instrumentToVar(const Instrument& inst);
    static void varToInstrument(Instrument& inst, const juce::var& v);

    static juce::var patternToVar(const Pattern& pattern);
    static void varToPattern(Pattern& pattern, const juce::var& v);

    static juce::var chainToVar(const Chain& chain);
    static void varToChain(Chain& chain, const juce::var& v);
};

} // namespace model
```

**Step 2: Write ProjectSerializer.cpp**

```cpp
#include "ProjectSerializer.h"
#include <zlib.h>

namespace model {

bool ProjectSerializer::save(const Project& project, const juce::File& file)
{
    auto json = toJson(project);

    // Compress with zlib
    auto jsonData = json.toRawUTF8();
    uLongf compressedSize = compressBound(static_cast<uLong>(json.length()));
    std::vector<Bytef> compressed(compressedSize);

    if (compress(compressed.data(), &compressedSize,
                 reinterpret_cast<const Bytef*>(jsonData),
                 static_cast<uLong>(json.length())) != Z_OK)
    {
        return false;
    }

    juce::FileOutputStream stream(file);
    if (!stream.openedOk()) return false;

    // Write header
    stream.writeString("VIT1");
    stream.writeInt(static_cast<int>(json.length()));  // Original size
    stream.write(compressed.data(), compressedSize);

    return true;
}

bool ProjectSerializer::load(Project& project, const juce::File& file)
{
    juce::FileInputStream stream(file);
    if (!stream.openedOk()) return false;

    // Check header
    auto header = stream.readString();
    if (header != "VIT1") return false;

    int originalSize = stream.readInt();

    // Read compressed data
    auto compressedSize = stream.getTotalLength() - stream.getPosition();
    std::vector<Bytef> compressed(compressedSize);
    stream.read(compressed.data(), compressedSize);

    // Decompress
    std::vector<char> decompressed(originalSize + 1);
    uLongf destLen = originalSize;

    if (uncompress(reinterpret_cast<Bytef*>(decompressed.data()), &destLen,
                   compressed.data(), static_cast<uLong>(compressedSize)) != Z_OK)
    {
        return false;
    }

    decompressed[originalSize] = '\0';
    return fromJson(project, juce::String(decompressed.data()));
}

juce::String ProjectSerializer::toJson(const Project& project)
{
    juce::DynamicObject::Ptr root = new juce::DynamicObject();

    root->setProperty("version", "1.0");
    root->setProperty("name", juce::String(project.getName()));
    root->setProperty("tempo", project.getTempo());
    root->setProperty("groove", juce::String(project.getGrooveTemplate()));

    // Instruments
    juce::Array<juce::var> instruments;
    for (int i = 0; i < project.getInstrumentCount(); ++i)
    {
        instruments.add(instrumentToVar(*project.getInstrument(i)));
    }
    root->setProperty("instruments", instruments);

    // Patterns
    juce::Array<juce::var> patterns;
    for (int i = 0; i < project.getPatternCount(); ++i)
    {
        patterns.add(patternToVar(*project.getPattern(i)));
    }
    root->setProperty("patterns", patterns);

    // Chains
    juce::Array<juce::var> chains;
    for (int i = 0; i < project.getChainCount(); ++i)
    {
        chains.add(chainToVar(*project.getChain(i)));
    }
    root->setProperty("chains", chains);

    // Mixer
    juce::DynamicObject::Ptr mixer = new juce::DynamicObject();
    const auto& m = project.getMixer();

    juce::Array<juce::var> trackVols, trackPans;
    for (int i = 0; i < 16; ++i)
    {
        trackVols.add(m.trackVolumes[i]);
        trackPans.add(m.trackPans[i]);
    }
    mixer->setProperty("trackVolumes", trackVols);
    mixer->setProperty("trackPans", trackPans);
    mixer->setProperty("masterVolume", m.masterVolume);
    root->setProperty("mixer", juce::var(mixer.get()));

    return juce::JSON::toString(juce::var(root.get()));
}

bool ProjectSerializer::fromJson(Project& project, const juce::String& json)
{
    auto parsed = juce::JSON::parse(json);
    if (!parsed.isObject()) return false;

    auto* obj = parsed.getDynamicObject();
    if (!obj) return false;

    project.setName(obj->getProperty("name").toString().toStdString());
    project.setTempo(static_cast<float>(obj->getProperty("tempo")));
    project.setGrooveTemplate(obj->getProperty("groove").toString().toStdString());

    // Load instruments, patterns, chains, mixer...
    // (simplified - would need full implementation)

    return true;
}

juce::var ProjectSerializer::instrumentToVar(const Instrument& inst)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("name", juce::String(inst.getName()));

    const auto& p = inst.getParams();
    obj->setProperty("engine", p.engine);
    obj->setProperty("harmonics", p.harmonics);
    obj->setProperty("timbre", p.timbre);
    obj->setProperty("morph", p.morph);
    obj->setProperty("attack", p.attack);
    obj->setProperty("decay", p.decay);
    obj->setProperty("lpgColour", p.lpgColour);

    const auto& s = inst.getSends();
    juce::Array<juce::var> sends;
    sends.add(s.reverb);
    sends.add(s.delay);
    sends.add(s.chorus);
    sends.add(s.drive);
    sends.add(s.sidechainDuck);
    obj->setProperty("sends", sends);

    return juce::var(obj.get());
}

void ProjectSerializer::varToInstrument(Instrument& inst, const juce::var& v)
{
    auto* obj = v.getDynamicObject();
    if (!obj) return;

    inst.setName(obj->getProperty("name").toString().toStdString());

    auto& p = inst.getParams();
    p.engine = static_cast<int>(obj->getProperty("engine"));
    p.harmonics = static_cast<float>(obj->getProperty("harmonics"));
    p.timbre = static_cast<float>(obj->getProperty("timbre"));
    p.morph = static_cast<float>(obj->getProperty("morph"));
    p.attack = static_cast<float>(obj->getProperty("attack"));
    p.decay = static_cast<float>(obj->getProperty("decay"));
    p.lpgColour = static_cast<float>(obj->getProperty("lpgColour"));
}

juce::var ProjectSerializer::patternToVar(const Pattern& pattern)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("name", juce::String(pattern.getName()));
    obj->setProperty("length", pattern.getLength());

    // Steps - only save non-empty
    juce::Array<juce::var> steps;
    for (int t = 0; t < 16; ++t)
    {
        for (int r = 0; r < pattern.getLength(); ++r)
        {
            const auto& step = pattern.getStep(t, r);
            if (!step.isEmpty())
            {
                juce::DynamicObject::Ptr s = new juce::DynamicObject();
                s->setProperty("t", t);
                s->setProperty("r", r);
                s->setProperty("n", step.note);
                s->setProperty("i", step.instrument);
                s->setProperty("v", step.volume);
                steps.add(juce::var(s.get()));
            }
        }
    }
    obj->setProperty("steps", steps);

    return juce::var(obj.get());
}

void ProjectSerializer::varToPattern(Pattern& pattern, const juce::var& v)
{
    auto* obj = v.getDynamicObject();
    if (!obj) return;

    pattern.setName(obj->getProperty("name").toString().toStdString());
    pattern.setLength(static_cast<int>(obj->getProperty("length")));
}

juce::var ProjectSerializer::chainToVar(const Chain& chain)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("name", juce::String(chain.getName()));
    obj->setProperty("scaleLock", juce::String(chain.getScaleLock()));

    juce::Array<juce::var> patterns;
    for (int idx : chain.getPatternIndices())
    {
        patterns.add(idx);
    }
    obj->setProperty("patterns", patterns);

    return juce::var(obj.get());
}

void ProjectSerializer::varToChain(Chain& chain, const juce::var& v)
{
    auto* obj = v.getDynamicObject();
    if (!obj) return;

    chain.setName(obj->getProperty("name").toString().toStdString());
    chain.setScaleLock(obj->getProperty("scaleLock").toString().toStdString());
}

} // namespace model
```

**Step 3: Add zlib to CMakeLists.txt**

After target_link_libraries, add:
```cmake
find_package(ZLIB REQUIRED)
target_link_libraries(Vitracker PRIVATE ZLIB::ZLIB)
```

And add to target_sources:
```cmake
    src/model/ProjectSerializer.cpp
```

**Step 4: Commit**

```bash
git add src/model/ProjectSerializer.h src/model/ProjectSerializer.cpp CMakeLists.txt
git commit -m "feat: add ProjectSerializer for .vit file save/load"
```

---

### Task 10: Add save/load commands

**Files:**
- Modify: `src/input/KeyHandler.cpp`
- Modify: `src/App.h`
- Modify: `src/App.cpp`

**Step 1: Update App.h**

Add:
```cpp
    void saveProject(const std::string& filename = "");
    void loadProject(const std::string& filename);
    void newProject();
```

**Step 2: Implement in App.cpp**

```cpp
#include "model/ProjectSerializer.h"

void App::saveProject(const std::string& filename)
{
    juce::File file;

    if (filename.empty())
    {
        juce::FileChooser chooser("Save Project", juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.vit");
        if (chooser.browseForFileToSave(true))
            file = chooser.getResult();
        else
            return;
    }
    else
    {
        file = juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(filename + ".vit");
    }

    if (model::ProjectSerializer::save(project_, file))
    {
        DBG("Project saved to: " << file.getFullPathName());
    }
}

void App::loadProject(const std::string& filename)
{
    juce::File file;

    if (filename.empty())
    {
        juce::FileChooser chooser("Load Project", juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.vit");
        if (chooser.browseForFileToOpen())
            file = chooser.getResult();
        else
            return;
    }
    else
    {
        file = juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(filename + ".vit");
    }

    if (model::ProjectSerializer::load(project_, file))
    {
        DBG("Project loaded from: " << file.getFullPathName());
        repaint();
    }
}

void App::newProject()
{
    project_ = model::Project("Untitled");
    repaint();
}
```

**Step 3: Wire up commands in KeyHandler**

In executeCommand:
```cpp
void KeyHandler::executeCommand(const std::string& command)
{
    DBG("Executing command: " << command);

    if (command == "w")
    {
        if (onSave) onSave("");
    }
    else if (command.substr(0, 2) == "w ")
    {
        if (onSave) onSave(command.substr(2));
    }
    else if (command == "e")
    {
        if (onLoad) onLoad("");
    }
    else if (command.substr(0, 2) == "e ")
    {
        if (onLoad) onLoad(command.substr(2));
    }
    else if (command == "new")
    {
        if (onNew) onNew();
    }
    else if (command == "q")
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

    if (onCommand) onCommand(command);
}
```

Add to KeyHandler.h:
```cpp
    std::function<void(const std::string&)> onSave;
    std::function<void(const std::string&)> onLoad;
    std::function<void()> onNew;
```

**Step 4: Wire up in App constructor**

```cpp
    keyHandler_->onSave = [this](const std::string& filename) { saveProject(filename); };
    keyHandler_->onLoad = [this](const std::string& filename) { loadProject(filename); };
    keyHandler_->onNew = [this]() { newProject(); };
```

**Step 5: Build and test**

Run: `cmake --build build && ./build/Vitracker_artefacts/Debug/Vitracker.app/Contents/MacOS/Vitracker`
Expected: `:w` saves, `:e` loads, `:new` creates new project

**Step 6: Commit**

```bash
git add src/App.h src/App.cpp src/input/KeyHandler.h src/input/KeyHandler.cpp
git commit -m "feat: add :w :e :new commands for file operations"
```

---

## Summary

This plan covers:

1. **Phase 6: Audio Engine** (Tasks 1-4)
   - Voice class wrapping Plaits DSP
   - AudioEngine with 64-voice pool
   - Pattern playback with transport
   - Note entry in Edit mode with preview

2. **Phase 7: Remaining Screens** (Tasks 5-8)
   - ProjectScreen (tempo, settings)
   - InstrumentScreen (Plaits params, sends)
   - MixerScreen (track faders, master)
   - Full screen integration

3. **Phase 8: File I/O** (Tasks 9-10)
   - ProjectSerializer with zlib compression
   - Save/load commands (:w, :e, :new)

**Still TODO for future phases:**
- SongScreen and ChainScreen
- Effects (Reverb, Delay, Chorus, Drive, Sidechain)
- Undo/Redo
- Autosave
- Groove templates
- Copy/paste in Visual mode
