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
