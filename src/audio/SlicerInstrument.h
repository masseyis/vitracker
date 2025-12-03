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
    const char* getParameterName(int index) const override { (void)index; return ""; }
    float getParameter(int index) const override { (void)index; return 0.0f; }
    void setParameter(int index, float value) override { (void)index; (void)value; }

    void getState(void* data, int maxSize) const override { (void)data; (void)maxSize; }
    void setState(const void* data, int size) override { (void)data; (void)size; }

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
