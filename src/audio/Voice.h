#pragma once

#include "plaits/dsp/voice.h"
#include "../model/Instrument.h"
#include "../model/Step.h"
#include <memory>
#include <cmath>

namespace audio {

class Voice
{
public:
    Voice();
    ~Voice();

    void init();
    void setSampleRate(double sampleRate) { sampleRate_ = sampleRate; }
    void noteOn(int note, float velocity, const model::Instrument& instrument);
    void noteOff();

    void render(float* outL, float* outR, int numSamples);

    bool isActive() const { return active_; }
    int getNote() const { return currentNote_; }
    uint64_t getStartTime() const { return startTime_; }

    // Send levels for effects (read by AudioEngine)
    float getReverbSend() const { return reverbSend_; }
    float getDelaySend() const { return delaySend_; }
    float getChorusSend() const { return chorusSend_; }
    float getSidechainSend() const { return sidechainSend_; }

    // FX command processing
    void setFX(model::FXType type, uint8_t value);
    void processFX();
    void setPortamentoTarget(int targetNote) { portamentoTarget_ = targetNote; }

private:
    plaits::Voice plaitsVoice_;
    plaits::Patch patch_;
    plaits::Modulations modulations_;

    float outBuffer_[24];
    float auxBuffer_[24];

    char sharedBuffer_[16384];

    bool active_ = false;
    int currentNote_ = -1;
    uint64_t startTime_ = 0;

    float reverbSend_ = 0.0f;
    float delaySend_ = 0.0f;
    float chorusSend_ = 0.0f;
    float sidechainSend_ = 0.0f;

    // FX state
    double sampleRate_ = 48000.0;
    float currentPitch_ = 0.0f;
    float portamentoTarget_ = 0.0f;
    float portamentoSpeed_ = 0.0f;
    float vibratoPhase_ = 0.0f;
    float vibratoSpeed_ = 0.0f;
    float vibratoDepth_ = 0.0f;
    int arpIndex_ = 0;
    int arpNotes_[3] = {0, 0, 0};
    int arpTicks_ = 0;
    float volumeSlide_ = 0.0f;

    static uint64_t globalTime_;
};

} // namespace audio
