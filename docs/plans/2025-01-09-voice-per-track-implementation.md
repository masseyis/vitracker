# Voice-Per-Track Architecture Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Refactor from polyphonic instruments to voice-per-track model where each track owns a single voice instance, enabling independent tracker FX per track.

**Architecture:** Create Voice interface for all instrument types, move TrackerFX from instruments to Track struct in AudioEngine, remove polyphony management from instruments. Each track owns one voice, instruments become parameter storage + voice factories.

**Tech Stack:** C++17, JUCE, CMake, existing instrument DSP (Plaits, DX7, VASynth, Sampler, Slicer)

---

## Task 1: Create Voice Base Interface

**Files:**
- Create: `src/audio/Voice.h`

**Step 1: Create Voice.h with interface**

```cpp
#pragma once

namespace audio {

// Base interface for all voice types
// Voice owns synthesis state (envelopes, oscillator phase, internal LFOs)
// Track owns sequencing state (TrackerFX, modulation calculation)
class Voice
{
public:
    virtual ~Voice() = default;

    // Note control
    virtual void noteOn(int note, float velocity) = 0;
    virtual void noteOff() = 0;

    // Audio rendering with tracker FX modulation
    // pitchMod: semitones offset from current note (for POR/VIB)
    // cutoffMod: 0-1 normalized (for CUT command, ignored if no filter)
    // volumeMod: 0-1 normalized (for VOL command)
    // panMod: 0-1 normalized, 0=left, 0.5=center, 1=right (for PAN command)
    virtual void process(float* outL, float* outR, int numSamples,
                        float pitchMod = 0.0f,
                        float cutoffMod = 0.0f,
                        float volumeMod = 1.0f,
                        float panMod = 0.5f) = 0;

    // State queries
    virtual bool isActive() const = 0;
    virtual int getCurrentNote() const = 0;

    // Sample rate management
    virtual void setSampleRate(double sampleRate) = 0;
};

} // namespace audio
```

**Step 2: Verify it compiles**

Run: `cmake --build build --config Debug 2>&1 | grep -i error || echo "SUCCESS"`
Expected: "SUCCESS"

**Step 3: Commit**

```bash
git add src/audio/Voice.h
git commit -m "feat: Add Voice base interface for single-voice synthesis"
```

---

## Task 2: Create VASynthVoice Implementation

**Files:**
- Create: `src/audio/VASynthVoice.h`
- Create: `src/audio/VASynthVoice.cpp`
- Reference: `src/audio/VASynthInstrument.h` (for current implementation)

**Step 1: Create VASynthVoice.h**

```cpp
#pragma once

#include "Voice.h"
#include "../dsp/VAFilter.h"
#include "../dsp/Envelope.h"
#include <cmath>

namespace audio {

// Single-voice VA synthesizer
class VASynthVoice : public Voice
{
public:
    VASynthVoice();
    ~VASynthVoice() override = default;

    // Voice interface
    void noteOn(int note, float velocity) override;
    void noteOff() override;
    void process(float* outL, float* outR, int numSamples,
                float pitchMod, float cutoffMod,
                float volumeMod, float panMod) override;
    bool isActive() const override { return active_; }
    int getCurrentNote() const override { return currentNote_; }
    void setSampleRate(double sampleRate) override;

    // Parameter setters (called by VASynthInstrument)
    void setWaveform(int waveform) { waveform_ = waveform; }
    void setAttack(float attack) { attack_ = attack; }
    void setDecay(float decay) { decay_ = decay; }
    void setSustain(float sustain) { sustain_ = sustain; }
    void setRelease(float release) { release_ = release; }
    void setCutoff(float cutoff) { cutoff_ = cutoff; }
    void setResonance(float resonance) { resonance_ = resonance; }

private:
    double sampleRate_ = 48000.0;
    bool active_ = false;
    int currentNote_ = -1;
    float velocity_ = 0.0f;
    float phase_ = 0.0f;

    // Parameters
    int waveform_ = 0;  // 0=saw, 1=square, 2=triangle, 3=sine
    float attack_ = 0.01f;
    float decay_ = 0.1f;
    float sustain_ = 0.7f;
    float release_ = 0.2f;
    float cutoff_ = 1.0f;
    float resonance_ = 0.0f;

    // DSP components
    dsp::Envelope envelope_;
    dsp::VAFilter filter_;

    float renderOscillator(float frequency);
};

} // namespace audio
```

**Step 2: Create VASynthVoice.cpp**

```cpp
#include "VASynthVoice.h"
#include <cmath>
#include <algorithm>

namespace audio {

VASynthVoice::VASynthVoice()
{
    filter_.setMode(dsp::VAFilter::Mode::LowPass24);
}

void VASynthVoice::setSampleRate(double sampleRate)
{
    sampleRate_ = sampleRate;
    filter_.setSampleRate(sampleRate);
    envelope_.setSampleRate(sampleRate);
}

void VASynthVoice::noteOn(int note, float velocity)
{
    currentNote_ = note;
    velocity_ = velocity;
    phase_ = 0.0f;
    active_ = true;

    // Trigger envelope
    envelope_.setADSR(attack_, decay_, sustain_, release_);
    envelope_.noteOn();
}

void VASynthVoice::noteOff()
{
    envelope_.noteOff();
}

void VASynthVoice::process(float* outL, float* outR, int numSamples,
                          float pitchMod, float cutoffMod,
                          float volumeMod, float panMod)
{
    if (!active_) return;

    for (int i = 0; i < numSamples; ++i) {
        // Calculate frequency with pitch modulation
        float midiNote = currentNote_ + pitchMod;
        float frequency = 440.0f * std::pow(2.0f, (midiNote - 69.0f) / 12.0f);

        // Generate oscillator
        float sample = renderOscillator(frequency);

        // Apply envelope
        float env = envelope_.process();
        sample *= env * velocity_ * volumeMod;

        // Apply filter with cutoff modulation
        float modulatedCutoff = std::clamp(cutoff_ + cutoffMod - 0.5f, 0.0f, 1.0f);
        filter_.setCutoff(modulatedCutoff);
        filter_.setResonance(resonance_);
        sample = filter_.process(sample);

        // Apply panning
        float leftGain = std::sqrt(1.0f - panMod);
        float rightGain = std::sqrt(panMod);

        outL[i] += sample * leftGain;
        outR[i] += sample * rightGain;

        // Check if voice finished
        if (env == 0.0f && !envelope_.isActive()) {
            active_ = false;
        }
    }
}

float VASynthVoice::renderOscillator(float frequency)
{
    float sample = 0.0f;
    float increment = frequency / static_cast<float>(sampleRate_);

    switch (waveform_) {
        case 0: // Sawtooth
            sample = 2.0f * phase_ - 1.0f;
            break;
        case 1: // Square
            sample = phase_ < 0.5f ? 1.0f : -1.0f;
            break;
        case 2: // Triangle
            sample = 4.0f * std::abs(phase_ - 0.5f) - 1.0f;
            break;
        case 3: // Sine
            sample = std::sin(2.0f * M_PI * phase_);
            break;
    }

    phase_ += increment;
    if (phase_ >= 1.0f) phase_ -= 1.0f;

    return sample;
}

} // namespace audio
```

**Step 3: Add to CMakeLists.txt**

In `CMakeLists.txt`, find the `add_executable(Vitracker` section and add:
```cmake
src/audio/VASynthVoice.cpp
```

**Step 4: Build and verify**

Run: `cmake --build build --config Debug 2>&1 | grep -i error || echo "SUCCESS"`
Expected: "SUCCESS"

**Step 5: Commit**

```bash
git add src/audio/VASynthVoice.h src/audio/VASynthVoice.cpp CMakeLists.txt
git commit -m "feat: Add VASynthVoice single-voice implementation"
```

---

## Task 3: Update InstrumentProcessor Interface

**Files:**
- Modify: `src/audio/InstrumentProcessor.h`

**Step 1: Add Voice factory methods to InstrumentProcessor**

In `src/audio/InstrumentProcessor.h`, add these pure virtual methods after the existing interface:

```cpp
#include "Voice.h"
#include <memory>

// ... existing methods ...

// Voice factory (creates new voice instance)
virtual std::unique_ptr<Voice> createVoice() = 0;

// Update voice with current parameters
virtual void updateVoiceParameters(Voice* voice) = 0;
```

**Step 2: Build and verify (will fail - instruments don't implement yet)**

Run: `cmake --build build --config Debug 2>&1 | grep "pure virtual" | wc -l`
Expected: Should show multiple errors (5 instrument types × 2 methods = 10 errors)

**Step 3: Commit interface change**

```bash
git add src/audio/InstrumentProcessor.h
git commit -m "feat: Add Voice factory methods to InstrumentProcessor interface"
```

---

## Task 4: Stub Implementation for All Instruments

**Files:**
- Modify: `src/audio/VASynthInstrument.h`
- Modify: `src/audio/VASynthInstrument.cpp`
- Modify: `src/audio/SamplerInstrument.h`
- Modify: `src/audio/SamplerInstrument.cpp`
- Modify: `src/audio/SlicerInstrument.h`
- Modify: `src/audio/SlicerInstrument.cpp`
- Modify: `src/audio/PlaitsInstrument.h`
- Modify: `src/audio/PlaitsInstrument.cpp`
- Modify: `src/audio/DX7Instrument.h`
- Modify: `src/audio/DX7Instrument.cpp`

**Step 1: Add stub to VASynthInstrument.h**

```cpp
#include "Voice.h"

// In class VASynthInstrument, add:
std::unique_ptr<Voice> createVoice() override;
void updateVoiceParameters(Voice* voice) override;
```

**Step 2: Add stub to VASynthInstrument.cpp**

```cpp
std::unique_ptr<Voice> VASynthInstrument::createVoice() {
    // TODO: Will implement in later task
    return nullptr;
}

void VASynthInstrument::updateVoiceParameters(Voice* voice) {
    // TODO: Will implement in later task
}
```

**Step 3: Repeat for Sampler, Slicer, Plaits, DX7**

Add same stub declarations in headers and stub implementations in .cpp files.

**Step 4: Build and verify**

Run: `cmake --build build --config Debug 2>&1 | grep -i error || echo "SUCCESS"`
Expected: "SUCCESS"

**Step 5: Commit**

```bash
git add src/audio/*Instrument.h src/audio/*Instrument.cpp
git commit -m "feat: Add stub Voice factory methods to all instruments"
```

---

## Task 5: Implement VASynthInstrument Voice Factory

**Files:**
- Modify: `src/audio/VASynthInstrument.h`
- Modify: `src/audio/VASynthInstrument.cpp`

**Step 1: Include VASynthVoice in VASynthInstrument.cpp**

```cpp
#include "VASynthVoice.h"
```

**Step 2: Implement createVoice()**

Replace the stub in `VASynthInstrument.cpp`:

```cpp
std::unique_ptr<Voice> VASynthInstrument::createVoice() {
    auto voice = std::make_unique<VASynthVoice>();
    voice->setSampleRate(sampleRate_);
    updateVoiceParameters(voice.get());
    return voice;
}
```

**Step 3: Implement updateVoiceParameters()**

```cpp
void VASynthInstrument::updateVoiceParameters(Voice* v) {
    auto* voice = static_cast<VASynthVoice*>(v);
    voice->setWaveform(waveform_);
    voice->setAttack(attack_);
    voice->setDecay(decay_);
    voice->setSustain(sustain_);
    voice->setRelease(release_);
    voice->setCutoff(cutoff_);
    voice->setResonance(resonance_);
}
```

**Step 4: Build and verify**

Run: `cmake --build build --config Debug 2>&1 | grep -i error || echo "SUCCESS"`
Expected: "SUCCESS"

**Step 5: Commit**

```bash
git add src/audio/VASynthInstrument.h src/audio/VASynthInstrument.cpp
git commit -m "feat: Implement VASynthInstrument voice factory"
```

---

## Task 6: Create Track Structure in AudioEngine

**Files:**
- Modify: `src/audio/AudioEngine.h`
- Modify: `src/audio/AudioEngine.cpp`

**Step 1: Add Track struct to AudioEngine.h**

Add after includes, before AudioEngine class:

```cpp
#include "Voice.h"
#include "UniversalTrackerFX.h"

namespace audio {

// Forward declaration
class InstrumentProcessor;

// Track owns a single voice and manages its FX state
struct Track {
    std::unique_ptr<Voice> voice;
    UniversalTrackerFX trackerFX;
    int currentInstrumentIndex = -1;
    bool hasPendingFX = false;

    void triggerNote(int note, float velocity, const model::Step& step,
                    InstrumentProcessor* instrument, double sampleRate, float tempo);
    void releaseNote();
    void process(float* outL, float* outR, int numSamples,
                InstrumentProcessor* instrument);
};
```

**Step 2: Add tracks array to AudioEngine class**

In `AudioEngine` class private members:

```cpp
std::array<Track, NUM_TRACKS> tracks_;
```

**Step 3: Implement Track::triggerNote in AudioEngine.cpp**

At top of file after includes:

```cpp
void Track::triggerNote(int note, float velocity, const model::Step& step,
                       InstrumentProcessor* instrument, double sampleRate, float tempo)
{
    if (!instrument) return;

    // Create new voice if instrument changed
    if (!voice || currentInstrumentIndex != instrument->getIndex()) {
        voice = instrument->createVoice();
        if (voice) {
            voice->setSampleRate(sampleRate);
            currentInstrumentIndex = instrument->getIndex();
        }
    }

    // Stop previous note if active
    if (voice && voice->isActive()) {
        voice->noteOff();
    }

    // Start tracker FX
    trackerFX.setSampleRate(sampleRate);
    trackerFX.setTempo(tempo);
    trackerFX.triggerNote(note, velocity, step);
    hasPendingFX = true;
}

void Track::releaseNote()
{
    if (voice && voice->isActive()) {
        voice->noteOff();
    }
}

void Track::process(float* outL, float* outR, int numSamples,
                   InstrumentProcessor* instrument)
{
    if (!voice || !hasPendingFX || !instrument) return;

    // Callbacks for tracker FX timing events
    auto onNoteOn = [&](int note, float vel) {
        if (voice->isActive()) {
            voice->noteOff();
        }
        instrument->updateVoiceParameters(voice.get());
        voice->noteOn(note, vel);
    };

    auto onNoteOff = [&]() {
        if (voice) {
            voice->noteOff();
        }
    };

    // Process FX timing (handles DLY, RET, CUT, OFF, ARP)
    float pitch = trackerFX.process(numSamples, onNoteOn, onNoteOff);

    // Render with modulation
    if (voice && voice->isActive()) {
        float pitchMod = pitch - voice->getCurrentNote();
        voice->process(outL, outR, numSamples, pitchMod, 0.0f, 1.0f, 0.5f);
    }

    if (pitch < 0.0f && !trackerFX.isActive()) {
        hasPendingFX = false;
    }
}
```

**Step 4: Build and verify**

Run: `cmake --build build --config Debug 2>&1 | grep -i error || echo "SUCCESS"`
Expected: "SUCCESS"

**Step 5: Commit**

```bash
git add src/audio/AudioEngine.h src/audio/AudioEngine.cpp
git commit -m "feat: Add Track structure to AudioEngine"
```

---

## Task 7: Wire Up VASynth Track Processing

**Files:**
- Modify: `src/audio/AudioEngine.cpp`

**Step 1: Update triggerNote to use Track for VASynth**

Find the VASynth section in `AudioEngine::triggerNote` (around line 134):

```cpp
else if (instrument->getType() == model::InstrumentType::VASynth)
{
    // Use track-based processing
    if (auto* vaSynth = getVASynthProcessor(instrumentIndex))
    {
        tracks_[track].triggerNote(note, velocity, step, vaSynth, sampleRate_, getCurrentTempo());
        trackInstruments_[track] = instrumentIndex;
        trackNotes_[track] = note;
    }
}
```

**Step 2: Add getCurrentTempo helper**

In `AudioEngine` class private section:

```cpp
float getCurrentTempo() const {
    return project_ ? project_->getTempo() : 120.0f;
}
```

**Step 3: Update getNextAudioBlock to process VASynth tracks**

In `AudioEngine::getNextAudioBlock`, after existing processing, before effects:

```cpp
// Process track-based instruments (VASynth for now)
for (int t = 0; t < NUM_TRACKS; ++t) {
    int instIdx = trackInstruments_[t];
    if (instIdx >= 0 && instIdx < NUM_INSTRUMENTS) {
        auto* inst = project_->getInstrument(instIdx);
        if (inst && inst->getType() == model::InstrumentType::VASynth) {
            auto* vaSynth = getVASynthProcessor(instIdx);
            if (vaSynth) {
                tracks_[t].process(outputL, outputR, numSamplesToProcess, vaSynth);
            }
        }
    }
}
```

**Step 4: Build and verify**

Run: `cmake --build build --config Debug 2>&1 | grep -i error || echo "SUCCESS"`
Expected: "SUCCESS"

**Step 5: Test manually**

Run app, create VASynth note with ARP, verify it works independently per track.

**Step 6: Commit**

```bash
git add src/audio/AudioEngine.cpp src/audio/AudioEngine.h
git commit -m "feat: Wire VASynth to track-based processing"
```

---

## Task 8: Create Remaining Voice Implementations

Follow same pattern as VASynthVoice for each instrument:

### Task 8a: SamplerVoice

**Files:**
- Create: `src/audio/SamplerVoice.h`
- Create: `src/audio/SamplerVoice.cpp`

Extract single-voice playback from SamplerInstrument. Key members:
- `Sample* currentSample_`
- `int playbackPosition_`
- `dsp::Envelope envelope_`
- Handle loop points, pitch shifting for `pitchMod`

### Task 8b: SlicerVoice

**Files:**
- Create: `src/audio/SlicerVoice.h`
- Create: `src/audio/SlicerVoice.cpp`

Extract single-voice slice playback from SlicerInstrument. Similar to SamplerVoice but:
- Plays slice from `slices_[sliceIndex]`
- Maps MIDI note to slice index

### Task 8c: PlaitsVoice

**Files:**
- Create: `src/audio/PlaitsVoice.h`
- Create: `src/audio/PlaitsVoice.cpp`

Extract from VoiceAllocator. Key members:
- `plaits::Voice plaitsVoice_`
- `plaits::Patch patch_`
- `dsp::Envelope envelope_`
- Resampling for host sample rate

### Task 8d: DX7Voice

**Files:**
- Create: `src/audio/DX7Voice.h`
- Create: `src/audio/DX7Voice.cpp`

Extract from DX7Instrument voice array. Key members:
- `std::unique_ptr<Dx7Note> dx7Note_`
- `uint8_t patch_[156]`
- Handle Q24 int32 to float conversion
- Apply 0.5x gain scaling

**For each voice:**

1. Create .h with Voice interface
2. Create .cpp with implementation
3. Add to CMakeLists.txt
4. Build and verify
5. Commit with `feat: Add [Type]Voice implementation`

---

## Task 9: Implement Voice Factories for All Instruments

**Files:**
- Modify: `src/audio/SamplerInstrument.cpp`
- Modify: `src/audio/SlicerInstrument.cpp`
- Modify: `src/audio/PlaitsInstrument.cpp`
- Modify: `src/audio/DX7Instrument.cpp`

For each instrument, replace the stub createVoice/updateVoiceParameters with real implementations following VASynthInstrument pattern.

**Example for PlaitsInstrument:**

```cpp
std::unique_ptr<Voice> PlaitsInstrument::createVoice() {
    auto voice = std::make_unique<PlaitsVoice>();
    voice->setSampleRate(sampleRate_);
    updateVoiceParameters(voice.get());
    return voice;
}

void PlaitsInstrument::updateVoiceParameters(Voice* v) {
    auto* voice = static_cast<PlaitsVoice*>(v);
    voice->setEngine(engine_);
    voice->setHarmonics(harmonics_);
    voice->setTimbre(timbre_);
    voice->setMorph(morph_);
    voice->setDecay(decay_);
    voice->setLpgColour(lpgColour_);
}
```

Commit each instrument separately.

---

## Task 10: Wire All Instruments to Track Processing

**Files:**
- Modify: `src/audio/AudioEngine.cpp`

**Step 1: Update triggerNote for all instrument types**

For each instrument type (Sampler, Slicer, Plaits, DX7), replace old polyphonic code with:

```cpp
else if (instrument->getType() == model::InstrumentType::Plaits)
{
    if (auto* plaits = getInstrumentProcessor(instrumentIndex))
    {
        tracks_[track].triggerNote(note, velocity, step, plaits, sampleRate_, getCurrentTempo());
        trackInstruments_[track] = instrumentIndex;
        trackNotes_[track] = note;
    }
}
```

**Step 2: Update getNextAudioBlock to process all tracks**

Replace the track processing loop to handle all types:

```cpp
// Process all tracks with track-based architecture
for (int t = 0; t < NUM_TRACKS; ++t) {
    int instIdx = trackInstruments_[t];
    if (instIdx >= 0 && instIdx < NUM_INSTRUMENTS) {
        auto* inst = project_->getInstrument(instIdx);
        if (!inst) continue;

        InstrumentProcessor* processor = nullptr;

        switch (inst->getType()) {
            case model::InstrumentType::VASynth:
                processor = getVASynthProcessor(instIdx);
                break;
            case model::InstrumentType::Sampler:
                processor = getSamplerProcessor(instIdx);
                break;
            case model::InstrumentType::Slicer:
                processor = getSlicerProcessor(instIdx);
                break;
            case model::InstrumentType::Plaits:
                processor = getInstrumentProcessor(instIdx);
                break;
            case model::InstrumentType::DXPreset:
                processor = getDX7Processor(instIdx);
                break;
        }

        if (processor) {
            tracks_[t].process(outputL, outputR, numSamplesToProcess, processor);
        }
    }
}
```

**Step 3: Add master gain compensation**

After track processing:

```cpp
// Apply master gain compensation for 16-track maximum
constexpr float masterGain = 0.5f;
for (int i = 0; i < numSamplesToProcess; ++i) {
    outputL[i] *= masterGain;
    outputR[i] *= masterGain;
}
```

**Step 4: Build and verify**

Run: `cmake --build build --config Debug 2>&1 | grep -i error || echo "SUCCESS"`
Expected: "SUCCESS"

**Step 5: Commit**

```bash
git add src/audio/AudioEngine.cpp
git commit -m "feat: Wire all instruments to track-based processing with master gain"
```

---

## Task 11: Remove Old Polyphony Code from Instruments

**Files:**
- Modify: `src/audio/PlaitsInstrument.h`
- Modify: `src/audio/PlaitsInstrument.cpp`
- Modify: `src/audio/DX7Instrument.h`
- Modify: `src/audio/DX7Instrument.cpp`
- Modify: `src/audio/VASynthInstrument.h`
- Modify: `src/audio/VASynthInstrument.cpp`
- Modify: `src/audio/SamplerInstrument.h`
- Modify: `src/audio/SamplerInstrument.cpp`
- Modify: `src/audio/SlicerInstrument.h`
- Modify: `src/audio/SlicerInstrument.cpp`

**For each instrument:**

**Step 1: Remove from header:**
- `UniversalTrackerFX trackerFX_` or `TrackerFX trackerFX_`
- `bool hasPendingFX_`
- `int lastArpNote_`
- `VoiceAllocator voiceAllocator_` (Plaits)
- `std::array<Voice, MAX_VOICES> voices_` (DX7)
- `int polyphony_` and related methods
- `setPolyphony()` method

**Step 2: Remove from cpp:**
- `noteOn()`, `noteOff()`, `allNotesOff()` implementations
- `noteOnWithFX()` implementation
- `process()` implementation
- All TrackerFX processing code

**Step 3: Keep:**
- All parameter storage (engine, timbre, etc.)
- Parameter getters/setters
- `init()`, `setSampleRate()`
- `getState()`, `setState()`
- `createVoice()`, `updateVoiceParameters()`

**Step 4: Build after each instrument**

Run: `cmake --build build --config Debug 2>&1 | grep -i error || echo "SUCCESS"`

**Step 5: Commit each instrument separately**

```bash
git add src/audio/PlaitsInstrument.h src/audio/PlaitsInstrument.cpp
git commit -m "refactor: Remove polyphony code from PlaitsInstrument"
```

---

## Task 12: Remove Polyphony UI Controls

**Files:**
- Modify: `src/ui/InstrumentScreen.cpp`
- Modify: `src/ui/InstrumentScreen.h`

**Step 1: Find and remove polyphony controls**

Search for "polyphony" or "voices" in InstrumentScreen.cpp and remove:
- Polyphony slider/label components
- Update methods that set polyphony
- Layout code for polyphony controls

**Step 2: Build and verify**

Run: `cmake --build build --config Debug 2>&1 | grep -i error || echo "SUCCESS"`

**Step 3: Test UI**

Launch app, verify instrument screens work without polyphony controls.

**Step 4: Commit**

```bash
git add src/ui/InstrumentScreen.h src/ui/InstrumentScreen.cpp
git commit -m "refactor: Remove polyphony UI controls"
```

---

## Task 13: Gain Staging Validation

**Files:**
- Modify: All *Voice.cpp files as needed

**Step 1: Test each voice at maximum level**

For each Voice implementation, add temporary test to output at full velocity:

```cpp
// Temporary test in main()
auto voice = std::make_unique<PlaitsVoice>();
voice->setSampleRate(48000);
voice->noteOn(60, 1.0f);

float testBuf[1024] = {0};
voice->process(testBuf, testBuf, 1024, 0, 0, 1, 0.5f);

float peak = 0.0f;
for (int i = 0; i < 1024; ++i) {
    peak = std::max(peak, std::abs(testBuf[i]));
}
std::cout << "PlaitsVoice peak: " << peak << std::endl;
// Should be < 1.0
```

**Step 2: Verify all voices output ≤ 1.0**

Run test for each voice type. If any exceed 1.0, add scaling:

```cpp
// In process(), before output
sample *= 0.7f;  // Adjust factor as needed
```

**Step 3: Test 16-voice polyphony**

Create pattern with 16 tracks, all playing same note:
- VASynth: verify no clipping
- Plaits: verify no clipping
- DX7: verify no clipping
- Sampler: verify no clipping
- Slicer: verify no clipping

**Step 4: Document gain factors**

In each Voice .cpp file, add comment explaining gain scaling:

```cpp
// Output scaled to ±1.0 range before master gain
// Master gain (0.5) provides headroom for 16 tracks
```

**Step 5: Commit**

```bash
git add src/audio/*Voice.cpp
git commit -m "fix: Normalize all voice outputs to ±1.0 range"
```

---

## Task 14: Integration Testing

**Files:**
- Test existing project loading
- Test FX independence per track

**Step 1: Load existing project**

Open a project created before this refactor. Verify:
- All instruments load correctly
- Notes play correctly
- Parameters work
- No crashes

**Step 2: Test chord with independent FX**

Create pattern:
```
Track 0: Inst 0 (Plaits), C-5, ARP47
Track 1: Inst 0 (Plaits), E-5, (no FX)
Track 2: Inst 0 (Plaits), G-5, OFF02
```

Verify:
- Track 0 arpeggios independently
- Track 1 plays sustained
- Track 2 cuts at tick 2
- No interference between tracks

**Step 3: Test all FX types**

For each FX (ARP, POR, VIB, VOL, PAN, DLY, RET, CUT, OFF):
- Create test pattern
- Verify FX works correctly
- Verify independent per track

**Step 4: Document any issues found**

Create GitHub issues for any bugs discovered.

**Step 5: Commit any fixes**

```bash
git commit -m "fix: [specific issue found during testing]"
```

---

## Task 15: Update Tests

**Files:**
- Modify: `tests/DX7InstrumentTest.cpp` (if exists)
- Create: `tests/VoiceTest.cpp`
- Create: `tests/TrackTest.cpp`

**Step 1: Update existing tests**

Any tests that directly use instrument `noteOn/process` need updating to use Track or Voice.

**Step 2: Create Voice tests**

Test each Voice implementation:
- noteOn sets active
- noteOff triggers release
- process outputs in range
- Pitch modulation works
- Gain modulation works

**Step 3: Create Track tests**

Test Track integration:
- triggerNote creates voice
- FX processing works
- Instrument switching works
- Multiple notes on same track work

**Step 4: Run all tests**

```bash
build/DX7InstrumentTest
# Add other test executables
```

**Step 5: Commit**

```bash
git add tests/*
git commit -m "test: Update tests for voice-per-track architecture"
```

---

## Task 16: Documentation

**Files:**
- Update: `README.md`
- Update: `docs/plans/2025-01-09-voice-per-track-architecture.md`

**Step 1: Update README**

Add section explaining track-based architecture:

```markdown
## Architecture

Vitracker uses a voice-per-track architecture:
- Each of 16 tracks owns one voice
- Tracks have independent FX processing
- Polyphony = number of tracks used (max 16)
- Instruments are parameter storage + voice factories
```

**Step 2: Mark design document as implemented**

Update status in `docs/plans/2025-01-09-voice-per-track-architecture.md`:

```markdown
**Status:** ✅ Implemented (2025-01-09)
```

Add implementation notes section documenting any deviations from plan.

**Step 3: Commit**

```bash
git add README.md docs/plans/*.md
git commit -m "docs: Update for voice-per-track architecture"
```

---

## Task 17: Final Verification

**Step 1: Clean build**

```bash
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

**Step 2: Run all tests**

```bash
find build -name "*Test" -type f -executable -exec {} \;
```

Expected: All tests pass

**Step 3: Launch app and verify**

- Load existing project - works
- Create new pattern - works
- All 5 instrument types - work
- All 9 FX types - work
- Chords with independent FX - work
- 16-voice polyphony - no distortion

**Step 4: Performance check**

Monitor CPU usage with 16 tracks playing. Should be comparable to before (no major performance regression).

**Step 5: Create summary of changes**

Document:
- Files created (10 Voice implementations)
- Files modified (15+ instrument/engine files)
- Files deleted (none, only code removed)
- Lines of code changed (estimate)

---

## Success Criteria Checklist

- [ ] All 5 instrument types implement Voice interface
- [ ] Each track has independent FX state
- [ ] Chords (3 tracks, same instrument) with different FX work correctly
- [ ] Arpeggio on one track doesn't affect other tracks
- [ ] OFF command only affects its own track's note
- [ ] No distortion with 16 tracks active (master gain 0.5)
- [ ] All voice outputs normalized to ±1.0 range
- [ ] Existing projects load and sound identical
- [ ] All tracker FX types work correctly (ARP, POR, VIB, VOL, PAN, DLY, RET, CUT, OFF)
- [ ] UI polyphony controls removed
- [ ] Tests updated and passing
- [ ] Documentation updated
- [ ] Clean build succeeds
- [ ] No performance regression

---

## Rollback Plan

If critical issues found:

```bash
git checkout feature/tracker-fx
git branch -D feature/voice-per-track
git worktree remove .worktrees/voice-per-track
```

Document issues, revise plan, try again.
