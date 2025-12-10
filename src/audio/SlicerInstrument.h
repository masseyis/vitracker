#pragma once

#include "InstrumentProcessor.h"
#include "SlicerVoice.h"
#include "../model/Instrument.h"
#include "../dsp/sampler_modulation.h"
#include <JuceHeader.h>
#include <array>
#include <atomic>

namespace audio {

class SlicerInstrument : public InstrumentProcessor {
public:
    static constexpr int NUM_VOICES = 8;
    static constexpr int BASE_NOTE = 12;  // C-1 (tracker notation) = slice 0
    static constexpr int kMaxBlockSize = 512;

    SlicerInstrument();
    ~SlicerInstrument() override = default;

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
    void chopByTransients(float sensitivity);  // Auto-detect transients and create slices
    int addSliceAtPosition(size_t samplePosition);  // Returns index of new slice
    void removeSlice(int sliceIndex);
    void clearSlices();
    int getNumSlices() const;

    // Zero-crossing snap helper
    size_t findNearestZeroCrossing(size_t position) const;

    // For waveform display
    const juce::AudioBuffer<float>& getSampleBuffer() const { return sampleBuffer_; }
    int getSampleRate() const { return loadedSampleRate_; }

    // Time-stretched buffer (when repitch=false)
    const juce::AudioBuffer<float>& getStretchedBuffer() const { return stretchedBuffer_; }
    bool hasStretchedBuffer() const { return stretchedBuffer_.getNumSamples() > 0; }
    void regenerateStretchedBuffer();  // Call when speed changes and repitch=false

    // Three-way dependency editing (Original BPM/Bars, Target BPM, Speed/Pitch)
    // Call these when user edits a value - they handle dependency recalculation
    void editOriginalBars(int newBars);
    void editOriginalBPM(float newBPM);
    void editTargetBPM(float newBPM);
    void editSpeed(float newSpeed);
    void editPitch(int newSemitones);  // Only works when repitch=true
    void setRepitch(bool enabled);

    // Lazy chop preview - plays entire sample and tracks position
    void startLazyChopPreview();
    void stopLazyChopPreview();
    bool isLazyChopPlaying() const { return lazyChopPlaying_; }
    size_t getLazyChopPlayhead() const { return lazyChopPlayhead_; }
    void addSliceAtPlayhead();  // Adds slice at current playhead position

    // Playhead position for UI display (-1 if no voice active)
    int64_t getPlayheadPosition() const;

    // Modulation matrix access
    const dsp::SamplerModulationMatrix& getModMatrix() const { return modMatrix_; }
    float getModulatedVolume() const;
    float getModulatedCutoff() const;
    float getModulatedResonance() const;

    // Tempo for LFO sync
    void setTempo(double bpm);

private:
    void updateModulationParams();
    void applyModulation(float* outL, float* outR, int numSamples);
    // Three-way dependency helpers
    void markEdited(model::SlicerLastEdited which);
    void recalculateDependencies();
    float calculateBPMFromBars(int bars) const;
    int calculateBarsFromBPM(float bpm) const;
    SlicerVoice* findFreeVoice(int maxVoices = NUM_VOICES);
    SlicerVoice* findVoiceToSteal();
    int midiNoteToSliceIndex(int midiNote) const;

    model::Instrument* instrument_ = nullptr;
    double sampleRate_ = 48000.0;

    std::array<SlicerVoice, NUM_VOICES> voices_;
    juce::AudioBuffer<float> sampleBuffer_;
    int loadedSampleRate_ = 44100;

    // Time-stretched buffer (pre-rendered when repitch=false)
    juce::AudioBuffer<float> stretchedBuffer_;
    float lastStretchSpeed_ = 1.0f;  // Track when regeneration is needed
    std::atomic<bool> stretchedBufferReady_{false};

    juce::AudioFormatManager formatManager_;

    // Lazy chop preview state
    bool lazyChopPlaying_ = false;
    size_t lazyChopPlayhead_ = 0;

    // Modulation
    dsp::SamplerModulationMatrix modMatrix_;
    double tempo_ = 120.0;
    int activeVoiceCount_ = 0;
    std::array<float, kMaxBlockSize> tempBufferL_;
    std::array<float, kMaxBlockSize> tempBufferR_;
};

} // namespace audio
