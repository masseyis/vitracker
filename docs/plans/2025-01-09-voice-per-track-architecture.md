# Voice-Per-Track Architecture Design

**Date:** 2025-01-09
**Status:** Approved for implementation

## Overview

Refactor audio architecture from polyphonic instruments to voice-per-track model, aligning with traditional tracker design. Each track owns a single voice instance, enabling independent tracker FX per track and eliminating chord/arpeggio interference issues.

## Problem Statement

Current architecture has tracker FX at instrument level, but tracks are independent. This causes:
- Chords (multiple tracks, same instrument) share FX state → OFF command stops all notes
- Arpeggio on one track affects all tracks using that instrument
- Last-triggered-note-wins creates unpredictable behavior
- Polyphony settings confusing in tracker context

## Solution: Voice-Per-Track Model

### Core Architecture

**Voice Interface** - Synthesis abstraction
```cpp
class Voice {
public:
    virtual ~Voice() = default;

    // Note control
    virtual void noteOn(int note, float velocity) = 0;
    virtual void noteOff() = 0;

    // Audio rendering with tracker FX modulation
    virtual void process(float* outL, float* outR, int numSamples,
                        float pitchMod = 0.0f,    // Semitones (POR/VIB)
                        float cutoffMod = 0.0f,   // 0-1 (CUT command)
                        float volumeMod = 1.0f,   // 0-1 (VOL command)
                        float panMod = 0.5f) = 0; // 0-1 (PAN command)

    // State queries
    virtual bool isActive() const = 0;
    virtual int getCurrentNote() const = 0;
};
```

**Track Structure** - Sequencing + FX
```cpp
struct Track {
    std::unique_ptr<Voice> voice;          // Owned voice instance
    UniversalTrackerFX trackerFX;          // FX processor
    int currentInstrumentIndex = -1;       // Which instrument params to use
    bool hasPendingFX = false;

    void triggerNote(int note, float vel, const model::Step& step,
                    InstrumentProcessor* instrument);
    void process(float* outL, float* outR, int numSamples,
                InstrumentProcessor* instrument);
};
```

**Instrument as Factory** - Parameters + voice creation
```cpp
class InstrumentProcessor {
    virtual std::unique_ptr<Voice> createVoice() = 0;
    virtual void updateVoiceParameters(Voice* voice) = 0;
    // ... existing parameter getters/setters
};
```

### Responsibility Split

| Component | Owns |
|-----------|------|
| **Voice** | Instrument-specific synthesis, envelopes, internal LFOs, oscillator phase |
| **Track** | Tracker FX state, calculates per-sample modulation (pitch/cutoff/volume/pan) |
| **Instrument** | Parameter storage, voice factory, UI parameter interface |

### Track Lifecycle

**Note Triggering:**
```cpp
void Track::triggerNote(int note, float velocity, const model::Step& step,
                       InstrumentProcessor* instrument) {
    // Create new voice if instrument changed
    if (!voice || currentInstrumentIndex != instrument->getIndex()) {
        voice = instrument->createVoice();
        currentInstrumentIndex = instrument->getIndex();
    }

    // Stop previous note if active
    if (voice && voice->isActive()) {
        voice->noteOff();
    }

    // Start tracker FX
    trackerFX.triggerNote(note, velocity, step);
    hasPendingFX = true;
}
```

**Audio Processing:**
```cpp
void Track::process(float* outL, float* outR, int numSamples,
                   InstrumentProcessor* instrument) {
    if (!voice || !hasPendingFX) return;

    // Callbacks for tracker FX timing events
    auto onNoteOn = [&](int note, float vel) {
        if (voice->isActive()) voice->noteOff();
        instrument->updateVoiceParameters(voice.get());
        voice->noteOn(note, vel);
    };

    auto onNoteOff = [&]() {
        if (voice) voice->noteOff();
    };

    // Process FX timing (handles DLY, RET, CUT, OFF, ARP)
    float pitch = trackerFX.process(numSamples, onNoteOn, onNoteOff);

    // Render with modulation
    if (voice && voice->isActive()) {
        float pitchMod = pitch - voice->getCurrentNote();
        voice->process(outL, outR, numSamples, pitchMod);
    }

    if (pitch < 0 && !trackerFX.isActive()) {
        hasPendingFX = false;
    }
}
```

## Instrument Migration

### Voice Implementations

Each instrument type provides single-voice implementation:

- **PlaitsVoice** - Extract from VoiceAllocator, single plaits::Voice instance
- **DX7Voice** - Single Dx7Note instance, no voice array
- **VASynthVoice** - Already single-voice internally, simple extraction
- **SamplerVoice** - Single sample playback, envelope
- **SlicerVoice** - Single slice playback, envelope

### Instrument Refactoring

**Remove from instruments:**
- VoiceAllocator / voice arrays
- UniversalTrackerFX / TrackerFX
- hasPendingFX_ / lastArpNote_
- Polyphony settings and management
- noteOn/noteOff/process methods

**Keep in instruments:**
- All parameter storage (engine, timbre, harmonics, etc.)
- Parameter getters/setters for UI
- Serialization (getState/setState)

**Add to instruments:**
```cpp
std::unique_ptr<Voice> createVoice() override {
    auto voice = std::make_unique<PlaitsVoice>();
    voice->setSampleRate(sampleRate_);
    updateVoiceParameters(voice.get());
    return voice;
}

void updateVoiceParameters(Voice* v) override {
    auto* pv = static_cast<PlaitsVoice*>(v);
    pv->setEngine(engine_);
    pv->setTimbre(timbre_);
    // ... all parameters
}
```

### Migration Order

1. Create Voice base class
2. VASynth (simplest - already single voice)
3. Sampler & Slicer (similar architecture)
4. Plaits (extract from VoiceAllocator)
5. DX7 (most complex - extract from polyphonic structure)

## Gain Staging

### Simplified Master Gain

With fixed 16 voices maximum, single master gain compensation:

```cpp
void AudioEngine::getNextAudioBlock(...) {
    // Clear output
    std::fill_n(outputL, numSamples, 0.0f);
    std::fill_n(outputR, numSamples, 0.0f);

    // Each track renders into output
    for (int t = 0; t < NUM_TRACKS; ++t) {
        tracks_[t].process(outputL, outputR, numSamples, ...);
    }

    // Apply master gain compensation (16 tracks max)
    constexpr float masterGain = 0.5f;  // Headroom for 16 tracks
    for (int i = 0; i < numSamples; ++i) {
        outputL[i] *= masterGain;
        outputR[i] *= masterGain;
    }

    // Pass to effects chain and channel strips
    effects_.process(outputL, outputR, numSamples);
}
```

### Per-Voice Output Requirements

Each Voice::process() must output in ±1.0 range before master gain:

- **PlaitsVoice**: Remove VoiceAllocator's 0.7/voiceCount scaling
- **DX7Voice**: Remove 0.5 * (1/voiceCount) scaling
- **VASynthVoice**: Verify output levels, normalize to ±1.0
- **SamplerVoice**: Verify sample playback gain, ensure no clipping at unity
- **SlicerVoice**: Verify slice playback gain, normalize like Sampler

## Backwards Compatibility

**Project Files:** Existing step data unchanged `{note, instrument, fx1, fx2, fx3}`

**Instrument References:** Track uses `instruments_[index]` to create/update voice

**Polyphony Settings:** Removed from UI and instrument serialization

**Migration:** Load existing project, verify same sound output

## Benefits

1. **Natural tracker model**: Polyphony = track count (standard in trackers)
2. **Independent FX per track**: Chords with different FX work correctly
3. **Predictable behavior**: No voice stealing, no last-wins conflicts
4. **Simpler gain staging**: Fixed 16-voice maximum, single compensation point
5. **Cleaner code**: Separation of synthesis, sequencing, parameters
6. **Easier to extend**: New instruments just implement Voice interface

## Testing Strategy

1. **Unit tests**: Each Voice implementation tested independently
2. **Integration tests**: Track + Voice + TrackerFX combinations
3. **Migration validation**: Load existing project, verify same output
4. **FX regression tests**: All 9 FX types (ARP, POR, VIB, VOL, PAN, DLY, RET, CUT, OFF)
5. **Chord tests**: Verify independent FX per track in chords
6. **Gain tests**: All instruments at 16-voice polyphony, measure peak levels

## Implementation Files

### Files to Create
- `src/audio/Voice.h` - Voice interface base class
- `src/audio/PlaitsVoice.h/cpp` - Extract from PlaitsInstrument
- `src/audio/DX7Voice.h/cpp` - Extract from DX7Instrument
- `src/audio/VASynthVoice.h/cpp` - Extract from VASynthInstrument
- `src/audio/SamplerVoice.h/cpp` - Extract from SamplerInstrument
- `src/audio/SlicerVoice.h/cpp` - Extract from SlicerInstrument

### Files to Modify
- `src/audio/AudioEngine.h/cpp` - Add Track struct, orchestrate tracks
- `src/audio/InstrumentProcessor.h` - Add createVoice/updateVoiceParameters
- `src/audio/PlaitsInstrument.h/cpp` - Remove VoiceAllocator, TrackerFX, polyphony
- `src/audio/DX7Instrument.h/cpp` - Remove voice array, TrackerFX, polyphony
- `src/audio/VASynthInstrument.h/cpp` - Remove TrackerFX
- `src/audio/SamplerInstrument.h/cpp` - Remove TrackerFX
- `src/audio/SlicerInstrument.h/cpp` - Remove TrackerFX
- `src/ui/InstrumentScreen.cpp` - Remove polyphony UI controls

## Gain Staging Validation Checklist

- [ ] PlaitsVoice: Remove VoiceAllocator gain scaling, verify ±1.0 output
- [ ] DX7Voice: Remove per-voice scaling, verify ±1.0 output
- [ ] VASynthVoice: Check and normalize output levels
- [ ] SamplerVoice: Check and normalize sample playback gain
- [ ] SlicerVoice: Check and normalize slice playback gain
- [ ] Master output: Apply 0.5x gain for 16-track headroom
- [ ] Verification: Test all 5 instruments at 16-voice polyphony, measure peaks

## Success Criteria

- [ ] All 5 instrument types implement Voice interface
- [ ] Each track has independent FX state
- [ ] Chords (3 tracks, same instrument) with different FX work correctly
- [ ] Arpeggio on one track doesn't affect other tracks
- [ ] OFF command only affects its own track's note
- [ ] No distortion with 16 tracks active
- [ ] Existing projects load and sound identical
- [ ] All tracker FX types work correctly
- [ ] UI polyphony controls removed
