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

    // InstrumentProcessor interface
    void init(double sampleRate) override;
    void setSampleRate(double sampleRate) override;
    void noteOn(int note, float velocity) override;
    void noteOff(int note) override;
    void allNotesOff() override;
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
