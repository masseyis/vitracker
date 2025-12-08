#include "ChannelStrip.h"
#include "Effects.h"
#include <algorithm>

namespace audio {

ChannelStrip::ChannelStrip() {
    drive_ = std::make_unique<Drive>();
}

ChannelStrip::~ChannelStrip() = default;

void ChannelStrip::prepare(double sampleRate, int samplesPerBlock) {
    sampleRate_ = sampleRate;

    // Initialize HPF
    for (int i = 0; i < 2; ++i) {
        hpfL_[i].setSampleRate(sampleRate);
        hpfR_[i].setSampleRate(sampleRate);
    }

    // Initialize EQ
    lowShelfL_.setSampleRate(sampleRate);
    lowShelfR_.setSampleRate(sampleRate);
    midPeakL_.setSampleRate(sampleRate);
    midPeakR_.setSampleRate(sampleRate);
    highShelfL_.setSampleRate(sampleRate);
    highShelfR_.setSampleRate(sampleRate);

    // Initialize dynamics
    drive_->init(sampleRate);
    punch_.prepare(sampleRate);
    ott_.prepare(sampleRate, samplesPerBlock);

    reset();
}

void ChannelStrip::reset() {
    for (int i = 0; i < 2; ++i) {
        hpfL_[i].reset();
        hpfR_[i].reset();
    }
    lowShelfL_.reset();
    lowShelfR_.reset();
    midPeakL_.reset();
    midPeakR_.reset();
    highShelfL_.reset();
    highShelfR_.reset();
    punch_.reset();
    ott_.reset();
}

void ChannelStrip::updateParams(const model::ChannelStripParams& params) {
    params_ = params;
    updateHPF();
    updateEQ();
    drive_->setParams(params_.driveAmount, params_.driveTone);
    punch_.setAmount(params_.punchAmount);
    ott_.setParams(params_.ottLowDepth, params_.ottMidDepth, params_.ottHighDepth, params_.ottMix);
}

void ChannelStrip::updateHPF() {
    if (params_.hpfSlope > 0) {
        float freq = std::clamp(params_.hpfFreq, 20.0f, 500.0f);
        for (int i = 0; i < 2; ++i) {
            hpfL_[i].setHighpass(freq, 0.707f);
            hpfR_[i].setHighpass(freq, 0.707f);
        }
    }
}

void ChannelStrip::updateEQ() {
    lowShelfL_.setLowShelf(params_.lowShelfFreq, params_.lowShelfGain);
    lowShelfR_.setLowShelf(params_.lowShelfFreq, params_.lowShelfGain);
    midPeakL_.setPeak(params_.midFreq, params_.midGain, params_.midQ);
    midPeakR_.setPeak(params_.midFreq, params_.midGain, params_.midQ);
    highShelfL_.setHighShelf(params_.highShelfFreq, params_.highShelfGain);
    highShelfR_.setHighShelf(params_.highShelfFreq, params_.highShelfGain);
}

void ChannelStrip::process(float* left, float* right, int numSamples) {
    for (int i = 0; i < numSamples; ++i) {
        float l = left[i];
        float r = right[i];

        // HPF (if enabled)
        if (params_.hpfSlope >= 1) {
            l = hpfL_[0].process(l);
            r = hpfR_[0].process(r);
        }
        if (params_.hpfSlope >= 2) {
            l = hpfL_[1].process(l);
            r = hpfR_[1].process(r);
        }

        // EQ (always active, but gain=0 means no change)
        l = lowShelfL_.process(l);
        r = lowShelfR_.process(r);
        l = midPeakL_.process(l);
        r = midPeakR_.process(r);
        l = highShelfL_.process(l);
        r = highShelfR_.process(r);

        // Drive
        if (params_.driveAmount > 0.001f) {
            drive_->process(l, r);
        }

        // Punch (transient shaper)
        punch_.process(l, r);

        // OTT (multiband dynamics)
        ott_.process(l, r);

        left[i] = l;
        right[i] = r;
    }
}

} // namespace audio
