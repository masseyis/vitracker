#pragma once

#include "BiquadFilter.h"
#include "TransientShaper.h"
#include "MultibandOTT.h"
#include "../model/Instrument.h"
#include <memory>

namespace audio {

// Forward declare Drive from Effects.h
class Drive;

// Per-instrument channel strip processor
// Signal flow: HPF -> Low Shelf -> Mid EQ -> High Shelf -> Drive -> Punch -> OTT
class ChannelStrip {
public:
    ChannelStrip();
    ~ChannelStrip();

    void prepare(double sampleRate, int samplesPerBlock);
    void process(float* left, float* right, int numSamples);
    void updateParams(const model::ChannelStripParams& params);
    void reset();

private:
    void updateHPF();
    void updateEQ();

    double sampleRate_ = 44100.0;
    model::ChannelStripParams params_;

    // HPF - up to 2 cascaded biquads for 24dB/oct
    BiquadFilter hpfL_[2];
    BiquadFilter hpfR_[2];

    // EQ
    BiquadFilter lowShelfL_, lowShelfR_;
    BiquadFilter midPeakL_, midPeakR_;
    BiquadFilter highShelfL_, highShelfR_;

    // Drive - using existing class from Effects.h
    std::unique_ptr<Drive> drive_;

    // Punch (transient shaper)
    TransientShaper punch_;

    // OTT (multiband dynamics)
    MultibandOTT ott_;
};

} // namespace audio
