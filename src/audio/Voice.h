#pragma once

#include "plaits/dsp/voice.h"
#include "../model/Instrument.h"
#include <memory>

namespace audio {

class Voice
{
public:
    Voice();
    ~Voice();

    void init();
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
    float getDriveSend() const { return driveSend_; }
    float getSidechainSend() const { return sidechainSend_; }

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
    float driveSend_ = 0.0f;
    float sidechainSend_ = 0.0f;

    static uint64_t globalTime_;
};

} // namespace audio
