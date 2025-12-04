# Slicer Phase 2 Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a Slicer instrument type that chops samples into slices triggered by MIDI notes (C1 = slice 0, C#1 = slice 1, etc.) with no pitch shifting.

**Architecture:** Slicer follows the same pattern as Sampler (SlicerVoice + SlicerInstrument). Slice points stored as sample positions. WaveformDisplay extended with SliceWaveformDisplay subclass for colored regions. SlicerInstrument maps MIDI notes to slices starting from C1 (note 36).

**Tech Stack:** JUCE (AudioBuffer, Component), existing MoogFilter, existing ADSR envelope. Builds on Phase 1 sampler infrastructure.

---

## Task 1: Create SlicerParams Structure

**Files:**
- Create: `src/model/SlicerParams.h`
- Modify: `src/model/Instrument.h`

**Step 1: Create SlicerParams header**

Create `src/model/SlicerParams.h`:

```cpp
#pragma once

#include <vector>
#include <cstddef>
#include "SamplerParams.h"  // Reuse SampleRef, AdsrParams, FilterParams

namespace model {

enum class ChopMode {
    Divisions,      // Equal divisions
    Transients,     // Auto-detect transients (Phase 3)
    Manual          // User-placed slices
};

struct SlicerParams {
    SampleRef sample;

    // Slice points (sample positions)
    std::vector<size_t> slicePoints;  // Start position of each slice
    int currentSlice = 0;             // Currently selected slice for UI

    // Chop settings
    ChopMode chopMode = ChopMode::Divisions;
    int numDivisions = 8;             // For Divisions mode

    // Audio processing (same as Sampler)
    FilterParams filter;
    AdsrParams ampEnvelope;
    float filterEnvAmount = 0.0f;
    AdsrParams filterEnvelope;
};

} // namespace model
```

**Step 2: Add SlicerParams to Instrument class**

In `src/model/Instrument.h`, add include after SamplerParams:

```cpp
#include "SlicerParams.h"
```

Add to private section of Instrument class:

```cpp
    SlicerParams slicerParams_;
```

Add to public section:

```cpp
    SlicerParams& getSlicerParams() { return slicerParams_; }
    const SlicerParams& getSlicerParams() const { return slicerParams_; }
```

**Step 3: Build to verify compilation**

Run: `cd /Users/jamesmassey/ai-dev/vitracker/.worktrees/sampler/build && cmake --build . --parallel 2>&1 | tail -10`
Expected: `[100%] Built target Vitracker`

**Step 4: Commit**

```bash
git add src/model/SlicerParams.h src/model/Instrument.h
git commit -m "feat: add SlicerParams structure for slice-based playback"
```

---

## Task 2: Create SlicerVoice Class

**Files:**
- Create: `src/audio/SlicerVoice.h`
- Create: `src/audio/SlicerVoice.cpp`

**Step 1: Create SlicerVoice header**

Create `src/audio/SlicerVoice.h`:

```cpp
#pragma once

#include "../model/SlicerParams.h"
#include "../dsp/adsr_envelope.h"
#include "../dsp/moog_filter.h"

namespace audio {

class SlicerVoice {
public:
    SlicerVoice();

    void setSampleRate(double sampleRate);
    void setSampleData(const float* data, int numChannels, size_t numSamples, int originalSampleRate);

    // Trigger a specific slice (sliceIndex maps from MIDI note)
    void trigger(int sliceIndex, float velocity, const model::SlicerParams& params);
    void release();

    void render(float* leftOut, float* rightOut, int numSamples);

    bool isActive() const { return active_; }
    int getSliceIndex() const { return currentSlice_; }

private:
    double sampleRate_ = 48000.0;
    const float* sampleData_ = nullptr;
    int sampleChannels_ = 0;
    size_t sampleLength_ = 0;
    int originalSampleRate_ = 44100;

    bool active_ = false;
    int currentSlice_ = -1;
    float velocity_ = 1.0f;

    // Slice boundaries
    size_t sliceStart_ = 0;
    size_t sliceEnd_ = 0;
    size_t playPosition_ = 0;

    // Playback rate for sample rate conversion only (no pitch shift)
    double playbackRate_ = 1.0;

    dsp::AdsrEnvelope ampEnvelope_;
    dsp::AdsrEnvelope filterEnvelope_;
    plaits::MoogFilter filterL_;
    plaits::MoogFilter filterR_;

    float baseCutoff_ = 1.0f;
    float filterEnvAmount_ = 0.0f;
};

} // namespace audio
```

**Step 2: Create SlicerVoice implementation**

Create `src/audio/SlicerVoice.cpp`:

```cpp
#include "SlicerVoice.h"
#include <cmath>
#include <algorithm>

namespace audio {

SlicerVoice::SlicerVoice() {
    ampEnvelope_.setSampleRate(48000.0f);
    filterEnvelope_.setSampleRate(48000.0f);
}

void SlicerVoice::setSampleRate(double sampleRate) {
    sampleRate_ = sampleRate;
    ampEnvelope_.setSampleRate(static_cast<float>(sampleRate));
    filterEnvelope_.setSampleRate(static_cast<float>(sampleRate));
    filterL_.Init();
    filterR_.Init();
}

void SlicerVoice::setSampleData(const float* data, int numChannels, size_t numSamples, int originalSampleRate) {
    sampleData_ = data;
    sampleChannels_ = numChannels;
    sampleLength_ = numSamples;
    originalSampleRate_ = originalSampleRate;
    // Calculate playback rate for sample rate conversion only
    playbackRate_ = static_cast<double>(originalSampleRate) / sampleRate_;
}

void SlicerVoice::trigger(int sliceIndex, float velocity, const model::SlicerParams& params) {
    if (!sampleData_ || sampleLength_ == 0) return;

    const auto& slices = params.slicePoints;
    if (slices.empty()) {
        // No slices defined - play whole sample
        sliceStart_ = 0;
        sliceEnd_ = sampleLength_;
    } else if (sliceIndex >= 0 && sliceIndex < static_cast<int>(slices.size())) {
        sliceStart_ = slices[sliceIndex];
        // End is either next slice or end of sample
        if (sliceIndex + 1 < static_cast<int>(slices.size())) {
            sliceEnd_ = slices[sliceIndex + 1];
        } else {
            sliceEnd_ = sampleLength_;
        }
    } else {
        // Invalid slice index
        return;
    }

    currentSlice_ = sliceIndex;
    velocity_ = velocity;
    active_ = true;
    playPosition_ = sliceStart_;

    // Set up envelopes
    ampEnvelope_.setAttack(params.ampEnvelope.attack);
    ampEnvelope_.setDecay(params.ampEnvelope.decay);
    ampEnvelope_.setSustain(params.ampEnvelope.sustain);
    ampEnvelope_.setRelease(params.ampEnvelope.release);
    ampEnvelope_.trigger();

    filterEnvelope_.setAttack(params.filterEnvelope.attack);
    filterEnvelope_.setDecay(params.filterEnvelope.decay);
    filterEnvelope_.setSustain(params.filterEnvelope.sustain);
    filterEnvelope_.setRelease(params.filterEnvelope.release);
    filterEnvelope_.trigger();

    // Store filter params
    baseCutoff_ = params.filter.cutoff;
    filterEnvAmount_ = params.filterEnvAmount;

    filterL_.set_resonance(params.filter.resonance);
    filterR_.set_resonance(params.filter.resonance);
}

void SlicerVoice::release() {
    ampEnvelope_.release();
    filterEnvelope_.release();
}

void SlicerVoice::render(float* leftOut, float* rightOut, int numSamples) {
    if (!active_ || !sampleData_) {
        for (int i = 0; i < numSamples; ++i) {
            leftOut[i] = 0.0f;
            rightOut[i] = 0.0f;
        }
        return;
    }

    for (int i = 0; i < numSamples; ++i) {
        // Check if we've reached end of slice
        if (playPosition_ >= sliceEnd_) {
            active_ = false;
            leftOut[i] = 0.0f;
            rightOut[i] = 0.0f;
            continue;
        }

        // Linear interpolation for sample playback
        size_t pos0 = playPosition_;
        size_t pos1 = std::min(pos0 + 1, sampleLength_ - 1);
        double frac = playPosition_ - static_cast<double>(pos0);

        float sampleL, sampleR;
        if (sampleChannels_ == 1) {
            sampleL = static_cast<float>(sampleData_[pos0] * (1.0 - frac) + sampleData_[pos1] * frac);
            sampleR = sampleL;
        } else {
            sampleL = static_cast<float>(sampleData_[pos0 * 2] * (1.0 - frac) + sampleData_[pos1 * 2] * frac);
            sampleR = static_cast<float>(sampleData_[pos0 * 2 + 1] * (1.0 - frac) + sampleData_[pos1 * 2 + 1] * frac);
        }

        // Process envelopes
        float ampEnv = ampEnvelope_.process();
        float filterEnv = filterEnvelope_.process();

        // Apply filter with envelope modulation
        float cutoff = baseCutoff_ + filterEnvAmount_ * filterEnv;
        cutoff = std::clamp(cutoff, 0.0f, 1.0f);
        filterL_.set_frequency(cutoff);
        filterR_.set_frequency(cutoff);

        sampleL = filterL_.Process(sampleL);
        sampleR = filterR_.Process(sampleR);

        // Apply amplitude envelope and velocity
        leftOut[i] = sampleL * ampEnv * velocity_;
        rightOut[i] = sampleR * ampEnv * velocity_;

        // Advance position (sample rate conversion only, no pitch shift)
        playPosition_ += static_cast<size_t>(playbackRate_);

        // Check if envelope finished
        if (!ampEnvelope_.isActive()) {
            active_ = false;
        }
    }
}

} // namespace audio
```

**Step 3: Add to CMakeLists.txt**

In `CMakeLists.txt`, add after `src/audio/SamplerInstrument.cpp`:

```cmake
    src/audio/SlicerVoice.cpp
```

**Step 4: Build to verify compilation**

Run: `cd /Users/jamesmassey/ai-dev/vitracker/.worktrees/sampler/build && cmake --build . --parallel 2>&1 | tail -10`
Expected: `[100%] Built target Vitracker`

**Step 5: Commit**

```bash
git add src/audio/SlicerVoice.h src/audio/SlicerVoice.cpp CMakeLists.txt
git commit -m "feat: add SlicerVoice for slice-based sample playback"
```

---

## Task 3: Create SlicerInstrument Class

**Files:**
- Create: `src/audio/SlicerInstrument.h`
- Create: `src/audio/SlicerInstrument.cpp`

**Step 1: Create SlicerInstrument header**

Create `src/audio/SlicerInstrument.h`:

```cpp
#pragma once

#include "InstrumentProcessor.h"
#include "SlicerVoice.h"
#include "../model/Instrument.h"
#include <JuceHeader.h>
#include <array>

namespace audio {

class SlicerInstrument : public InstrumentProcessor {
public:
    static constexpr int NUM_VOICES = 8;
    static constexpr int BASE_NOTE = 36;  // C1 = slice 0

    SlicerInstrument();
    ~SlicerInstrument() override = default;

    // InstrumentProcessor interface
    void init(double sampleRate) override;
    void setSampleRate(double sampleRate) override;
    void noteOn(int note, float velocity) override;
    void noteOff(int note) override;
    void allNotesOff() override;
    void process(float* outL, float* outR, int numSamples) override;
    const char* getTypeName() const override { return "Slicer"; }

    int getNumParameters() const override { return 0; }
    const char* getParameterName(int index) const override { return ""; }
    float getParameter(int index) const override { return 0.0f; }
    void setParameter(int index, float value) override {}

    void getState(void* data, int maxSize) const override {}
    void setState(const void* data, int size) override {}

    // Slicer-specific interface
    void setInstrument(model::Instrument* instrument) { instrument_ = instrument; }
    model::Instrument* getInstrument() const { return instrument_; }

    bool loadSample(const juce::File& file);
    bool hasSample() const { return sampleBuffer_.getNumSamples() > 0; }

    // Slice management
    void chopIntoDivisions(int numDivisions);
    void addSliceAtPosition(size_t samplePosition);
    void removeSlice(int sliceIndex);
    void clearSlices();
    int getNumSlices() const;

    // Zero-crossing snap helper
    size_t findNearestZeroCrossing(size_t position) const;

    // For waveform display
    const juce::AudioBuffer<float>& getSampleBuffer() const { return sampleBuffer_; }
    int getSampleRate() const { return loadedSampleRate_; }

private:
    SlicerVoice* findFreeVoice();
    SlicerVoice* findVoiceToSteal();
    int midiNoteToSliceIndex(int midiNote) const;

    model::Instrument* instrument_ = nullptr;
    double sampleRate_ = 48000.0;

    std::array<SlicerVoice, NUM_VOICES> voices_;
    juce::AudioBuffer<float> sampleBuffer_;
    int loadedSampleRate_ = 44100;

    juce::AudioFormatManager formatManager_;
};

} // namespace audio
```

**Step 2: Create SlicerInstrument implementation**

Create `src/audio/SlicerInstrument.cpp`:

```cpp
#include "SlicerInstrument.h"
#include <algorithm>
#include <cmath>

namespace audio {

SlicerInstrument::SlicerInstrument() {
    formatManager_.registerBasicFormats();
}

void SlicerInstrument::init(double sampleRate) {
    setSampleRate(sampleRate);
}

void SlicerInstrument::setSampleRate(double sampleRate) {
    sampleRate_ = sampleRate;
    for (auto& voice : voices_) {
        voice.setSampleRate(sampleRate);
    }
}

void SlicerInstrument::process(float* outL, float* outR, int numSamples) {
    // Clear output
    std::fill(outL, outL + numSamples, 0.0f);
    std::fill(outR, outR + numSamples, 0.0f);

    if (!hasSample() || !instrument_) return;

    // Temp buffers for voice mixing
    std::vector<float> tempL(numSamples, 0.0f);
    std::vector<float> tempR(numSamples, 0.0f);

    for (auto& voice : voices_) {
        if (voice.isActive()) {
            voice.render(tempL.data(), tempR.data(), numSamples);

            for (int i = 0; i < numSamples; ++i) {
                outL[i] += tempL[i];
                outR[i] += tempR[i];
            }

            // Clear temp for next voice
            std::fill(tempL.begin(), tempL.end(), 0.0f);
            std::fill(tempR.begin(), tempR.end(), 0.0f);
        }
    }
}

int SlicerInstrument::midiNoteToSliceIndex(int midiNote) const {
    // C1 (36) = slice 0, C#1 (37) = slice 1, etc.
    return midiNote - BASE_NOTE;
}

void SlicerInstrument::noteOn(int midiNote, float velocity) {
    if (!hasSample() || !instrument_) return;

    int sliceIndex = midiNoteToSliceIndex(midiNote);
    if (sliceIndex < 0) return;  // Note below C1

    const auto& params = instrument_->getSlicerParams();

    // Check if slice exists
    if (sliceIndex >= static_cast<int>(params.slicePoints.size()) && !params.slicePoints.empty()) {
        return;  // Slice doesn't exist
    }

    auto* voice = findFreeVoice();
    if (!voice) {
        voice = findVoiceToSteal();
    }

    if (voice) {
        voice->setSampleData(
            sampleBuffer_.getReadPointer(0),
            sampleBuffer_.getNumChannels(),
            static_cast<size_t>(sampleBuffer_.getNumSamples()),
            loadedSampleRate_
        );
        voice->trigger(sliceIndex, velocity, params);
    }
}

void SlicerInstrument::noteOff(int midiNote) {
    int sliceIndex = midiNoteToSliceIndex(midiNote);
    for (auto& voice : voices_) {
        if (voice.isActive() && voice.getSliceIndex() == sliceIndex) {
            voice.release();
        }
    }
}

void SlicerInstrument::allNotesOff() {
    for (auto& voice : voices_) {
        voice.release();
    }
}

SlicerVoice* SlicerInstrument::findFreeVoice() {
    for (auto& voice : voices_) {
        if (!voice.isActive()) {
            return &voice;
        }
    }
    return nullptr;
}

SlicerVoice* SlicerInstrument::findVoiceToSteal() {
    // Simple voice stealing: return first voice
    return &voices_[0];
}

bool SlicerInstrument::loadSample(const juce::File& file) {
    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager_.createReaderFor(file)
    );

    if (!reader) {
        DBG("Failed to create reader for: " << file.getFullPathName());
        return false;
    }

    loadedSampleRate_ = static_cast<int>(reader->sampleRate);
    const int numChannels = static_cast<int>(reader->numChannels);
    const juce::int64 numSamples = reader->lengthInSamples;

    // Check file size (warn if > 50MB of audio data)
    const juce::int64 dataSize = numSamples * numChannels * sizeof(float);
    if (dataSize > 50 * 1024 * 1024) {
        DBG("Warning: Large sample file (" << (dataSize / (1024 * 1024)) << " MB)");
    }

    sampleBuffer_.setSize(numChannels, static_cast<int>(numSamples));
    reader->read(&sampleBuffer_, 0, static_cast<int>(numSamples), 0, true, true);

    // Update instrument's sample ref
    if (instrument_) {
        auto& sampleRef = instrument_->getSlicerParams().sample;
        sampleRef.path = file.getFullPathName().toStdString();
        sampleRef.numChannels = numChannels;
        sampleRef.sampleRate = loadedSampleRate_;
        sampleRef.numSamples = static_cast<size_t>(numSamples);

        // Default chop into 8 divisions
        chopIntoDivisions(8);
    }

    // Update all voices with new sample data
    for (auto& voice : voices_) {
        voice.setSampleRate(sampleRate_);
    }

    DBG("Loaded sample for slicer: " << file.getFileName()
        << " (" << numChannels << " ch, "
        << loadedSampleRate_ << " Hz, "
        << numSamples << " samples)");

    return true;
}

void SlicerInstrument::chopIntoDivisions(int numDivisions) {
    if (!instrument_ || sampleBuffer_.getNumSamples() == 0) return;

    auto& params = instrument_->getSlicerParams();
    params.slicePoints.clear();
    params.numDivisions = numDivisions;
    params.chopMode = model::ChopMode::Divisions;

    size_t samplesPerSlice = static_cast<size_t>(sampleBuffer_.getNumSamples()) / numDivisions;

    for (int i = 0; i < numDivisions; ++i) {
        size_t position = i * samplesPerSlice;
        // Snap to zero crossing
        position = findNearestZeroCrossing(position);
        params.slicePoints.push_back(position);
    }
}

void SlicerInstrument::addSliceAtPosition(size_t samplePosition) {
    if (!instrument_) return;

    auto& params = instrument_->getSlicerParams();

    // Snap to zero crossing
    size_t snappedPosition = findNearestZeroCrossing(samplePosition);

    // Insert in sorted order
    auto it = std::lower_bound(params.slicePoints.begin(), params.slicePoints.end(), snappedPosition);
    params.slicePoints.insert(it, snappedPosition);

    params.chopMode = model::ChopMode::Manual;
}

void SlicerInstrument::removeSlice(int sliceIndex) {
    if (!instrument_) return;

    auto& params = instrument_->getSlicerParams();
    if (sliceIndex >= 0 && sliceIndex < static_cast<int>(params.slicePoints.size())) {
        params.slicePoints.erase(params.slicePoints.begin() + sliceIndex);
        params.chopMode = model::ChopMode::Manual;
    }
}

void SlicerInstrument::clearSlices() {
    if (!instrument_) return;
    instrument_->getSlicerParams().slicePoints.clear();
}

int SlicerInstrument::getNumSlices() const {
    if (!instrument_) return 0;
    return static_cast<int>(instrument_->getSlicerParams().slicePoints.size());
}

size_t SlicerInstrument::findNearestZeroCrossing(size_t position) const {
    if (sampleBuffer_.getNumSamples() == 0) return position;

    const float* data = sampleBuffer_.getReadPointer(0);
    const size_t numSamples = static_cast<size_t>(sampleBuffer_.getNumSamples());

    // Search window: +/- 1024 samples
    const size_t searchRadius = 1024;
    size_t start = (position > searchRadius) ? position - searchRadius : 0;
    size_t end = std::min(position + searchRadius, numSamples - 1);

    size_t bestPosition = position;
    float bestDistance = std::abs(data[position]);

    for (size_t i = start; i < end; ++i) {
        // Check for sign change (zero crossing)
        if (i > 0 && data[i - 1] * data[i] <= 0.0f) {
            // Found a zero crossing
            size_t distance = (i > position) ? i - position : position - i;
            if (distance < (bestPosition > position ? bestPosition - position : position - bestPosition) ||
                std::abs(data[i]) < bestDistance) {
                bestPosition = i;
                bestDistance = std::abs(data[i]);
            }
        }
    }

    return bestPosition;
}

} // namespace audio
```

**Step 3: Add to CMakeLists.txt**

In `CMakeLists.txt`, add after `src/audio/SlicerVoice.cpp`:

```cmake
    src/audio/SlicerInstrument.cpp
```

**Step 4: Build to verify compilation**

Run: `cd /Users/jamesmassey/ai-dev/vitracker/.worktrees/sampler/build && cmake --build . --parallel 2>&1 | tail -10`
Expected: `[100%] Built target Vitracker`

**Step 5: Commit**

```bash
git add src/audio/SlicerInstrument.h src/audio/SlicerInstrument.cpp CMakeLists.txt
git commit -m "feat: add SlicerInstrument with slice-per-key triggering"
```

---

## Task 4: Create SliceWaveformDisplay Component

**Files:**
- Create: `src/ui/SliceWaveformDisplay.h`
- Create: `src/ui/SliceWaveformDisplay.cpp`

**Step 1: Create SliceWaveformDisplay header**

Create `src/ui/SliceWaveformDisplay.h`:

```cpp
#pragma once

#include "WaveformDisplay.h"
#include <vector>

namespace ui {

class SliceWaveformDisplay : public WaveformDisplay {
public:
    SliceWaveformDisplay();

    void paint(juce::Graphics& g) override;

    // Slice management
    void setSlicePoints(const std::vector<size_t>& slices);
    void setCurrentSlice(int index);
    int getCurrentSlice() const { return currentSlice_; }

    // Navigation
    void nextSlice();
    void previousSlice();

    // Editing
    void addSliceAtCursor();
    void deleteCurrentSlice();

    // Get slice position for adding
    size_t getCursorSamplePosition() const;

    std::function<void(size_t)> onAddSlice;
    std::function<void(int)> onDeleteSlice;

private:
    void paintSliceRegions(juce::Graphics& g, juce::Rectangle<int> bounds);
    void paintSliceMarkers(juce::Graphics& g, juce::Rectangle<int> bounds);

    std::vector<size_t> slicePoints_;
    int currentSlice_ = 0;

    // Alternating colors for slice regions
    juce::Colour sliceColourA_ = juce::Colour(0xFF2D5A27);  // Green tint
    juce::Colour sliceColourB_ = juce::Colour(0xFF27455A);  // Blue tint
    juce::Colour currentSliceColour_ = juce::Colour(0xFF5A4827);  // Highlighted
    juce::Colour markerColour_ = juce::Colour(0xFFFFFFFF);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SliceWaveformDisplay)
};

} // namespace ui
```

**Step 2: Create SliceWaveformDisplay implementation**

Create `src/ui/SliceWaveformDisplay.cpp`:

```cpp
#include "SliceWaveformDisplay.h"

namespace ui {

SliceWaveformDisplay::SliceWaveformDisplay() {
}

void SliceWaveformDisplay::setSlicePoints(const std::vector<size_t>& slices) {
    slicePoints_ = slices;
    if (currentSlice_ >= static_cast<int>(slicePoints_.size())) {
        currentSlice_ = slicePoints_.empty() ? 0 : static_cast<int>(slicePoints_.size()) - 1;
    }
    repaint();
}

void SliceWaveformDisplay::setCurrentSlice(int index) {
    if (slicePoints_.empty()) {
        currentSlice_ = 0;
    } else {
        currentSlice_ = juce::jlimit(0, static_cast<int>(slicePoints_.size()) - 1, index);
    }
    repaint();
}

void SliceWaveformDisplay::nextSlice() {
    if (!slicePoints_.empty()) {
        setCurrentSlice(currentSlice_ + 1);
    }
}

void SliceWaveformDisplay::previousSlice() {
    if (!slicePoints_.empty()) {
        setCurrentSlice(currentSlice_ - 1);
    }
}

void SliceWaveformDisplay::addSliceAtCursor() {
    if (onAddSlice) {
        onAddSlice(getCursorSamplePosition());
    }
}

void SliceWaveformDisplay::deleteCurrentSlice() {
    if (onDeleteSlice && !slicePoints_.empty()) {
        onDeleteSlice(currentSlice_);
    }
}

size_t SliceWaveformDisplay::getCursorSamplePosition() const {
    // Calculate sample position at center of current view
    if (!audioBuffer_ || audioBuffer_->getNumSamples() == 0) return 0;

    const int numSamples = audioBuffer_->getNumSamples();
    const float visibleRatio = 1.0f / getZoom();
    const int visibleSamples = static_cast<int>(numSamples * visibleRatio);
    const int startSample = static_cast<int>(getScrollPosition() * (numSamples - visibleSamples));

    return static_cast<size_t>(startSample + visibleSamples / 2);
}

void SliceWaveformDisplay::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();

    // Draw background
    g.fillAll(backgroundColour_);

    // Draw slice regions first (behind waveform)
    if (!slicePoints_.empty() && audioBuffer_ && audioBuffer_->getNumSamples() > 0) {
        paintSliceRegions(g, bounds);
    }

    // Draw centre line
    g.setColour(centreLineColour_);
    g.drawHorizontalLine(bounds.getCentreY(), 0.0f, static_cast<float>(bounds.getWidth()));

    // Draw waveform
    if (audioBuffer_ && audioBuffer_->getNumSamples() > 0) {
        const int numSamples = audioBuffer_->getNumSamples();
        const float* data = audioBuffer_->getReadPointer(0);

        const float visibleRatio = 1.0f / getZoom();
        const int visibleSamples = static_cast<int>(numSamples * visibleRatio);
        const int startSample = static_cast<int>(getScrollPosition() * (numSamples - visibleSamples));

        const float centreY = static_cast<float>(bounds.getCentreY());
        const float halfHeight = bounds.getHeight() * 0.45f;

        g.setColour(waveformColour_);

        juce::Path waveformPath;
        bool pathStarted = false;

        const int width = bounds.getWidth();
        const float samplesPerPixel = static_cast<float>(visibleSamples) / width;

        for (int x = 0; x < width; ++x) {
            const int sampleIndex = startSample + static_cast<int>(x * samplesPerPixel);

            if (sampleIndex >= 0 && sampleIndex < numSamples) {
                float minVal = 0.0f, maxVal = 0.0f;
                const int rangeStart = sampleIndex;
                const int rangeEnd = std::min(static_cast<int>(rangeStart + samplesPerPixel + 1), numSamples);

                for (int i = rangeStart; i < rangeEnd; ++i) {
                    minVal = std::min(minVal, data[i]);
                    maxVal = std::max(maxVal, data[i]);
                }

                const float y1 = centreY - maxVal * halfHeight;
                const float y2 = centreY - minVal * halfHeight;

                if (!pathStarted) {
                    waveformPath.startNewSubPath(static_cast<float>(x), y1);
                    pathStarted = true;
                }

                waveformPath.lineTo(static_cast<float>(x), y1);
                waveformPath.lineTo(static_cast<float>(x), y2);
            }
        }

        g.strokePath(waveformPath, juce::PathStrokeType(1.0f));

        // Draw slice markers on top
        paintSliceMarkers(g, bounds);
    } else {
        g.setColour(juce::Colours::grey);
        g.drawText("No sample loaded", bounds, juce::Justification::centred);
    }

    // Draw info text
    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.setFont(12.0f);

    juce::String infoText;
    if (!slicePoints_.empty()) {
        infoText = juce::String::formatted("Slice: %d/%d  Zoom: %.0f%%",
            currentSlice_ + 1, static_cast<int>(slicePoints_.size()), getZoom() * 100.0f);
    } else {
        infoText = juce::String::formatted("No slices  Zoom: %.0f%%", getZoom() * 100.0f);
    }
    g.drawText(infoText, bounds.reduced(4), juce::Justification::bottomLeft);
}

void SliceWaveformDisplay::paintSliceRegions(juce::Graphics& g, juce::Rectangle<int> bounds) {
    if (!audioBuffer_ || audioBuffer_->getNumSamples() == 0) return;

    const int numSamples = audioBuffer_->getNumSamples();
    const float visibleRatio = 1.0f / getZoom();
    const int visibleSamples = static_cast<int>(numSamples * visibleRatio);
    const int startSample = static_cast<int>(getScrollPosition() * (numSamples - visibleSamples));
    const int endSample = startSample + visibleSamples;
    const float samplesPerPixel = static_cast<float>(visibleSamples) / bounds.getWidth();

    for (int i = 0; i < static_cast<int>(slicePoints_.size()); ++i) {
        size_t sliceStart = slicePoints_[i];
        size_t sliceEnd = (i + 1 < static_cast<int>(slicePoints_.size()))
            ? slicePoints_[i + 1]
            : static_cast<size_t>(numSamples);

        // Skip if slice is outside visible range
        if (sliceEnd < static_cast<size_t>(startSample) || sliceStart > static_cast<size_t>(endSample)) continue;

        // Calculate pixel positions
        int x1 = static_cast<int>((static_cast<float>(sliceStart) - startSample) / samplesPerPixel);
        int x2 = static_cast<int>((static_cast<float>(sliceEnd) - startSample) / samplesPerPixel);

        x1 = juce::jlimit(0, bounds.getWidth(), x1);
        x2 = juce::jlimit(0, bounds.getWidth(), x2);

        // Choose color based on slice index and selection
        juce::Colour regionColour;
        if (i == currentSlice_) {
            regionColour = currentSliceColour_;
        } else {
            regionColour = (i % 2 == 0) ? sliceColourA_ : sliceColourB_;
        }

        g.setColour(regionColour);
        g.fillRect(x1, 0, x2 - x1, bounds.getHeight());
    }
}

void SliceWaveformDisplay::paintSliceMarkers(juce::Graphics& g, juce::Rectangle<int> bounds) {
    if (!audioBuffer_ || audioBuffer_->getNumSamples() == 0) return;

    const int numSamples = audioBuffer_->getNumSamples();
    const float visibleRatio = 1.0f / getZoom();
    const int visibleSamples = static_cast<int>(numSamples * visibleRatio);
    const int startSample = static_cast<int>(getScrollPosition() * (numSamples - visibleSamples));
    const float samplesPerPixel = static_cast<float>(visibleSamples) / bounds.getWidth();

    g.setFont(10.0f);

    for (int i = 0; i < static_cast<int>(slicePoints_.size()); ++i) {
        int x = static_cast<int>((static_cast<float>(slicePoints_[i]) - startSample) / samplesPerPixel);

        if (x >= 0 && x < bounds.getWidth()) {
            // Draw marker line
            g.setColour(markerColour_);
            g.drawVerticalLine(x, 0.0f, static_cast<float>(bounds.getHeight()));

            // Draw slice number
            g.setColour(juce::Colours::white);
            g.drawText(juce::String(i), x + 2, 2, 20, 14, juce::Justification::centredLeft);
        }
    }
}

} // namespace ui
```

**Step 3: Add to CMakeLists.txt**

In `CMakeLists.txt`, add after `src/ui/WaveformDisplay.cpp`:

```cmake
    src/ui/SliceWaveformDisplay.cpp
```

**Step 4: Build to verify compilation**

Run: `cd /Users/jamesmassey/ai-dev/vitracker/.worktrees/sampler/build && cmake --build . --parallel 2>&1 | tail -10`
Expected: `[100%] Built target Vitracker`

**Step 5: Commit**

```bash
git add src/ui/SliceWaveformDisplay.h src/ui/SliceWaveformDisplay.cpp CMakeLists.txt
git commit -m "feat: add SliceWaveformDisplay with colored regions and markers"
```

---

## Task 5: Integrate SlicerInstrument into AudioEngine

**Files:**
- Modify: `src/audio/AudioEngine.h`
- Modify: `src/audio/AudioEngine.cpp`

**Step 1: Add SlicerInstrument include and array**

In `src/audio/AudioEngine.h`, add include after SamplerInstrument:

```cpp
#include "SlicerInstrument.h"
```

Add to private section after samplerProcessors_:

```cpp
    std::array<std::unique_ptr<SlicerInstrument>, NUM_INSTRUMENTS> slicerProcessors_;
```

Add public method after getSamplerProcessor:

```cpp
    SlicerInstrument* getSlicerProcessor(int index);
```

**Step 2: Initialize slicer processors in AudioEngine.cpp**

In `src/audio/AudioEngine.cpp`, in the constructor, after initializing samplerProcessors_, add:

```cpp
    for (int i = 0; i < NUM_INSTRUMENTS; ++i) {
        slicerProcessors_[i] = std::make_unique<SlicerInstrument>();
    }
```

In `prepareToPlay()`, after the samplerProcessors_ loop, add:

```cpp
    for (auto& slicer : slicerProcessors_) {
        if (slicer) {
            slicer->init(sampleRate);
        }
    }
```

**Step 3: Implement getSlicerProcessor**

Add to `AudioEngine.cpp`:

```cpp
SlicerInstrument* AudioEngine::getSlicerProcessor(int index) {
    if (index >= 0 && index < NUM_INSTRUMENTS) {
        return slicerProcessors_[index].get();
    }
    return nullptr;
}
```

**Step 4: Route note triggering for Slicer type**

In `AudioEngine::triggerNote()`, after the Sampler check, add:

```cpp
    if (instrument.getType() == model::InstrumentType::Slicer) {
        if (auto* slicer = getSlicerProcessor(instrumentIndex)) {
            slicer->setInstrument(&instrument);
            slicer->noteOn(note, velocity);
        }
        return;
    }
```

In `releaseNote()`, after the Sampler check, add similar handling:

```cpp
    if (trackInstruments_[track] >= 0) {
        auto& instrument = project_->getInstrument(trackInstruments_[track]);
        if (instrument.getType() == model::InstrumentType::Slicer) {
            if (auto* slicer = getSlicerProcessor(trackInstruments_[track])) {
                slicer->noteOff(note);
            }
            trackVoices_[track] = nullptr;
            return;
        }
    }
```

**Step 5: Mix slicer output in getNextAudioBlock**

In `getNextAudioBlock()`, after the sampler processing section, add:

```cpp
    // Process slicer instruments
    for (int i = 0; i < NUM_INSTRUMENTS; ++i) {
        if (project_ && project_->getInstrument(i).getType() == model::InstrumentType::Slicer) {
            auto* slicer = slicerProcessors_[i].get();
            if (slicer && slicer->hasSample()) {
                std::vector<float> slicerL(bufferToFill.numSamples, 0.0f);
                std::vector<float> slicerR(bufferToFill.numSamples, 0.0f);
                slicer->process(slicerL.data(), slicerR.data(), bufferToFill.numSamples);

                // Mix into main buffer with instrument volume/pan
                const auto& inst = project_->getInstrument(i);
                float vol = inst.getVolume();
                float pan = inst.getPan();
                float leftGain = vol * (1.0f - std::max(0.0f, pan));
                float rightGain = vol * (1.0f + std::min(0.0f, pan));

                for (int s = 0; s < bufferToFill.numSamples; ++s) {
                    bufferToFill.buffer->addSample(0, bufferToFill.startSample + s,
                        slicerL[s] * leftGain);
                    bufferToFill.buffer->addSample(1, bufferToFill.startSample + s,
                        slicerR[s] * rightGain);
                }
            }
        }
    }
```

**Step 6: Build to verify compilation**

Run: `cd /Users/jamesmassey/ai-dev/vitracker/.worktrees/sampler/build && cmake --build . --parallel 2>&1 | tail -10`
Expected: `[100%] Built target Vitracker`

**Step 7: Commit**

```bash
git add src/audio/AudioEngine.h src/audio/AudioEngine.cpp
git commit -m "feat: integrate SlicerInstrument into AudioEngine"
```

---

## Task 6: Add Slicer UI to InstrumentScreen

**Files:**
- Modify: `src/ui/InstrumentScreen.h`
- Modify: `src/ui/InstrumentScreen.cpp`

**Step 1: Add SliceWaveformDisplay include and member**

In `src/ui/InstrumentScreen.h`, add include:

```cpp
#include "SliceWaveformDisplay.h"
#include "../audio/SlicerInstrument.h"
```

Add member variable:

```cpp
    std::unique_ptr<SliceWaveformDisplay> sliceWaveformDisplay_;
```

**Step 2: Initialize waveform display in constructor**

In `src/ui/InstrumentScreen.cpp` constructor, add:

```cpp
    sliceWaveformDisplay_ = std::make_unique<SliceWaveformDisplay>();
    addChildComponent(sliceWaveformDisplay_.get());

    sliceWaveformDisplay_->onAddSlice = [this](size_t pos) {
        if (audioEngine_) {
            if (auto* slicer = audioEngine_->getSlicerProcessor(currentInstrument_)) {
                slicer->addSliceAtPosition(pos);
                updateSlicerDisplay();
            }
        }
    };

    sliceWaveformDisplay_->onDeleteSlice = [this](int index) {
        if (audioEngine_) {
            if (auto* slicer = audioEngine_->getSlicerProcessor(currentInstrument_)) {
                slicer->removeSlice(index);
                updateSlicerDisplay();
            }
        }
    };
```

**Step 3: Add updateSlicerDisplay helper method**

In header, add:

```cpp
    void updateSlicerDisplay();
    bool handleSlicerKey(const juce::KeyPress& key, bool isEditMode);
    void paintSlicerUI(juce::Graphics& g);
```

In implementation:

```cpp
void InstrumentScreen::updateSlicerDisplay() {
    if (!audioEngine_ || !project_) return;

    auto& inst = project_->getInstrument(currentInstrument_);
    if (inst.getType() != model::InstrumentType::Slicer) return;

    auto* slicer = audioEngine_->getSlicerProcessor(currentInstrument_);
    if (!slicer) return;

    sliceWaveformDisplay_->setAudioData(&slicer->getSampleBuffer(), slicer->getSampleRate());
    sliceWaveformDisplay_->setSlicePoints(inst.getSlicerParams().slicePoints);
    sliceWaveformDisplay_->setCurrentSlice(inst.getSlicerParams().currentSlice);
}
```

**Step 4: Add paintSlicerUI method**

```cpp
void InstrumentScreen::paintSlicerUI(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    g.fillAll(juce::Colour(0xFF1E1E1E));

    g.setColour(juce::Colours::white);
    g.setFont(16.0f);

    auto& inst = project_->getInstrument(currentInstrument_);
    g.drawText("SLICER - " + juce::String(inst.getName()),
               bounds.removeFromTop(28), juce::Justification::centredLeft);

    // Waveform takes top portion
    bounds.removeFromTop(100);  // Space for waveform

    g.setFont(14.0f);
    g.setColour(juce::Colours::grey);

    auto& params = inst.getSlicerParams();
    juce::String sampleName = params.sample.path.empty() ? "(no sample)"
        : juce::File(params.sample.path).getFileName();

    g.drawText("Sample: " + sampleName, bounds.removeFromTop(24), juce::Justification::centredLeft);
    g.drawText("Slices: " + juce::String(params.slicePoints.size()),
               bounds.removeFromTop(24), juce::Justification::centredLeft);
    g.drawText("Current: " + juce::String(params.currentSlice + 1),
               bounds.removeFromTop(24), juce::Justification::centredLeft);

    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.drawText("Press 'o' to load sample, h/l to navigate slices, s to add, d to delete",
               bounds.removeFromTop(24), juce::Justification::centredLeft);
}
```

**Step 5: Add handleSlicerKey method**

```cpp
bool InstrumentScreen::handleSlicerKey(const juce::KeyPress& key, bool /*isEditMode*/) {
    auto textChar = key.getTextCharacter();
    auto keyCode = key.getKeyCode();

    // Load sample
    if (textChar == 'o' || textChar == 'O') {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Load Sample",
            juce::File::getSpecialLocation(juce::File::userHomeDirectory),
            "*.wav;*.aiff;*.aif"
        );

        chooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc) {
                auto file = fc.getResult();
                if (file.existsAsFile() && audioEngine_) {
                    if (auto* slicer = audioEngine_->getSlicerProcessor(currentInstrument_)) {
                        slicer->setInstrument(&project_->getInstrument(currentInstrument_));
                        slicer->loadSample(file);
                        updateSlicerDisplay();
                        repaint();
                    }
                }
            }
        );
        return true;
    }

    // Navigate slices: h = previous, l = next
    if (textChar == 'h' || textChar == 'H') {
        if (sliceWaveformDisplay_) {
            sliceWaveformDisplay_->previousSlice();
            if (project_) {
                project_->getInstrument(currentInstrument_).getSlicerParams().currentSlice =
                    sliceWaveformDisplay_->getCurrentSlice();
            }
            repaint();
        }
        return true;
    }

    if (textChar == 'l' || textChar == 'L') {
        if (sliceWaveformDisplay_) {
            sliceWaveformDisplay_->nextSlice();
            if (project_) {
                project_->getInstrument(currentInstrument_).getSlicerParams().currentSlice =
                    sliceWaveformDisplay_->getCurrentSlice();
            }
            repaint();
        }
        return true;
    }

    // Add slice at cursor: s
    if (textChar == 's') {
        if (sliceWaveformDisplay_) {
            sliceWaveformDisplay_->addSliceAtCursor();
            repaint();
        }
        return true;
    }

    // Delete current slice: d
    if (textChar == 'd') {
        if (sliceWaveformDisplay_) {
            sliceWaveformDisplay_->deleteCurrentSlice();
            repaint();
        }
        return true;
    }

    // Zoom: +/-
    if (textChar == '+' || textChar == '=') {
        if (sliceWaveformDisplay_) sliceWaveformDisplay_->zoomIn();
        return true;
    }
    if (textChar == '-') {
        if (sliceWaveformDisplay_) sliceWaveformDisplay_->zoomOut();
        return true;
    }

    // Scroll: Shift+h/l
    if (key.getModifiers().isShiftDown()) {
        if (textChar == 'H' || textChar == 'h') {
            if (sliceWaveformDisplay_) sliceWaveformDisplay_->scrollLeft();
            return true;
        }
        if (textChar == 'L' || textChar == 'l') {
            if (sliceWaveformDisplay_) sliceWaveformDisplay_->scrollRight();
            return true;
        }
    }

    return false;
}
```

**Step 6: Update paint() to handle Slicer type**

In `paint()`, after the Sampler check, add:

```cpp
    if (project_ && project_->getInstrument(currentInstrument_).getType() == model::InstrumentType::Slicer) {
        paintSlicerUI(g);
        return;
    }
```

**Step 7: Update handleEdit() to route to handleSlicerKey**

In `handleEdit()`, after the Sampler check, add:

```cpp
    if (project_ && project_->getInstrument(currentInstrument_).getType() == model::InstrumentType::Slicer) {
        return handleSlicerKey(key, true);
    }
```

**Step 8: Update resized() to position waveform display**

In `resized()`, add:

```cpp
    if (sliceWaveformDisplay_) {
        auto bounds = getLocalBounds();
        bounds.removeFromTop(28);  // Title
        sliceWaveformDisplay_->setBounds(bounds.removeFromTop(100));
    }
```

**Step 9: Show/hide waveform based on instrument type**

Add method to show/hide display based on type:

```cpp
void InstrumentScreen::setCurrentInstrument(int index) {
    currentInstrument_ = index;

    if (project_) {
        auto type = project_->getInstrument(index).getType();
        if (sliceWaveformDisplay_) {
            sliceWaveformDisplay_->setVisible(type == model::InstrumentType::Slicer);
            if (type == model::InstrumentType::Slicer) {
                updateSlicerDisplay();
            }
        }
    }
    repaint();
}
```

**Step 10: Build to verify compilation**

Run: `cd /Users/jamesmassey/ai-dev/vitracker/.worktrees/sampler/build && cmake --build . --parallel 2>&1 | tail -10`
Expected: `[100%] Built target Vitracker`

**Step 11: Commit**

```bash
git add src/ui/InstrumentScreen.h src/ui/InstrumentScreen.cpp
git commit -m "feat: add Slicer UI with slice visualization and navigation"
```

---

## Task 7: Add :slicer Command

**Files:**
- Modify: `src/input/KeyHandler.h`
- Modify: `src/input/KeyHandler.cpp`
- Modify: `src/App.cpp`

**Step 1: Add onCreateSlicer callback**

In `src/input/KeyHandler.h`, add callback:

```cpp
    std::function<void()> onCreateSlicer;
```

**Step 2: Add :slicer command handling**

In `src/input/KeyHandler.cpp`, in `executeCommand()`, after the "sampler" case, add:

```cpp
    else if (command == "slicer") {
        if (onCreateSlicer) onCreateSlicer();
    }
```

**Step 3: Implement in App.cpp**

In `src/App.cpp`, after onCreateSampler, add:

```cpp
    keyHandler_->onCreateSlicer = [this]() {
        // Find first unused instrument slot or use current
        int slot = -1;
        for (int i = 0; i < 128; ++i) {
            if (project_.getInstrument(i).getName() == "Instrument " + std::to_string(i)) {
                slot = i;
                break;
            }
        }
        if (slot == -1) slot = 0;

        project_.getInstrument(slot).setType(model::InstrumentType::Slicer);
        project_.getInstrument(slot).setName("Slicer " + std::to_string(slot));

        // Switch to instrument screen showing this instrument
        if (auto* instScreen = dynamic_cast<ui::InstrumentScreen*>(screens_[4].get())) {
            instScreen->setCurrentInstrument(slot);
        }
        switchScreen(4);  // Instrument screen is index 4
    };
```

**Step 4: Build to verify compilation**

Run: `cd /Users/jamesmassey/ai-dev/vitracker/.worktrees/sampler/build && cmake --build . --parallel 2>&1 | tail -10`
Expected: `[100%] Built target Vitracker`

**Step 5: Commit**

```bash
git add src/input/KeyHandler.h src/input/KeyHandler.cpp src/App.cpp
git commit -m "feat: add :slicer command to create slicer instrument"
```

---

## Task 8: Add :chop Command

**Files:**
- Modify: `src/input/KeyHandler.h`
- Modify: `src/input/KeyHandler.cpp`
- Modify: `src/App.cpp`

**Step 1: Add onChop callback**

In `src/input/KeyHandler.h`, add callback:

```cpp
    std::function<void(int)> onChop;  // Takes number of divisions
```

**Step 2: Add :chop command handling**

In `src/input/KeyHandler.cpp`, in `executeCommand()`, add:

```cpp
    else if (command.substr(0, 4) == "chop") {
        // Parse :chop N
        int divisions = 8;  // default
        if (command.length() > 5) {
            try {
                divisions = std::stoi(command.substr(5));
            } catch (...) {
                divisions = 8;
            }
        }
        if (onChop) onChop(divisions);
    }
```

**Step 3: Implement in App.cpp**

In `src/App.cpp`, add:

```cpp
    keyHandler_->onChop = [this](int divisions) {
        if (auto* instScreen = dynamic_cast<ui::InstrumentScreen*>(screens_[4].get())) {
            int currentInst = instScreen->getCurrentInstrument();
            auto& inst = project_.getInstrument(currentInst);

            if (inst.getType() == model::InstrumentType::Slicer) {
                if (auto* slicer = audioEngine_.getSlicerProcessor(currentInst)) {
                    slicer->chopIntoDivisions(divisions);
                    instScreen->repaint();
                }
            }
        }
    };
```

**Step 4: Build to verify compilation**

Run: `cd /Users/jamesmassey/ai-dev/vitracker/.worktrees/sampler/build && cmake --build . --parallel 2>&1 | tail -10`
Expected: `[100%] Built target Vitracker`

**Step 5: Commit**

```bash
git add src/input/KeyHandler.h src/input/KeyHandler.cpp src/App.cpp
git commit -m "feat: add :chop command to divide sample into equal slices"
```

---

## Task 9: Test Slicer End-to-End

**Files:** None (manual testing)

**Step 1: Build and run**

```bash
cd /Users/jamesmassey/ai-dev/vitracker/.worktrees/sampler/build
cmake --build . --parallel
./Vitracker_artefacts/Debug/Vitracker.app/Contents/MacOS/Vitracker
```

**Step 2: Test flow**

1. Press `:` to enter command mode
2. Type `slicer` and press Enter
3. Verify instrument screen shows "SLICER - Slicer 0"
4. Press `o` to open file chooser
5. Select a WAV file (ideally a drum loop)
6. Verify waveform displays with 8 colored slice regions
7. Press `h`/`l` to navigate between slices
8. Play notes C1-G1 (MIDI 36-43) - verify different slices play
9. Press `s` to add a slice at cursor position
10. Press `d` to delete current slice
11. Type `:chop 16` to re-divide into 16 slices
12. Press `+`/`-` to zoom waveform
13. Press `Shift+h`/`Shift+l` to scroll waveform

**Step 3: Document any bugs found**

If bugs are found, create follow-up fix tasks.

**Step 4: Final commit**

```bash
git add -A
git commit -m "feat: complete Phase 2 slicer implementation"
```

---

## Summary

Phase 2 delivers:
- SlicerParams structure with slice points and chop mode
- SlicerVoice for slice playback (no pitch shifting)
- SlicerInstrument with slice-per-key triggering (C1 = slice 0)
- SliceWaveformDisplay with colored regions and markers
- Slicer UI integrated into InstrumentScreen
- h/l navigation between slices
- s to add slice, d to delete slice
- Zero-crossing snap for all slice points
- `:slicer` command to create slicer instrument
- `:chop N` command to divide sample into N equal slices

Next: Phase 3 (Time Manipulation) adds BPM detection, pitch detection, and Rubber Band time-stretching.
