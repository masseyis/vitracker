# Vitracker Complete Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Complete Vitracker v1 - a fully functional keyboard-first tracker DAW with all screens, effects, visual mode operations, and song playback.

**Architecture:** Six screens (Project, Song, Chain, Pattern, Instrument, Mixer) with vim-style modal editing. Audio engine with 64-voice pool, 5 effect buses (Reverb, Delay, Chorus, Drive, Sidechain), and per-instrument sends. Song playback traverses chains, patterns play steps with FX commands.

**Tech Stack:** C++17, JUCE 8.0.4, Plaits DSP (embedded), CMake

---

## Phase 1: Missing Screens

### Task 1: Create SongScreen

**Files:**
- Create: `src/ui/SongScreen.h`
- Create: `src/ui/SongScreen.cpp`
- Modify: `src/App.cpp:39-42` (add SongScreen)

**Step 1: Create SongScreen.h**

```cpp
#pragma once

#include "Screen.h"

namespace ui {

class SongScreen : public Screen
{
public:
    SongScreen(model::Project& project, input::ModeManager& modeManager);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void navigate(int dx, int dy) override;
    void handleEditKey(const juce::KeyPress& key) override;

private:
    void drawGrid(juce::Graphics& g, juce::Rectangle<int> area);
    void drawCell(juce::Graphics& g, juce::Rectangle<int> area, int track, int row);

    int cursorTrack_ = 0;
    int cursorRow_ = 0;
    int scrollOffset_ = 0;
    static constexpr int VISIBLE_ROWS = 16;
};

} // namespace ui
```

**Step 2: Create SongScreen.cpp**

```cpp
#include "SongScreen.h"

namespace ui {

SongScreen::SongScreen(model::Project& project, input::ModeManager& modeManager)
    : Screen(project, modeManager)
{
}

void SongScreen::paint(juce::Graphics& g)
{
    g.fillAll(bgColor);

    auto area = getLocalBounds();

    // Header
    g.setFont(20.0f);
    g.setColour(fgColor);
    g.drawText("SONG", area.removeFromTop(40).reduced(20, 0), juce::Justification::centredLeft);

    area.removeFromTop(10);
    drawGrid(g, area.reduced(10));
}

void SongScreen::drawGrid(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto& song = project_.getSong();

    // Track headers
    int cellWidth = area.getWidth() / 16;
    int cellHeight = 24;
    auto headerArea = area.removeFromTop(cellHeight);

    g.setFont(12.0f);
    for (int t = 0; t < 16; ++t)
    {
        g.setColour(cursorTrack_ == t ? cursorColor : fgColor.darker(0.3f));
        g.drawText(juce::String(t + 1), headerArea.removeFromLeft(cellWidth),
                   juce::Justification::centred, true);
    }

    // Grid rows
    for (int row = 0; row < VISIBLE_ROWS && (scrollOffset_ + row) < 64; ++row)
    {
        int songRow = scrollOffset_ + row;
        auto rowArea = area.removeFromTop(cellHeight);

        for (int t = 0; t < 16; ++t)
        {
            auto cellArea = rowArea.removeFromLeft(cellWidth);
            drawCell(g, cellArea, t, songRow);
        }
    }
}

void SongScreen::drawCell(juce::Graphics& g, juce::Rectangle<int> area, int track, int row)
{
    bool isSelected = (cursorTrack_ == track && cursorRow_ == row);
    auto& song = project_.getSong();

    // Background
    if (isSelected)
    {
        g.setColour(cursorColor.withAlpha(0.3f));
        g.fillRect(area.reduced(1));
    }

    // Chain reference
    const auto& chainRef = song.getChainRef(track, row);
    if (!chainRef.empty())
    {
        auto* chain = project_.getChainByName(chainRef);
        g.setColour(isSelected ? cursorColor : fgColor);
        g.setFont(10.0f);
        g.drawText(chainRef, area, juce::Justification::centred, true);
    }
    else
    {
        g.setColour(highlightColor);
        g.drawText("---", area, juce::Justification::centred, true);
    }

    // Cell border
    g.setColour(highlightColor);
    g.drawRect(area.reduced(1), 1);
}

void SongScreen::resized()
{
}

void SongScreen::navigate(int dx, int dy)
{
    cursorTrack_ = std::clamp(cursorTrack_ + dx, 0, 15);
    cursorRow_ = std::clamp(cursorRow_ + dy, 0, 63);

    // Scroll if needed
    if (cursorRow_ < scrollOffset_)
        scrollOffset_ = cursorRow_;
    else if (cursorRow_ >= scrollOffset_ + VISIBLE_ROWS)
        scrollOffset_ = cursorRow_ - VISIBLE_ROWS + 1;

    repaint();
}

void SongScreen::handleEditKey(const juce::KeyPress& key)
{
    auto& song = project_.getSong();
    auto textChar = key.getTextCharacter();

    // Tab through chains
    if (key.getKeyCode() == juce::KeyPress::tabKey)
    {
        // Cycle through available chains
        int chainCount = project_.getChainCount();
        if (chainCount == 0) return;

        const auto& currentRef = song.getChainRef(cursorTrack_, cursorRow_);
        int currentIndex = -1;

        for (int i = 0; i < chainCount; ++i)
        {
            if (project_.getChain(i)->getName() == currentRef)
            {
                currentIndex = i;
                break;
            }
        }

        int nextIndex = (currentIndex + 1) % chainCount;
        song.setChainRef(cursorTrack_, cursorRow_, project_.getChain(nextIndex)->getName());
        repaint();
    }
    // Delete/clear with 'd' or Delete key
    else if (textChar == 'd' || key.getKeyCode() == juce::KeyPress::deleteKey ||
             key.getKeyCode() == juce::KeyPress::backspaceKey)
    {
        song.setChainRef(cursorTrack_, cursorRow_, "");
        repaint();
    }

    repaint();
}

} // namespace ui
```

**Step 3: Update Song model if needed**

Check/modify `src/model/Song.h` to ensure it has:
```cpp
const std::string& getChainRef(int track, int row) const;
void setChainRef(int track, int row, const std::string& chainName);
```

If missing, add to Song.h:
```cpp
#pragma once

#include <string>
#include <array>
#include <vector>

namespace model {

class Song
{
public:
    static constexpr int NUM_TRACKS = 16;
    static constexpr int MAX_ROWS = 64;

    Song() {
        for (auto& track : chainRefs_)
            track.resize(MAX_ROWS);
    }

    const std::string& getChainRef(int track, int row) const {
        static const std::string empty;
        if (track < 0 || track >= NUM_TRACKS || row < 0 || row >= MAX_ROWS)
            return empty;
        return chainRefs_[track][row];
    }

    void setChainRef(int track, int row, const std::string& chainName) {
        if (track >= 0 && track < NUM_TRACKS && row >= 0 && row < MAX_ROWS)
            chainRefs_[track][row] = chainName;
    }

    int getLength() const {
        int maxRow = 0;
        for (const auto& track : chainRefs_) {
            for (int i = MAX_ROWS - 1; i >= 0; --i) {
                if (!track[i].empty()) {
                    maxRow = std::max(maxRow, i + 1);
                    break;
                }
            }
        }
        return maxRow;
    }

private:
    std::array<std::vector<std::string>, NUM_TRACKS> chainRefs_;
};

} // namespace model
```

**Step 4: Wire up SongScreen in App.cpp**

Add include at top:
```cpp
#include "ui/SongScreen.h"
```

Add to screen creation (around line 41):
```cpp
screens_[1] = std::make_unique<ui::SongScreen>(project_, modeManager_);
```

Add edit key forwarding (around line 63):
```cpp
else if (auto* songScreen = dynamic_cast<ui::SongScreen*>(screens_[currentScreen_].get()))
{
    songScreen->handleEditKey(key);
}
```

**Step 5: Build and verify**

```bash
cd /Users/jamesmassey/ai-dev/vitracker
mkdir -p build && cd build && cmake .. && make -j8
```

**Step 6: Commit**

```bash
git add -A && git commit -m "feat: add SongScreen with grid navigation and chain selection"
```

---

### Task 2: Create ChainScreen

**Files:**
- Create: `src/ui/ChainScreen.h`
- Create: `src/ui/ChainScreen.cpp`
- Modify: `src/App.cpp` (add ChainScreen)

**Step 1: Create ChainScreen.h**

```cpp
#pragma once

#include "Screen.h"

namespace ui {

class ChainScreen : public Screen
{
public:
    ChainScreen(model::Project& project, input::ModeManager& modeManager);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void navigate(int dx, int dy) override;
    void handleEditKey(const juce::KeyPress& key) override;

private:
    void drawChainList(juce::Graphics& g, juce::Rectangle<int> area);
    void drawPatternList(juce::Graphics& g, juce::Rectangle<int> area);
    void drawScaleLock(juce::Graphics& g, juce::Rectangle<int> area);

    int currentChain_ = 0;
    int cursorRow_ = 0;  // 0 = name, 1 = scale lock, 2+ = patterns
    int scrollOffset_ = 0;
    bool editingName_ = false;
    std::string nameBuffer_;
};

} // namespace ui
```

**Step 2: Create ChainScreen.cpp**

```cpp
#include "ChainScreen.h"

namespace ui {

static const char* scaleNames[] = {
    "None", "C Major", "C Minor", "C# Major", "C# Minor",
    "D Major", "D Minor", "D# Major", "D# Minor",
    "E Major", "E Minor", "F Major", "F Minor",
    "F# Major", "F# Minor", "G Major", "G Minor",
    "G# Major", "G# Minor", "A Major", "A Minor",
    "A# Major", "A# Minor", "B Major", "B Minor"
};
static constexpr int NUM_SCALES = 25;

ChainScreen::ChainScreen(model::Project& project, input::ModeManager& modeManager)
    : Screen(project, modeManager)
{
}

void ChainScreen::paint(juce::Graphics& g)
{
    g.fillAll(bgColor);

    auto area = getLocalBounds().reduced(20);

    // Header with chain name
    auto* chain = project_.getChain(currentChain_);
    g.setFont(20.0f);
    g.setColour(cursorRow_ == 0 ? cursorColor : fgColor);

    juce::String title = "CHAIN " + juce::String(currentChain_ + 1) + ": ";
    if (editingName_ && cursorRow_ == 0)
        title += juce::String(nameBuffer_) + "_";
    else if (chain)
        title += chain->getName();
    else
        title += "---";

    g.drawText(title, area.removeFromTop(40), juce::Justification::centredLeft);

    area.removeFromTop(10);

    // Scale lock
    drawScaleLock(g, area.removeFromTop(30));

    area.removeFromTop(20);

    // Two columns: chain list (left), pattern list (right)
    auto leftCol = area.removeFromLeft(150);
    area.removeFromLeft(20);

    drawChainList(g, leftCol);
    drawPatternList(g, area);
}

void ChainScreen::drawChainList(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setFont(14.0f);
    g.setColour(fgColor);
    g.drawText("CHAINS", area.removeFromTop(25), juce::Justification::centredLeft);

    int numChains = project_.getChainCount();
    for (int i = 0; i < numChains && i < 20; ++i)
    {
        auto* chain = project_.getChain(i);
        bool isSelected = (i == currentChain_);

        g.setColour(isSelected ? cursorColor : fgColor.darker(0.3f));
        juce::String text = juce::String(i + 1) + ". " + (chain ? chain->getName() : "---");
        g.drawText(text, area.removeFromTop(20), juce::Justification::centredLeft);
    }
}

void ChainScreen::drawScaleLock(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto* chain = project_.getChain(currentChain_);
    if (!chain) return;

    g.setFont(14.0f);
    g.setColour(cursorRow_ == 1 ? cursorColor : fgColor);

    juce::String scaleText = "Scale Lock: " + juce::String(scaleNames[chain->getScaleLockIndex()]);
    g.drawText(scaleText, area, juce::Justification::centredLeft);
}

void ChainScreen::drawPatternList(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto* chain = project_.getChain(currentChain_);
    if (!chain) return;

    g.setFont(14.0f);
    g.setColour(fgColor);
    g.drawText("PATTERNS IN CHAIN", area.removeFromTop(25), juce::Justification::centredLeft);

    const auto& patterns = chain->getPatternNames();
    for (size_t i = 0; i < patterns.size() && i < 32; ++i)
    {
        bool isSelected = (cursorRow_ == static_cast<int>(i) + 2);

        g.setColour(isSelected ? cursorColor : fgColor);
        juce::String text = juce::String(i + 1) + ". " + patterns[i];
        g.drawText(text, area.removeFromTop(20), juce::Justification::centredLeft);
    }

    // Add slot
    bool isAddSelected = (cursorRow_ == static_cast<int>(patterns.size()) + 2);
    g.setColour(isAddSelected ? cursorColor : fgColor.darker(0.5f));
    g.drawText("+ Add Pattern", area.removeFromTop(20), juce::Justification::centredLeft);
}

void ChainScreen::resized()
{
}

void ChainScreen::navigate(int dx, int dy)
{
    auto* chain = project_.getChain(currentChain_);

    if (dx != 0)
    {
        // Switch chains
        int numChains = project_.getChainCount();
        currentChain_ = (currentChain_ + dx + numChains) % numChains;
        cursorRow_ = std::min(cursorRow_, 2);  // Reset to safe position
    }

    if (dy != 0)
    {
        int maxRow = 2;  // name, scale lock, first pattern slot
        if (chain)
            maxRow = static_cast<int>(chain->getPatternNames().size()) + 2;

        cursorRow_ = std::clamp(cursorRow_ + dy, 0, maxRow);
    }

    repaint();
}

void ChainScreen::handleEditKey(const juce::KeyPress& key)
{
    auto* chain = project_.getChain(currentChain_);
    if (!chain) return;

    auto textChar = key.getTextCharacter();

    // Name editing (row 0)
    if (cursorRow_ == 0)
    {
        if (!editingName_)
        {
            editingName_ = true;
            nameBuffer_ = chain->getName();
        }

        if (key.getKeyCode() == juce::KeyPress::returnKey)
        {
            chain->setName(nameBuffer_);
            editingName_ = false;
        }
        else if (key.getKeyCode() == juce::KeyPress::backspaceKey && !nameBuffer_.empty())
        {
            nameBuffer_.pop_back();
        }
        else if (textChar >= ' ' && textChar <= '~' && nameBuffer_.length() < 16)
        {
            nameBuffer_ += static_cast<char>(textChar);
        }
    }
    // Scale lock (row 1)
    else if (cursorRow_ == 1)
    {
        int currentScale = chain->getScaleLockIndex();
        if (textChar == '+' || textChar == '=' || key.getKeyCode() == juce::KeyPress::rightKey)
            chain->setScaleLockIndex((currentScale + 1) % NUM_SCALES);
        else if (textChar == '-' || key.getKeyCode() == juce::KeyPress::leftKey)
            chain->setScaleLockIndex((currentScale - 1 + NUM_SCALES) % NUM_SCALES);
    }
    // Pattern list (row 2+)
    else
    {
        int patternIdx = cursorRow_ - 2;
        auto& patterns = chain->getPatternNames();

        if (patternIdx == static_cast<int>(patterns.size()))
        {
            // Add slot - Tab to add first available pattern
            if (key.getKeyCode() == juce::KeyPress::tabKey)
            {
                if (project_.getPatternCount() > 0)
                {
                    chain->addPattern(project_.getPattern(0)->getName());
                }
            }
        }
        else if (patternIdx < static_cast<int>(patterns.size()))
        {
            // Edit existing pattern reference
            if (key.getKeyCode() == juce::KeyPress::tabKey)
            {
                // Cycle through patterns
                int numPatterns = project_.getPatternCount();
                if (numPatterns > 0)
                {
                    int currentIdx = 0;
                    for (int i = 0; i < numPatterns; ++i)
                    {
                        if (project_.getPattern(i)->getName() == patterns[patternIdx])
                        {
                            currentIdx = i;
                            break;
                        }
                    }
                    int nextIdx = (currentIdx + 1) % numPatterns;
                    chain->setPattern(patternIdx, project_.getPattern(nextIdx)->getName());
                }
            }
            else if (textChar == 'd' || key.getKeyCode() == juce::KeyPress::deleteKey)
            {
                chain->removePattern(patternIdx);
                if (cursorRow_ > 2) cursorRow_--;
            }
        }
    }

    repaint();
}

} // namespace ui
```

**Step 3: Update Chain model**

Ensure `src/model/Chain.h` has:
```cpp
#pragma once

#include <string>
#include <vector>

namespace model {

class Chain
{
public:
    Chain(const std::string& name = "New Chain") : name_(name) {}

    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

    int getScaleLockIndex() const { return scaleLockIndex_; }
    void setScaleLockIndex(int index) { scaleLockIndex_ = index; }

    const std::vector<std::string>& getPatternNames() const { return patternNames_; }

    void addPattern(const std::string& name) { patternNames_.push_back(name); }

    void setPattern(int index, const std::string& name) {
        if (index >= 0 && index < static_cast<int>(patternNames_.size()))
            patternNames_[index] = name;
    }

    void removePattern(int index) {
        if (index >= 0 && index < static_cast<int>(patternNames_.size()))
            patternNames_.erase(patternNames_.begin() + index);
    }

    void insertPattern(int index, const std::string& name) {
        if (index >= 0 && index <= static_cast<int>(patternNames_.size()))
            patternNames_.insert(patternNames_.begin() + index, name);
    }

private:
    std::string name_;
    int scaleLockIndex_ = 0;  // 0 = None
    std::vector<std::string> patternNames_;
};

} // namespace model
```

**Step 4: Wire up ChainScreen in App.cpp**

Add include:
```cpp
#include "ui/ChainScreen.h"
```

Add screen creation:
```cpp
screens_[2] = std::make_unique<ui::ChainScreen>(project_, modeManager_);
```

Add edit key forwarding:
```cpp
else if (auto* chainScreen = dynamic_cast<ui::ChainScreen*>(screens_[currentScreen_].get()))
{
    chainScreen->handleEditKey(key);
}
```

**Step 5: Build and verify**

```bash
cd build && make -j8
```

**Step 6: Commit**

```bash
git add -A && git commit -m "feat: add ChainScreen with pattern list and scale lock"
```

---

## Phase 2: Effects System

### Task 3: Create Effects Processor

**Files:**
- Create: `src/audio/Effects.h`
- Create: `src/audio/Effects.cpp`

**Step 1: Create Effects.h**

```cpp
#pragma once

#include <array>
#include <cmath>

namespace audio {

// Simple reverb using Schroeder algorithm
class Reverb
{
public:
    void init(double sampleRate);
    void setParams(float size, float damping, float mix);
    void process(float& left, float& right);

private:
    static constexpr int NUM_COMBS = 4;
    static constexpr int NUM_ALLPASS = 2;

    std::array<std::vector<float>, NUM_COMBS> combBuffersL_;
    std::array<std::vector<float>, NUM_COMBS> combBuffersR_;
    std::array<int, NUM_COMBS> combIndices_{};
    std::array<float, NUM_COMBS> combFilters_{};

    std::array<std::vector<float>, NUM_ALLPASS> allpassBuffersL_;
    std::array<std::vector<float>, NUM_ALLPASS> allpassBuffersR_;
    std::array<int, NUM_ALLPASS> allpassIndices_{};

    float size_ = 0.5f;
    float damping_ = 0.5f;
    float mix_ = 0.3f;
};

// Tempo-synced delay
class Delay
{
public:
    void init(double sampleRate);
    void setParams(float time, float feedback, float mix);
    void setTempo(float bpm);
    void process(float& left, float& right);

private:
    std::vector<float> bufferL_;
    std::vector<float> bufferR_;
    int writeIndex_ = 0;
    int delaysamples_ = 0;

    double sampleRate_ = 48000.0;
    float time_ = 0.5f;  // 0-1 maps to 1/16 - 1/1
    float feedback_ = 0.4f;
    float mix_ = 0.3f;
    float bpm_ = 120.0f;
};

// Simple chorus
class Chorus
{
public:
    void init(double sampleRate);
    void setParams(float rate, float depth, float mix);
    void process(float& left, float& right);

private:
    std::vector<float> bufferL_;
    std::vector<float> bufferR_;
    int writeIndex_ = 0;
    float phase_ = 0.0f;

    double sampleRate_ = 48000.0;
    float rate_ = 0.5f;
    float depth_ = 0.5f;
    float mix_ = 0.3f;
};

// Soft saturation drive
class Drive
{
public:
    void setParams(float gain, float tone);
    void process(float& left, float& right);

private:
    float gain_ = 0.5f;
    float tone_ = 0.5f;
    float lpState_ = 0.0f;
};

// Sidechain compressor
class Sidechain
{
public:
    void init(double sampleRate);
    void trigger();  // Called when kick/trigger instrument plays
    void process(float& left, float& right, float duckAmount);

private:
    double sampleRate_ = 48000.0;
    float envelope_ = 0.0f;
    float attack_ = 0.001f;
    float release_ = 0.2f;
};

// All effects combined
class EffectsProcessor
{
public:
    void init(double sampleRate);
    void setTempo(float bpm);

    Reverb reverb;
    Delay delay;
    Chorus chorus;
    Drive drive;
    Sidechain sidechain;

    // Process audio with send levels
    void process(float& left, float& right,
                 float reverbSend, float delaySend, float chorusSend,
                 float driveSend, float sidechainDuck);
};

} // namespace audio
```

**Step 2: Create Effects.cpp**

```cpp
#include "Effects.h"
#include <algorithm>

namespace audio {

// ============ REVERB ============

void Reverb::init(double sampleRate)
{
    const int combDelays[] = {1116, 1188, 1277, 1356};
    const int allpassDelays[] = {556, 441};

    for (int i = 0; i < NUM_COMBS; ++i)
    {
        int size = static_cast<int>(combDelays[i] * sampleRate / 44100.0);
        combBuffersL_[i].resize(size, 0.0f);
        combBuffersR_[i].resize(size, 0.0f);
        combIndices_[i] = 0;
        combFilters_[i] = 0.0f;
    }

    for (int i = 0; i < NUM_ALLPASS; ++i)
    {
        int size = static_cast<int>(allpassDelays[i] * sampleRate / 44100.0);
        allpassBuffersL_[i].resize(size, 0.0f);
        allpassBuffersR_[i].resize(size, 0.0f);
        allpassIndices_[i] = 0;
    }
}

void Reverb::setParams(float size, float damping, float mix)
{
    size_ = size;
    damping_ = damping;
    mix_ = mix;
}

void Reverb::process(float& left, float& right)
{
    float inputL = left;
    float inputR = right;
    float outL = 0.0f;
    float outR = 0.0f;

    float feedback = 0.7f + size_ * 0.28f;
    float damp = damping_ * 0.4f;

    // Comb filters
    for (int i = 0; i < NUM_COMBS; ++i)
    {
        auto& bufL = combBuffersL_[i];
        auto& bufR = combBuffersR_[i];
        int& idx = combIndices_[i];
        float& filt = combFilters_[i];

        float readL = bufL[idx];
        float readR = bufR[idx];

        filt = readL * (1.0f - damp) + filt * damp;

        bufL[idx] = inputL + filt * feedback;
        bufR[idx] = inputR + readR * (1.0f - damp) * feedback;

        outL += readL;
        outR += readR;

        idx = (idx + 1) % static_cast<int>(bufL.size());
    }

    outL /= NUM_COMBS;
    outR /= NUM_COMBS;

    // Allpass filters
    for (int i = 0; i < NUM_ALLPASS; ++i)
    {
        auto& bufL = allpassBuffersL_[i];
        auto& bufR = allpassBuffersR_[i];
        int& idx = allpassIndices_[i];

        float readL = bufL[idx];
        float readR = bufR[idx];

        bufL[idx] = outL + readL * 0.5f;
        bufR[idx] = outR + readR * 0.5f;

        outL = readL - outL * 0.5f;
        outR = readR - outR * 0.5f;

        idx = (idx + 1) % static_cast<int>(bufL.size());
    }

    left = inputL * (1.0f - mix_) + outL * mix_;
    right = inputR * (1.0f - mix_) + outR * mix_;
}

// ============ DELAY ============

void Delay::init(double sampleRate)
{
    sampleRate_ = sampleRate;
    int maxDelay = static_cast<int>(sampleRate * 2.0);  // 2 seconds max
    bufferL_.resize(maxDelay, 0.0f);
    bufferR_.resize(maxDelay, 0.0f);
    writeIndex_ = 0;
}

void Delay::setParams(float time, float feedback, float mix)
{
    time_ = time;
    feedback_ = feedback;
    mix_ = mix;

    // Calculate delay in samples based on tempo sync
    // time 0-1 maps to 1/16 to 1/1 note
    float beatFraction = 0.0625f * std::pow(2.0f, time_ * 4.0f);  // 1/16 to 1/1
    float delaySeconds = (60.0f / bpm_) * beatFraction;
    delaysamples_ = static_cast<int>(delaySeconds * sampleRate_);
    delaysamples_ = std::clamp(delaysamples_, 1, static_cast<int>(bufferL_.size()) - 1);
}

void Delay::setTempo(float bpm)
{
    bpm_ = bpm;
    setParams(time_, feedback_, mix_);  // Recalculate delay
}

void Delay::process(float& left, float& right)
{
    int readIndex = (writeIndex_ - delaysamples_ + static_cast<int>(bufferL_.size()))
                    % static_cast<int>(bufferL_.size());

    float delayedL = bufferL_[readIndex];
    float delayedR = bufferR_[readIndex];

    bufferL_[writeIndex_] = left + delayedL * feedback_;
    bufferR_[writeIndex_] = right + delayedR * feedback_;

    writeIndex_ = (writeIndex_ + 1) % static_cast<int>(bufferL_.size());

    left = left * (1.0f - mix_) + delayedL * mix_;
    right = right * (1.0f - mix_) + delayedR * mix_;
}

// ============ CHORUS ============

void Chorus::init(double sampleRate)
{
    sampleRate_ = sampleRate;
    int size = static_cast<int>(sampleRate * 0.05);  // 50ms buffer
    bufferL_.resize(size, 0.0f);
    bufferR_.resize(size, 0.0f);
    writeIndex_ = 0;
    phase_ = 0.0f;
}

void Chorus::setParams(float rate, float depth, float mix)
{
    rate_ = rate;
    depth_ = depth;
    mix_ = mix;
}

void Chorus::process(float& left, float& right)
{
    // LFO
    float lfoL = std::sin(phase_ * 2.0f * 3.14159f);
    float lfoR = std::sin((phase_ + 0.25f) * 2.0f * 3.14159f);

    // Update phase
    float freq = 0.1f + rate_ * 5.0f;  // 0.1 - 5.1 Hz
    phase_ += freq / sampleRate_;
    if (phase_ >= 1.0f) phase_ -= 1.0f;

    // Calculate delay times
    float baseDelay = 0.007f * sampleRate_;  // 7ms
    float modDepth = depth_ * 0.003f * sampleRate_;  // +/- 3ms

    float delayL = baseDelay + lfoL * modDepth;
    float delayR = baseDelay + lfoR * modDepth;

    // Write to buffer
    bufferL_[writeIndex_] = left;
    bufferR_[writeIndex_] = right;

    // Read with interpolation
    auto readInterp = [](const std::vector<float>& buf, int writeIdx, float delay) {
        int size = static_cast<int>(buf.size());
        float readPos = writeIdx - delay;
        while (readPos < 0) readPos += size;

        int idx0 = static_cast<int>(readPos) % size;
        int idx1 = (idx0 + 1) % size;
        float frac = readPos - std::floor(readPos);

        return buf[idx0] * (1.0f - frac) + buf[idx1] * frac;
    };

    float chorusL = readInterp(bufferL_, writeIndex_, delayL);
    float chorusR = readInterp(bufferR_, writeIndex_, delayR);

    writeIndex_ = (writeIndex_ + 1) % static_cast<int>(bufferL_.size());

    left = left * (1.0f - mix_) + chorusL * mix_;
    right = right * (1.0f - mix_) + chorusR * mix_;
}

// ============ DRIVE ============

void Drive::setParams(float gain, float tone)
{
    gain_ = gain;
    tone_ = tone;
}

void Drive::process(float& left, float& right)
{
    float driveAmount = 1.0f + gain_ * 10.0f;

    // Soft clip
    auto softClip = [](float x, float drive) {
        x *= drive;
        return x / (1.0f + std::abs(x));
    };

    left = softClip(left, driveAmount);
    right = softClip(right, driveAmount);

    // Simple tone filter (low pass)
    float cutoff = 0.3f + tone_ * 0.7f;
    lpState_ = lpState_ + cutoff * (left - lpState_);
    left = lpState_ * tone_ + left * (1.0f - tone_ * 0.5f);

    // Normalize output
    left *= 1.0f / driveAmount;
    right *= 1.0f / driveAmount;
}

// ============ SIDECHAIN ============

void Sidechain::init(double sampleRate)
{
    sampleRate_ = sampleRate;
    envelope_ = 0.0f;
}

void Sidechain::trigger()
{
    envelope_ = 1.0f;
}

void Sidechain::process(float& left, float& right, float duckAmount)
{
    // Envelope follower
    float target = 0.0f;
    float coeff = (envelope_ > target) ? release_ : attack_;

    envelope_ = envelope_ + (target - envelope_) * (1.0f - std::exp(-1.0f / (coeff * sampleRate_)));

    // Apply ducking
    float gain = 1.0f - envelope_ * duckAmount;
    left *= gain;
    right *= gain;
}

// ============ EFFECTS PROCESSOR ============

void EffectsProcessor::init(double sampleRate)
{
    reverb.init(sampleRate);
    delay.init(sampleRate);
    chorus.init(sampleRate);
    sidechain.init(sampleRate);

    // Default settings
    reverb.setParams(0.5f, 0.5f, 0.3f);
    delay.setParams(0.5f, 0.4f, 0.3f);
    chorus.setParams(0.5f, 0.5f, 0.3f);
    drive.setParams(0.3f, 0.5f);
}

void EffectsProcessor::setTempo(float bpm)
{
    delay.setTempo(bpm);
}

void EffectsProcessor::process(float& left, float& right,
                               float reverbSend, float delaySend, float chorusSend,
                               float driveSend, float sidechainDuck)
{
    float dryL = left;
    float dryR = right;

    // Process each effect with send level
    if (reverbSend > 0.001f)
    {
        float wetL = dryL * reverbSend;
        float wetR = dryR * reverbSend;
        reverb.process(wetL, wetR);
        left += wetL;
        right += wetR;
    }

    if (delaySend > 0.001f)
    {
        float wetL = dryL * delaySend;
        float wetR = dryR * delaySend;
        delay.process(wetL, wetR);
        left += wetL;
        right += wetR;
    }

    if (chorusSend > 0.001f)
    {
        float wetL = dryL * chorusSend;
        float wetR = dryR * chorusSend;
        chorus.process(wetL, wetR);
        left += wetL;
        right += wetR;
    }

    if (driveSend > 0.001f)
    {
        float wetL = left * driveSend;
        float wetR = right * driveSend;
        drive.process(wetL, wetR);
        left = left * (1.0f - driveSend) + wetL;
        right = right * (1.0f - driveSend) + wetR;
    }

    if (sidechainDuck > 0.001f)
    {
        sidechain.process(left, right, sidechainDuck);
    }
}

} // namespace audio
```

**Step 3: Build and verify**

```bash
cd build && make -j8
```

**Step 4: Commit**

```bash
git add -A && git commit -m "feat: add Effects processor with reverb, delay, chorus, drive, sidechain"
```

---

### Task 4: Integrate Effects into AudioEngine

**Files:**
- Modify: `src/audio/AudioEngine.h`
- Modify: `src/audio/AudioEngine.cpp`

**Step 1: Add Effects to AudioEngine.h**

Add include:
```cpp
#include "Effects.h"
```

Add member:
```cpp
EffectsProcessor effects_;
```

**Step 2: Update AudioEngine.cpp**

In `prepareToPlay`:
```cpp
effects_.init(sampleRate);
effects_.setTempo(project_ ? project_->getTempo() : 120.0f);
```

In `getNextAudioBlock`, after mixing voices but before writing to buffer, add effects processing for each voice based on instrument sends:
```cpp
void AudioEngine::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto* leftChannel = bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
    auto* rightChannel = bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample);

    // Update tempo
    if (project_)
        effects_.setTempo(project_->getTempo());

    for (int i = 0; i < bufferToFill.numSamples; ++i)
    {
        float left = 0.0f;
        float right = 0.0f;

        // Advance playhead
        if (playing_)
        {
            samplesUntilNextRow_ -= 1.0;
            if (samplesUntilNextRow_ <= 0)
            {
                advancePlayhead();
                double bpm = project_ ? project_->getTempo() : 120.0;
                double samplesPerBeat = sampleRate_ * 60.0 / bpm;
                double samplesPerRow = samplesPerBeat / 4.0;  // 16th notes
                samplesUntilNextRow_ += samplesPerRow;
            }
        }

        // Mix all voices with their instrument's effect sends
        for (auto& voice : voices_)
        {
            if (voice.isActive())
            {
                float voiceL, voiceR;
                voice.process(voiceL, voiceR);

                // Get instrument sends
                int instIdx = voice.getInstrumentIndex();
                float reverbSend = 0.0f, delaySend = 0.0f, chorusSend = 0.0f;
                float driveSend = 0.0f, sidechainDuck = 0.0f;

                if (project_ && instIdx >= 0 && instIdx < project_->getInstrumentCount())
                {
                    const auto& sends = project_->getInstrument(instIdx)->getSends();
                    reverbSend = sends.reverb;
                    delaySend = sends.delay;
                    chorusSend = sends.chorus;
                    driveSend = sends.drive;
                    sidechainDuck = sends.sidechainDuck;
                }

                // Process effects for this voice
                effects_.process(voiceL, voiceR, reverbSend, delaySend, chorusSend,
                                driveSend, sidechainDuck);

                left += voiceL;
                right += voiceR;
            }
        }

        // Apply master volume
        float masterVol = project_ ? project_->getMixer().masterVolume : 0.8f;
        left *= masterVol;
        right *= masterVol;

        // Clip
        left = std::clamp(left, -1.0f, 1.0f);
        right = std::clamp(right, -1.0f, 1.0f);

        leftChannel[i] = left;
        rightChannel[i] = right;
    }
}
```

**Step 3: Add sidechain trigger on certain instruments**

When triggering a note, check if it should trigger sidechain:
```cpp
void AudioEngine::triggerNote(int track, int note, int instrumentIndex, float velocity)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Release previous note on this track
    if (trackVoices_[track])
    {
        trackVoices_[track]->release();
    }

    Voice* voice = allocateVoice(note);
    if (voice && project_)
    {
        auto* instrument = project_->getInstrument(instrumentIndex);
        if (instrument)
        {
            voice->trigger(note, velocity, instrumentIndex, instrument->getParams());
            trackVoices_[track] = voice;

            // Check if this instrument triggers sidechain
            // (e.g., if it's the bass drum or has high sidechain duck amount itself)
            // For simplicity, trigger sidechain on any note with sidechainDuck > 0
            if (instrument->getSends().sidechainDuck > 0.5f)
            {
                effects_.sidechain.trigger();
            }
        }
    }
}
```

**Step 4: Build and verify**

```bash
cd build && make -j8
```

**Step 5: Commit**

```bash
git add -A && git commit -m "feat: integrate effects processor into AudioEngine"
```

---

## Phase 3: Visual Mode and Clipboard

### Task 5: Implement Selection and Clipboard

**Files:**
- Create: `src/model/Clipboard.h`
- Modify: `src/ui/PatternScreen.h`
- Modify: `src/ui/PatternScreen.cpp`

**Step 1: Create Clipboard.h**

```cpp
#pragma once

#include "Step.h"
#include <vector>

namespace model {

struct Selection
{
    int startTrack = 0;
    int startRow = 0;
    int endTrack = 0;
    int endRow = 0;

    bool isValid() const { return startTrack >= 0 && startRow >= 0; }

    int minTrack() const { return std::min(startTrack, endTrack); }
    int maxTrack() const { return std::max(startTrack, endTrack); }
    int minRow() const { return std::min(startRow, endRow); }
    int maxRow() const { return std::max(startRow, endRow); }

    int width() const { return maxTrack() - minTrack() + 1; }
    int height() const { return maxRow() - minRow() + 1; }
};

class Clipboard
{
public:
    static Clipboard& instance() {
        static Clipboard inst;
        return inst;
    }

    void copy(const std::vector<std::vector<Step>>& data) {
        data_ = data;
    }

    const std::vector<std::vector<Step>>& getData() const { return data_; }
    bool isEmpty() const { return data_.empty(); }

    int getWidth() const { return data_.empty() ? 0 : static_cast<int>(data_.size()); }
    int getHeight() const { return data_.empty() || data_[0].empty() ? 0 : static_cast<int>(data_[0].size()); }

private:
    Clipboard() = default;
    std::vector<std::vector<Step>> data_;  // [track][row]
};

} // namespace model
```

**Step 2: Update PatternScreen.h**

Add members:
```cpp
#include "../model/Clipboard.h"

// In class:
model::Selection selection_;
bool hasSelection_ = false;

void startSelection();
void updateSelection();
void yankSelection();
void deleteSelection();
void paste();
void transpose(int semitones);
```

**Step 3: Update PatternScreen.cpp**

Add visual mode handling:
```cpp
void PatternScreen::startSelection()
{
    selection_.startTrack = cursorTrack_;
    selection_.startRow = cursorRow_;
    selection_.endTrack = cursorTrack_;
    selection_.endRow = cursorRow_;
    hasSelection_ = true;
}

void PatternScreen::updateSelection()
{
    if (hasSelection_)
    {
        selection_.endTrack = cursorTrack_;
        selection_.endRow = cursorRow_;
    }
}

void PatternScreen::yankSelection()
{
    if (!hasSelection_) return;

    auto* pattern = project_.getPattern(currentPattern_);
    if (!pattern) return;

    int minT = selection_.minTrack();
    int maxT = selection_.maxTrack();
    int minR = selection_.minRow();
    int maxR = selection_.maxRow();

    std::vector<std::vector<model::Step>> data;
    data.resize(maxT - minT + 1);

    for (int t = minT; t <= maxT; ++t)
    {
        for (int r = minR; r <= maxR; ++r)
        {
            data[t - minT].push_back(pattern->getStep(t, r));
        }
    }

    model::Clipboard::instance().copy(data);
    hasSelection_ = false;
}

void PatternScreen::deleteSelection()
{
    if (!hasSelection_) return;

    auto* pattern = project_.getPattern(currentPattern_);
    if (!pattern) return;

    int minT = selection_.minTrack();
    int maxT = selection_.maxTrack();
    int minR = selection_.minRow();
    int maxR = selection_.maxRow();

    for (int t = minT; t <= maxT; ++t)
    {
        for (int r = minR; r <= maxR; ++r)
        {
            pattern->setStep(t, r, model::Step{});
        }
    }

    hasSelection_ = false;
    repaint();
}

void PatternScreen::paste()
{
    auto& clipboard = model::Clipboard::instance();
    if (clipboard.isEmpty()) return;

    auto* pattern = project_.getPattern(currentPattern_);
    if (!pattern) return;

    const auto& data = clipboard.getData();
    int width = clipboard.getWidth();
    int height = clipboard.getHeight();

    for (int t = 0; t < width && (cursorTrack_ + t) < 16; ++t)
    {
        for (int r = 0; r < height && (cursorRow_ + r) < pattern->getLength(); ++r)
        {
            pattern->setStep(cursorTrack_ + t, cursorRow_ + r, data[t][r]);
        }
    }

    repaint();
}

void PatternScreen::transpose(int semitones)
{
    auto* pattern = project_.getPattern(currentPattern_);
    if (!pattern) return;

    int minT = hasSelection_ ? selection_.minTrack() : cursorTrack_;
    int maxT = hasSelection_ ? selection_.maxTrack() : cursorTrack_;
    int minR = hasSelection_ ? selection_.minRow() : cursorRow_;
    int maxR = hasSelection_ ? selection_.maxRow() : cursorRow_;

    for (int t = minT; t <= maxT; ++t)
    {
        for (int r = minR; r <= maxR; ++r)
        {
            auto step = pattern->getStep(t, r);
            if (step.note > 0 && step.note < 128)  // Valid MIDI note
            {
                step.note = std::clamp(step.note + semitones, 0, 127);
                pattern->setStep(t, r, step);
            }
        }
    }

    repaint();
}
```

Update `paint()` to draw selection:
```cpp
// In drawGrid, add selection highlighting:
if (hasSelection_)
{
    int minT = selection_.minTrack();
    int maxT = selection_.maxTrack();
    int minR = selection_.minRow();
    int maxR = selection_.maxRow();

    for (int t = minT; t <= maxT; ++t)
    {
        for (int r = minR; r <= maxR; ++r)
        {
            // Add selection highlight to cell drawing
        }
    }
}
```

**Step 4: Wire up callbacks in App.cpp**

```cpp
// In constructor:
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
```

**Step 5: Update KeyHandler for Visual mode**

In `handleVisualMode`:
```cpp
if (textChar == '>')
{
    if (onTranspose) onTranspose(1);
    return true;
}
if (textChar == '<')
{
    if (onTranspose) onTranspose(-1);
    return true;
}
```

**Step 6: Build and verify**

```bash
cd build && make -j8
```

**Step 7: Commit**

```bash
git add -A && git commit -m "feat: add visual mode selection, clipboard, yank, paste, transpose"
```

---

## Phase 4: FX Commands

### Task 6: Implement FX Command Processing

**Files:**
- Modify: `src/model/Step.h`
- Modify: `src/audio/Voice.h`
- Modify: `src/audio/Voice.cpp`
- Modify: `src/audio/AudioEngine.cpp`

**Step 1: Update Step.h with FX command types**

```cpp
#pragma once

#include <cstdint>
#include <string>

namespace model {

enum class FXCommand : uint8_t
{
    None = 0,
    ARP,   // Arpeggio - value = semitones (high nibble up, low nibble down)
    POR,   // Portamento - value = speed
    VIB,   // Vibrato - value = speed/depth
    VOL,   // Volume slide - value = up/down amount
    PAN,   // Pan - value = position
    DLY    // Retrigger/delay - value = ticks
};

struct FX
{
    FXCommand command = FXCommand::None;
    uint8_t value = 0;

    bool isEmpty() const { return command == FXCommand::None; }

    std::string toString() const {
        if (command == FXCommand::None) return "---";
        static const char* names[] = {"---", "ARP", "POR", "VIB", "VOL", "PAN", "DLY"};
        char buf[8];
        snprintf(buf, sizeof(buf), "%s%02X", names[static_cast<int>(command)], value);
        return buf;
    }
};

struct Step
{
    int note = -1;          // -1 = empty, 0 = OFF, 1-127 = MIDI note
    int instrument = -1;    // -1 = empty
    int volume = -1;        // -1 = empty, 0-255
    FX fx1;
    FX fx2;
    FX fx3;

    bool isEmpty() const {
        return note < 0 && instrument < 0 && volume < 0 &&
               fx1.isEmpty() && fx2.isEmpty() && fx3.isEmpty();
    }
};

} // namespace model
```

**Step 2: Update Voice.h with FX state**

Add to Voice class:
```cpp
// FX state
float portamentoTarget_ = 0.0f;
float portamentoSpeed_ = 0.0f;
float vibratoPhase_ = 0.0f;
float vibratoSpeed_ = 0.0f;
float vibratoDepth_ = 0.0f;
int arpIndex_ = 0;
int arpNotes_[3] = {0, 0, 0};
int arpTicks_ = 0;

void setFX(model::FXCommand cmd, uint8_t value);
void processFX();
```

**Step 3: Implement FX processing in Voice.cpp**

```cpp
void Voice::setFX(model::FXCommand cmd, uint8_t value)
{
    switch (cmd)
    {
        case model::FXCommand::ARP:
            arpNotes_[0] = 0;
            arpNotes_[1] = (value >> 4) & 0x0F;
            arpNotes_[2] = value & 0x0F;
            arpIndex_ = 0;
            arpTicks_ = 0;
            break;

        case model::FXCommand::POR:
            portamentoSpeed_ = value / 255.0f * 0.1f;  // Speed factor
            break;

        case model::FXCommand::VIB:
            vibratoSpeed_ = ((value >> 4) & 0x0F) / 15.0f * 10.0f;  // 0-10 Hz
            vibratoDepth_ = (value & 0x0F) / 15.0f * 0.5f;  // 0-0.5 semitones
            break;

        case model::FXCommand::VOL:
            // Volume slide handled per-tick in processFX
            break;

        case model::FXCommand::PAN:
            // Pan handled in mixer
            break;

        case model::FXCommand::DLY:
            // Retrigger handled in AudioEngine
            break;

        default:
            break;
    }
}

void Voice::processFX()
{
    // Arpeggio
    if (arpNotes_[1] != 0 || arpNotes_[2] != 0)
    {
        arpTicks_++;
        if (arpTicks_ >= 6)  // Every 6 samples
        {
            arpTicks_ = 0;
            arpIndex_ = (arpIndex_ + 1) % 3;
        }
        // Adjust pitch based on arp
        float arpOffset = arpNotes_[arpIndex_];
        // Apply to pitch calculation
    }

    // Portamento
    if (portamentoSpeed_ > 0.0f)
    {
        float diff = portamentoTarget_ - currentPitch_;
        if (std::abs(diff) > 0.01f)
        {
            currentPitch_ += diff * portamentoSpeed_;
        }
    }

    // Vibrato
    if (vibratoDepth_ > 0.0f)
    {
        vibratoPhase_ += vibratoSpeed_ / sampleRate_;
        if (vibratoPhase_ >= 1.0f) vibratoPhase_ -= 1.0f;

        float vibrato = std::sin(vibratoPhase_ * 2.0f * 3.14159f) * vibratoDepth_;
        // Apply to pitch
    }
}
```

**Step 4: Process FX in AudioEngine when triggering notes**

```cpp
void AudioEngine::triggerNote(int track, int note, int instrumentIndex, float velocity,
                               const model::Step& step)
{
    // ... existing code ...

    if (voice)
    {
        // Apply FX commands
        if (!step.fx1.isEmpty()) voice->setFX(step.fx1.command, step.fx1.value);
        if (!step.fx2.isEmpty()) voice->setFX(step.fx2.command, step.fx2.value);
        if (!step.fx3.isEmpty()) voice->setFX(step.fx3.command, step.fx3.value);
    }
}
```

**Step 5: Build and verify**

```bash
cd build && make -j8
```

**Step 6: Commit**

```bash
git add -A && git commit -m "feat: implement FX commands (ARP, POR, VIB, VOL, PAN, DLY)"
```

---

## Phase 5: Undo/Redo

### Task 7: Implement Undo/Redo System

**Files:**
- Create: `src/model/UndoManager.h`
- Create: `src/model/UndoManager.cpp`
- Modify: `src/App.cpp`

**Step 1: Create UndoManager.h**

```cpp
#pragma once

#include <functional>
#include <vector>
#include <memory>
#include <string>

namespace model {

class Action
{
public:
    virtual ~Action() = default;
    virtual void undo() = 0;
    virtual void redo() = 0;
    virtual std::string getDescription() const = 0;
};

template<typename T>
class ValueAction : public Action
{
public:
    ValueAction(T* target, T oldValue, T newValue, const std::string& desc)
        : target_(target), oldValue_(oldValue), newValue_(newValue), description_(desc) {}

    void undo() override { *target_ = oldValue_; }
    void redo() override { *target_ = newValue_; }
    std::string getDescription() const override { return description_; }

private:
    T* target_;
    T oldValue_;
    T newValue_;
    std::string description_;
};

class UndoManager
{
public:
    static UndoManager& instance() {
        static UndoManager inst;
        return inst;
    }

    void push(std::unique_ptr<Action> action) {
        // Clear redo stack
        while (currentIndex_ < static_cast<int>(actions_.size()) - 1)
            actions_.pop_back();

        actions_.push_back(std::move(action));
        currentIndex_ = static_cast<int>(actions_.size()) - 1;

        // Limit history
        while (actions_.size() > MAX_HISTORY)
        {
            actions_.erase(actions_.begin());
            currentIndex_--;
        }
    }

    bool canUndo() const { return currentIndex_ >= 0; }
    bool canRedo() const { return currentIndex_ < static_cast<int>(actions_.size()) - 1; }

    void undo() {
        if (canUndo())
        {
            actions_[currentIndex_]->undo();
            currentIndex_--;
            if (onStateChanged) onStateChanged();
        }
    }

    void redo() {
        if (canRedo())
        {
            currentIndex_++;
            actions_[currentIndex_]->redo();
            if (onStateChanged) onStateChanged();
        }
    }

    void clear() {
        actions_.clear();
        currentIndex_ = -1;
    }

    std::function<void()> onStateChanged;

private:
    UndoManager() = default;

    std::vector<std::unique_ptr<Action>> actions_;
    int currentIndex_ = -1;
    static constexpr size_t MAX_HISTORY = 100;
};

// Helper to create value actions
template<typename T>
void recordChange(T* target, T newValue, const std::string& desc) {
    T oldValue = *target;
    *target = newValue;
    UndoManager::instance().push(
        std::make_unique<ValueAction<T>>(target, oldValue, newValue, desc)
    );
}

} // namespace model
```

**Step 2: Wire up undo/redo in App.cpp**

```cpp
keyHandler_->onUndo = [this]() {
    model::UndoManager::instance().undo();
    repaint();
};

keyHandler_->onRedo = [this]() {
    model::UndoManager::instance().redo();
    repaint();
};
```

**Step 3: Use recordChange when modifying data**

Example in PatternScreen when entering notes:
```cpp
// Instead of direct assignment:
// pattern->setStep(track, row, step);

// Use:
auto oldStep = pattern->getStep(track, row);
pattern->setStep(track, row, step);
// Record compound action for undo
```

**Step 4: Build and verify**

```bash
cd build && make -j8
```

**Step 5: Commit**

```bash
git add -A && git commit -m "feat: add undo/redo system with action history"
```

---

## Phase 6: Groove Templates

### Task 8: Implement Groove System

**Files:**
- Create: `src/model/Groove.h`
- Modify: `src/audio/AudioEngine.cpp`

**Step 1: Create Groove.h**

```cpp
#pragma once

#include <array>
#include <string>
#include <vector>

namespace model {

struct GrooveTemplate
{
    std::string name;
    std::array<float, 16> timings;  // Timing offset for each of 16 steps (-0.5 to 0.5)

    static GrooveTemplate straight() {
        GrooveTemplate g;
        g.name = "Straight";
        g.timings.fill(0.0f);
        return g;
    }

    static GrooveTemplate swing50() {
        GrooveTemplate g;
        g.name = "Swing 50%";
        for (int i = 0; i < 16; ++i)
            g.timings[i] = (i % 2 == 1) ? 0.167f : 0.0f;  // Delay odd steps
        return g;
    }

    static GrooveTemplate swing66() {
        GrooveTemplate g;
        g.name = "Swing 66%";
        for (int i = 0; i < 16; ++i)
            g.timings[i] = (i % 2 == 1) ? 0.33f : 0.0f;
        return g;
    }

    static GrooveTemplate mpc60() {
        GrooveTemplate g;
        g.name = "MPC 60%";
        for (int i = 0; i < 16; ++i)
            g.timings[i] = (i % 2 == 1) ? 0.2f : 0.0f;
        return g;
    }

    static GrooveTemplate humanize() {
        GrooveTemplate g;
        g.name = "Humanize";
        // Random-ish but consistent pattern
        float offsets[] = {0, 0.02f, -0.01f, 0.03f, 0, -0.02f, 0.01f, 0.02f,
                          -0.01f, 0.02f, 0, 0.01f, -0.02f, 0.03f, 0.01f, -0.01f};
        for (int i = 0; i < 16; ++i)
            g.timings[i] = offsets[i];
        return g;
    }
};

class GrooveManager
{
public:
    GrooveManager() {
        templates_.push_back(GrooveTemplate::straight());
        templates_.push_back(GrooveTemplate::swing50());
        templates_.push_back(GrooveTemplate::swing66());
        templates_.push_back(GrooveTemplate::mpc60());
        templates_.push_back(GrooveTemplate::humanize());
    }

    const std::vector<GrooveTemplate>& getTemplates() const { return templates_; }

    const GrooveTemplate& getTemplate(int index) const {
        if (index >= 0 && index < static_cast<int>(templates_.size()))
            return templates_[index];
        return templates_[0];
    }

    int getTemplateCount() const { return static_cast<int>(templates_.size()); }

private:
    std::vector<GrooveTemplate> templates_;
};

} // namespace model
```

**Step 2: Add groove to Project**

In Project.h:
```cpp
std::array<int, 16> trackGrooves_;  // Groove template index per track

int getTrackGroove(int track) const { return trackGrooves_[track]; }
void setTrackGroove(int track, int grooveIndex) { trackGrooves_[track] = grooveIndex; }
```

**Step 3: Apply groove in AudioEngine playback**

When calculating next row timing:
```cpp
void AudioEngine::advancePlayhead()
{
    // Get groove offset for current row
    float grooveOffset = 0.0f;
    if (project_)
    {
        int grooveIdx = project_->getTrackGroove(0);  // Use track 0 groove for now
        const auto& groove = grooveManager_.getTemplate(grooveIdx);
        grooveOffset = groove.timings[currentRow_ % 16];
    }

    // Apply groove offset to timing
    double bpm = project_ ? project_->getTempo() : 120.0;
    double samplesPerBeat = sampleRate_ * 60.0 / bpm;
    double samplesPerRow = samplesPerBeat / 4.0;

    samplesUntilNextRow_ = samplesPerRow * (1.0 + grooveOffset);

    // ... rest of playhead advancement
}
```

**Step 4: Build and verify**

```bash
cd build && make -j8
```

**Step 5: Commit**

```bash
git add -A && git commit -m "feat: add groove templates with per-track selection"
```

---

## Phase 7: Autosave

### Task 9: Implement Autosave

**Files:**
- Modify: `src/App.h`
- Modify: `src/App.cpp`

**Step 1: Add autosave timer to App.h**

```cpp
void autosave();
juce::File getAutosavePath() const;
int autosaveCounter_ = 0;
```

**Step 2: Implement autosave in App.cpp**

```cpp
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

// In timerCallback:
void App::timerCallback()
{
    if (audioEngine_.isPlaying())
    {
        repaint();
    }

    // Autosave every 60 seconds (1800 frames at 30fps)
    autosaveCounter_++;
    if (autosaveCounter_ >= 1800)
    {
        autosaveCounter_ = 0;
        autosave();
    }
}
```

**Step 3: Build and verify**

```bash
cd build && make -j8
```

**Step 4: Commit**

```bash
git add -A && git commit -m "feat: add autosave every 60 seconds to ~/.vitracker/autosave/"
```

---

## Phase 8: Additional Commands

### Task 10: Implement Remaining Commands

**Files:**
- Modify: `src/input/KeyHandler.cpp`
- Modify: `src/App.cpp`

**Step 1: Add command handlers to KeyHandler.cpp**

```cpp
void KeyHandler::executeCommand(const std::string& command)
{
    DBG("Executing command: " << command);

    if (command == "w")
    {
        if (onSave) onSave("");
    }
    else if (command.length() > 2 && command.substr(0, 2) == "w ")
    {
        if (onSave) onSave(command.substr(2));
    }
    else if (command == "e")
    {
        if (onLoad) onLoad("");
    }
    else if (command.length() > 2 && command.substr(0, 2) == "e ")
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
    else if (command.substr(0, 10) == "transpose ")
    {
        try {
            int semitones = std::stoi(command.substr(10));
            if (onTranspose) onTranspose(semitones);
        } catch (...) {}
    }
    else if (command == "interpolate")
    {
        if (onInterpolate) onInterpolate();
    }
    else if (command.substr(0, 10) == "randomize ")
    {
        try {
            int percent = std::stoi(command.substr(10));
            if (onRandomize) onRandomize(percent);
        } catch (...) {}
    }
    else if (command == "double")
    {
        if (onDouble) onDouble();
    }
    else if (command == "halve")
    {
        if (onHalve) onHalve();
    }

    if (onCommand) onCommand(command);
}
```

**Step 2: Add callbacks to KeyHandler.h**

```cpp
std::function<void(int)> onTranspose;
std::function<void()> onInterpolate;
std::function<void(int)> onRandomize;
std::function<void()> onDouble;
std::function<void()> onHalve;
```

**Step 3: Implement commands in PatternScreen**

```cpp
void PatternScreen::interpolate()
{
    if (!hasSelection_) return;

    auto* pattern = project_.getPattern(currentPattern_);
    if (!pattern) return;

    // For each track in selection
    for (int t = selection_.minTrack(); t <= selection_.maxTrack(); ++t)
    {
        int minR = selection_.minRow();
        int maxR = selection_.maxRow();

        auto first = pattern->getStep(t, minR);
        auto last = pattern->getStep(t, maxR);

        // Interpolate volume if both have values
        if (first.volume >= 0 && last.volume >= 0)
        {
            int range = maxR - minR;
            for (int r = minR + 1; r < maxR; ++r)
            {
                auto step = pattern->getStep(t, r);
                float t_factor = static_cast<float>(r - minR) / range;
                step.volume = static_cast<int>(first.volume + (last.volume - first.volume) * t_factor);
                pattern->setStep(t, r, step);
            }
        }
    }

    repaint();
}

void PatternScreen::randomize(int percent)
{
    auto* pattern = project_.getPattern(currentPattern_);
    if (!pattern) return;

    int minT = hasSelection_ ? selection_.minTrack() : cursorTrack_;
    int maxT = hasSelection_ ? selection_.maxTrack() : cursorTrack_;
    int minR = hasSelection_ ? selection_.minRow() : cursorRow_;
    int maxR = hasSelection_ ? selection_.maxRow() : cursorRow_;

    for (int t = minT; t <= maxT; ++t)
    {
        for (int r = minR; r <= maxR; ++r)
        {
            auto step = pattern->getStep(t, r);
            if (step.note > 0 && step.note < 128)
            {
                int variance = (std::rand() % (percent * 2 + 1)) - percent;
                int semitoneVariance = variance * 12 / 100;
                step.note = std::clamp(step.note + semitoneVariance, 1, 127);
                pattern->setStep(t, r, step);
            }
        }
    }

    repaint();
}

void PatternScreen::doublePattern()
{
    auto* pattern = project_.getPattern(currentPattern_);
    if (!pattern) return;

    int oldLength = pattern->getLength();
    int newLength = std::min(oldLength * 2, 128);

    pattern->setLength(newLength);

    // Duplicate steps
    for (int t = 0; t < 16; ++t)
    {
        for (int r = 0; r < oldLength; ++r)
        {
            pattern->setStep(t, r + oldLength, pattern->getStep(t, r));
        }
    }

    repaint();
}

void PatternScreen::halvePattern()
{
    auto* pattern = project_.getPattern(currentPattern_);
    if (!pattern) return;

    int newLength = std::max(pattern->getLength() / 2, 1);
    pattern->setLength(newLength);
    repaint();
}
```

**Step 4: Wire up in App.cpp**

```cpp
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
```

**Step 5: Build and verify**

```bash
cd build && make -j8
```

**Step 6: Commit**

```bash
git add -A && git commit -m "feat: add :transpose, :interpolate, :randomize, :double, :halve commands"
```

---

## Phase 9: Song Playback

### Task 11: Implement Full Song Playback

**Files:**
- Modify: `src/audio/AudioEngine.h`
- Modify: `src/audio/AudioEngine.cpp`

**Step 1: Add song playback state to AudioEngine.h**

```cpp
enum class PlayMode { Pattern, Song };

PlayMode playMode_ = PlayMode::Pattern;
int currentSongRow_ = 0;
int currentChainIndex_ = 0;  // Position within current chain
std::string currentChainName_;

void setPlayMode(PlayMode mode) { playMode_ = mode; }
PlayMode getPlayMode() const { return playMode_; }
int getSongRow() const { return currentSongRow_; }
```

**Step 2: Update advancePlayhead for song mode**

```cpp
void AudioEngine::advancePlayhead()
{
    if (playMode_ == PlayMode::Pattern)
    {
        // Current pattern-only playback
        currentRow_++;
        auto* pattern = project_->getPattern(currentPattern_);
        if (pattern && currentRow_ >= pattern->getLength())
        {
            currentRow_ = 0;  // Loop pattern
        }
    }
    else // PlayMode::Song
    {
        currentRow_++;

        auto* pattern = project_->getPatternByName(getCurrentPatternName());
        if (pattern && currentRow_ >= pattern->getLength())
        {
            currentRow_ = 0;
            advanceChain();
        }
    }

    // Trigger notes for current row
    triggerRowNotes();
}

void AudioEngine::advanceChain()
{
    auto& song = project_->getSong();

    // Find current chain
    const auto& chainRef = song.getChainRef(0, currentSongRow_);  // Track 0 drives timing
    auto* chain = project_->getChainByName(chainRef);

    if (chain)
    {
        currentChainIndex_++;
        if (currentChainIndex_ >= static_cast<int>(chain->getPatternNames().size()))
        {
            // Move to next song row
            currentChainIndex_ = 0;
            currentSongRow_++;

            // Check if we've reached the end
            if (currentSongRow_ >= song.getLength())
            {
                currentSongRow_ = 0;  // Loop song
            }
        }
    }
    else
    {
        // No chain, move to next song row
        currentSongRow_++;
        currentChainIndex_ = 0;

        if (currentSongRow_ >= song.getLength())
        {
            currentSongRow_ = 0;
        }
    }

    currentChainName_ = song.getChainRef(0, currentSongRow_);
}

std::string AudioEngine::getCurrentPatternName() const
{
    if (playMode_ == PlayMode::Pattern)
    {
        auto* pattern = project_->getPattern(currentPattern_);
        return pattern ? pattern->getName() : "";
    }

    auto* chain = project_->getChainByName(currentChainName_);
    if (chain && currentChainIndex_ < static_cast<int>(chain->getPatternNames().size()))
    {
        return chain->getPatternNames()[currentChainIndex_];
    }

    return "";
}

void AudioEngine::triggerRowNotes()
{
    auto* pattern = project_->getPatternByName(getCurrentPatternName());
    if (!pattern) return;

    for (int track = 0; track < 16; ++track)
    {
        auto step = pattern->getStep(track, currentRow_);

        if (step.note == 0)  // Note OFF
        {
            releaseNote(track);
        }
        else if (step.note > 0)
        {
            int inst = step.instrument >= 0 ? step.instrument : 0;
            float vel = step.volume >= 0 ? step.volume / 255.0f : 1.0f;
            triggerNote(track, step.note, inst, vel);
        }
    }
}
```

**Step 3: Add play mode toggle**

In App.cpp, modify play/stop:
```cpp
keyHandler_->onPlayStop = [this]() {
    if (audioEngine_.isPlaying())
    {
        audioEngine_.stop();
    }
    else
    {
        // Play from current screen context
        if (currentScreen_ == 1)  // Song screen
            audioEngine_.setPlayMode(audio::AudioEngine::PlayMode::Song);
        else
            audioEngine_.setPlayMode(audio::AudioEngine::PlayMode::Pattern);

        audioEngine_.play();
    }
    repaint();
};
```

**Step 4: Build and verify**

```bash
cd build && make -j8
```

**Step 5: Commit**

```bash
git add -A && git commit -m "feat: implement full song playback traversing chains and patterns"
```

---

## Phase 10: Final Polish

### Task 12: Fix Parameter Editing Bugs

**Files:**
- Modify: `src/ui/InstrumentScreen.cpp`
- Modify: `src/input/KeyHandler.cpp`

**Step 1: Verify Edit mode is entered on InstrumentScreen**

Check that pressing 'i' enters Edit mode and the InstrumentScreen receives key events:

In App.cpp, verify the edit key forwarding is correct:
```cpp
keyHandler_->onEditKey = [this](const juce::KeyPress& key) {
    if (screens_[currentScreen_])
    {
        if (auto* patternScreen = dynamic_cast<ui::PatternScreen*>(screens_[currentScreen_].get()))
            patternScreen->handleEditKey(key);
        else if (auto* instrumentScreen = dynamic_cast<ui::InstrumentScreen*>(screens_[currentScreen_].get()))
            instrumentScreen->handleEditKey(key);
        else if (auto* mixerScreen = dynamic_cast<ui::MixerScreen*>(screens_[currentScreen_].get()))
            mixerScreen->handleEditKey(key);
        else if (auto* chainScreen = dynamic_cast<ui::ChainScreen*>(screens_[currentScreen_].get()))
            chainScreen->handleEditKey(key);
        else if (auto* projectScreen = dynamic_cast<ui::ProjectScreen*>(screens_[currentScreen_].get()))
            projectScreen->handleEditKey(key);
    }
};
```

**Step 2: Verify InstrumentScreen parameter modification**

The `handleEditKey` in InstrumentScreen.cpp should be modifying params correctly. Make sure `instrument->getParams()` returns a mutable reference:

In Instrument.h:
```cpp
PlaitsParams& getParams() { return params_; }
const PlaitsParams& getParams() const { return params_; }

Sends& getSends() { return sends_; }
const Sends& getSends() const { return sends_; }
```

**Step 3: Build and test**

```bash
cd build && make -j8
./Vitracker
```

Test:
1. Press 5 to go to Instrument screen
2. Press i to enter Edit mode
3. Use arrow keys to navigate to a parameter
4. Press +/- to adjust value
5. Value should change and sound should preview

**Step 4: Commit**

```bash
git add -A && git commit -m "fix: ensure InstrumentScreen parameter editing works correctly"
```

---

### Task 13: Run Full Integration Test

**Step 1: Build release version**

```bash
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j8
```

**Step 2: Test all screens**

1. **Project (1)**: Change tempo, verify BPM updates
2. **Song (2)**: Add chain references with Tab, navigate grid
3. **Chain (3)**: Add patterns to chain, change scale lock
4. **Pattern (4)**: Enter notes, change instruments, add FX
5. **Instrument (5)**: Change engine, adjust params, set sends
6. **Mixer (6)**: Adjust volumes, pan, mute/solo

**Step 3: Test playback**

1. Enter some notes in pattern
2. Press Space to play
3. Go to Song screen, add chain
4. Press Space to play song mode

**Step 4: Test save/load**

1. Press : then type `w test` and Enter
2. Press : then type `new` and Enter
3. Press : then type `e test` and Enter
4. Verify project restored

**Step 5: Final commit**

```bash
git add -A && git commit -m "chore: complete v1 integration testing"
```

---

## Summary

This plan covers:

1. **Missing Screens**: SongScreen, ChainScreen
2. **Effects**: Reverb, Delay, Chorus, Drive, Sidechain with per-instrument sends
3. **Visual Mode**: Selection, yank, paste, transpose
4. **FX Commands**: ARP, POR, VIB, VOL, PAN, DLY
5. **Undo/Redo**: Action-based history
6. **Groove Templates**: Straight, Swing 50%, Swing 66%, MPC 60%, Humanize
7. **Autosave**: Every 60 seconds to ~/.vitracker/autosave/
8. **Commands**: :transpose, :interpolate, :randomize, :double, :halve
9. **Song Playback**: Chain/pattern traversal
10. **Bug Fixes**: Parameter editing

Total: 13 tasks to complete Vitracker v1.
