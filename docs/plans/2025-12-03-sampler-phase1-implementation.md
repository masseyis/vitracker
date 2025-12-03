# Sampler Phase 1 Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a Sampler instrument type that loads WAV/AIFF files and plays them chromatically across the keyboard with filter and ADSR envelope.

**Architecture:** Sampler is a new instrument type alongside Plaits. SamplerInstrument processor handles polyphonic playback with voice stealing. InstrumentScreen detects type and renders appropriate UI with waveform split-view.

**Tech Stack:** JUCE (AudioFormatManager, Interpolators), existing MoogFilter, new ADSR envelope.

---

## Task 1: Add InstrumentType Enum

**Files:**
- Modify: `src/model/Instrument.h`
- Modify: `src/model/Instrument.cpp`

**Step 1: Add the InstrumentType enum to Instrument.h**

In `src/model/Instrument.h`, add after the includes, before `namespace model`:

```cpp
namespace model {

enum class InstrumentType {
    Plaits,
    Sampler,
    Slicer
};
```

**Step 2: Add type member and getter/setter to Instrument class**

In `src/model/Instrument.h`, add to the private section of `Instrument` class:

```cpp
private:
    InstrumentType type_ = InstrumentType::Plaits;
```

Add to the public section:

```cpp
    InstrumentType getType() const { return type_; }
    void setType(InstrumentType type) { type_ = type; }
```

**Step 3: Build to verify compilation**

Run: `cd /Users/jamesmassey/ai-dev/vitracker/.worktrees/sampler/build && cmake --build . --parallel 2>&1 | tail -10`
Expected: `[100%] Built target Vitracker`

**Step 4: Commit**

```bash
git add src/model/Instrument.h
git commit -m "feat: add InstrumentType enum (Plaits, Sampler, Slicer)"
```

---

## Task 2: Create ADSR Envelope

**Files:**
- Create: `src/dsp/adsr_envelope.h`

**Step 1: Create the ADSR envelope header**

Create `src/dsp/adsr_envelope.h`:

```cpp
#pragma once

#include <cmath>
#include <algorithm>

namespace dsp {

class AdsrEnvelope {
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void setSampleRate(float sampleRate) {
        sampleRate_ = sampleRate;
        recalculate();
    }

    void setAttack(float seconds) {
        attackTime_ = std::max(0.001f, seconds);
        recalculate();
    }

    void setDecay(float seconds) {
        decayTime_ = std::max(0.001f, seconds);
        recalculate();
    }

    void setSustain(float level) {
        sustainLevel_ = std::clamp(level, 0.0f, 1.0f);
    }

    void setRelease(float seconds) {
        releaseTime_ = std::max(0.001f, seconds);
        recalculate();
    }

    void trigger() {
        stage_ = Stage::Attack;
        currentValue_ = 0.0f;
    }

    void release() {
        if (stage_ != Stage::Idle) {
            stage_ = Stage::Release;
            releaseStartValue_ = currentValue_;
        }
    }

    float process() {
        switch (stage_) {
            case Stage::Idle:
                currentValue_ = 0.0f;
                break;

            case Stage::Attack:
                currentValue_ += attackRate_;
                if (currentValue_ >= 1.0f) {
                    currentValue_ = 1.0f;
                    stage_ = Stage::Decay;
                }
                break;

            case Stage::Decay:
                currentValue_ -= decayRate_;
                if (currentValue_ <= sustainLevel_) {
                    currentValue_ = sustainLevel_;
                    stage_ = Stage::Sustain;
                }
                break;

            case Stage::Sustain:
                currentValue_ = sustainLevel_;
                break;

            case Stage::Release:
                currentValue_ -= releaseRate_ * releaseStartValue_;
                if (currentValue_ <= 0.0f) {
                    currentValue_ = 0.0f;
                    stage_ = Stage::Idle;
                }
                break;
        }
        return currentValue_;
    }

    bool isActive() const { return stage_ != Stage::Idle; }
    Stage getStage() const { return stage_; }
    float getValue() const { return currentValue_; }

private:
    void recalculate() {
        if (sampleRate_ <= 0.0f) return;
        attackRate_ = 1.0f / (attackTime_ * sampleRate_);
        decayRate_ = (1.0f - sustainLevel_) / (decayTime_ * sampleRate_);
        releaseRate_ = 1.0f / (releaseTime_ * sampleRate_);
    }

    float sampleRate_ = 48000.0f;
    float attackTime_ = 0.01f;
    float decayTime_ = 0.1f;
    float sustainLevel_ = 0.8f;
    float releaseTime_ = 0.25f;

    float attackRate_ = 0.0f;
    float decayRate_ = 0.0f;
    float releaseRate_ = 0.0f;

    Stage stage_ = Stage::Idle;
    float currentValue_ = 0.0f;
    float releaseStartValue_ = 1.0f;
};

} // namespace dsp
```

**Step 2: Build to verify compilation**

Run: `cd /Users/jamesmassey/ai-dev/vitracker/.worktrees/sampler/build && cmake --build . --parallel 2>&1 | tail -10`
Expected: `[100%] Built target Vitracker`

**Step 3: Commit**

```bash
git add src/dsp/adsr_envelope.h
git commit -m "feat: add ADSR envelope for sampler"
```

---

## Task 3: Create SamplerParams Structure

**Files:**
- Create: `src/model/SamplerParams.h`

**Step 1: Create the SamplerParams header**

Create `src/model/SamplerParams.h`:

```cpp
#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace model {

struct SampleRef {
    std::string path;
    bool embedded = false;
    std::vector<float> embeddedData;
    int numChannels = 0;
    int sampleRate = 0;
    size_t numSamples = 0;
};

struct AdsrParams {
    float attack = 0.01f;   // seconds (0.001 - 10.0)
    float decay = 0.1f;     // seconds (0.001 - 10.0)
    float sustain = 0.8f;   // level (0.0 - 1.0)
    float release = 0.25f;  // seconds (0.001 - 10.0)
};

struct SamplerParams {
    SampleRef sample;
    int rootNote = 60;                  // MIDI note for original pitch
    float detectedPitchHz = 0.0f;       // Auto-detected pitch, 0 = unknown
    float detectedPitchCents = 0.0f;    // Cents deviation from nearest note
    int detectedMidiNote = -1;          // -1 = unknown

    FilterParams filter;
    AdsrParams ampEnvelope;
    float filterEnvAmount = 0.0f;       // -1.0 to +1.0
    AdsrParams filterEnvelope;
};

} // namespace model
```

**Step 2: Build to verify compilation**

Run: `cd /Users/jamesmassey/ai-dev/vitracker/.worktrees/sampler/build && cmake --build . --parallel 2>&1 | tail -10`
Expected: `[100%] Built target Vitracker`

**Step 3: Commit**

```bash
git add src/model/SamplerParams.h
git commit -m "feat: add SamplerParams and SampleRef structures"
```

---

## Task 4: Add Sampler Params to Instrument Class

**Files:**
- Modify: `src/model/Instrument.h`
- Modify: `src/model/Instrument.cpp`

**Step 1: Include SamplerParams and add member**

In `src/model/Instrument.h`, add include:

```cpp
#include "SamplerParams.h"
```

Add to private section of Instrument class:

```cpp
    SamplerParams samplerParams_;
```

Add to public section:

```cpp
    SamplerParams& getSamplerParams() { return samplerParams_; }
    const SamplerParams& getSamplerParams() const { return samplerParams_; }
```

**Step 2: Build to verify compilation**

Run: `cd /Users/jamesmassey/ai-dev/vitracker/.worktrees/sampler/build && cmake --build . --parallel 2>&1 | tail -10`
Expected: `[100%] Built target Vitracker`

**Step 3: Commit**

```bash
git add src/model/Instrument.h
git commit -m "feat: add SamplerParams to Instrument class"
```

---

## Task 5: Create SamplerVoice Class

**Files:**
- Create: `src/audio/SamplerVoice.h`
- Create: `src/audio/SamplerVoice.cpp`

**Step 1: Create SamplerVoice header**

Create `src/audio/SamplerVoice.h`:

```cpp
#pragma once

#include "../model/SamplerParams.h"
#include "../dsp/adsr_envelope.h"
#include "../dsp/moog_filter.h"
#include <JuceHeader.h>

namespace audio {

class SamplerVoice {
public:
    SamplerVoice();

    void setSampleRate(double sampleRate);
    void setSampleData(const float* data, int numChannels, size_t numSamples, int originalSampleRate);

    void trigger(int midiNote, float velocity, const model::SamplerParams& params);
    void release();

    void render(float* leftOut, float* rightOut, int numSamples);

    bool isActive() const { return active_; }
    int getNote() const { return currentNote_; }

private:
    double sampleRate_ = 48000.0;
    const float* sampleData_ = nullptr;
    int sampleChannels_ = 0;
    size_t sampleLength_ = 0;
    int originalSampleRate_ = 44100;

    bool active_ = false;
    int currentNote_ = -1;
    int rootNote_ = 60;
    float velocity_ = 1.0f;

    double playPosition_ = 0.0;
    double playbackRate_ = 1.0;

    dsp::AdsrEnvelope ampEnvelope_;
    dsp::AdsrEnvelope filterEnvelope_;
    dsp::MoogFilter filterL_;
    dsp::MoogFilter filterR_;

    float baseCutoff_ = 1.0f;
    float filterEnvAmount_ = 0.0f;

    juce::Interpolators::Lagrange interpolatorL_;
    juce::Interpolators::Lagrange interpolatorR_;
};

} // namespace audio
```

**Step 2: Create SamplerVoice implementation**

Create `src/audio/SamplerVoice.cpp`:

```cpp
#include "SamplerVoice.h"
#include <cmath>

namespace audio {

SamplerVoice::SamplerVoice() {
    ampEnvelope_.setSampleRate(48000.0f);
    filterEnvelope_.setSampleRate(48000.0f);
}

void SamplerVoice::setSampleRate(double sampleRate) {
    sampleRate_ = sampleRate;
    ampEnvelope_.setSampleRate(static_cast<float>(sampleRate));
    filterEnvelope_.setSampleRate(static_cast<float>(sampleRate));
    filterL_.Init();
    filterR_.Init();
}

void SamplerVoice::setSampleData(const float* data, int numChannels, size_t numSamples, int originalSampleRate) {
    sampleData_ = data;
    sampleChannels_ = numChannels;
    sampleLength_ = numSamples;
    originalSampleRate_ = originalSampleRate;
}

void SamplerVoice::trigger(int midiNote, float velocity, const model::SamplerParams& params) {
    if (!sampleData_ || sampleLength_ == 0) return;

    currentNote_ = midiNote;
    rootNote_ = params.rootNote;
    velocity_ = velocity;
    active_ = true;
    playPosition_ = 0.0;

    // Calculate playback rate for pitch shifting
    double semitones = midiNote - rootNote_;
    playbackRate_ = std::pow(2.0, semitones / 12.0);

    // Adjust for sample rate difference
    playbackRate_ *= static_cast<double>(originalSampleRate_) / sampleRate_;

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

    interpolatorL_.reset();
    interpolatorR_.reset();
}

void SamplerVoice::release() {
    ampEnvelope_.release();
    filterEnvelope_.release();
}

void SamplerVoice::render(float* leftOut, float* rightOut, int numSamples) {
    if (!active_ || !sampleData_) {
        for (int i = 0; i < numSamples; ++i) {
            leftOut[i] = 0.0f;
            rightOut[i] = 0.0f;
        }
        return;
    }

    for (int i = 0; i < numSamples; ++i) {
        // Check if we've reached end of sample
        if (playPosition_ >= static_cast<double>(sampleLength_ - 1)) {
            active_ = false;
            leftOut[i] = 0.0f;
            rightOut[i] = 0.0f;
            continue;
        }

        // Linear interpolation for sample playback
        size_t pos0 = static_cast<size_t>(playPosition_);
        size_t pos1 = std::min(pos0 + 1, sampleLength_ - 1);
        float frac = static_cast<float>(playPosition_ - pos0);

        float sampleL, sampleR;
        if (sampleChannels_ == 1) {
            sampleL = sampleData_[pos0] * (1.0f - frac) + sampleData_[pos1] * frac;
            sampleR = sampleL;
        } else {
            sampleL = sampleData_[pos0 * 2] * (1.0f - frac) + sampleData_[pos1 * 2] * frac;
            sampleR = sampleData_[pos0 * 2 + 1] * (1.0f - frac) + sampleData_[pos1 * 2 + 1] * frac;
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

        // Advance position
        playPosition_ += playbackRate_;

        // Check if envelope finished
        if (!ampEnvelope_.isActive()) {
            active_ = false;
        }
    }
}

} // namespace audio
```

**Step 3: Add to CMakeLists.txt**

In `CMakeLists.txt`, add to target_sources after `src/audio/PlaitsInstrument.cpp`:

```cmake
    src/audio/SamplerVoice.cpp
```

**Step 4: Build to verify compilation**

Run: `cd /Users/jamesmassey/ai-dev/vitracker/.worktrees/sampler/build && cmake --build . --parallel 2>&1 | tail -15`
Expected: `[100%] Built target Vitracker`

**Step 5: Commit**

```bash
git add src/audio/SamplerVoice.h src/audio/SamplerVoice.cpp CMakeLists.txt
git commit -m "feat: add SamplerVoice for sample playback"
```

---

## Task 6: Create SamplerInstrument Class

**Files:**
- Create: `src/audio/SamplerInstrument.h`
- Create: `src/audio/SamplerInstrument.cpp`

**Step 1: Create SamplerInstrument header**

Create `src/audio/SamplerInstrument.h`:

```cpp
#pragma once

#include "InstrumentProcessor.h"
#include "SamplerVoice.h"
#include "../model/Instrument.h"
#include <JuceHeader.h>
#include <array>
#include <memory>

namespace audio {

class SamplerInstrument : public InstrumentProcessor {
public:
    static constexpr int NUM_VOICES = 8;

    SamplerInstrument();
    ~SamplerInstrument() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer) override;

    void noteOn(int midiNote, float velocity) override;
    void noteOff(int midiNote) override;
    void allNotesOff() override;

    void setInstrument(model::Instrument* instrument) { instrument_ = instrument; }
    model::Instrument* getInstrument() const { return instrument_; }

    bool loadSample(const juce::File& file);
    bool hasSample() const { return sampleBuffer_.getNumSamples() > 0; }

    // For waveform display
    const juce::AudioBuffer<float>& getSampleBuffer() const { return sampleBuffer_; }
    int getSampleRate() const { return loadedSampleRate_; }

private:
    SamplerVoice* findFreeVoice();
    SamplerVoice* findVoiceToSteal();

    model::Instrument* instrument_ = nullptr;
    double sampleRate_ = 48000.0;

    std::array<SamplerVoice, NUM_VOICES> voices_;
    juce::AudioBuffer<float> sampleBuffer_;
    int loadedSampleRate_ = 44100;

    juce::AudioFormatManager formatManager_;
};

} // namespace audio
```

**Step 2: Create SamplerInstrument implementation**

Create `src/audio/SamplerInstrument.cpp`:

```cpp
#include "SamplerInstrument.h"

namespace audio {

SamplerInstrument::SamplerInstrument() {
    formatManager_.registerBasicFormats();
}

void SamplerInstrument::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    sampleRate_ = sampleRate;
    for (auto& voice : voices_) {
        voice.setSampleRate(sampleRate);
    }
}

void SamplerInstrument::processBlock(juce::AudioBuffer<float>& buffer) {
    buffer.clear();

    if (!hasSample() || !instrument_) return;

    const int numSamples = buffer.getNumSamples();
    auto* leftOut = buffer.getWritePointer(0);
    auto* rightOut = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : leftOut;

    // Temp buffers for voice mixing
    std::vector<float> tempL(numSamples, 0.0f);
    std::vector<float> tempR(numSamples, 0.0f);

    for (auto& voice : voices_) {
        if (voice.isActive()) {
            voice.render(tempL.data(), tempR.data(), numSamples);

            for (int i = 0; i < numSamples; ++i) {
                leftOut[i] += tempL[i];
                rightOut[i] += tempR[i];
            }

            // Clear temp for next voice
            std::fill(tempL.begin(), tempL.end(), 0.0f);
            std::fill(tempR.begin(), tempR.end(), 0.0f);
        }
    }
}

void SamplerInstrument::noteOn(int midiNote, float velocity) {
    if (!hasSample() || !instrument_) return;

    auto* voice = findFreeVoice();
    if (!voice) {
        voice = findVoiceToSteal();
    }

    if (voice) {
        const auto& params = instrument_->getSamplerParams();
        voice->setSampleData(
            sampleBuffer_.getReadPointer(0),
            sampleBuffer_.getNumChannels(),
            static_cast<size_t>(sampleBuffer_.getNumSamples()),
            loadedSampleRate_
        );
        voice->trigger(midiNote, velocity, params);
    }
}

void SamplerInstrument::noteOff(int midiNote) {
    for (auto& voice : voices_) {
        if (voice.isActive() && voice.getNote() == midiNote) {
            voice.release();
        }
    }
}

void SamplerInstrument::allNotesOff() {
    for (auto& voice : voices_) {
        voice.release();
    }
}

SamplerVoice* SamplerInstrument::findFreeVoice() {
    for (auto& voice : voices_) {
        if (!voice.isActive()) {
            return &voice;
        }
    }
    return nullptr;
}

SamplerVoice* SamplerInstrument::findVoiceToSteal() {
    // Simple voice stealing: return first voice
    return &voices_[0];
}

bool SamplerInstrument::loadSample(const juce::File& file) {
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
        auto& sampleRef = instrument_->getSamplerParams().sample;
        sampleRef.path = file.getFullPathName().toStdString();
        sampleRef.numChannels = numChannels;
        sampleRef.sampleRate = loadedSampleRate_;
        sampleRef.numSamples = static_cast<size_t>(numSamples);
    }

    // Update all voices with new sample data
    for (auto& voice : voices_) {
        voice.setSampleRate(sampleRate_);
    }

    DBG("Loaded sample: " << file.getFileName()
        << " (" << numChannels << " ch, "
        << loadedSampleRate_ << " Hz, "
        << numSamples << " samples)");

    return true;
}

} // namespace audio
```

**Step 3: Add to CMakeLists.txt**

In `CMakeLists.txt`, add after `src/audio/SamplerVoice.cpp`:

```cmake
    src/audio/SamplerInstrument.cpp
```

**Step 4: Build to verify compilation**

Run: `cd /Users/jamesmassey/ai-dev/vitracker/.worktrees/sampler/build && cmake --build . --parallel 2>&1 | tail -15`
Expected: `[100%] Built target Vitracker`

**Step 5: Commit**

```bash
git add src/audio/SamplerInstrument.h src/audio/SamplerInstrument.cpp CMakeLists.txt
git commit -m "feat: add SamplerInstrument with polyphonic voice management"
```

---

## Task 7: Integrate SamplerInstrument into AudioEngine

**Files:**
- Modify: `src/audio/AudioEngine.h`
- Modify: `src/audio/AudioEngine.cpp`

**Step 1: Add SamplerInstrument includes and array**

In `src/audio/AudioEngine.h`, add include:

```cpp
#include "SamplerInstrument.h"
```

Add to private section:

```cpp
    std::array<std::unique_ptr<SamplerInstrument>, NUM_INSTRUMENTS> samplerProcessors_;
```

Add public method:

```cpp
    SamplerInstrument* getSamplerProcessor(int index);
```

**Step 2: Initialize sampler processors in AudioEngine.cpp**

In `src/audio/AudioEngine.cpp`, in the constructor, after initializing instrumentProcessors_, add:

```cpp
    for (int i = 0; i < NUM_INSTRUMENTS; ++i) {
        samplerProcessors_[i] = std::make_unique<SamplerInstrument>();
    }
```

In `prepareToPlay()`, after the instrumentProcessors_ loop, add:

```cpp
    for (auto& sampler : samplerProcessors_) {
        if (sampler) {
            sampler->prepareToPlay(sampleRate, samplesPerBlockExpected);
        }
    }
```

**Step 3: Implement getSamplerProcessor**

Add to `AudioEngine.cpp`:

```cpp
SamplerInstrument* AudioEngine::getSamplerProcessor(int index) {
    if (index >= 0 && index < NUM_INSTRUMENTS) {
        return samplerProcessors_[index].get();
    }
    return nullptr;
}
```

**Step 4: Route note triggering based on instrument type**

In `AudioEngine::triggerNote()`, after getting the instrument, check its type:

```cpp
    auto& instrument = project_->getInstrument(instrumentIndex);

    if (instrument.getType() == model::InstrumentType::Sampler) {
        if (auto* sampler = getSamplerProcessor(instrumentIndex)) {
            sampler->setInstrument(&instrument);
            sampler->noteOn(note, velocity);
        }
        return;
    }

    // Existing Plaits code follows...
```

Similarly in `releaseNote()`, add before existing code:

```cpp
    if (trackInstruments_[track] >= 0) {
        auto& instrument = project_->getInstrument(trackInstruments_[track]);
        if (instrument.getType() == model::InstrumentType::Sampler) {
            if (auto* sampler = getSamplerProcessor(trackInstruments_[track])) {
                sampler->noteOff(trackVoices_[track] ? trackVoices_[track]->getCurrentNote() : -1);
            }
            trackVoices_[track] = nullptr;
            return;
        }
    }
```

**Step 5: Mix sampler output in getNextAudioBlock**

In `getNextAudioBlock()`, after processing Plaits instruments, add sampler processing:

```cpp
    // Process sampler instruments
    juce::AudioBuffer<float> samplerBuffer(2, bufferToFill.numSamples);
    for (int i = 0; i < NUM_INSTRUMENTS; ++i) {
        if (project_ && project_->getInstrument(i).getType() == model::InstrumentType::Sampler) {
            auto* sampler = samplerProcessors_[i].get();
            if (sampler && sampler->hasSample()) {
                sampler->processBlock(samplerBuffer);

                // Mix into main buffer with instrument volume/pan
                const auto& inst = project_->getInstrument(i);
                float vol = inst.getVolume();
                float pan = inst.getPan();
                float leftGain = vol * (1.0f - std::max(0.0f, pan));
                float rightGain = vol * (1.0f + std::min(0.0f, pan));

                for (int s = 0; s < bufferToFill.numSamples; ++s) {
                    bufferToFill.buffer->addSample(0, bufferToFill.startSample + s,
                        samplerBuffer.getSample(0, s) * leftGain);
                    bufferToFill.buffer->addSample(1, bufferToFill.startSample + s,
                        samplerBuffer.getSample(1, s) * rightGain);
                }
            }
        }
    }
```

**Step 6: Build to verify compilation**

Run: `cd /Users/jamesmassey/ai-dev/vitracker/.worktrees/sampler/build && cmake --build . --parallel 2>&1 | tail -15`
Expected: `[100%] Built target Vitracker`

**Step 7: Commit**

```bash
git add src/audio/AudioEngine.h src/audio/AudioEngine.cpp
git commit -m "feat: integrate SamplerInstrument into AudioEngine"
```

---

## Task 8: Add Waveform Display Component

**Files:**
- Create: `src/ui/WaveformDisplay.h`
- Create: `src/ui/WaveformDisplay.cpp`

**Step 1: Create WaveformDisplay header**

Create `src/ui/WaveformDisplay.h`:

```cpp
#pragma once

#include <JuceHeader.h>

namespace ui {

class WaveformDisplay : public juce::Component {
public:
    WaveformDisplay();

    void setAudioData(const juce::AudioBuffer<float>* buffer, int sampleRate);
    void paint(juce::Graphics& g) override;

    void setZoom(float zoom);
    void setScrollPosition(float position);
    float getZoom() const { return zoom_; }
    float getScrollPosition() const { return scrollPosition_; }

    void zoomIn();
    void zoomOut();
    void scrollLeft();
    void scrollRight();

private:
    const juce::AudioBuffer<float>* audioBuffer_ = nullptr;
    int sampleRate_ = 44100;
    float zoom_ = 1.0f;           // 1.0 = fit whole sample
    float scrollPosition_ = 0.0f; // 0.0-1.0 position in sample

    juce::Colour waveformColour_ = juce::Colour(0xFF4EC9B0);
    juce::Colour backgroundColour_ = juce::Colour(0xFF1E1E1E);
    juce::Colour centreLineColour_ = juce::Colour(0xFF3C3C3C);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};

} // namespace ui
```

**Step 2: Create WaveformDisplay implementation**

Create `src/ui/WaveformDisplay.cpp`:

```cpp
#include "WaveformDisplay.h"

namespace ui {

WaveformDisplay::WaveformDisplay() {
    setOpaque(true);
}

void WaveformDisplay::setAudioData(const juce::AudioBuffer<float>* buffer, int sampleRate) {
    audioBuffer_ = buffer;
    sampleRate_ = sampleRate;
    repaint();
}

void WaveformDisplay::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    g.fillAll(backgroundColour_);

    // Centre line
    g.setColour(centreLineColour_);
    g.drawHorizontalLine(bounds.getCentreY(), 0.0f, static_cast<float>(bounds.getWidth()));

    if (!audioBuffer_ || audioBuffer_->getNumSamples() == 0) {
        g.setColour(juce::Colours::grey);
        g.drawText("No sample loaded", bounds, juce::Justification::centred);
        return;
    }

    const int numSamples = audioBuffer_->getNumSamples();
    const float* data = audioBuffer_->getReadPointer(0);

    // Calculate visible range based on zoom and scroll
    const float visibleRatio = 1.0f / zoom_;
    const int visibleSamples = static_cast<int>(numSamples * visibleRatio);
    const int startSample = static_cast<int>(scrollPosition_ * (numSamples - visibleSamples));

    const float centreY = bounds.getCentreY();
    const float halfHeight = bounds.getHeight() * 0.45f;

    g.setColour(waveformColour_);

    juce::Path waveformPath;
    bool pathStarted = false;

    const int width = bounds.getWidth();
    const float samplesPerPixel = static_cast<float>(visibleSamples) / width;

    for (int x = 0; x < width; ++x) {
        const int sampleIndex = startSample + static_cast<int>(x * samplesPerPixel);

        if (sampleIndex >= 0 && sampleIndex < numSamples) {
            // Find min/max in this pixel's sample range
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

    // Draw time info
    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.setFont(12.0f);

    const float startTime = static_cast<float>(startSample) / sampleRate_;
    const float endTime = static_cast<float>(startSample + visibleSamples) / sampleRate_;
    const float totalTime = static_cast<float>(numSamples) / sampleRate_;

    juce::String timeText = juce::String::formatted("%.2fs - %.2fs / %.2fs  Zoom: %.0f%%",
        startTime, endTime, totalTime, zoom_ * 100.0f);
    g.drawText(timeText, bounds.reduced(4), juce::Justification::bottomLeft);
}

void WaveformDisplay::setZoom(float zoom) {
    zoom_ = juce::jlimit(1.0f, 100.0f, zoom);
    repaint();
}

void WaveformDisplay::setScrollPosition(float position) {
    scrollPosition_ = juce::jlimit(0.0f, 1.0f, position);
    repaint();
}

void WaveformDisplay::zoomIn() {
    setZoom(zoom_ * 1.5f);
}

void WaveformDisplay::zoomOut() {
    setZoom(zoom_ / 1.5f);
}

void WaveformDisplay::scrollLeft() {
    setScrollPosition(scrollPosition_ - 0.1f / zoom_);
}

void WaveformDisplay::scrollRight() {
    setScrollPosition(scrollPosition_ + 0.1f / zoom_);
}

} // namespace ui
```

**Step 3: Add to CMakeLists.txt**

Add after `src/ui/ChainScreen.cpp`:

```cmake
    src/ui/WaveformDisplay.cpp
```

**Step 4: Build to verify compilation**

Run: `cd /Users/jamesmassey/ai-dev/vitracker/.worktrees/sampler/build && cmake --build . --parallel 2>&1 | tail -15`
Expected: `[100%] Built target Vitracker`

**Step 5: Commit**

```bash
git add src/ui/WaveformDisplay.h src/ui/WaveformDisplay.cpp CMakeLists.txt
git commit -m "feat: add WaveformDisplay component for sample visualization"
```

---

## Task 9: Create SamplerScreen UI

**Files:**
- Create: `src/ui/SamplerScreen.h`
- Create: `src/ui/SamplerScreen.cpp`

**Step 1: Create SamplerScreen header**

Create `src/ui/SamplerScreen.h`:

```cpp
#pragma once

#include "Screen.h"
#include "WaveformDisplay.h"
#include "../model/Project.h"
#include "../audio/SamplerInstrument.h"

namespace ui {

class SamplerScreen : public Screen {
public:
    enum class RowType {
        Sample,
        RootNote,
        FilterCutoff,
        FilterResonance,
        AmpAttack,
        AmpDecay,
        AmpSustain,
        AmpRelease,
        FilterEnvAmount,
        SendReverb,
        SendDelay,
        SendChorus,
        SendDrive,
        COUNT
    };

    SamplerScreen();

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setProject(model::Project* project) { project_ = project; }
    void setAudioEngine(audio::AudioEngine* engine) { audioEngine_ = engine; }
    void setCurrentInstrument(int index);
    int getCurrentInstrument() const { return currentInstrument_; }

    bool handleKey(const juce::KeyPress& key, bool isEditMode) override;
    void navigate(int dx, int dy) override;

private:
    void updateWaveformDisplay();
    void loadSample();
    void adjustValue(int delta);
    float getRowValue(RowType row) const;
    void setRowValue(RowType row, float value);
    juce::String getRowName(RowType row) const;
    juce::String getRowValueString(RowType row) const;

    model::Project* project_ = nullptr;
    audio::AudioEngine* audioEngine_ = nullptr;
    int currentInstrument_ = 0;
    int currentRow_ = 0;

    WaveformDisplay waveformDisplay_;

    static constexpr int WAVEFORM_HEIGHT = 100;
    static constexpr int ROW_HEIGHT = 24;
    static constexpr int LABEL_WIDTH = 140;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SamplerScreen)
};

} // namespace ui
```

**Step 2: Create SamplerScreen implementation**

Create `src/ui/SamplerScreen.cpp`:

```cpp
#include "SamplerScreen.h"

namespace ui {

SamplerScreen::SamplerScreen() {
    addAndMakeVisible(waveformDisplay_);
}

void SamplerScreen::setCurrentInstrument(int index) {
    currentInstrument_ = index;
    updateWaveformDisplay();
    repaint();
}

void SamplerScreen::updateWaveformDisplay() {
    if (audioEngine_) {
        if (auto* sampler = audioEngine_->getSamplerProcessor(currentInstrument_)) {
            waveformDisplay_.setAudioData(&sampler->getSampleBuffer(), sampler->getSampleRate());
            return;
        }
    }
    waveformDisplay_.setAudioData(nullptr, 44100);
}

void SamplerScreen::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();

    // Background
    g.fillAll(juce::Colour(0xFF1E1E1E));

    // Title
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);

    juce::String title = "SAMPLER";
    if (project_) {
        title += " - " + juce::String(project_->getInstrument(currentInstrument_).getName());
    }
    g.drawText(title, bounds.removeFromTop(28), juce::Justification::centredLeft);

    // Skip waveform area
    bounds.removeFromTop(WAVEFORM_HEIGHT + 8);

    // Draw parameter rows
    g.setFont(14.0f);

    for (int i = 0; i < static_cast<int>(RowType::COUNT); ++i) {
        auto rowBounds = bounds.removeFromTop(ROW_HEIGHT);
        auto row = static_cast<RowType>(i);

        // Highlight current row
        if (i == currentRow_) {
            g.setColour(juce::Colour(0xFF2D2D2D));
            g.fillRect(rowBounds);
        }

        // Label
        g.setColour(juce::Colours::grey);
        g.drawText(getRowName(row), rowBounds.removeFromLeft(LABEL_WIDTH),
                   juce::Justification::centredLeft);

        // Value
        g.setColour(i == currentRow_ ? juce::Colour(0xFF4EC9B0) : juce::Colours::white);
        g.drawText(getRowValueString(row), rowBounds.removeFromLeft(200),
                   juce::Justification::centredLeft);

        // Value bar for numeric params
        if (row != RowType::Sample) {
            float value = getRowValue(row);
            auto barBounds = rowBounds.removeFromLeft(100).reduced(2);
            g.setColour(juce::Colour(0xFF3C3C3C));
            g.fillRect(barBounds);
            g.setColour(juce::Colour(0xFF4EC9B0));
            g.fillRect(barBounds.removeFromLeft(static_cast<int>(barBounds.getWidth() * value)));
        }
    }
}

void SamplerScreen::resized() {
    auto bounds = getLocalBounds();
    bounds.removeFromTop(28); // Title
    waveformDisplay_.setBounds(bounds.removeFromTop(WAVEFORM_HEIGHT));
}

bool SamplerScreen::handleKey(const juce::KeyPress& key, bool isEditMode) {
    auto keyCode = key.getKeyCode();
    auto textChar = key.getTextCharacter();

    // Waveform controls (always active)
    if (textChar == '+' || textChar == '=') {
        waveformDisplay_.zoomIn();
        return true;
    }
    if (textChar == '-') {
        waveformDisplay_.zoomOut();
        return true;
    }
    if (key.getModifiers().isShiftDown()) {
        if (textChar == 'H' || textChar == 'h') {
            waveformDisplay_.scrollLeft();
            return true;
        }
        if (textChar == 'L' || textChar == 'l') {
            waveformDisplay_.scrollRight();
            return true;
        }
    }

    // Load sample command simulation
    if (textChar == 'o' || textChar == 'O') {
        loadSample();
        return true;
    }

    // Value adjustment
    if (key.getModifiers().isAltDown()) {
        if (keyCode == juce::KeyPress::upKey) {
            adjustValue(1);
            return true;
        }
        if (keyCode == juce::KeyPress::downKey) {
            adjustValue(-1);
            return true;
        }
    }

    // Left/right for non-grid adjustments
    if (keyCode == juce::KeyPress::leftKey) {
        adjustValue(-1);
        return true;
    }
    if (keyCode == juce::KeyPress::rightKey) {
        adjustValue(1);
        return true;
    }

    return false;
}

void SamplerScreen::navigate(int /*dx*/, int dy) {
    int numRows = static_cast<int>(RowType::COUNT);
    currentRow_ = (currentRow_ + dy + numRows) % numRows;
    repaint();
}

void SamplerScreen::loadSample() {
    auto chooser = std::make_unique<juce::FileChooser>(
        "Load Sample",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "*.wav;*.aiff;*.aif;*.mp3;*.flac"
    );

    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser = std::move(chooser)](const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file.existsAsFile() && audioEngine_) {
                if (auto* sampler = audioEngine_->getSamplerProcessor(currentInstrument_)) {
                    if (sampler->loadSample(file)) {
                        updateWaveformDisplay();
                        repaint();
                    }
                }
            }
        }
    );
}

void SamplerScreen::adjustValue(int delta) {
    if (!project_) return;

    auto row = static_cast<RowType>(currentRow_);
    float current = getRowValue(row);
    float step = 0.01f;

    if (row == RowType::RootNote) {
        step = 1.0f / 127.0f;
    }

    setRowValue(row, current + delta * step);
    repaint();
}

float SamplerScreen::getRowValue(RowType row) const {
    if (!project_) return 0.0f;

    const auto& params = project_->getInstrument(currentInstrument_).getSamplerParams();
    const auto& sends = project_->getInstrument(currentInstrument_).getSends();

    switch (row) {
        case RowType::Sample: return 0.0f;
        case RowType::RootNote: return params.rootNote / 127.0f;
        case RowType::FilterCutoff: return params.filter.cutoff;
        case RowType::FilterResonance: return params.filter.resonance;
        case RowType::AmpAttack: return params.ampEnvelope.attack;
        case RowType::AmpDecay: return params.ampEnvelope.decay;
        case RowType::AmpSustain: return params.ampEnvelope.sustain;
        case RowType::AmpRelease: return params.ampEnvelope.release;
        case RowType::FilterEnvAmount: return (params.filterEnvAmount + 1.0f) / 2.0f;
        case RowType::SendReverb: return sends.reverb;
        case RowType::SendDelay: return sends.delay;
        case RowType::SendChorus: return sends.chorus;
        case RowType::SendDrive: return sends.drive;
        default: return 0.0f;
    }
}

void SamplerScreen::setRowValue(RowType row, float value) {
    if (!project_) return;

    value = juce::jlimit(0.0f, 1.0f, value);
    auto& params = project_->getInstrument(currentInstrument_).getSamplerParams();
    auto& sends = project_->getInstrument(currentInstrument_).getSends();

    switch (row) {
        case RowType::Sample: break;
        case RowType::RootNote: params.rootNote = static_cast<int>(value * 127); break;
        case RowType::FilterCutoff: params.filter.cutoff = value; break;
        case RowType::FilterResonance: params.filter.resonance = value; break;
        case RowType::AmpAttack: params.ampEnvelope.attack = value; break;
        case RowType::AmpDecay: params.ampEnvelope.decay = value; break;
        case RowType::AmpSustain: params.ampEnvelope.sustain = value; break;
        case RowType::AmpRelease: params.ampEnvelope.release = value; break;
        case RowType::FilterEnvAmount: params.filterEnvAmount = value * 2.0f - 1.0f; break;
        case RowType::SendReverb: sends.reverb = value; break;
        case RowType::SendDelay: sends.delay = value; break;
        case RowType::SendChorus: sends.chorus = value; break;
        case RowType::SendDrive: sends.drive = value; break;
        default: break;
    }
}

juce::String SamplerScreen::getRowName(RowType row) const {
    switch (row) {
        case RowType::Sample: return "Sample";
        case RowType::RootNote: return "Root Note";
        case RowType::FilterCutoff: return "Filter Cutoff";
        case RowType::FilterResonance: return "Filter Resonance";
        case RowType::AmpAttack: return "Amp Attack";
        case RowType::AmpDecay: return "Amp Decay";
        case RowType::AmpSustain: return "Amp Sustain";
        case RowType::AmpRelease: return "Amp Release";
        case RowType::FilterEnvAmount: return "Filter Env Amt";
        case RowType::SendReverb: return "Send Reverb";
        case RowType::SendDelay: return "Send Delay";
        case RowType::SendChorus: return "Send Chorus";
        case RowType::SendDrive: return "Send Drive";
        default: return "";
    }
}

juce::String SamplerScreen::getRowValueString(RowType row) const {
    if (!project_) return "";

    const auto& params = project_->getInstrument(currentInstrument_).getSamplerParams();

    switch (row) {
        case RowType::Sample:
            return params.sample.path.empty() ? "(no sample)" : juce::File(params.sample.path).getFileName();
        case RowType::RootNote: {
            static const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
            int note = params.rootNote;
            return juce::String(noteNames[note % 12]) + juce::String(note / 12 - 1);
        }
        default:
            return juce::String(getRowValue(row), 2);
    }
}

} // namespace ui
```

**Step 3: Add to CMakeLists.txt**

Add after `src/ui/WaveformDisplay.cpp`:

```cmake
    src/ui/SamplerScreen.cpp
```

**Step 4: Build to verify compilation**

Run: `cd /Users/jamesmassey/ai-dev/vitracker/.worktrees/sampler/build && cmake --build . --parallel 2>&1 | tail -15`
Expected: `[100%] Built target Vitracker`

**Step 5: Commit**

```bash
git add src/ui/SamplerScreen.h src/ui/SamplerScreen.cpp CMakeLists.txt
git commit -m "feat: add SamplerScreen UI with waveform display"
```

---

## Task 10: Integrate SamplerScreen into App

**Files:**
- Modify: `src/App.cpp`
- Modify: `src/ui/InstrumentScreen.h`
- Modify: `src/ui/InstrumentScreen.cpp`

**Step 1: Make InstrumentScreen type-aware**

The InstrumentScreen should detect instrument type and delegate to SamplerScreen when appropriate. For now, add a simple approach where InstrumentScreen shows different content based on type.

In `src/ui/InstrumentScreen.h`, add:

```cpp
#include "../audio/SamplerInstrument.h"
```

Add member:

```cpp
    audio::AudioEngine* audioEngine_ = nullptr;
```

Add method:

```cpp
    void setAudioEngine(audio::AudioEngine* engine) { audioEngine_ = engine; }
```

**Step 2: In InstrumentScreen.cpp, route to sampler handling**

Add at the start of `paint()`:

```cpp
    if (project_ && project_->getInstrument(currentInstrument_).getType() == model::InstrumentType::Sampler) {
        paintSamplerUI(g);
        return;
    }
```

Add new method `paintSamplerUI`:

```cpp
void InstrumentScreen::paintSamplerUI(juce::Graphics& g) {
    // Simplified sampler UI - full implementation would use SamplerScreen
    auto bounds = getLocalBounds();
    g.fillAll(juce::Colour(0xFF1E1E1E));

    g.setColour(juce::Colours::white);
    g.setFont(16.0f);

    auto& inst = project_->getInstrument(currentInstrument_);
    g.drawText("SAMPLER - " + juce::String(inst.getName()),
               bounds.removeFromTop(28), juce::Justification::centredLeft);

    g.setFont(14.0f);
    g.drawText("Press 'o' to load a sample",
               bounds, juce::Justification::centred);
}
```

**Step 3: Handle sampler key input in InstrumentScreen**

In `handleKey()`, add at the start:

```cpp
    if (project_ && project_->getInstrument(currentInstrument_).getType() == model::InstrumentType::Sampler) {
        return handleSamplerKey(key, isEditMode);
    }
```

Add new method:

```cpp
bool InstrumentScreen::handleSamplerKey(const juce::KeyPress& key, bool /*isEditMode*/) {
    auto textChar = key.getTextCharacter();

    if (textChar == 'o' || textChar == 'O') {
        // Load sample
        auto chooser = std::make_unique<juce::FileChooser>(
            "Load Sample",
            juce::File::getSpecialLocation(juce::File::userHomeDirectory),
            "*.wav;*.aiff;*.aif"
        );

        auto* chooserPtr = chooser.get();
        chooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser = std::move(chooser)](const juce::FileChooser& fc) {
                auto file = fc.getResult();
                if (file.existsAsFile() && audioEngine_) {
                    if (auto* sampler = audioEngine_->getSamplerProcessor(currentInstrument_)) {
                        sampler->setInstrument(&project_->getInstrument(currentInstrument_));
                        sampler->loadSample(file);
                        repaint();
                    }
                }
            }
        );
        return true;
    }

    return false;
}
```

**Step 4: Wire up audioEngine in App.cpp**

In `App.cpp`, in the constructor where InstrumentScreen is set up, add:

```cpp
    instrumentScreen->setAudioEngine(&audioEngine_);
```

**Step 5: Build to verify compilation**

Run: `cd /Users/jamesmassey/ai-dev/vitracker/.worktrees/sampler/build && cmake --build . --parallel 2>&1 | tail -15`
Expected: `[100%] Built target Vitracker`

**Step 6: Commit**

```bash
git add src/ui/InstrumentScreen.h src/ui/InstrumentScreen.cpp src/App.cpp
git commit -m "feat: integrate sampler UI into InstrumentScreen"
```

---

## Task 11: Add Command to Create Sampler Instrument

**Files:**
- Modify: `src/input/KeyHandler.cpp`
- Modify: `src/App.cpp`

**Step 1: Add :sampler command**

In `src/input/KeyHandler.cpp`, in `executeCommand()`, add:

```cpp
    else if (command == "sampler") {
        if (onCreateSampler) onCreateSampler();
    }
```

In `src/input/KeyHandler.h`, add callback:

```cpp
    std::function<void()> onCreateSampler;
```

**Step 2: Implement in App.cpp**

In `App.cpp`, in the constructor where keyHandler_ callbacks are set, add:

```cpp
    keyHandler_->onCreateSampler = [this]() {
        // Find first unused instrument slot or use current
        int slot = -1;
        for (int i = 0; i < 128; ++i) {
            if (project_.getInstrument(i).getName() == "Instrument " + std::to_string(i)) {
                slot = i;
                break;
            }
        }
        if (slot == -1) slot = 0;

        project_.getInstrument(slot).setType(model::InstrumentType::Sampler);
        project_.getInstrument(slot).setName("Sampler " + std::to_string(slot));

        // Switch to instrument screen showing this instrument
        if (auto* instScreen = dynamic_cast<ui::InstrumentScreen*>(screens_[4].get())) {
            instScreen->setCurrentInstrument(slot);
        }
        switchScreen(5);
    };
```

**Step 3: Build to verify compilation**

Run: `cd /Users/jamesmassey/ai-dev/vitracker/.worktrees/sampler/build && cmake --build . --parallel 2>&1 | tail -15`
Expected: `[100%] Built target Vitracker`

**Step 4: Commit**

```bash
git add src/input/KeyHandler.h src/input/KeyHandler.cpp src/App.cpp
git commit -m "feat: add :sampler command to create sampler instrument"
```

---

## Task 12: Test Sampler End-to-End

**Files:** None (manual testing)

**Step 1: Build and run**

```bash
cd /Users/jamesmassey/ai-dev/vitracker/.worktrees/sampler/build
cmake --build . --parallel
./Vitracker_artefacts/Debug/Vitracker.app/Contents/MacOS/Vitracker
```

**Step 2: Test flow**

1. Press `:` to enter command mode
2. Type `sampler` and press Enter
3. Verify instrument screen shows "SAMPLER - Sampler 0"
4. Press `o` to open file chooser
5. Select a WAV file
6. Play notes on keyboard - verify pitched playback
7. Press `5` to go to instrument screen, verify sample name shown

**Step 3: Document any bugs found**

If bugs are found, create a follow-up task list.

**Step 4: Final commit**

```bash
git add -A
git commit -m "feat: complete Phase 1 sampler implementation"
```

---

## Summary

Phase 1 delivers:
- InstrumentType enum (Plaits, Sampler, Slicer)
- ADSR envelope DSP
- SamplerParams data structure
- SamplerVoice with interpolated playback
- SamplerInstrument with voice management
- AudioEngine integration
- WaveformDisplay component
- SamplerScreen UI
- `:sampler` command
- Sample loading via file chooser

Next: Phase 2 (Slicer) builds on this foundation.
