// VA Synth Voice - Single voice for the VA synthesizer
// Combines 3 oscillators, Moog filter, and ADSR envelopes

#pragma once

#include "../dsp/va_oscillator.h"
#include "../dsp/va_filter.h"
#include "../model/VAParams.h"
#include <cmath>

namespace audio {

class VASynthVoice {
public:
    VASynthVoice() = default;
    ~VASynthVoice() = default;

    void init(float sampleRate) {
        sampleRate_ = sampleRate;
        osc1_.init(sampleRate);
        osc2_.init(sampleRate);
        osc3_.init(sampleRate);
        noise_.reset();
        filter_.init(sampleRate);
        ampEnv_.init(sampleRate);
        filterEnv_.init(sampleRate);
        lfo_.init(sampleRate);
        reset();
    }

    void reset() {
        osc1_.reset();
        osc2_.reset();
        osc3_.reset();
        filter_.reset();
        ampEnv_.reset();
        filterEnv_.reset();
        lfo_.reset();
        active_ = false;
        note_ = -1;
        velocity_ = 0.0f;
        targetFreq_ = 440.0f;
        currentFreq_ = 440.0f;
    }

    void trigger(int midiNote, float velocity, const model::VAParams& params) {
        note_ = midiNote;
        velocity_ = velocity;
        active_ = true;

        // Calculate base frequency
        targetFreq_ = midiNoteToFreq(midiNote);

        // Handle glide
        if (params.glide > 0.0f && currentFreq_ > 0.0f) {
            // Exponential glide
            glideRate_ = 1.0f - std::exp(-1.0f / (params.glide * sampleRate_ * 0.5f));
        } else {
            currentFreq_ = targetFreq_;
            glideRate_ = 1.0f;
        }

        // Update oscillator parameters
        updateOscParams(params);

        // Update filter parameters
        updateFilterParams(params);

        // Update envelope parameters
        updateEnvelopeParams(params);

        // Trigger envelopes
        ampEnv_.trigger();
        filterEnv_.trigger();

        // Reset LFO on note trigger (optional, could be free-running)
        lfo_.reset();
    }

    void release() {
        ampEnv_.release();
        filterEnv_.release();
    }

    bool isActive() const {
        return active_ && ampEnv_.isActive();
    }

    int getNote() const { return note_; }

    // Process a single sample
    float process(const model::VAParams& params) {
        if (!isActive()) {
            active_ = false;
            return 0.0f;
        }

        // Apply glide
        currentFreq_ += (targetFreq_ - currentFreq_) * glideRate_;

        // Process LFO
        float lfoValue = lfo_.process();

        // Apply LFO pitch modulation
        float pitchMod = 1.0f + lfoValue * params.lfo.toPitch * 0.5f;  // +/- 50% = 1 octave
        float modulatedFreq = currentFreq_ * pitchMod;

        // Update oscillator frequencies with individual tuning
        updateOscFrequencies(modulatedFreq, params);

        // Apply LFO to pulse width
        float pwMod = lfoValue * params.lfo.toPW * 0.4f;

        // Mix oscillators
        float mix = 0.0f;

        if (params.osc1.enabled) {
            osc1_.setPulseWidth(params.osc1.pulseWidth + pwMod);
            mix += osc1_.process() * params.osc1.level;
        }

        if (params.osc2.enabled) {
            osc2_.setPulseWidth(params.osc2.pulseWidth + pwMod);
            mix += osc2_.process() * params.osc2.level;
        }

        if (params.osc3.enabled) {
            osc3_.setPulseWidth(params.osc3.pulseWidth + pwMod);
            mix += osc3_.process() * params.osc3.level;
        }

        // Add noise
        if (params.noiseLevel > 0.0f) {
            mix += noise_.process() * params.noiseLevel;
        }

        // Normalize mix level
        mix *= 0.33f;

        // Process envelopes
        float ampEnvValue = ampEnv_.process();
        float filterEnvValue = filterEnv_.process();

        // Calculate filter cutoff with modulation
        float baseCutoff = params.filter.cutoff;
        float envMod = filterEnvValue * params.filter.envAmount;
        float lfoMod = lfoValue * params.lfo.toFilter * 0.5f;
        float keyTrack = (static_cast<float>(note_) - 60.0f) / 60.0f * params.filter.keyTrack;

        float finalCutoff = baseCutoff + envMod + lfoMod + keyTrack;
        finalCutoff = std::clamp(finalCutoff, 0.0f, 1.0f);

        // Update filter
        filter_.setCutoffNormalized(finalCutoff);
        filter_.setResonance(params.filter.resonance);

        // Filter the signal
        float filtered = filter_.process(mix);

        // Apply amplitude envelope and velocity
        float output = filtered * ampEnvValue * velocity_ * params.masterLevel;

        return output;
    }

    void setSampleRate(float sampleRate) {
        sampleRate_ = sampleRate;
        osc1_.init(sampleRate);
        osc2_.init(sampleRate);
        osc3_.init(sampleRate);
        filter_.init(sampleRate);
        ampEnv_.init(sampleRate);
        filterEnv_.init(sampleRate);
        lfo_.init(sampleRate);
    }

private:
    void updateOscParams(const model::VAParams& params) {
        osc1_.setWaveform(static_cast<dsp::VAOscWaveform>(params.osc1.waveform));
        osc1_.setPulseWidth(params.osc1.pulseWidth);

        osc2_.setWaveform(static_cast<dsp::VAOscWaveform>(params.osc2.waveform));
        osc2_.setPulseWidth(params.osc2.pulseWidth);

        osc3_.setWaveform(static_cast<dsp::VAOscWaveform>(params.osc3.waveform));
        osc3_.setPulseWidth(params.osc3.pulseWidth);

        // Set LFO waveform
        lfo_.setWaveform(static_cast<dsp::VAOscWaveform>(params.lfo.shapeIndex));
    }

    void updateOscFrequencies(float baseFreq, const model::VAParams& params) {
        // OSC1
        float freq1 = baseFreq * getOctaveMultiplier(params.osc1.octave);
        freq1 *= getSemitoneMultiplier(params.osc1.semitones);
        freq1 *= getCentsMultiplier(params.osc1.cents);
        osc1_.setFrequency(freq1);

        // OSC2
        float freq2 = baseFreq * getOctaveMultiplier(params.osc2.octave);
        freq2 *= getSemitoneMultiplier(params.osc2.semitones);
        freq2 *= getCentsMultiplier(params.osc2.cents);
        osc2_.setFrequency(freq2);

        // OSC3
        float freq3 = baseFreq * getOctaveMultiplier(params.osc3.octave);
        freq3 *= getSemitoneMultiplier(params.osc3.semitones);
        freq3 *= getCentsMultiplier(params.osc3.cents);
        osc3_.setFrequency(freq3);

        // LFO frequency (fixed rate for now, could be tempo-synced)
        float lfoFreq = 0.5f + params.lfo.rateIndex * 0.5f;  // 0.5Hz to 8Hz
        lfo_.setFrequency(lfoFreq);
    }

    void updateFilterParams(const model::VAParams& params) {
        filter_.setCutoffNormalized(params.filter.cutoff);
        filter_.setResonance(params.filter.resonance);
    }

    void updateEnvelopeParams(const model::VAParams& params) {
        ampEnv_.setAttack(params.ampEnv.attack);
        ampEnv_.setDecay(params.ampEnv.decay);
        ampEnv_.setSustain(params.ampEnv.sustain);
        ampEnv_.setRelease(params.ampEnv.release);

        filterEnv_.setAttack(params.filterEnv.attack);
        filterEnv_.setDecay(params.filterEnv.decay);
        filterEnv_.setSustain(params.filterEnv.sustain);
        filterEnv_.setRelease(params.filterEnv.release);
    }

    float midiNoteToFreq(int note) const {
        return 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
    }

    float getOctaveMultiplier(int octave) const {
        return std::pow(2.0f, static_cast<float>(octave));
    }

    float getSemitoneMultiplier(int semitones) const {
        return std::pow(2.0f, semitones / 12.0f);
    }

    float getCentsMultiplier(float cents) const {
        return std::pow(2.0f, cents / 1200.0f);
    }

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
};

} // namespace audio
