# Vitracker Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a keyboard-first tracker DAW with vim-style modal editing and Plaits synth engine.

**Architecture:** Standalone JUCE app with embedded Plaits DSP. Model layer holds project state (instruments, patterns, chains, song). Audio engine manages voice pool and mixing. UI layer renders 6 modal screens. Input layer dispatches keys based on current mode.

**Tech Stack:** JUCE 8.0.4, C++17, Plaits DSP, CMake, GoogleTest

---

## Phase 1: Project Foundation

### Task 1: Create CMakeLists.txt

**Files:**
- Create: `CMakeLists.txt`

**Step 1: Write CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.22)
project(Vitracker VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch JUCE
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
)
FetchContent_MakeAvailable(JUCE)

# Fetch GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

# Main application
juce_add_gui_app(Vitracker
    COMPANY_NAME "MasseyIS"
    PRODUCT_NAME "Vitracker"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.png")

target_sources(Vitracker PRIVATE
    src/main.cpp
    src/App.cpp)

target_include_directories(Vitracker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_definitions(Vitracker PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    STMLIB_X86=1
    $<$<CXX_COMPILER_ID:MSVC>:_USE_MATH_DEFINES>)

target_link_libraries(Vitracker PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
```

**Step 2: Verify structure**

Run: `ls -la`
Expected: CMakeLists.txt exists

**Step 3: Commit**

```bash
git add CMakeLists.txt
git commit -m "feat: add CMakeLists.txt for JUCE app"
```

---

### Task 2: Create main.cpp entry point

**Files:**
- Create: `src/main.cpp`

**Step 1: Write main.cpp**

```cpp
#include <JuceHeader.h>
#include "App.h"

class VitrackerApplication : public juce::JUCEApplication
{
public:
    VitrackerApplication() {}

    const juce::String getApplicationName() override { return "Vitracker"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise(const juce::String& commandLine) override
    {
        juce::ignoreUnused(commandLine);
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(juce::String name)
            : DocumentWindow(name,
                juce::Desktop::getInstance().getDefaultLookAndFeel()
                    .findColour(ResizableWindow::backgroundColourId),
                DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new App(), true);
            setResizable(true, true);
            centreWithSize(1280, 800);
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(VitrackerApplication)
```

**Step 2: Commit**

```bash
git add src/main.cpp
git commit -m "feat: add main.cpp entry point"
```

---

### Task 3: Create App component

**Files:**
- Create: `src/App.h`
- Create: `src/App.cpp`

**Step 1: Write App.h**

```cpp
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
```

**Step 2: Write App.cpp**

```cpp
#include "App.h"

App::App()
{
    setSize(1280, 800);
    setWantsKeyboardFocus(true);
    addKeyListener(this);
}

App::~App()
{
    removeKeyListener(this);
}

void App::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));

    g.setColour(juce::Colours::white);
    g.setFont(24.0f);
    g.drawText("VITRACKER", getLocalBounds(), juce::Justification::centred, true);

    g.setFont(14.0f);
    g.drawText("Press 1-6 to switch screens | i=Edit | v=Visual | :=Command | Space=Play",
               getLocalBounds().removeFromBottom(40), juce::Justification::centred, true);
}

void App::resized()
{
}

bool App::keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent)
{
    juce::ignoreUnused(originatingComponent);

    // Placeholder - will dispatch to ModeManager
    DBG("Key pressed: " << key.getTextDescription());
    return true;
}
```

**Step 3: Build and verify**

Run: `cmake -B build && cmake --build build`
Expected: Build succeeds

**Step 4: Run app**

Run: `./build/Vitracker_artefacts/Debug/Vitracker.app/Contents/MacOS/Vitracker`
Expected: Window opens with "VITRACKER" text

**Step 5: Commit**

```bash
git add src/App.h src/App.cpp
git commit -m "feat: add App component with basic UI"
```

---

## Phase 2: Data Model

### Task 4: Create Step data structure

**Files:**
- Create: `src/model/Step.h`

**Step 1: Write Step.h**

```cpp
#pragma once

#include <cstdint>
#include <string>

namespace model {

struct FXCommand
{
    uint8_t command = 0;   // 0 = none, 1 = ARP, 2 = POR, etc.
    uint8_t value = 0;

    bool isEmpty() const { return command == 0; }
    void clear() { command = 0; value = 0; }
};

struct Step
{
    int8_t note = -1;           // -1 = empty, 0-127 = MIDI note, -2 = OFF
    int16_t instrument = -1;    // -1 = empty, 0+ = instrument index
    uint8_t volume = 0xFF;      // 0x00-0xFF, 0xFF = default
    FXCommand fx1;
    FXCommand fx2;
    FXCommand fx3;

    static constexpr int8_t NOTE_EMPTY = -1;
    static constexpr int8_t NOTE_OFF = -2;

    bool isEmpty() const
    {
        return note == NOTE_EMPTY && instrument == -1 && volume == 0xFF
            && fx1.isEmpty() && fx2.isEmpty() && fx3.isEmpty();
    }

    void clear()
    {
        note = NOTE_EMPTY;
        instrument = -1;
        volume = 0xFF;
        fx1.clear();
        fx2.clear();
        fx3.clear();
    }
};

} // namespace model
```

**Step 2: Commit**

```bash
git add src/model/Step.h
git commit -m "feat: add Step data structure"
```

---

### Task 5: Create Pattern data structure

**Files:**
- Create: `src/model/Pattern.h`
- Create: `src/model/Pattern.cpp`

**Step 1: Write Pattern.h**

```cpp
#pragma once

#include "Step.h"
#include <string>
#include <vector>
#include <array>

namespace model {

class Pattern
{
public:
    static constexpr int NUM_TRACKS = 16;
    static constexpr int MAX_LENGTH = 128;
    static constexpr int DEFAULT_LENGTH = 16;

    Pattern();
    explicit Pattern(const std::string& name);

    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

    int getLength() const { return length_; }
    void setLength(int length);

    Step& getStep(int track, int row);
    const Step& getStep(int track, int row) const;

    void clear();

private:
    std::string name_;
    int length_ = DEFAULT_LENGTH;
    std::array<std::vector<Step>, NUM_TRACKS> tracks_;
};

} // namespace model
```

**Step 2: Write Pattern.cpp**

```cpp
#include "Pattern.h"
#include <algorithm>

namespace model {

Pattern::Pattern() : Pattern("Untitled") {}

Pattern::Pattern(const std::string& name) : name_(name)
{
    for (auto& track : tracks_)
    {
        track.resize(MAX_LENGTH);
    }
}

void Pattern::setLength(int length)
{
    length_ = std::clamp(length, 1, MAX_LENGTH);
}

Step& Pattern::getStep(int track, int row)
{
    return tracks_[track][row];
}

const Step& Pattern::getStep(int track, int row) const
{
    return tracks_[track][row];
}

void Pattern::clear()
{
    for (auto& track : tracks_)
    {
        for (auto& step : track)
        {
            step.clear();
        }
    }
}

} // namespace model
```

**Step 3: Add to CMakeLists.txt**

Add to target_sources:
```cmake
    src/model/Pattern.cpp
```

**Step 4: Commit**

```bash
git add src/model/Pattern.h src/model/Pattern.cpp CMakeLists.txt
git commit -m "feat: add Pattern data structure"
```

---

### Task 6: Create Instrument data structure

**Files:**
- Create: `src/model/Instrument.h`
- Create: `src/model/Instrument.cpp`

**Step 1: Write Instrument.h**

```cpp
#pragma once

#include <string>
#include <array>

namespace model {

struct PlaitsParams
{
    int engine = 0;           // 0-15 engine type
    float harmonics = 0.5f;   // 0.0-1.0
    float timbre = 0.5f;      // 0.0-1.0
    float morph = 0.5f;       // 0.0-1.0
    float attack = 0.0f;      // 0.0-1.0
    float decay = 0.5f;       // 0.0-1.0
    float lpgColour = 0.5f;   // 0.0-1.0
};

struct SendLevels
{
    float reverb = 0.0f;
    float delay = 0.0f;
    float chorus = 0.0f;
    float drive = 0.0f;
    float sidechainDuck = 0.0f;
};

class Instrument
{
public:
    Instrument();
    explicit Instrument(const std::string& name);

    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

    PlaitsParams& getParams() { return params_; }
    const PlaitsParams& getParams() const { return params_; }

    SendLevels& getSends() { return sends_; }
    const SendLevels& getSends() const { return sends_; }

private:
    std::string name_;
    PlaitsParams params_;
    SendLevels sends_;
};

} // namespace model
```

**Step 2: Write Instrument.cpp**

```cpp
#include "Instrument.h"

namespace model {

Instrument::Instrument() : Instrument("Untitled") {}

Instrument::Instrument(const std::string& name) : name_(name) {}

} // namespace model
```

**Step 3: Add to CMakeLists.txt**

Add to target_sources:
```cmake
    src/model/Instrument.cpp
```

**Step 4: Commit**

```bash
git add src/model/Instrument.h src/model/Instrument.cpp CMakeLists.txt
git commit -m "feat: add Instrument data structure"
```

---

### Task 7: Create Chain data structure

**Files:**
- Create: `src/model/Chain.h`
- Create: `src/model/Chain.cpp`

**Step 1: Write Chain.h**

```cpp
#pragma once

#include <string>
#include <vector>

namespace model {

class Chain
{
public:
    Chain();
    explicit Chain(const std::string& name);

    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

    const std::string& getScaleLock() const { return scaleLock_; }
    void setScaleLock(const std::string& scale) { scaleLock_ = scale; }

    const std::vector<int>& getPatternIndices() const { return patternIndices_; }
    void addPattern(int patternIndex);
    void removePattern(int position);
    void setPattern(int position, int patternIndex);
    int getPatternCount() const { return static_cast<int>(patternIndices_.size()); }

private:
    std::string name_;
    std::string scaleLock_;  // e.g., "C minor", empty = no lock
    std::vector<int> patternIndices_;
};

} // namespace model
```

**Step 2: Write Chain.cpp**

```cpp
#include "Chain.h"

namespace model {

Chain::Chain() : Chain("Untitled") {}

Chain::Chain(const std::string& name) : name_(name) {}

void Chain::addPattern(int patternIndex)
{
    patternIndices_.push_back(patternIndex);
}

void Chain::removePattern(int position)
{
    if (position >= 0 && position < static_cast<int>(patternIndices_.size()))
    {
        patternIndices_.erase(patternIndices_.begin() + position);
    }
}

void Chain::setPattern(int position, int patternIndex)
{
    if (position >= 0 && position < static_cast<int>(patternIndices_.size()))
    {
        patternIndices_[position] = patternIndex;
    }
}

} // namespace model
```

**Step 3: Add to CMakeLists.txt**

Add to target_sources:
```cmake
    src/model/Chain.cpp
```

**Step 4: Commit**

```bash
git add src/model/Chain.h src/model/Chain.cpp CMakeLists.txt
git commit -m "feat: add Chain data structure"
```

---

### Task 8: Create Song data structure

**Files:**
- Create: `src/model/Song.h`
- Create: `src/model/Song.cpp`

**Step 1: Write Song.h**

```cpp
#pragma once

#include <array>
#include <vector>

namespace model {

class Song
{
public:
    static constexpr int NUM_TRACKS = 16;

    Song();

    // Each track is a sequence of chain indices (-1 = empty)
    const std::vector<int>& getTrack(int trackIndex) const;
    void setChain(int trackIndex, int position, int chainIndex);
    void addChain(int trackIndex, int chainIndex);
    void removeChain(int trackIndex, int position);

    int getLength() const;  // Length of longest track

    void clear();

private:
    std::array<std::vector<int>, NUM_TRACKS> tracks_;
};

} // namespace model
```

**Step 2: Write Song.cpp**

```cpp
#include "Song.h"
#include <algorithm>

namespace model {

Song::Song()
{
    clear();
}

const std::vector<int>& Song::getTrack(int trackIndex) const
{
    return tracks_[trackIndex];
}

void Song::setChain(int trackIndex, int position, int chainIndex)
{
    if (trackIndex < 0 || trackIndex >= NUM_TRACKS) return;

    auto& track = tracks_[trackIndex];
    while (static_cast<int>(track.size()) <= position)
    {
        track.push_back(-1);
    }
    track[position] = chainIndex;
}

void Song::addChain(int trackIndex, int chainIndex)
{
    if (trackIndex >= 0 && trackIndex < NUM_TRACKS)
    {
        tracks_[trackIndex].push_back(chainIndex);
    }
}

void Song::removeChain(int trackIndex, int position)
{
    if (trackIndex < 0 || trackIndex >= NUM_TRACKS) return;

    auto& track = tracks_[trackIndex];
    if (position >= 0 && position < static_cast<int>(track.size()))
    {
        track.erase(track.begin() + position);
    }
}

int Song::getLength() const
{
    int maxLen = 0;
    for (const auto& track : tracks_)
    {
        maxLen = std::max(maxLen, static_cast<int>(track.size()));
    }
    return maxLen;
}

void Song::clear()
{
    for (auto& track : tracks_)
    {
        track.clear();
    }
}

} // namespace model
```

**Step 3: Add to CMakeLists.txt**

Add to target_sources:
```cmake
    src/model/Song.cpp
```

**Step 4: Commit**

```bash
git add src/model/Song.h src/model/Song.cpp CMakeLists.txt
git commit -m "feat: add Song data structure"
```

---

### Task 9: Create Project data structure

**Files:**
- Create: `src/model/Project.h`
- Create: `src/model/Project.cpp`

**Step 1: Write Project.h**

```cpp
#pragma once

#include "Instrument.h"
#include "Pattern.h"
#include "Chain.h"
#include "Song.h"
#include <string>
#include <vector>
#include <memory>

namespace model {

class Project
{
public:
    static constexpr int MAX_INSTRUMENTS = 128;
    static constexpr int MAX_PATTERNS = 256;
    static constexpr int MAX_CHAINS = 128;

    Project();
    explicit Project(const std::string& name);

    // Project metadata
    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

    float getTempo() const { return tempo_; }
    void setTempo(float bpm) { tempo_ = bpm; }

    const std::string& getGrooveTemplate() const { return grooveTemplate_; }
    void setGrooveTemplate(const std::string& groove) { grooveTemplate_ = groove; }

    // Instruments
    int addInstrument(const std::string& name = "Untitled");
    Instrument* getInstrument(int index);
    const Instrument* getInstrument(int index) const;
    int getInstrumentCount() const { return static_cast<int>(instruments_.size()); }

    // Patterns
    int addPattern(const std::string& name = "Untitled");
    Pattern* getPattern(int index);
    const Pattern* getPattern(int index) const;
    int getPatternCount() const { return static_cast<int>(patterns_.size()); }

    // Chains
    int addChain(const std::string& name = "Untitled");
    Chain* getChain(int index);
    const Chain* getChain(int index) const;
    int getChainCount() const { return static_cast<int>(chains_.size()); }

    // Song
    Song& getSong() { return song_; }
    const Song& getSong() const { return song_; }

    // Mixer state
    struct MixerState
    {
        std::array<float, 16> trackVolumes;
        std::array<float, 16> trackPans;
        std::array<bool, 16> trackMutes;
        std::array<bool, 16> trackSolos;
        std::array<float, 5> busLevels;  // reverb, delay, chorus, drive, sidechain
        float masterVolume = 1.0f;

        MixerState();
    };

    MixerState& getMixer() { return mixer_; }
    const MixerState& getMixer() const { return mixer_; }

private:
    std::string name_;
    float tempo_ = 120.0f;
    std::string grooveTemplate_ = "None";

    std::vector<std::unique_ptr<Instrument>> instruments_;
    std::vector<std::unique_ptr<Pattern>> patterns_;
    std::vector<std::unique_ptr<Chain>> chains_;
    Song song_;
    MixerState mixer_;
};

} // namespace model
```

**Step 2: Write Project.cpp**

```cpp
#include "Project.h"

namespace model {

Project::MixerState::MixerState()
{
    trackVolumes.fill(1.0f);
    trackPans.fill(0.0f);
    trackMutes.fill(false);
    trackSolos.fill(false);
    busLevels.fill(0.5f);
}

Project::Project() : Project("Untitled") {}

Project::Project(const std::string& name) : name_(name)
{
    // Create one default instrument and pattern
    addInstrument("Init");
    addPattern("Pattern 1");
}

int Project::addInstrument(const std::string& name)
{
    if (instruments_.size() >= MAX_INSTRUMENTS) return -1;
    instruments_.push_back(std::make_unique<Instrument>(name));
    return static_cast<int>(instruments_.size()) - 1;
}

Instrument* Project::getInstrument(int index)
{
    if (index < 0 || index >= static_cast<int>(instruments_.size())) return nullptr;
    return instruments_[index].get();
}

const Instrument* Project::getInstrument(int index) const
{
    if (index < 0 || index >= static_cast<int>(instruments_.size())) return nullptr;
    return instruments_[index].get();
}

int Project::addPattern(const std::string& name)
{
    if (patterns_.size() >= MAX_PATTERNS) return -1;
    patterns_.push_back(std::make_unique<Pattern>(name));
    return static_cast<int>(patterns_.size()) - 1;
}

Pattern* Project::getPattern(int index)
{
    if (index < 0 || index >= static_cast<int>(patterns_.size())) return nullptr;
    return patterns_[index].get();
}

const Pattern* Project::getPattern(int index) const
{
    if (index < 0 || index >= static_cast<int>(patterns_.size())) return nullptr;
    return patterns_[index].get();
}

int Project::addChain(const std::string& name)
{
    if (chains_.size() >= MAX_CHAINS) return -1;
    chains_.push_back(std::make_unique<Chain>(name));
    return static_cast<int>(chains_.size()) - 1;
}

Chain* Project::getChain(int index)
{
    if (index < 0 || index >= static_cast<int>(chains_.size())) return nullptr;
    return chains_[index].get();
}

const Chain* Project::getChain(int index) const
{
    if (index < 0 || index >= static_cast<int>(chains_.size())) return nullptr;
    return chains_[index].get();
}

} // namespace model
```

**Step 3: Add to CMakeLists.txt**

Add to target_sources:
```cmake
    src/model/Project.cpp
```

**Step 4: Commit**

```bash
git add src/model/Project.h src/model/Project.cpp CMakeLists.txt
git commit -m "feat: add Project data structure"
```

---

## Phase 3: Input System

### Task 10: Create ModeManager

**Files:**
- Create: `src/input/ModeManager.h`
- Create: `src/input/ModeManager.cpp`

**Step 1: Write ModeManager.h**

```cpp
#pragma once

#include <functional>
#include <string>

namespace input {

enum class Mode
{
    Normal,
    Edit,
    Visual,
    Command
};

class ModeManager
{
public:
    ModeManager();

    Mode getMode() const { return currentMode_; }
    void setMode(Mode mode);

    std::string getModeString() const;

    // Command mode buffer
    const std::string& getCommandBuffer() const { return commandBuffer_; }
    void appendToCommandBuffer(char c);
    void backspaceCommandBuffer();
    void clearCommandBuffer();

    // Callback when mode changes
    std::function<void(Mode)> onModeChanged;

private:
    Mode currentMode_ = Mode::Normal;
    std::string commandBuffer_;
};

} // namespace input
```

**Step 2: Write ModeManager.cpp**

```cpp
#include "ModeManager.h"

namespace input {

ModeManager::ModeManager() {}

void ModeManager::setMode(Mode mode)
{
    if (mode != currentMode_)
    {
        currentMode_ = mode;
        if (mode != Mode::Command)
        {
            clearCommandBuffer();
        }
        if (onModeChanged)
        {
            onModeChanged(mode);
        }
    }
}

std::string ModeManager::getModeString() const
{
    switch (currentMode_)
    {
        case Mode::Normal: return "NORMAL";
        case Mode::Edit: return "EDIT";
        case Mode::Visual: return "VISUAL";
        case Mode::Command: return ":" + commandBuffer_;
    }
    return "";
}

void ModeManager::appendToCommandBuffer(char c)
{
    commandBuffer_ += c;
}

void ModeManager::backspaceCommandBuffer()
{
    if (!commandBuffer_.empty())
    {
        commandBuffer_.pop_back();
    }
}

void ModeManager::clearCommandBuffer()
{
    commandBuffer_.clear();
}

} // namespace input
```

**Step 3: Add to CMakeLists.txt**

Add to target_sources:
```cmake
    src/input/ModeManager.cpp
```

**Step 4: Commit**

```bash
git add src/input/ModeManager.h src/input/ModeManager.cpp CMakeLists.txt
git commit -m "feat: add ModeManager for vim-style modes"
```

---

### Task 11: Create KeyHandler

**Files:**
- Create: `src/input/KeyHandler.h`
- Create: `src/input/KeyHandler.cpp`

**Step 1: Write KeyHandler.h**

```cpp
#pragma once

#include "ModeManager.h"
#include <JuceHeader.h>
#include <functional>

namespace input {

class KeyHandler
{
public:
    KeyHandler(ModeManager& modeManager);

    // Returns true if key was handled
    bool handleKey(const juce::KeyPress& key);

    // Callbacks for actions
    std::function<void(int)> onScreenSwitch;      // 1-6
    std::function<void()> onPlayStop;
    std::function<void(int, int)> onNavigate;     // dx, dy
    std::function<void()> onYank;
    std::function<void()> onPaste;
    std::function<void()> onDelete;
    std::function<void()> onUndo;
    std::function<void()> onRedo;
    std::function<void(const std::string&)> onCommand;  // Executed command

private:
    bool handleNormalMode(const juce::KeyPress& key);
    bool handleEditMode(const juce::KeyPress& key);
    bool handleVisualMode(const juce::KeyPress& key);
    bool handleCommandMode(const juce::KeyPress& key);

    void executeCommand(const std::string& command);

    ModeManager& modeManager_;
};

} // namespace input
```

**Step 2: Write KeyHandler.cpp**

```cpp
#include "KeyHandler.h"

namespace input {

KeyHandler::KeyHandler(ModeManager& modeManager) : modeManager_(modeManager) {}

bool KeyHandler::handleKey(const juce::KeyPress& key)
{
    switch (modeManager_.getMode())
    {
        case Mode::Normal: return handleNormalMode(key);
        case Mode::Edit: return handleEditMode(key);
        case Mode::Visual: return handleVisualMode(key);
        case Mode::Command: return handleCommandMode(key);
    }
    return false;
}

bool KeyHandler::handleNormalMode(const juce::KeyPress& key)
{
    auto keyCode = key.getKeyCode();
    auto textChar = key.getTextCharacter();

    // Screen switching: 1-6
    if (textChar >= '1' && textChar <= '6')
    {
        if (onScreenSwitch) onScreenSwitch(textChar - '0');
        return true;
    }

    // Navigation
    if (keyCode == juce::KeyPress::upKey)
    {
        if (onNavigate) onNavigate(0, -1);
        return true;
    }
    if (keyCode == juce::KeyPress::downKey)
    {
        if (onNavigate) onNavigate(0, 1);
        return true;
    }
    if (keyCode == juce::KeyPress::leftKey)
    {
        if (onNavigate) onNavigate(-1, 0);
        return true;
    }
    if (keyCode == juce::KeyPress::rightKey)
    {
        if (onNavigate) onNavigate(1, 0);
        return true;
    }

    // Mode switches
    if (textChar == 'i')
    {
        modeManager_.setMode(Mode::Edit);
        return true;
    }
    if (textChar == 'v')
    {
        modeManager_.setMode(Mode::Visual);
        return true;
    }
    if (textChar == ':')
    {
        modeManager_.setMode(Mode::Command);
        return true;
    }

    // Actions
    if (keyCode == juce::KeyPress::spaceKey)
    {
        if (onPlayStop) onPlayStop();
        return true;
    }
    if (textChar == 'y')
    {
        if (onYank) onYank();
        return true;
    }
    if (textChar == 'p')
    {
        if (onPaste) onPaste();
        return true;
    }
    if (textChar == 'd')
    {
        if (onDelete) onDelete();
        return true;
    }
    if (textChar == 'u')
    {
        if (onUndo) onUndo();
        return true;
    }
    if (key.getModifiers().isCtrlDown() && textChar == 'r')
    {
        if (onRedo) onRedo();
        return true;
    }

    return false;
}

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

    // TODO: Handle value entry
    return false;
}

bool KeyHandler::handleVisualMode(const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        modeManager_.setMode(Mode::Normal);
        return true;
    }

    // Navigation extends selection
    auto keyCode = key.getKeyCode();
    if (keyCode == juce::KeyPress::upKey || keyCode == juce::KeyPress::downKey ||
        keyCode == juce::KeyPress::leftKey || keyCode == juce::KeyPress::rightKey)
    {
        // TODO: Extend selection
        return handleNormalMode(key);
    }

    auto textChar = key.getTextCharacter();
    if (textChar == 'y')
    {
        if (onYank) onYank();
        modeManager_.setMode(Mode::Normal);
        return true;
    }
    if (textChar == 'd')
    {
        if (onDelete) onDelete();
        modeManager_.setMode(Mode::Normal);
        return true;
    }

    return false;
}

bool KeyHandler::handleCommandMode(const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        modeManager_.setMode(Mode::Normal);
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::returnKey)
    {
        executeCommand(modeManager_.getCommandBuffer());
        modeManager_.setMode(Mode::Normal);
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::backspaceKey)
    {
        modeManager_.backspaceCommandBuffer();
        return true;
    }

    auto textChar = key.getTextCharacter();
    if (textChar >= ' ' && textChar <= '~')
    {
        modeManager_.appendToCommandBuffer(static_cast<char>(textChar));
        return true;
    }

    return false;
}

void KeyHandler::executeCommand(const std::string& command)
{
    DBG("Executing command: " << command);
    if (onCommand) onCommand(command);
}

} // namespace input
```

**Step 3: Add to CMakeLists.txt**

Add to target_sources:
```cmake
    src/input/KeyHandler.cpp
```

**Step 4: Commit**

```bash
git add src/input/KeyHandler.h src/input/KeyHandler.cpp CMakeLists.txt
git commit -m "feat: add KeyHandler for keyboard input dispatch"
```

---

## Phase 4: UI Screens

### Task 12: Create base Screen class

**Files:**
- Create: `src/ui/Screen.h`
- Create: `src/ui/Screen.cpp`

**Step 1: Write Screen.h**

```cpp
#pragma once

#include <JuceHeader.h>
#include "../model/Project.h"
#include "../input/ModeManager.h"

namespace ui {

class Screen : public juce::Component
{
public:
    Screen(model::Project& project, input::ModeManager& modeManager);
    ~Screen() override = default;

    virtual void onEnter() {}   // Called when screen becomes active
    virtual void onExit() {}    // Called when leaving screen

    virtual void navigate(int dx, int dy) { juce::ignoreUnused(dx, dy); }
    virtual void handleEdit(const juce::KeyPress& key) { juce::ignoreUnused(key); }

    virtual std::string getTitle() const = 0;

protected:
    model::Project& project_;
    input::ModeManager& modeManager_;

    // Common colors
    static inline const juce::Colour bgColor{0xff1a1a2e};
    static inline const juce::Colour fgColor{0xffeaeaea};
    static inline const juce::Colour highlightColor{0xff4a4a6e};
    static inline const juce::Colour cursorColor{0xff7c7cff};
    static inline const juce::Colour headerColor{0xff2a2a4e};
};

} // namespace ui
```

**Step 2: Write Screen.cpp**

```cpp
#include "Screen.h"

namespace ui {

Screen::Screen(model::Project& project, input::ModeManager& modeManager)
    : project_(project), modeManager_(modeManager)
{
}

} // namespace ui
```

**Step 3: Add to CMakeLists.txt**

Add to target_sources:
```cmake
    src/ui/Screen.cpp
```

**Step 4: Commit**

```bash
git add src/ui/Screen.h src/ui/Screen.cpp CMakeLists.txt
git commit -m "feat: add base Screen class"
```

---

### Task 13: Create PatternScreen

**Files:**
- Create: `src/ui/PatternScreen.h`
- Create: `src/ui/PatternScreen.cpp`

**Step 1: Write PatternScreen.h**

```cpp
#pragma once

#include "Screen.h"

namespace ui {

class PatternScreen : public Screen
{
public:
    PatternScreen(model::Project& project, input::ModeManager& modeManager);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void navigate(int dx, int dy) override;
    void handleEdit(const juce::KeyPress& key) override;

    std::string getTitle() const override { return "PATTERN"; }

    int getCurrentPatternIndex() const { return currentPattern_; }
    void setCurrentPattern(int index) { currentPattern_ = index; }

private:
    void drawHeader(juce::Graphics& g, juce::Rectangle<int> area);
    void drawTrackHeaders(juce::Graphics& g, juce::Rectangle<int> area);
    void drawGrid(juce::Graphics& g, juce::Rectangle<int> area);
    void drawStep(juce::Graphics& g, juce::Rectangle<int> area,
                  const model::Step& step, bool isCurrentRow, bool isCurrentCell);

    std::string noteToString(int8_t note) const;

    int currentPattern_ = 0;
    int cursorTrack_ = 0;
    int cursorRow_ = 0;
    int cursorColumn_ = 0;  // 0=note, 1=inst, 2=vol, 3-5=fx

    static constexpr int HEADER_HEIGHT = 40;
    static constexpr int TRACK_HEADER_HEIGHT = 24;
    static constexpr int ROW_HEIGHT = 18;
    static constexpr int COLUMN_WIDTHS[] = {36, 24, 24, 48, 48, 48};  // note, inst, vol, fx1, fx2, fx3
};

} // namespace ui
```

**Step 2: Write PatternScreen.cpp**

```cpp
#include "PatternScreen.h"

namespace ui {

PatternScreen::PatternScreen(model::Project& project, input::ModeManager& modeManager)
    : Screen(project, modeManager)
{
}

void PatternScreen::paint(juce::Graphics& g)
{
    g.fillAll(bgColor);

    auto area = getLocalBounds();

    drawHeader(g, area.removeFromTop(HEADER_HEIGHT));
    drawTrackHeaders(g, area.removeFromTop(TRACK_HEADER_HEIGHT));
    drawGrid(g, area);
}

void PatternScreen::resized()
{
}

void PatternScreen::navigate(int dx, int dy)
{
    auto* pattern = project_.getPattern(currentPattern_);
    if (!pattern) return;

    if (dx != 0)
    {
        // Move between columns within track, then between tracks
        cursorColumn_ += dx;
        if (cursorColumn_ < 0)
        {
            cursorTrack_ = std::max(0, cursorTrack_ - 1);
            cursorColumn_ = 5;
        }
        else if (cursorColumn_ > 5)
        {
            cursorTrack_ = std::min(15, cursorTrack_ + 1);
            cursorColumn_ = 0;
        }
    }

    if (dy != 0)
    {
        cursorRow_ = std::clamp(cursorRow_ + dy, 0, pattern->getLength() - 1);
    }

    repaint();
}

void PatternScreen::handleEdit(const juce::KeyPress& key)
{
    // TODO: Implement value editing
    juce::ignoreUnused(key);
}

void PatternScreen::drawHeader(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(headerColor);
    g.fillRect(area);

    g.setColour(fgColor);
    g.setFont(18.0f);

    auto* pattern = project_.getPattern(currentPattern_);
    juce::String title = "PATTERN";
    if (pattern)
    {
        title += ": " + juce::String(pattern->getName());
        title += " [" + juce::String(pattern->getLength()) + " steps]";
    }

    g.drawText(title, area.reduced(10, 0), juce::Justification::centredLeft, true);
}

void PatternScreen::drawTrackHeaders(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(headerColor.darker(0.2f));
    g.fillRect(area);

    g.setFont(12.0f);

    int trackWidth = 0;
    for (int w : COLUMN_WIDTHS) trackWidth += w;
    trackWidth += 4;  // padding

    int x = 40;  // row number column
    for (int t = 0; t < 16 && x < area.getWidth(); ++t)
    {
        g.setColour(t == cursorTrack_ ? cursorColor : fgColor.darker(0.3f));
        g.drawText(juce::String(t + 1), x, area.getY(), trackWidth, area.getHeight(),
                   juce::Justification::centred, true);
        x += trackWidth;
    }
}

void PatternScreen::drawGrid(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto* pattern = project_.getPattern(currentPattern_);
    if (!pattern) return;

    int trackWidth = 0;
    for (int w : COLUMN_WIDTHS) trackWidth += w;
    trackWidth += 4;

    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));

    int y = area.getY();
    for (int row = 0; row < pattern->getLength() && y < area.getBottom(); ++row)
    {
        bool isCurrentRow = (row == cursorRow_);

        // Row number
        g.setColour(isCurrentRow ? cursorColor : fgColor.darker(0.5f));
        g.drawText(juce::String::toHexString(row).toUpperCase().paddedLeft('0', 2),
                   0, y, 36, ROW_HEIGHT, juce::Justification::centred, true);

        // Tracks
        int x = 40;
        for (int t = 0; t < 16 && x < area.getWidth(); ++t)
        {
            const auto& step = pattern->getStep(t, row);
            bool isCurrentCell = isCurrentRow && (t == cursorTrack_);

            juce::Rectangle<int> cellArea(x, y, trackWidth, ROW_HEIGHT);
            drawStep(g, cellArea, step, isCurrentRow, isCurrentCell);

            x += trackWidth;
        }

        y += ROW_HEIGHT;
    }
}

void PatternScreen::drawStep(juce::Graphics& g, juce::Rectangle<int> area,
                              const model::Step& step, bool isCurrentRow, bool isCurrentCell)
{
    // Background
    if (isCurrentCell)
    {
        g.setColour(highlightColor);
        g.fillRect(area);
    }
    else if (isCurrentRow)
    {
        g.setColour(bgColor.brighter(0.1f));
        g.fillRect(area);
    }

    int x = area.getX() + 2;
    int y = area.getY();
    int h = area.getHeight();

    // Note
    g.setColour(step.note >= 0 ? fgColor : fgColor.darker(0.6f));
    g.drawText(noteToString(step.note), x, y, COLUMN_WIDTHS[0], h, juce::Justification::centredLeft, true);
    x += COLUMN_WIDTHS[0];

    // Instrument
    g.setColour(step.instrument >= 0 ? fgColor : fgColor.darker(0.6f));
    juce::String instStr = step.instrument >= 0 ? juce::String::toHexString(step.instrument).toUpperCase().paddedLeft('0', 2) : "..";
    g.drawText(instStr, x, y, COLUMN_WIDTHS[1], h, juce::Justification::centredLeft, true);
    x += COLUMN_WIDTHS[1];

    // Volume
    g.setColour(step.volume < 0xFF ? fgColor : fgColor.darker(0.6f));
    juce::String volStr = step.volume < 0xFF ? juce::String::toHexString(step.volume).toUpperCase().paddedLeft('0', 2) : "..";
    g.drawText(volStr, x, y, COLUMN_WIDTHS[2], h, juce::Justification::centredLeft, true);
    x += COLUMN_WIDTHS[2];

    // FX columns (simplified for now)
    for (int fx = 0; fx < 3; ++fx)
    {
        g.setColour(fgColor.darker(0.6f));
        g.drawText("....", x, y, COLUMN_WIDTHS[3 + fx], h, juce::Justification::centredLeft, true);
        x += COLUMN_WIDTHS[3 + fx];
    }
}

std::string PatternScreen::noteToString(int8_t note) const
{
    if (note == model::Step::NOTE_EMPTY) return "...";
    if (note == model::Step::NOTE_OFF) return "OFF";

    static const char* noteNames[] = {"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"};
    int octave = note / 12;
    int noteName = note % 12;
    return std::string(noteNames[noteName]) + std::to_string(octave);
}

} // namespace ui
```

**Step 3: Add to CMakeLists.txt**

Add to target_sources:
```cmake
    src/ui/PatternScreen.cpp
```

**Step 4: Commit**

```bash
git add src/ui/PatternScreen.h src/ui/PatternScreen.cpp CMakeLists.txt
git commit -m "feat: add PatternScreen with grid display"
```

---

### Task 14: Integrate screens into App

**Files:**
- Modify: `src/App.h`
- Modify: `src/App.cpp`

**Step 1: Update App.h**

```cpp
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
```

**Step 2: Update App.cpp**

```cpp
#include "App.h"
#include "ui/PatternScreen.h"

App::App()
{
    setSize(1280, 800);
    setWantsKeyboardFocus(true);
    addKeyListener(this);

    // Create key handler
    keyHandler_ = std::make_unique<input::KeyHandler>(modeManager_);

    // Set up key handler callbacks
    keyHandler_->onScreenSwitch = [this](int screen) { switchScreen(screen - 1); };
    keyHandler_->onPlayStop = [this]() { isPlaying_ = !isPlaying_; repaint(); };
    keyHandler_->onNavigate = [this](int dx, int dy) {
        if (screens_[currentScreen_])
            screens_[currentScreen_]->navigate(dx, dy);
    };

    // Create screens (only PatternScreen for now, others will be placeholders)
    screens_[3] = std::make_unique<ui::PatternScreen>(project_, modeManager_);

    // Add active screen as child
    if (screens_[currentScreen_])
    {
        addAndMakeVisible(screens_[currentScreen_].get());
    }

    // Mode change callback
    modeManager_.onModeChanged = [this](input::Mode) { repaint(); };
}

App::~App()
{
    removeKeyListener(this);
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

    // Transport (center)
    g.setColour(isPlaying_ ? juce::Colours::lightgreen : juce::Colours::grey);
    g.drawText(isPlaying_ ? "PLAYING" : "STOPPED",
               area.reduced(100, 0), juce::Justification::centred, true);

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

**Step 3: Build and test**

Run: `cmake --build build && ./build/Vitracker_artefacts/Debug/Vitracker.app/Contents/MacOS/Vitracker`
Expected: App shows pattern grid, can navigate with arrows, mode shows in status bar

**Step 4: Commit**

```bash
git add src/App.h src/App.cpp
git commit -m "feat: integrate PatternScreen with App and status bar"
```

---

## Phase 5: Initial Commit & Verify

### Task 15: Create .gitignore and initial commit

**Files:**
- Create: `.gitignore`

**Step 1: Write .gitignore**

```
build/
.DS_Store
*.user
.idea/
.vscode/
*.swp
*~
```

**Step 2: Verify build works**

Run: `cmake -B build && cmake --build build`
Expected: Build succeeds

**Step 3: Run and verify**

Run: `./build/Vitracker_artefacts/Debug/Vitracker.app/Contents/MacOS/Vitracker`
Expected:
- Window opens with pattern grid
- Press 1-6 to switch screens (4 shows grid, others show placeholders)
- Arrow keys navigate grid
- 'i' switches to EDIT mode
- 'v' switches to VISUAL mode
- ':' opens command mode
- Esc returns to NORMAL

**Step 4: Commit all**

```bash
git add .gitignore
git add -A
git commit -m "feat: initial Vitracker with pattern editor"
```

---

## Summary

This plan covers the foundation of Vitracker:

1. **Project setup** - CMake, JUCE, main entry point
2. **Data model** - Step, Pattern, Instrument, Chain, Song, Project
3. **Input system** - ModeManager (vim modes), KeyHandler (key dispatch)
4. **UI** - Base Screen class, PatternScreen with working navigation

**Next phases (not in this plan):**
- Phase 6: Remaining screens (Project, Song, Chain, Instrument, Mixer)
- Phase 7: Audio engine (Voice, playback, mixing)
- Phase 8: Effects (Reverb, Delay, Chorus, Drive, Sidechain)
- Phase 9: File I/O (save/load .vit files)
- Phase 10: Polish (undo/redo, autosave, groove templates)
