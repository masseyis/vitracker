#include "Effects.h"
#include <algorithm>

namespace audio {

// ============ REVERB ============

void Reverb::init(double sampleRate)
{
    const int combDelays[] = {1116, 1188, 1277, 1356};
    const int allpassDelays[] = {556, 441};

    for (int i = 0; i < NUM_COMBS; ++i)
    {
        int size = static_cast<int>(combDelays[i] * sampleRate / 44100.0);
        combBuffersL_[i].resize(static_cast<size_t>(size), 0.0f);
        combBuffersR_[i].resize(static_cast<size_t>(size), 0.0f);
        combIndices_[i] = 0;
        combFilters_[i] = 0.0f;
    }

    for (int i = 0; i < NUM_ALLPASS; ++i)
    {
        int size = static_cast<int>(allpassDelays[i] * sampleRate / 44100.0);
        allpassBuffersL_[i].resize(static_cast<size_t>(size), 0.0f);
        allpassBuffersR_[i].resize(static_cast<size_t>(size), 0.0f);
        allpassIndices_[i] = 0;
    }
}

void Reverb::setParams(float size, float damping, float mix)
{
    size_ = size;
    damping_ = damping;
    mix_ = mix;
}

void Reverb::process(float& left, float& right)
{
    float inputL = left;
    float inputR = right;
    float outL = 0.0f;
    float outR = 0.0f;

    float feedback = 0.7f + size_ * 0.28f;
    float damp = damping_ * 0.4f;

    // Comb filters
    for (int i = 0; i < NUM_COMBS; ++i)
    {
        auto& bufL = combBuffersL_[i];
        auto& bufR = combBuffersR_[i];
        int& idx = combIndices_[i];
        float& filt = combFilters_[i];

        float readL = bufL[static_cast<size_t>(idx)];
        float readR = bufR[static_cast<size_t>(idx)];

        filt = readL * (1.0f - damp) + filt * damp;

        bufL[static_cast<size_t>(idx)] = inputL + filt * feedback;
        bufR[static_cast<size_t>(idx)] = inputR + readR * (1.0f - damp) * feedback;

        outL += readL;
        outR += readR;

        idx = (idx + 1) % static_cast<int>(bufL.size());
    }

    outL /= NUM_COMBS;
    outR /= NUM_COMBS;

    // Allpass filters
    for (int i = 0; i < NUM_ALLPASS; ++i)
    {
        auto& bufL = allpassBuffersL_[i];
        auto& bufR = allpassBuffersR_[i];
        int& idx = allpassIndices_[i];

        float readL = bufL[static_cast<size_t>(idx)];
        float readR = bufR[static_cast<size_t>(idx)];

        bufL[static_cast<size_t>(idx)] = outL + readL * 0.5f;
        bufR[static_cast<size_t>(idx)] = outR + readR * 0.5f;

        outL = readL - outL * 0.5f;
        outR = readR - outR * 0.5f;

        idx = (idx + 1) % static_cast<int>(bufL.size());
    }

    left = inputL * (1.0f - mix_) + outL * mix_;
    right = inputR * (1.0f - mix_) + outR * mix_;
}

// ============ DELAY ============

void Delay::init(double sampleRate)
{
    sampleRate_ = sampleRate;
    int maxDelay = static_cast<int>(sampleRate * 2.0);  // 2 seconds max
    bufferL_.resize(static_cast<size_t>(maxDelay), 0.0f);
    bufferR_.resize(static_cast<size_t>(maxDelay), 0.0f);
    writeIndex_ = 0;
}

void Delay::setParams(float time, float feedback, float mix)
{
    time_ = time;
    feedback_ = feedback;
    mix_ = mix;

    // Calculate delay in samples based on tempo sync
    // time 0-1 maps to 1/16 to 1/1 note
    float beatFraction = 0.0625f * std::pow(2.0f, time_ * 4.0f);  // 1/16 to 1/1
    float delaySeconds = (60.0f / bpm_) * beatFraction;
    delaySamples_ = static_cast<int>(delaySeconds * static_cast<float>(sampleRate_));
    delaySamples_ = std::clamp(delaySamples_, 1, static_cast<int>(bufferL_.size()) - 1);
}

void Delay::setTempo(float bpm)
{
    bpm_ = bpm;
    setParams(time_, feedback_, mix_);  // Recalculate delay
}

void Delay::process(float& left, float& right)
{
    int bufSize = static_cast<int>(bufferL_.size());
    int readIndex = (writeIndex_ - delaySamples_ + bufSize) % bufSize;

    float delayedL = bufferL_[static_cast<size_t>(readIndex)];
    float delayedR = bufferR_[static_cast<size_t>(readIndex)];

    bufferL_[static_cast<size_t>(writeIndex_)] = left + delayedL * feedback_;
    bufferR_[static_cast<size_t>(writeIndex_)] = right + delayedR * feedback_;

    writeIndex_ = (writeIndex_ + 1) % bufSize;

    left = left * (1.0f - mix_) + delayedL * mix_;
    right = right * (1.0f - mix_) + delayedR * mix_;
}

// ============ CHORUS ============

void Chorus::init(double sampleRate)
{
    sampleRate_ = sampleRate;
    int size = static_cast<int>(sampleRate * 0.05);  // 50ms buffer
    bufferL_.resize(static_cast<size_t>(size), 0.0f);
    bufferR_.resize(static_cast<size_t>(size), 0.0f);
    writeIndex_ = 0;
    phase_ = 0.0f;
}

void Chorus::setParams(float rate, float depth, float mix)
{
    rate_ = rate;
    depth_ = depth;
    mix_ = mix;
}

void Chorus::process(float& left, float& right)
{
    // LFO
    float lfoL = std::sin(phase_ * 2.0f * 3.14159f);
    float lfoR = std::sin((phase_ + 0.25f) * 2.0f * 3.14159f);

    // Update phase
    float freq = 0.1f + rate_ * 5.0f;  // 0.1 - 5.1 Hz
    phase_ += static_cast<float>(freq / sampleRate_);
    if (phase_ >= 1.0f) phase_ -= 1.0f;

    // Calculate delay times
    float baseDelay = 0.007f * static_cast<float>(sampleRate_);  // 7ms
    float modDepth = depth_ * 0.003f * static_cast<float>(sampleRate_);  // +/- 3ms

    float delayL = baseDelay + lfoL * modDepth;
    float delayR = baseDelay + lfoR * modDepth;

    // Write to buffer
    bufferL_[static_cast<size_t>(writeIndex_)] = left;
    bufferR_[static_cast<size_t>(writeIndex_)] = right;

    // Read with interpolation
    auto readInterp = [](const std::vector<float>& buf, int writeIdx, float delay) {
        int size = static_cast<int>(buf.size());
        float readPos = static_cast<float>(writeIdx) - delay;
        while (readPos < 0) readPos += static_cast<float>(size);

        int idx0 = static_cast<int>(readPos) % size;
        int idx1 = (idx0 + 1) % size;
        float frac = readPos - std::floor(readPos);

        return buf[static_cast<size_t>(idx0)] * (1.0f - frac) + buf[static_cast<size_t>(idx1)] * frac;
    };

    float chorusL = readInterp(bufferL_, writeIndex_, delayL);
    float chorusR = readInterp(bufferR_, writeIndex_, delayR);

    writeIndex_ = (writeIndex_ + 1) % static_cast<int>(bufferL_.size());

    left = left * (1.0f - mix_) + chorusL * mix_;
    right = right * (1.0f - mix_) + chorusR * mix_;
}

// ============ DRIVE ============

void Drive::setParams(float gain, float tone)
{
    gain_ = gain;
    tone_ = tone;
}

void Drive::process(float& left, float& right)
{
    float driveAmount = 1.0f + gain_ * 10.0f;

    // Soft clip
    auto softClip = [](float x, float drive) {
        x *= drive;
        return x / (1.0f + std::abs(x));
    };

    left = softClip(left, driveAmount);
    right = softClip(right, driveAmount);

    // Simple tone filter (low pass)
    float cutoff = 0.3f + tone_ * 0.7f;
    lpState_ = lpState_ + cutoff * (left - lpState_);
    left = lpState_ * tone_ + left * (1.0f - tone_ * 0.5f);

    // Normalize output
    left *= 1.0f / driveAmount;
    right *= 1.0f / driveAmount;
}

// ============ SIDECHAIN ============

void Sidechain::init(double sampleRate)
{
    sampleRate_ = sampleRate;
    envelope_ = 0.0f;
}

void Sidechain::trigger()
{
    envelope_ = 1.0f;
}

void Sidechain::process(float& left, float& right, float duckAmount)
{
    // Envelope follower
    float target = 0.0f;
    float coeff = (envelope_ > target) ? release_ : attack_;

    envelope_ = envelope_ + (target - envelope_) * (1.0f - std::exp(-1.0f / (coeff * static_cast<float>(sampleRate_))));

    // Apply ducking
    float gain = 1.0f - envelope_ * duckAmount;
    left *= gain;
    right *= gain;
}

// ============ EFFECTS PROCESSOR ============

void EffectsProcessor::init(double sampleRate)
{
    reverb.init(sampleRate);
    delay.init(sampleRate);
    chorus.init(sampleRate);
    sidechain.init(sampleRate);

    // Default settings
    reverb.setParams(0.5f, 0.5f, 0.3f);
    delay.setParams(0.5f, 0.4f, 0.3f);
    chorus.setParams(0.5f, 0.5f, 0.3f);
    drive.setParams(0.3f, 0.5f);
}

void EffectsProcessor::setTempo(float bpm)
{
    delay.setTempo(bpm);
}

void EffectsProcessor::process(float& left, float& right,
                               float reverbSend, float delaySend, float chorusSend,
                               float driveSend, float sidechainDuck)
{
    float dryL = left;
    float dryR = right;

    // Process each effect with send level
    if (reverbSend > 0.001f)
    {
        float wetL = dryL * reverbSend;
        float wetR = dryR * reverbSend;
        reverb.process(wetL, wetR);
        left += wetL;
        right += wetR;
    }

    if (delaySend > 0.001f)
    {
        float wetL = dryL * delaySend;
        float wetR = dryR * delaySend;
        delay.process(wetL, wetR);
        left += wetL;
        right += wetR;
    }

    if (chorusSend > 0.001f)
    {
        float wetL = dryL * chorusSend;
        float wetR = dryR * chorusSend;
        chorus.process(wetL, wetR);
        left += wetL;
        right += wetR;
    }

    if (driveSend > 0.001f)
    {
        float wetL = left * driveSend;
        float wetR = right * driveSend;
        drive.process(wetL, wetR);
        left = left * (1.0f - driveSend) + wetL;
        right = right * (1.0f - driveSend) + wetR;
    }

    if (sidechainDuck > 0.001f)
    {
        sidechain.process(left, right, sidechainDuck);
    }
}

} // namespace audio
