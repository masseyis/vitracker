// VA Synth Voice - Single voice for the VA synthesizer
// Combines 3 oscillators, Moog filter, and ADSR envelopes

#pragma once

#include "Voice.h"
#include "../dsp/va_oscillator.h"
#include "../dsp/va_filter.h"
#include "../model/VAParams.h"
#include <cmath>

namespace audio {

class VASynthVoice : public Voice {
public:
    VASynthVoice() = default;
    ~VASynthVoice() override = default;

    // Voice interface implementation
    void noteOn(int note, float velocity) override;
    void noteOff() override;
    void process(float* outL, float* outR, int numSamples,
                float pitchMod = 0.0f,
                float cutoffMod = 0.0f,
                float volumeMod = 1.0f,
                float panMod = 0.5f) override;
    bool isActive() const override;
    int getCurrentNote() const override { return note_; }
    void setSampleRate(double sampleRate) override;

    // VASynth-specific interface for parameter updates
    void updateParameters(const model::VAParams& params);

private:
    void init(float sampleRate);
    void reset();
    void updateOscParams(const model::VAParams& params);
    void updateOscFrequencies(float baseFreq, const model::VAParams& params);
    void updateFilterParams(const model::VAParams& params);
    void updateEnvelopeParams(const model::VAParams& params);

    // Process a single sample with modulation
    float processSample(const model::VAParams& params, float pitchMod, float cutoffMod);

    float midiNoteToFreq(float note) const;
    float getOctaveMultiplier(int octave) const;
    float getSemitoneMultiplier(int semitones) const;
    float getCentsMultiplier(float cents) const;

    // Member variables
    float sampleRate_ = 48000.0f;

    // Oscillators
    dsp::VAOscillator osc1_;
    dsp::VAOscillator osc2_;
    dsp::VAOscillator osc3_;
    dsp::VAOscillator lfo_;  // LFO uses same oscillator class
    dsp::NoiseGenerator noise_;

    // Filter
    dsp::VAMoogFilter filter_;

    // Envelopes
    dsp::VAEnvelope ampEnv_;
    dsp::VAEnvelope filterEnv_;

    // State
    bool active_ = false;
    int note_ = -1;
    float velocity_ = 0.0f;
    float targetFreq_ = 440.0f;
    float currentFreq_ = 440.0f;
    float glideRate_ = 1.0f;

    // Cached parameters for current processing block
    model::VAParams params_;
};

} // namespace audio
