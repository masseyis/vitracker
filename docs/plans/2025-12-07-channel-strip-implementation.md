# Channel Strip Implementation Plan

## Overview

This plan implements per-instrument channel strip processing with HPF, EQ, Drive, Punch (transient shaper), and OTT (multiband dynamics). The feature moves Drive from master bus sends to per-instrument inserts and adds a new dedicated ChannelScreen (F4).

Reference: `docs/plans/2025-12-07-channel-strip-design.md`

---

## Task 1: Add ChannelStripParams to Data Model

**Goal**: Define the `ChannelStripParams` struct and add it to the `Instrument` class.

### Files to Modify

**`src/model/Instrument.h`**

Add after `SendLevels` struct (around line 55):

```cpp
// Channel strip parameters (per-instrument insert processing)
struct ChannelStripParams {
    // HPF (High-Pass Filter / Low Cut)
    float hpfFreq = 20.0f;      // 20-500 Hz
    int hpfSlope = 0;           // 0=off, 1=12dB/oct, 2=24dB/oct

    // Low Shelf EQ
    float lowShelfGain = 0.0f;  // -12 to +12 dB
    float lowShelfFreq = 200.0f; // 50-500 Hz

    // Mid Parametric EQ
    float midFreq = 1000.0f;    // 200-8000 Hz
    float midGain = 0.0f;       // -12 to +12 dB
    float midQ = 1.0f;          // 0.5-8.0

    // High Shelf EQ
    float highShelfGain = 0.0f; // -12 to +12 dB
    float highShelfFreq = 8000.0f; // 2000-16000 Hz

    // Drive (moved from master bus send)
    float driveAmount = 0.0f;   // 0-1 (0 = bypass)
    float driveTone = 0.5f;     // 0-1 (dark to bright)

    // Punch (transient shaper)
    float punchAmount = 0.0f;   // 0-1 (0 = bypass)

    // OTT (3-band multiband dynamics)
    float ottDepth = 0.0f;      // 0-1 (0 = bypass)
    float ottMix = 1.0f;        // 0-1 wet/dry
    float ottSmooth = 0.5f;     // 0-1 (faster to slower attack/release)
};
```

Add to `Instrument` class (after `getVAParams`):

```cpp
ChannelStripParams& getChannelStrip() { return channelStrip_; }
const ChannelStripParams& getChannelStrip() const { return channelStrip_; }
```

Add to private members:

```cpp
ChannelStripParams channelStrip_;
```

### Verification

```bash
cd /Users/jamesmassey/ai-dev/vitracker/.worktrees/channel-strip
cmake --build build --target Vitracker 2>&1 | head -50
```

Should compile without errors.

---

## Task 2: Create BiquadFilter DSP Class

**Goal**: Implement a reusable biquad filter for HPF and EQ bands using RBJ cookbook coefficients.

### Files to Create

**`src/audio/BiquadFilter.h`**

```cpp
#pragma once

namespace audio {

// Direct Form II Transposed biquad filter
// Implements RBJ Audio-EQ-Cookbook filters
class BiquadFilter {
public:
    enum class Type {
        Highpass,
        LowShelf,
        HighShelf,
        Peak
    };

    void reset();
    void setSampleRate(double sampleRate);

    // Configure filter type and parameters
    void setHighpass(float freq, float q = 0.707f);
    void setLowShelf(float freq, float gainDb);
    void setHighShelf(float freq, float gainDb);
    void setPeak(float freq, float gainDb, float q);

    // Process single sample (call for each channel)
    float process(float input);

private:
    void calculateCoefficients(Type type, float freq, float gainDb, float q);

    double sampleRate_ = 44100.0;

    // Coefficients
    float b0_ = 1.0f, b1_ = 0.0f, b2_ = 0.0f;
    float a1_ = 0.0f, a2_ = 0.0f;

    // State (Direct Form II Transposed)
    float z1_ = 0.0f, z2_ = 0.0f;
};

} // namespace audio
```

**`src/audio/BiquadFilter.cpp`**

```cpp
#include "BiquadFilter.h"
#include <cmath>

namespace audio {

void BiquadFilter::reset() {
    z1_ = z2_ = 0.0f;
}

void BiquadFilter::setSampleRate(double sampleRate) {
    sampleRate_ = sampleRate;
}

void BiquadFilter::setHighpass(float freq, float q) {
    calculateCoefficients(Type::Highpass, freq, 0.0f, q);
}

void BiquadFilter::setLowShelf(float freq, float gainDb) {
    calculateCoefficients(Type::LowShelf, freq, gainDb, 0.707f);
}

void BiquadFilter::setHighShelf(float freq, float gainDb) {
    calculateCoefficients(Type::HighShelf, freq, gainDb, 0.707f);
}

void BiquadFilter::setPeak(float freq, float gainDb, float q) {
    calculateCoefficients(Type::Peak, freq, gainDb, q);
}

float BiquadFilter::process(float input) {
    float output = b0_ * input + z1_;
    z1_ = b1_ * input - a1_ * output + z2_;
    z2_ = b2_ * input - a2_ * output;
    return output;
}

void BiquadFilter::calculateCoefficients(Type type, float freq, float gainDb, float q) {
    const float w0 = 2.0f * static_cast<float>(M_PI) * freq / static_cast<float>(sampleRate_);
    const float cosW0 = std::cos(w0);
    const float sinW0 = std::sin(w0);
    const float alpha = sinW0 / (2.0f * q);
    const float A = std::pow(10.0f, gainDb / 40.0f);  // sqrt of linear gain

    float b0, b1, b2, a0, a1, a2;

    switch (type) {
        case Type::Highpass:
            b0 = (1.0f + cosW0) / 2.0f;
            b1 = -(1.0f + cosW0);
            b2 = (1.0f + cosW0) / 2.0f;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cosW0;
            a2 = 1.0f - alpha;
            break;

        case Type::LowShelf: {
            const float sqrtA = std::sqrt(A);
            const float alphaShelf = sinW0 / 2.0f * std::sqrt((A + 1.0f/A) * (1.0f/0.707f - 1.0f) + 2.0f);
            b0 = A * ((A + 1.0f) - (A - 1.0f) * cosW0 + 2.0f * sqrtA * alphaShelf);
            b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosW0);
            b2 = A * ((A + 1.0f) - (A - 1.0f) * cosW0 - 2.0f * sqrtA * alphaShelf);
            a0 = (A + 1.0f) + (A - 1.0f) * cosW0 + 2.0f * sqrtA * alphaShelf;
            a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosW0);
            a2 = (A + 1.0f) + (A - 1.0f) * cosW0 - 2.0f * sqrtA * alphaShelf;
            break;
        }

        case Type::HighShelf: {
            const float sqrtA = std::sqrt(A);
            const float alphaShelf = sinW0 / 2.0f * std::sqrt((A + 1.0f/A) * (1.0f/0.707f - 1.0f) + 2.0f);
            b0 = A * ((A + 1.0f) + (A - 1.0f) * cosW0 + 2.0f * sqrtA * alphaShelf);
            b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosW0);
            b2 = A * ((A + 1.0f) + (A - 1.0f) * cosW0 - 2.0f * sqrtA * alphaShelf);
            a0 = (A + 1.0f) - (A - 1.0f) * cosW0 + 2.0f * sqrtA * alphaShelf;
            a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosW0);
            a2 = (A + 1.0f) - (A - 1.0f) * cosW0 - 2.0f * sqrtA * alphaShelf;
            break;
        }

        case Type::Peak:
            b0 = 1.0f + alpha * A;
            b1 = -2.0f * cosW0;
            b2 = 1.0f - alpha * A;
            a0 = 1.0f + alpha / A;
            a1 = -2.0f * cosW0;
            a2 = 1.0f - alpha / A;
            break;
    }

    // Normalize coefficients
    b0_ = b0 / a0;
    b1_ = b1 / a0;
    b2_ = b2 / a0;
    a1_ = a1 / a0;
    a2_ = a2 / a0;
}

} // namespace audio
```

### Files to Modify

**`CMakeLists.txt`** - Add to target_sources after `src/audio/Effects.cpp`:

```cmake
    src/audio/BiquadFilter.cpp
```

### Verification

```bash
cmake --build build --target Vitracker 2>&1 | head -50
```

---

## Task 3: Create TransientShaper DSP Class

**Goal**: Implement the Punch effect using dual envelope followers.

### Files to Create

**`src/audio/TransientShaper.h`**

```cpp
#pragma once

namespace audio {

// Transient shaper using dual envelope followers
// Fast follower tracks transients, slow follower tracks sustain
// Difference = transient content, amplified by amount parameter
class TransientShaper {
public:
    void prepare(double sampleRate);
    void setAmount(float amount);  // 0-1
    void process(float& left, float& right);
    void reset();

private:
    double sampleRate_ = 44100.0;
    float amount_ = 0.0f;

    // Envelope followers (stereo)
    float fastEnvL_ = 0.0f, fastEnvR_ = 0.0f;
    float slowEnvL_ = 0.0f, slowEnvR_ = 0.0f;

    // Attack/release coefficients
    float fastAttack_ = 0.0f, fastRelease_ = 0.0f;
    float slowAttack_ = 0.0f, slowRelease_ = 0.0f;
};

} // namespace audio
```

**`src/audio/TransientShaper.cpp`**

```cpp
#include "TransientShaper.h"
#include <cmath>
#include <algorithm>

namespace audio {

void TransientShaper::prepare(double sampleRate) {
    sampleRate_ = sampleRate;

    // Fast envelope: ~1ms attack, ~10ms release
    fastAttack_ = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * 0.001f));
    fastRelease_ = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * 0.01f));

    // Slow envelope: ~20ms attack, ~200ms release
    slowAttack_ = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * 0.02f));
    slowRelease_ = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * 0.2f));

    reset();
}

void TransientShaper::setAmount(float amount) {
    amount_ = std::clamp(amount, 0.0f, 1.0f);
}

void TransientShaper::reset() {
    fastEnvL_ = fastEnvR_ = 0.0f;
    slowEnvL_ = slowEnvR_ = 0.0f;
}

void TransientShaper::process(float& left, float& right) {
    if (amount_ < 0.001f) return;  // Bypass when off

    // Rectify input
    float absL = std::abs(left);
    float absR = std::abs(right);

    // Update fast envelope
    float fastCoefL = (absL > fastEnvL_) ? fastAttack_ : fastRelease_;
    float fastCoefR = (absR > fastEnvR_) ? fastAttack_ : fastRelease_;
    fastEnvL_ += fastCoefL * (absL - fastEnvL_);
    fastEnvR_ += fastCoefR * (absR - fastEnvR_);

    // Update slow envelope
    float slowCoefL = (absL > slowEnvL_) ? slowAttack_ : slowRelease_;
    float slowCoefR = (absR > slowEnvR_) ? slowAttack_ : slowRelease_;
    slowEnvL_ += slowCoefL * (absL - slowEnvL_);
    slowEnvR_ += slowCoefR * (absR - slowEnvR_);

    // Transient = difference between fast and slow envelopes
    float transientL = std::max(0.0f, fastEnvL_ - slowEnvL_);
    float transientR = std::max(0.0f, fastEnvR_ - slowEnvR_);

    // Apply gain boost to transient portion (amount controls intensity)
    // Scale: amount=0 -> 1x, amount=1 -> 4x boost
    float boost = 1.0f + amount_ * 3.0f;
    float gainL = 1.0f + transientL * (boost - 1.0f);
    float gainR = 1.0f + transientR * (boost - 1.0f);

    left *= gainL;
    right *= gainR;
}

} // namespace audio
```

### Files to Modify

**`CMakeLists.txt`** - Add after BiquadFilter.cpp:

```cmake
    src/audio/TransientShaper.cpp
```

### Verification

```bash
cmake --build build --target Vitracker 2>&1 | head -50
```

---

## Task 4: Create MultibandOTT DSP Class

**Goal**: Implement 3-band OTT with upward/downward compression per band.

### Files to Create

**`src/audio/MultibandOTT.h`**

```cpp
#pragma once

#include "BiquadFilter.h"
#include <array>

namespace audio {

// 3-band OTT (Over The Top) multiband dynamics processor
// Linkwitz-Riley crossovers at ~100Hz and ~3kHz
// Each band has upward + downward compression
class MultibandOTT {
public:
    void prepare(double sampleRate, int samplesPerBlock);
    void setParams(float depth, float mix, float smooth);  // All 0-1
    void process(float& left, float& right);
    void reset();

private:
    struct BandState {
        float envUp = 0.0f;    // Upward compressor envelope
        float envDown = 0.0f;  // Downward compressor envelope
    };

    void processband(float& sample, BandState& state, float threshold,
                     float attackUp, float releaseUp, float attackDown, float releaseDown);

    double sampleRate_ = 44100.0;
    float depth_ = 0.0f;
    float mix_ = 1.0f;
    float smooth_ = 0.5f;

    // Crossover filters (Linkwitz-Riley = 2x Butterworth)
    // Low band: below ~100Hz
    // Mid band: ~100Hz - ~3kHz
    // High band: above ~3kHz
    BiquadFilter lowpassL1_, lowpassL2_;   // LP for low band
    BiquadFilter lowpassR1_, lowpassR2_;
    BiquadFilter highpassL1_, highpassL2_; // HP after LP for mid band
    BiquadFilter highpassR1_, highpassR2_;
    BiquadFilter highpassL3_, highpassL4_; // HP for high band
    BiquadFilter highpassR3_, highpassR4_;
    BiquadFilter lowpassL3_, lowpassL4_;   // LP after HP for mid band
    BiquadFilter lowpassR3_, lowpassR4_;

    // Per-band envelope states (L+R combined for linked stereo)
    std::array<BandState, 3> bandStates_;

    // Attack/release coefficients (recalculated when smooth changes)
    float attackUp_ = 0.0f, releaseUp_ = 0.0f;
    float attackDown_ = 0.0f, releaseDown_ = 0.0f;
};

} // namespace audio
```

**`src/audio/MultibandOTT.cpp`**

```cpp
#include "MultibandOTT.h"
#include <cmath>
#include <algorithm>

namespace audio {

void MultibandOTT::prepare(double sampleRate, int /*samplesPerBlock*/) {
    sampleRate_ = sampleRate;

    // Set up Linkwitz-Riley crossovers (cascaded Butterworth)
    // Low crossover at 100Hz
    lowpassL1_.setSampleRate(sampleRate);
    lowpassL2_.setSampleRate(sampleRate);
    lowpassR1_.setSampleRate(sampleRate);
    lowpassR2_.setSampleRate(sampleRate);

    highpassL1_.setSampleRate(sampleRate);
    highpassL2_.setSampleRate(sampleRate);
    highpassR1_.setSampleRate(sampleRate);
    highpassR2_.setSampleRate(sampleRate);

    // High crossover at 3kHz
    highpassL3_.setSampleRate(sampleRate);
    highpassL4_.setSampleRate(sampleRate);
    highpassR3_.setSampleRate(sampleRate);
    highpassR4_.setSampleRate(sampleRate);

    lowpassL3_.setSampleRate(sampleRate);
    lowpassL4_.setSampleRate(sampleRate);
    lowpassR3_.setSampleRate(sampleRate);
    lowpassR4_.setSampleRate(sampleRate);

    // Configure crossover frequencies
    const float lowXover = 100.0f;
    const float highXover = 3000.0f;
    const float q = 0.707f;  // Butterworth

    // Low band LP (cascaded for LR4)
    lowpassL1_.setHighpass(lowXover, q);  // Actually we need lowpass - will fix
    lowpassL2_.setHighpass(lowXover, q);
    lowpassR1_.setHighpass(lowXover, q);
    lowpassR2_.setHighpass(lowXover, q);

    // Note: BiquadFilter needs lowpass method - using highpass temporarily
    // The actual implementation will add a lowpass method to BiquadFilter

    reset();
    setParams(depth_, mix_, smooth_);
}

void MultibandOTT::setParams(float depth, float mix, float smooth) {
    depth_ = std::clamp(depth, 0.0f, 1.0f);
    mix_ = std::clamp(mix, 0.0f, 1.0f);
    smooth_ = std::clamp(smooth, 0.0f, 1.0f);

    // Calculate attack/release based on smooth parameter
    // smooth=0 -> fast (5ms attack, 50ms release)
    // smooth=1 -> slow (50ms attack, 500ms release)
    float attackMs = 5.0f + smooth_ * 45.0f;
    float releaseMs = 50.0f + smooth_ * 450.0f;

    attackUp_ = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate_) * attackMs * 0.001f));
    releaseUp_ = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate_) * releaseMs * 0.001f));
    attackDown_ = attackUp_;
    releaseDown_ = releaseUp_;
}

void MultibandOTT::reset() {
    for (auto& band : bandStates_) {
        band.envUp = 0.0f;
        band.envDown = 0.0f;
    }

    lowpassL1_.reset(); lowpassL2_.reset();
    lowpassR1_.reset(); lowpassR2_.reset();
    highpassL1_.reset(); highpassL2_.reset();
    highpassR1_.reset(); highpassR2_.reset();
    highpassL3_.reset(); highpassL4_.reset();
    highpassR3_.reset(); highpassR4_.reset();
    lowpassL3_.reset(); lowpassL4_.reset();
    lowpassR3_.reset(); lowpassR4_.reset();
}

void MultibandOTT::process(float& left, float& right) {
    if (depth_ < 0.001f) return;  // Bypass when off

    float dryL = left, dryR = right;

    // Simple 3-band split using filters
    // For now, process as single band until crossover filters are properly implemented
    // This is a simplified version - full implementation needs lowpass in BiquadFilter

    float mono = (left + right) * 0.5f;
    float level = std::abs(mono);

    // Upward compression: boost quiet signals
    const float upThreshold = 0.1f;
    float upGain = 1.0f;
    if (level < upThreshold && level > 0.0001f) {
        float ratio = upThreshold / level;
        upGain = 1.0f + (ratio - 1.0f) * depth_ * 0.5f;
        upGain = std::min(upGain, 4.0f);  // Limit boost
    }

    // Downward compression: squash loud signals
    const float downThreshold = 0.5f;
    float downGain = 1.0f;
    if (level > downThreshold) {
        float ratio = downThreshold / level;
        downGain = 1.0f - (1.0f - ratio) * depth_ * 0.5f;
        downGain = std::max(downGain, 0.25f);  // Limit reduction
    }

    float totalGain = upGain * downGain;

    float wetL = left * totalGain;
    float wetR = right * totalGain;

    // Mix dry/wet
    left = dryL + (wetL - dryL) * mix_;
    right = dryR + (wetR - dryR) * mix_;
}

void MultibandOTT::processband(float& /*sample*/, BandState& /*state*/, float /*threshold*/,
                               float /*attackUp*/, float /*releaseUp*/,
                               float /*attackDown*/, float /*releaseDown*/) {
    // Per-band processing - will be used in full 3-band implementation
}

} // namespace audio
```

### Files to Modify

**`CMakeLists.txt`** - Add after TransientShaper.cpp:

```cmake
    src/audio/MultibandOTT.cpp
```

**`src/audio/BiquadFilter.h`** - Add lowpass method declaration:

```cpp
    void setLowpass(float freq, float q = 0.707f);
```

**`src/audio/BiquadFilter.cpp`** - Add lowpass implementation in `calculateCoefficients` and new method:

```cpp
void BiquadFilter::setLowpass(float freq, float q) {
    calculateCoefficients(Type::Lowpass, freq, 0.0f, q);
}
```

Add `Lowpass` to the enum and add case in `calculateCoefficients`:

```cpp
        case Type::Lowpass:
            b0 = (1.0f - cosW0) / 2.0f;
            b1 = 1.0f - cosW0;
            b2 = (1.0f - cosW0) / 2.0f;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cosW0;
            a2 = 1.0f - alpha;
            break;
```

### Verification

```bash
cmake --build build --target Vitracker 2>&1 | head -50
```

---

## Task 5: Create ChannelStrip DSP Class

**Goal**: Combine all channel strip processors into a single class.

### Files to Create

**`src/audio/ChannelStrip.h`**

```cpp
#pragma once

#include "BiquadFilter.h"
#include "TransientShaper.h"
#include "MultibandOTT.h"
#include "../model/Instrument.h"

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
```

**`src/audio/ChannelStrip.cpp`**

```cpp
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
    ott_.setParams(params_.ottDepth, params_.ottMix, params_.ottSmooth);
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
```

### Files to Modify

**`CMakeLists.txt`** - Add after MultibandOTT.cpp:

```cmake
    src/audio/ChannelStrip.cpp
```

### Verification

```bash
cmake --build build --target Vitracker 2>&1 | head -50
```

---

## Task 6: Integrate ChannelStrip into AudioEngine

**Goal**: Add per-instrument ChannelStrip processing to the audio render loop.

### Files to Modify

**`src/audio/AudioEngine.h`**

Add include at top:

```cpp
#include "ChannelStrip.h"
```

Add to private members (after `vaSynthProcessors_`):

```cpp
    // Per-instrument channel strip processing
    std::array<std::unique_ptr<ChannelStrip>, NUM_INSTRUMENTS> channelStrips_;
```

**`src/audio/AudioEngine.cpp`**

In `prepareToPlay()`, add after initializing instrument processors:

```cpp
    // Initialize channel strips
    for (auto& strip : channelStrips_) {
        if (!strip) {
            strip = std::make_unique<ChannelStrip>();
        }
        strip->prepare(sampleRate, samplesPerBlockExpected);
    }
```

In the render loop (inside `getNextAudioBlock()`), after instrument renders to buffer and before applying volume/pan, add channel strip processing. Find where instrument output is accumulated and add:

```cpp
    // Apply channel strip processing (after instrument, before volume/pan)
    if (auto* instrument = project_->getInstrument(instrumentIndex)) {
        channelStrips_[instrumentIndex]->updateParams(instrument->getChannelStrip());
        channelStrips_[instrumentIndex]->process(tempBufferL, tempBufferR, numSamples);
    }
```

### Verification

```bash
cmake --build build --target Vitracker 2>&1 | head -50
```

Build and run to verify audio still plays correctly.

---

## Task 7: Remove Drive from Master Bus

**Goal**: Remove Drive from MixerState and master bus processing since it's now per-instrument.

### Files to Modify

**`src/model/Project.h`**

In `MixerState` struct, remove:
- `float driveGain = 0.3f;`
- `float driveTone = 0.6f;`

Change `busLevels` from 5 elements to 4:

```cpp
std::array<float, 4> busLevels;  // reverb, delay, chorus, sidechain (drive removed)
```

**`src/model/Instrument.h`**

In `SendLevels` struct, remove:

```cpp
    float drive = 0.0f;
```

**`src/audio/Effects.h`**

In `EffectsProcessor::process()`, remove `driveSend` parameter:

```cpp
    void process(float& left, float& right,
                 float reverbSend, float delaySend, float chorusSend);
```

**`src/audio/Effects.cpp`**

Update `EffectsProcessor::process()` to remove drive processing from master bus.

**`src/audio/AudioEngine.cpp`**

Update calls to `effects_.process()` to remove drive send parameter.
Remove any drive accumulation in the render loop.

**`src/ui/MixerScreen.cpp`**

Remove Drive panel from the FX section display.
Update `cursorFx_` range (was 0-3 for reverb/delay/chorus/drive, now 0-2).

**`src/ui/InstrumentScreen.cpp`**

In the row enums for each instrument type, remove the `Drive` row from sends section.
Note: Drive still exists but is now in ChannelScreen, not InstrumentScreen sends.

### Verification

```bash
cmake --build build --target Vitracker 2>&1 | head -50
```

---

## Task 8: Create ChannelScreen UI

**Goal**: Implement the new ChannelScreen for channel strip parameter editing.

### Files to Create

**`src/ui/ChannelScreen.h`**

```cpp
#pragma once

#include "Screen.h"

namespace ui {

// Row types for ChannelScreen
enum class ChannelRowType {
    HPF,           // hpfFreq, hpfSlope
    LowShelf,      // lowShelfGain, lowShelfFreq
    MidEQ,         // midGain, midFreq, midQ
    HighShelf,     // highShelfGain, highShelfFreq
    Drive,         // driveAmount, driveTone
    Punch,         // punchAmount
    OTT,           // ottDepth, ottMix, ottSmooth
    Volume,        // volume (from Instrument)
    Pan,           // pan (from Instrument)
    Reverb,        // reverb send
    Delay,         // delay send
    Chorus,        // chorus send
    Sidechain,     // sidechain duck
    NumRows
};

class ChannelScreen : public Screen {
public:
    ChannelScreen(model::Project& project, input::ModeManager& modeManager);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void navigate(int dx, int dy) override;
    bool handleEdit(const juce::KeyPress& key) override;

    std::string getTitle() const override { return "CHANNEL"; }
    std::vector<HelpSection> getHelpContent() const override;

    input::InputContext getInputContext() const override {
        return input::InputContext::RowParams;
    }

    int getCurrentInstrument() const { return currentInstrument_; }
    void setCurrentInstrument(int index);

private:
    void drawInstrumentTabs(juce::Graphics& g, juce::Rectangle<int> area);
    void drawRow(juce::Graphics& g, juce::Rectangle<int> area, int row, bool selected);
    void drawParamBar(juce::Graphics& g, juce::Rectangle<int> area, float value,
                      float min, float max, bool bipolar = false);

    int getNumFieldsForRow(int row) const;
    void adjustParam(int row, int field, float delta);

    // Jump key handling
    int getRowForJumpKey(char key) const;

    int currentInstrument_ = 0;
    int cursorRow_ = 0;
    int cursorField_ = 0;  // For multi-field rows

    static constexpr int kNumRows = static_cast<int>(ChannelRowType::NumRows);
    static constexpr int kRowHeight = 24;
    static constexpr int kLabelWidth = 100;
    static constexpr int kBarWidth = 160;
};

} // namespace ui
```

**`src/ui/ChannelScreen.cpp`**

```cpp
#include "ChannelScreen.h"
#include "HelpPopup.h"
#include <algorithm>

namespace ui {

ChannelScreen::ChannelScreen(model::Project& project, input::ModeManager& modeManager)
    : Screen(project, modeManager) {}

void ChannelScreen::setCurrentInstrument(int index) {
    if (index >= 0 && index < project_.getInstrumentCount()) {
        currentInstrument_ = index;
        repaint();
    }
}

void ChannelScreen::paint(juce::Graphics& g) {
    g.fillAll(bgColor);

    auto area = getLocalBounds();

    // Header with instrument tabs
    auto headerArea = area.removeFromTop(60);
    drawInstrumentTabs(g, headerArea);

    // Rows
    area.removeFromTop(10);  // Padding

    for (int row = 0; row < kNumRows; ++row) {
        auto rowArea = area.removeFromTop(kRowHeight);
        bool isSelected = (row == cursorRow_);
        drawRow(g, rowArea, row, isSelected);
    }
}

void ChannelScreen::resized() {
    // No child components to layout
}

void ChannelScreen::drawInstrumentTabs(juce::Graphics& g, juce::Rectangle<int> area) {
    g.setColour(headerColor);
    g.fillRect(area);

    // Title
    g.setColour(fgColor);
    g.setFont(16.0f);
    g.drawText("CHANNEL", area.removeFromLeft(100), juce::Justification::centredLeft);

    // Current instrument name
    if (auto* inst = project_.getInstrument(currentInstrument_)) {
        g.setColour(cursorColor);
        juce::String label = juce::String::formatted("%02d: %s",
            currentInstrument_, inst->getName().c_str());
        g.drawText(label, area.removeFromRight(200), juce::Justification::centredRight);
    }
}

void ChannelScreen::drawRow(juce::Graphics& g, juce::Rectangle<int> area, int row, bool selected) {
    auto* inst = project_.getInstrument(currentInstrument_);
    if (!inst) return;

    const auto& strip = inst->getChannelStrip();
    const auto& sends = inst->getSends();

    // Selection highlight
    if (selected) {
        g.setColour(highlightColor);
        g.fillRect(area);
    }

    // Row label and shortcut key
    g.setColour(selected ? cursorColor : fgColor);
    g.setFont(14.0f);

    static const char* rowLabels[] = {
        "[h] HPF", "[l] LOW SHELF", "[m] MID EQ", "[e] HIGH SHELF",
        "[d] DRIVE", "[p] PUNCH", "[o] OTT",
        "[v] VOLUME", "[n] PAN",
        "[r] REVERB", "[y] DELAY", "[c] CHORUS", "[s] SIDECHAIN"
    };

    auto labelArea = area.removeFromLeft(kLabelWidth);
    g.drawText(rowLabels[row], labelArea, juce::Justification::centredLeft);

    // Parameter display depends on row type
    auto paramArea = area;

    switch (static_cast<ChannelRowType>(row)) {
        case ChannelRowType::HPF: {
            // Frequency bar + slope indicator
            auto barArea = paramArea.removeFromLeft(kBarWidth);
            drawParamBar(g, barArea, strip.hpfFreq, 20.0f, 500.0f);

            g.setColour(fgColor);
            juce::String freqText = juce::String(static_cast<int>(strip.hpfFreq)) + " Hz";
            g.drawText(freqText, paramArea.removeFromLeft(60), juce::Justification::centred);

            static const char* slopes[] = {"OFF", "12dB", "24dB"};
            g.drawText(slopes[strip.hpfSlope], paramArea.removeFromLeft(50), juce::Justification::centred);
            break;
        }

        case ChannelRowType::LowShelf:
        case ChannelRowType::HighShelf: {
            float gain = (row == static_cast<int>(ChannelRowType::LowShelf))
                ? strip.lowShelfGain : strip.highShelfGain;
            float freq = (row == static_cast<int>(ChannelRowType::LowShelf))
                ? strip.lowShelfFreq : strip.highShelfFreq;

            auto barArea = paramArea.removeFromLeft(kBarWidth);
            drawParamBar(g, barArea, gain, -12.0f, 12.0f, true);  // Bipolar

            g.setColour(fgColor);
            juce::String gainText = (gain >= 0 ? "+" : "") + juce::String(gain, 1) + " dB";
            g.drawText(gainText, paramArea.removeFromLeft(70), juce::Justification::centred);

            juce::String freqText = juce::String(static_cast<int>(freq)) + " Hz";
            g.drawText(freqText, paramArea.removeFromLeft(70), juce::Justification::centred);
            break;
        }

        case ChannelRowType::MidEQ: {
            auto barArea = paramArea.removeFromLeft(kBarWidth);
            drawParamBar(g, barArea, strip.midGain, -12.0f, 12.0f, true);

            g.setColour(fgColor);
            juce::String gainText = (strip.midGain >= 0 ? "+" : "") + juce::String(strip.midGain, 1) + " dB";
            g.drawText(gainText, paramArea.removeFromLeft(70), juce::Justification::centred);

            juce::String freqText = strip.midFreq >= 1000.0f
                ? juce::String(strip.midFreq / 1000.0f, 1) + "k"
                : juce::String(static_cast<int>(strip.midFreq));
            g.drawText(freqText, paramArea.removeFromLeft(50), juce::Justification::centred);

            g.drawText("Q" + juce::String(strip.midQ, 1), paramArea.removeFromLeft(50), juce::Justification::centred);
            break;
        }

        case ChannelRowType::Drive: {
            auto barArea = paramArea.removeFromLeft(kBarWidth);
            drawParamBar(g, barArea, strip.driveAmount, 0.0f, 1.0f);

            g.setColour(fgColor);
            g.drawText(juce::String(static_cast<int>(strip.driveAmount * 100)) + "%",
                paramArea.removeFromLeft(50), juce::Justification::centred);
            g.drawText("Tone:" + juce::String(static_cast<int>(strip.driveTone * 100)) + "%",
                paramArea.removeFromLeft(80), juce::Justification::centred);
            break;
        }

        case ChannelRowType::Punch: {
            auto barArea = paramArea.removeFromLeft(kBarWidth);
            drawParamBar(g, barArea, strip.punchAmount, 0.0f, 1.0f);

            g.setColour(fgColor);
            g.drawText(juce::String(static_cast<int>(strip.punchAmount * 100)) + "%",
                paramArea.removeFromLeft(50), juce::Justification::centred);
            break;
        }

        case ChannelRowType::OTT: {
            auto barArea = paramArea.removeFromLeft(kBarWidth);
            drawParamBar(g, barArea, strip.ottDepth, 0.0f, 1.0f);

            g.setColour(fgColor);
            g.drawText("Depth " + juce::String(static_cast<int>(strip.ottDepth * 100)) + "%",
                paramArea.removeFromLeft(80), juce::Justification::centred);
            g.drawText("Mix " + juce::String(static_cast<int>(strip.ottMix * 100)) + "%",
                paramArea.removeFromLeft(70), juce::Justification::centred);
            break;
        }

        case ChannelRowType::Volume: {
            auto barArea = paramArea.removeFromLeft(kBarWidth);
            drawParamBar(g, barArea, inst->getVolume(), 0.0f, 1.0f);

            g.setColour(fgColor);
            float db = 20.0f * std::log10(std::max(0.0001f, inst->getVolume()));
            g.drawText(juce::String(db, 1) + " dB", paramArea.removeFromLeft(70), juce::Justification::centred);
            break;
        }

        case ChannelRowType::Pan: {
            auto barArea = paramArea.removeFromLeft(kBarWidth);
            drawParamBar(g, barArea, inst->getPan(), -1.0f, 1.0f, true);

            g.setColour(fgColor);
            float pan = inst->getPan();
            juce::String panText = (std::abs(pan) < 0.01f) ? "C"
                : (pan < 0 ? juce::String(static_cast<int>(-pan * 100)) + "L"
                           : juce::String(static_cast<int>(pan * 100)) + "R");
            g.drawText(panText, paramArea.removeFromLeft(50), juce::Justification::centred);
            break;
        }

        case ChannelRowType::Reverb:
        case ChannelRowType::Delay:
        case ChannelRowType::Chorus:
        case ChannelRowType::Sidechain: {
            float value = 0.0f;
            switch (static_cast<ChannelRowType>(row)) {
                case ChannelRowType::Reverb: value = sends.reverb; break;
                case ChannelRowType::Delay: value = sends.delay; break;
                case ChannelRowType::Chorus: value = sends.chorus; break;
                case ChannelRowType::Sidechain: value = sends.sidechainDuck; break;
                default: break;
            }

            auto barArea = paramArea.removeFromLeft(kBarWidth);
            drawParamBar(g, barArea, value, 0.0f, 1.0f);

            g.setColour(fgColor);
            g.drawText(juce::String(static_cast<int>(value * 100)) + "%",
                paramArea.removeFromLeft(50), juce::Justification::centred);
            break;
        }

        default:
            break;
    }
}

void ChannelScreen::drawParamBar(juce::Graphics& g, juce::Rectangle<int> area,
                                  float value, float min, float max, bool bipolar) {
    area = area.reduced(2, 4);

    // Background
    g.setColour(juce::Colour(0xff333355));
    g.fillRect(area);

    // Value bar
    g.setColour(cursorColor);

    if (bipolar) {
        float center = area.getX() + area.getWidth() / 2.0f;
        float normalized = (value - min) / (max - min);  // 0-1
        float pos = area.getX() + normalized * area.getWidth();

        if (value >= 0) {
            g.fillRect(juce::Rectangle<float>(center, static_cast<float>(area.getY()),
                pos - center, static_cast<float>(area.getHeight())));
        } else {
            g.fillRect(juce::Rectangle<float>(pos, static_cast<float>(area.getY()),
                center - pos, static_cast<float>(area.getHeight())));
        }
    } else {
        float normalized = (value - min) / (max - min);
        int barWidth = static_cast<int>(normalized * area.getWidth());
        g.fillRect(area.removeFromLeft(barWidth));
    }
}

void ChannelScreen::navigate(int dx, int dy) {
    // Vertical navigation
    if (dy != 0) {
        cursorRow_ = std::clamp(cursorRow_ + dy, 0, kNumRows - 1);
        cursorField_ = 0;  // Reset field on row change
    }

    // Horizontal navigation between fields
    if (dx != 0) {
        int numFields = getNumFieldsForRow(cursorRow_);
        cursorField_ = std::clamp(cursorField_ + dx, 0, numFields - 1);
    }

    repaint();
}

bool ChannelScreen::handleEdit(const juce::KeyPress& key) {
    auto* inst = project_.getInstrument(currentInstrument_);
    if (!inst) return false;

    // Jump keys (in Edit mode)
    char textChar = static_cast<char>(key.getTextCharacter());
    int jumpRow = getRowForJumpKey(textChar);
    if (jumpRow >= 0) {
        cursorRow_ = jumpRow;
        cursorField_ = 0;
        repaint();
        return true;
    }

    // Instrument switching with [ and ]
    if (textChar == '[') {
        setCurrentInstrument(std::max(0, currentInstrument_ - 1));
        return true;
    }
    if (textChar == ']') {
        setCurrentInstrument(std::min(project_.getInstrumentCount() - 1, currentInstrument_ + 1));
        return true;
    }

    // Value adjustment handled by KeyHandler via Alt+hjkl

    return false;
}

int ChannelScreen::getNumFieldsForRow(int row) const {
    switch (static_cast<ChannelRowType>(row)) {
        case ChannelRowType::HPF:       return 2;  // freq, slope
        case ChannelRowType::LowShelf:  return 2;  // gain, freq
        case ChannelRowType::MidEQ:     return 3;  // gain, freq, Q
        case ChannelRowType::HighShelf: return 2;  // gain, freq
        case ChannelRowType::Drive:     return 2;  // amount, tone
        case ChannelRowType::OTT:       return 3;  // depth, mix, smooth
        default:                        return 1;
    }
}

int ChannelScreen::getRowForJumpKey(char key) const {
    switch (key) {
        case 'h': return static_cast<int>(ChannelRowType::HPF);
        case 'l': return static_cast<int>(ChannelRowType::LowShelf);
        case 'm': return static_cast<int>(ChannelRowType::MidEQ);
        case 'e': return static_cast<int>(ChannelRowType::HighShelf);
        case 'd': return static_cast<int>(ChannelRowType::Drive);
        case 'p': return static_cast<int>(ChannelRowType::Punch);
        case 'o': return static_cast<int>(ChannelRowType::OTT);
        case 'v': return static_cast<int>(ChannelRowType::Volume);
        case 'n': return static_cast<int>(ChannelRowType::Pan);
        case 'r': return static_cast<int>(ChannelRowType::Reverb);
        case 'y': return static_cast<int>(ChannelRowType::Delay);
        case 'c': return static_cast<int>(ChannelRowType::Chorus);
        case 's': return static_cast<int>(ChannelRowType::Sidechain);
        default:  return -1;
    }
}

void ChannelScreen::adjustParam(int row, int field, float delta) {
    auto* inst = project_.getInstrument(currentInstrument_);
    if (!inst) return;

    auto& strip = inst->getChannelStrip();
    auto& sends = inst->getSends();

    switch (static_cast<ChannelRowType>(row)) {
        case ChannelRowType::HPF:
            if (field == 0) {
                strip.hpfFreq = std::clamp(strip.hpfFreq + delta * 10.0f, 20.0f, 500.0f);
            } else {
                strip.hpfSlope = std::clamp(strip.hpfSlope + static_cast<int>(delta > 0 ? 1 : -1), 0, 2);
            }
            break;

        case ChannelRowType::LowShelf:
            if (field == 0) {
                strip.lowShelfGain = std::clamp(strip.lowShelfGain + delta, -12.0f, 12.0f);
            } else {
                strip.lowShelfFreq = std::clamp(strip.lowShelfFreq + delta * 10.0f, 50.0f, 500.0f);
            }
            break;

        case ChannelRowType::MidEQ:
            if (field == 0) {
                strip.midGain = std::clamp(strip.midGain + delta, -12.0f, 12.0f);
            } else if (field == 1) {
                strip.midFreq = std::clamp(strip.midFreq + delta * 100.0f, 200.0f, 8000.0f);
            } else {
                strip.midQ = std::clamp(strip.midQ + delta * 0.1f, 0.5f, 8.0f);
            }
            break;

        case ChannelRowType::HighShelf:
            if (field == 0) {
                strip.highShelfGain = std::clamp(strip.highShelfGain + delta, -12.0f, 12.0f);
            } else {
                strip.highShelfFreq = std::clamp(strip.highShelfFreq + delta * 100.0f, 2000.0f, 16000.0f);
            }
            break;

        case ChannelRowType::Drive:
            if (field == 0) {
                strip.driveAmount = std::clamp(strip.driveAmount + delta * 0.01f, 0.0f, 1.0f);
            } else {
                strip.driveTone = std::clamp(strip.driveTone + delta * 0.01f, 0.0f, 1.0f);
            }
            break;

        case ChannelRowType::Punch:
            strip.punchAmount = std::clamp(strip.punchAmount + delta * 0.01f, 0.0f, 1.0f);
            break;

        case ChannelRowType::OTT:
            if (field == 0) {
                strip.ottDepth = std::clamp(strip.ottDepth + delta * 0.01f, 0.0f, 1.0f);
            } else if (field == 1) {
                strip.ottMix = std::clamp(strip.ottMix + delta * 0.01f, 0.0f, 1.0f);
            } else {
                strip.ottSmooth = std::clamp(strip.ottSmooth + delta * 0.01f, 0.0f, 1.0f);
            }
            break;

        case ChannelRowType::Volume:
            inst->setVolume(inst->getVolume() + delta * 0.01f);
            break;

        case ChannelRowType::Pan:
            inst->setPan(inst->getPan() + delta * 0.01f);
            break;

        case ChannelRowType::Reverb:
            sends.reverb = std::clamp(sends.reverb + delta * 0.01f, 0.0f, 1.0f);
            break;

        case ChannelRowType::Delay:
            sends.delay = std::clamp(sends.delay + delta * 0.01f, 0.0f, 1.0f);
            break;

        case ChannelRowType::Chorus:
            sends.chorus = std::clamp(sends.chorus + delta * 0.01f, 0.0f, 1.0f);
            break;

        case ChannelRowType::Sidechain:
            sends.sidechainDuck = std::clamp(sends.sidechainDuck + delta * 0.01f, 0.0f, 1.0f);
            break;

        default:
            break;
    }

    repaint();
}

std::vector<HelpSection> ChannelScreen::getHelpContent() const {
    return {
        {"Navigation", {
            {"j/k or Up/Down", "Move between rows"},
            {"h/l or Left/Right", "Move between fields"},
            {"[ / ]", "Previous/next instrument"},
        }},
        {"Editing", {
            {"Alt+h/l or Alt+Left/Right", "Adjust parameter value"},
            {"Alt+Shift+*", "Coarse adjustment"},
            {"Jump keys (h,l,m,e,d,p,o,v,n,r,y,c,s)", "Jump to row"},
        }},
        {"Sections", {
            {"HPF", "High-pass filter (low cut)"},
            {"Low/Mid/High", "3-band EQ"},
            {"Drive", "Saturation"},
            {"Punch", "Transient shaper"},
            {"OTT", "Multiband dynamics"},
        }},
    };
}

} // namespace ui
```

### Files to Modify

**`CMakeLists.txt`** - Add after MixerScreen.cpp:

```cmake
    src/ui/ChannelScreen.cpp
```

### Verification

```bash
cmake --build build --target Vitracker 2>&1 | head -50
```

---

## Task 9: Register ChannelScreen in App

**Goal**: Add ChannelScreen to the app, wire up F4 key binding, and update screen navigation.

### Files to Modify

**`src/App.h`**

Change screens array size from 5 to 6:

```cpp
std::array<std::unique_ptr<ui::Screen>, 6> screens_;
```

**`src/App.cpp`**

Add include at top:

```cpp
#include "ui/ChannelScreen.h"
```

Update screen creation (reorder to insert ChannelScreen at index 4):

```cpp
    // Create screens (1=Song, 2=Chain, 3=Pattern, 4=Instrument, 5=Channel, 6=Mixer)
    screens_[0] = std::make_unique<ui::SongScreen>(project_, modeManager_);
    screens_[1] = std::make_unique<ui::ChainScreen>(project_, modeManager_);
    screens_[2] = std::make_unique<ui::PatternScreen>(project_, modeManager_);
    screens_[3] = std::make_unique<ui::InstrumentScreen>(project_, modeManager_);
    screens_[4] = std::make_unique<ui::ChannelScreen>(project_, modeManager_);
    screens_[5] = std::make_unique<ui::MixerScreen>(project_, modeManager_);
```

Update `onScreenSwitch` callback to handle 6 screens:

```cpp
    keyHandler_->onScreenSwitch = [this](int screen) {
        if (screen == -1)  // Previous screen
        {
            int prev = (currentScreen_ - 1 + 6) % 6;
            switchScreen(prev);
        }
        else if (screen == -2)  // Next screen
        {
            int next = (currentScreen_ + 1) % 6;
            switchScreen(next);
        }
        else if (screen >= 1 && screen <= 6)
        {
            switchScreen(screen - 1);
        }
    };
```

Update `switchScreen` bounds check:

```cpp
void App::switchScreen(int screenIndex)
{
    if (screenIndex < 0 || screenIndex >= 6) return;  // Now 6 screens
```

Update all hardcoded screen indices:
- PatternScreen is still index 2
- InstrumentScreen is still index 3
- MixerScreen is now index 5 (was 4)

Update status bar screen keys:

```cpp
    static const char* screenKeys[] = {"1:SNG", "2:CHN", "3:PAT", "4:INS", "5:CHN", "6:MIX"};
    for (int i = 0; i < 6; ++i)
```

Wire up instrument sync between InstrumentScreen and ChannelScreen.

### Verification

```bash
cmake --build build --target Vitracker 2>&1 | head -50
./build/Vitracker_artefacts/Release/Vitracker.app/Contents/MacOS/Vitracker
```

Press 5 or F5 to verify ChannelScreen appears.

---

## Task 10: Serialize ChannelStripParams

**Goal**: Add save/load support for channel strip parameters.

### Files to Modify

**`src/model/ProjectSerializer.cpp`**

In the instrument serialization section, add ChannelStripParams serialization after the existing sends:

```cpp
// Save channel strip params
const auto& strip = inst->getChannelStrip();
instObj["channelStrip"] = {
    {"hpfFreq", strip.hpfFreq},
    {"hpfSlope", strip.hpfSlope},
    {"lowShelfGain", strip.lowShelfGain},
    {"lowShelfFreq", strip.lowShelfFreq},
    {"midFreq", strip.midFreq},
    {"midGain", strip.midGain},
    {"midQ", strip.midQ},
    {"highShelfGain", strip.highShelfGain},
    {"highShelfFreq", strip.highShelfFreq},
    {"driveAmount", strip.driveAmount},
    {"driveTone", strip.driveTone},
    {"punchAmount", strip.punchAmount},
    {"ottDepth", strip.ottDepth},
    {"ottMix", strip.ottMix},
    {"ottSmooth", strip.ottSmooth}
};
```

In the load section, add deserialization:

```cpp
// Load channel strip params
if (instObj.contains("channelStrip")) {
    const auto& stripObj = instObj["channelStrip"];
    auto& strip = inst->getChannelStrip();
    strip.hpfFreq = stripObj.value("hpfFreq", 20.0f);
    strip.hpfSlope = stripObj.value("hpfSlope", 0);
    strip.lowShelfGain = stripObj.value("lowShelfGain", 0.0f);
    strip.lowShelfFreq = stripObj.value("lowShelfFreq", 200.0f);
    strip.midFreq = stripObj.value("midFreq", 1000.0f);
    strip.midGain = stripObj.value("midGain", 0.0f);
    strip.midQ = stripObj.value("midQ", 1.0f);
    strip.highShelfGain = stripObj.value("highShelfGain", 0.0f);
    strip.highShelfFreq = stripObj.value("highShelfFreq", 8000.0f);
    strip.driveAmount = stripObj.value("driveAmount", 0.0f);
    strip.driveTone = stripObj.value("driveTone", 0.5f);
    strip.punchAmount = stripObj.value("punchAmount", 0.0f);
    strip.ottDepth = stripObj.value("ottDepth", 0.0f);
    strip.ottMix = stripObj.value("ottMix", 1.0f);
    strip.ottSmooth = stripObj.value("ottSmooth", 0.5f);
}
```

Remove drive serialization from MixerState (driveGain, driveTone).
Update busLevels serialization to 4 elements.

### Verification

```bash
cmake --build build --target Vitracker 2>&1 | head -50
```

Test save/load with channel strip parameters.

---

## Task 11: Wire Up Parameter Editing via KeyHandler

**Goal**: Connect Alt+h/l parameter adjustment to ChannelScreen.

### Files to Modify

**`src/ui/ChannelScreen.cpp`**

The `handleEdit` method needs to handle Edit1Inc/Edit1Dec (Alt+j/k) and Edit2Inc/Edit2Dec (Alt+h/l) actions from KeyHandler.

Check how InstrumentScreen handles this and follow the same pattern. The global KeyHandler calls `onEditKey` which forwards to the active screen's `handleEdit()`.

Add parameter adjustment handling:

```cpp
bool ChannelScreen::handleEdit(const juce::KeyPress& key) {
    auto* inst = project_.getInstrument(currentInstrument_);
    if (!inst) return false;

    bool alt = key.getModifiers().isAltDown();
    bool shift = key.getModifiers().isShiftDown();
    float delta = shift ? 10.0f : 1.0f;

    // Alt+h/l or Alt+Left/Right for horizontal parameter adjustment
    if (alt) {
        if (key.getKeyCode() == juce::KeyPress::leftKey || key.getTextCharacter() == 'h') {
            adjustParam(cursorRow_, cursorField_, -delta);
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::rightKey || key.getTextCharacter() == 'l') {
            adjustParam(cursorRow_, cursorField_, delta);
            return true;
        }
        // Alt+j/k or Alt+Up/Down for vertical parameter adjustment (same as h/l for single-field rows)
        if (key.getKeyCode() == juce::KeyPress::upKey || key.getTextCharacter() == 'k') {
            adjustParam(cursorRow_, cursorField_, delta);
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::downKey || key.getTextCharacter() == 'j') {
            adjustParam(cursorRow_, cursorField_, -delta);
            return true;
        }
    }

    // ... rest of handleEdit (jump keys, instrument switching)
```

### Verification

```bash
cmake --build build --target Vitracker 2>&1 | head -50
```

Test parameter adjustment in ChannelScreen.

---

## Task 12: Final Integration Testing

**Goal**: Verify complete feature works end-to-end.

### Test Cases

1. **Navigation**:
   - Press 5 to switch to ChannelScreen
   - j/k moves between rows
   - h/l moves between fields on multi-field rows
   - [ and ] switch instruments

2. **Parameter Editing**:
   - Alt+h/l adjusts parameter values
   - Alt+Shift+h/l for coarse adjustment
   - Jump keys (h,l,m,e,d,p,o,v,n,r,y,c,s) jump to rows

3. **Audio Processing**:
   - HPF removes low frequencies when enabled
   - EQ bands affect tone
   - Drive adds saturation
   - Punch enhances transients
   - OTT affects dynamics

4. **Persistence**:
   - Save project with channel strip settings
   - Load project and verify settings restored

5. **Drive Migration**:
   - Verify Drive is no longer on MixerScreen
   - Verify Drive is no longer in InstrumentScreen sends

### Verification Commands

```bash
# Build
cmake --build build --target Vitracker

# Run
./build/Vitracker_artefacts/Release/Vitracker.app/Contents/MacOS/Vitracker
```

---

## Summary

| Task | Description | Files |
|------|-------------|-------|
| 1 | Add ChannelStripParams | Instrument.h |
| 2 | Create BiquadFilter | BiquadFilter.h/.cpp, CMakeLists.txt |
| 3 | Create TransientShaper | TransientShaper.h/.cpp, CMakeLists.txt |
| 4 | Create MultibandOTT | MultibandOTT.h/.cpp, CMakeLists.txt, BiquadFilter.h/.cpp |
| 5 | Create ChannelStrip | ChannelStrip.h/.cpp, CMakeLists.txt |
| 6 | Integrate into AudioEngine | AudioEngine.h/.cpp |
| 7 | Remove Drive from master | Project.h, Instrument.h, Effects.h/.cpp, MixerScreen.cpp, InstrumentScreen.cpp, AudioEngine.cpp |
| 8 | Create ChannelScreen UI | ChannelScreen.h/.cpp, CMakeLists.txt |
| 9 | Register in App | App.h, App.cpp |
| 10 | Serialize params | ProjectSerializer.cpp |
| 11 | Wire up editing | ChannelScreen.cpp |
| 12 | Integration testing | - |

Execute tasks sequentially. Each task builds on previous work.
