#pragma once

#include "Voice.h"
#include "../dsp/voice.h"
#include "../model/Instrument.h"

namespace audio {

class PlaitsVoice : public Voice {
public:
    PlaitsVoice();
    ~PlaitsVoice() override = default;

    void setSampleRate(double sampleRate) override;
    void noteOn(int note, float velocity) override;
    void noteOff() override;
    void process(float* outL, float* outR, int numSamples,
                float pitchMod, float cutoffMod, float volumeMod, float panMod) override;

    bool isActive() const override;
    int getCurrentNote() const override;

    // Plaits-specific configuration
    void updateParameters(const model::PlaitsParams& params);

private:
    ::Voice plaitsVoiceWrapper_;  // Wrapper from dsp/voice.h
    double sampleRate_ = 48000.0;

    int currentNote_ = 60;
    float velocity_ = 1.0f;

    model::PlaitsParams params_;
};

} // namespace audio
