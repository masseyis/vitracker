// VASynthInstrument - 3-oscillator VA synthesizer with Moog-style filter
// Mini Moog inspired architecture

#pragma once

#include "InstrumentProcessor.h"
#include "VASynthVoice.h"
#include "../model/Instrument.h"
#include "../dsp/sampler_modulation.h"
#include <array>
#include <atomic>

namespace audio {

class VASynthInstrument : public InstrumentProcessor {
public:
    static constexpr int NUM_VOICES = 16;
    static constexpr int kMaxBlockSize = 512;

    VASynthInstrument();
    ~VASynthInstrument() override = default;

    // InstrumentProcessor interface
    void init(double sampleRate) override;
    void setSampleRate(double sampleRate) override;
    void noteOn(int note, float velocity) override;
    void noteOff(int note) override;
    void allNotesOff() override;
    void noteOnWithFX(int note, float velocity, const model::Step& step) override;
    void process(float* outL, float* outR, int numSamples) override;
    const char* getTypeName() const override { return "VASynth"; }

    int getNumParameters() const override { return 0; }
    const char* getParameterName(int index) const override { (void)index; return ""; }
    float getParameter(int index) const override { (void)index; return 0.0f; }
    void setParameter(int index, float value) override { (void)index; (void)value; }

    void getState(void* data, int maxSize) const override { (void)data; (void)maxSize; }
    void setState(const void* data, int size) override { (void)data; (void)size; }

    // VA synth specific interface
    void setInstrument(model::Instrument* instrument) { instrument_ = instrument; }
    model::Instrument* getInstrument() const { return instrument_; }

    // Modulation matrix access
    const dsp::SamplerModulationMatrix& getModMatrix() const { return modMatrix_; }
    float getModulatedVolume() const;
    float getModulatedCutoff() const;
    float getModulatedResonance() const;

    // Tempo for LFO sync
    void setTempo(double bpm);

    // Playhead position for UI display (-1 if no voice active)
    int64_t getPlayheadPosition() const;

private:
    void updateModulationParams();
    void applyModulation(float* outL, float* outR, int numSamples);
    VASynthVoice* findFreeVoice();
    VASynthVoice* findVoiceToSteal();

    model::Instrument* instrument_ = nullptr;
    double sampleRate_ = 48000.0;

    std::array<VASynthVoice, NUM_VOICES> voices_;

    // Modulation
    dsp::SamplerModulationMatrix modMatrix_;
    double tempo_ = 120.0;
    int activeVoiceCount_ = 0;
    std::array<float, kMaxBlockSize> tempBufferL_;
    std::array<float, kMaxBlockSize> tempBufferR_;
};

} // namespace audio
