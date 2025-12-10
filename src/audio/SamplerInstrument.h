#pragma once

#include "InstrumentProcessor.h"
#include "SamplerVoice.h"
#include "../model/Instrument.h"
#include "../dsp/sampler_modulation.h"
#include <JuceHeader.h>
#include <array>
#include <memory>

namespace audio {

class SamplerInstrument : public InstrumentProcessor {
public:
    static constexpr int NUM_VOICES = 8;
    static constexpr int kMaxBlockSize = 512;

    SamplerInstrument();
    ~SamplerInstrument() override = default;

    // InstrumentProcessor interface
    void init(double sampleRate) override;
    void setSampleRate(double sampleRate) override;

    // Voice-per-track interface
    std::unique_ptr<audio::Voice> createVoice() override;
    void updateVoiceParameters(audio::Voice* voice) override;

    // Legacy methods
    void noteOn(int note, float velocity) override;
    void noteOff(int note) override;
    void allNotesOff() override;
    void noteOnWithFX(int note, float velocity, const model::Step& step) override;
    void process(float* outL, float* outR, int numSamples) override;
    const char* getTypeName() const override { return "Sampler"; }

    int getNumParameters() const override { return 0; }
    const char* getParameterName(int index) const override { return ""; }
    float getParameter(int index) const override { return 0.0f; }
    void setParameter(int index, float value) override {}

    void getState(void* data, int maxSize) const override {}
    void setState(const void* data, int size) override {}

    // Sampler-specific interface
    void setInstrument(model::Instrument* instrument) { instrument_ = instrument; }
    model::Instrument* getInstrument() const { return instrument_; }

    bool loadSample(const juce::File& file);
    bool hasSample() const { return sampleBuffer_.getNumSamples() > 0; }

    // For waveform display
    const juce::AudioBuffer<float>& getSampleBuffer() const { return sampleBuffer_; }
    int getSampleRate() const { return loadedSampleRate_; }

    // Playhead position for UI display (-1 if no voice active)
    int64_t getPlayheadPosition() const;

    // Modulation access for UI
    const dsp::SamplerModulationMatrix& getModMatrix() const { return modMatrix_; }

    // Modulated values for UI visualization
    float getModulatedVolume() const;
    float getModulatedCutoff() const;
    float getModulatedResonance() const;

    // Set tempo for tempo-synced LFOs
    void setTempo(double bpm);

private:
    SamplerVoice* findFreeVoice();
    SamplerVoice* findVoiceToSteal();
    void updateModulationParams();
    void applyModulation(float* outL, float* outR, int numSamples);

    model::Instrument* instrument_ = nullptr;
    double sampleRate_ = 48000.0;
    double tempo_ = 120.0;
    int activeVoiceCount_ = 0;

    std::array<SamplerVoice, NUM_VOICES> voices_;
    juce::AudioBuffer<float> sampleBuffer_;
    int loadedSampleRate_ = 44100;

    juce::AudioFormatManager formatManager_;
    dsp::SamplerModulationMatrix modMatrix_;

    // Temporary buffers for processing
    std::array<float, kMaxBlockSize> tempBufferL_;
    std::array<float, kMaxBlockSize> tempBufferR_;
};

} // namespace audio
