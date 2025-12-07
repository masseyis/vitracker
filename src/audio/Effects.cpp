#include "Effects.h"
#include <algorithm>

namespace audio {

// ============ REVERB ============

void Reverb::init(double sampleRate)
{
    // Longer delays for a bigger, more natural hall sound (not spring-like)
    // These are in samples at 44100Hz, scaled to actual sample rate
    // Ranging from ~45ms to ~80ms for a medium-large room character
    const int combDelays[] = {2003, 2417, 2719, 3511};  // ~45-80ms at 44100
    const int allpassDelays[] = {997, 773, 617, 449};   // More allpass stages for diffusion

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

void Reverb::setParams(float size, float damping, float /*mix*/)
{
    size_ = size;
    damping_ = damping;
}

void Reverb::process(float& left, float& right)
{
    // Freeverb-style algorithm - outputs 100% wet for send usage
    float inputL = left;
    float inputR = right;

    // Mix input to mono for feeding reverb (creates wider stereo field)
    float input = (inputL + inputR) * 0.5f;

    float outL = 0.0f;
    float outR = 0.0f;

    // Scale feedback by room size (0.7 to 0.98 for good decay)
    float feedback = 0.7f + size_ * 0.28f;
    // Damping controls high frequency absorption
    float damp1 = damping_ * 0.4f;
    float damp2 = 1.0f - damp1;

    // Parallel comb filters (different delays for L/R create stereo width)
    for (int i = 0; i < NUM_COMBS; ++i)
    {
        auto& bufL = combBuffersL_[i];
        auto& bufR = combBuffersR_[i];
        int& idxL = combIndices_[i];

        // Offset R channel index for stereo spread
        int idxR = (idxL + 23) % static_cast<int>(bufR.size());

        float readL = bufL[static_cast<size_t>(idxL)];
        float readR = bufR[static_cast<size_t>(idxR)];

        // Low-pass filter in feedback path (damping)
        combFilters_[i] = readL * damp2 + combFilters_[i] * damp1;

        bufL[static_cast<size_t>(idxL)] = input + combFilters_[i] * feedback;
        bufR[static_cast<size_t>(idxR)] = input + readR * damp2 * feedback;

        outL += readL;
        outR += readR;

        idxL = (idxL + 1) % static_cast<int>(bufL.size());
    }

    // Scale down comb output
    outL *= 0.25f;
    outR *= 0.25f;

    // Series allpass filters for diffusion
    for (int i = 0; i < NUM_ALLPASS; ++i)
    {
        auto& bufL = allpassBuffersL_[i];
        auto& bufR = allpassBuffersR_[i];
        int& idx = allpassIndices_[i];

        float readL = bufL[static_cast<size_t>(idx)];
        float readR = bufR[static_cast<size_t>(idx)];

        float allpassCoeff = 0.5f;
        bufL[static_cast<size_t>(idx)] = outL + readL * allpassCoeff;
        bufR[static_cast<size_t>(idx)] = outR + readR * allpassCoeff;

        outL = readL - outL * allpassCoeff;
        outR = readR - outR * allpassCoeff;

        idx = (idx + 1) % static_cast<int>(bufL.size());
    }

    // Output 100% wet signal (send level controls mix externally)
    left = outL;
    right = outR;
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
    // time 0-1 maps to common musical divisions including dotted notes:
    // 0.0 = 1/16, 0.15 = 1/8, 0.3 = dotted 1/8, 0.45 = 1/4, 0.6 = dotted 1/4, 0.75 = 1/2, 0.9 = dotted 1/2, 1.0 = 1/1
    float beatFraction;
    if (time_ < 0.15f) {
        beatFraction = 0.25f;       // 1/16 note
    } else if (time_ < 0.3f) {
        beatFraction = 0.5f;        // 1/8 note
    } else if (time_ < 0.45f) {
        beatFraction = 0.75f;       // dotted 1/8
    } else if (time_ < 0.6f) {
        beatFraction = 1.0f;        // 1/4 note (crotchet)
    } else if (time_ < 0.75f) {
        beatFraction = 1.5f;        // dotted 1/4 (dotted crotchet)
    } else if (time_ < 0.9f) {
        beatFraction = 2.0f;        // 1/2 note
    } else {
        beatFraction = 3.0f;        // dotted 1/2
    }

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

void Chorus::setParams(float rate, float depth, float /*mix*/)
{
    rate_ = rate;
    depth_ = depth;
}

void Chorus::process(float& left, float& right)
{
    // Classic chorus with multiple voices - outputs 100% wet for send usage
    constexpr float PI = 3.14159265f;

    // Three LFOs with different phases for richer chorus
    float lfo1 = std::sin(phase_ * 2.0f * PI);
    float lfo2 = std::sin((phase_ + 0.33f) * 2.0f * PI);
    float lfo3 = std::sin((phase_ + 0.66f) * 2.0f * PI);

    // Update phase - rate controls LFO speed (0.2 - 3 Hz for classic chorus)
    float freq = 0.2f + rate_ * 2.8f;
    phase_ += static_cast<float>(freq / sampleRate_);
    if (phase_ >= 1.0f) phase_ -= 1.0f;

    // Base delay and modulation depth
    float baseDelay = 0.010f * static_cast<float>(sampleRate_);  // 10ms center
    float modDepth = depth_ * 0.005f * static_cast<float>(sampleRate_);  // +/- 5ms

    // Three delay taps for each channel
    float delayL1 = baseDelay + lfo1 * modDepth;
    float delayL2 = baseDelay * 0.8f + lfo2 * modDepth * 0.7f;
    float delayR1 = baseDelay + lfo2 * modDepth;  // Different LFO for stereo width
    float delayR2 = baseDelay * 1.2f + lfo3 * modDepth * 0.8f;

    // Write to buffer
    bufferL_[static_cast<size_t>(writeIndex_)] = left;
    bufferR_[static_cast<size_t>(writeIndex_)] = right;

    // Read with cubic interpolation for smoother modulation
    auto readInterp = [](const std::vector<float>& buf, int writeIdx, float delay) {
        int size = static_cast<int>(buf.size());
        float readPos = static_cast<float>(writeIdx) - delay;
        while (readPos < 0) readPos += static_cast<float>(size);

        int idx0 = static_cast<int>(readPos) % size;
        int idx1 = (idx0 + 1) % size;
        float frac = readPos - std::floor(readPos);

        // Linear interpolation (could upgrade to cubic for even smoother)
        return buf[static_cast<size_t>(idx0)] * (1.0f - frac) + buf[static_cast<size_t>(idx1)] * frac;
    };

    // Mix multiple chorus voices
    float chorusL = (readInterp(bufferL_, writeIndex_, delayL1) +
                     readInterp(bufferL_, writeIndex_, delayL2)) * 0.5f;
    float chorusR = (readInterp(bufferR_, writeIndex_, delayR1) +
                     readInterp(bufferR_, writeIndex_, delayR2)) * 0.5f;

    writeIndex_ = (writeIndex_ + 1) % static_cast<int>(bufferL_.size());

    // Output 100% wet signal (send level controls mix externally)
    left = chorusL;
    right = chorusR;
}

// ============ DRIVE ============

void Drive::init(double sampleRate)
{
    sampleRate_ = sampleRate;
    lpStateL_ = 0.0f;
    lpStateR_ = 0.0f;
    hpStateL_ = 0.0f;
    hpStateR_ = 0.0f;
}

void Drive::setParams(float gain, float tone)
{
    gain_ = gain;
    tone_ = tone;
}

void Drive::process(float& left, float& right)
{
    // Tube-style saturation with asymmetric clipping
    // Outputs 100% processed signal for send usage

    // Pre-gain (1x to 8x) - reduced range for better control
    float preGain = 1.0f + gain_ * 7.0f;

    // Asymmetric soft saturation (tube-like)
    auto saturate = [](float x, float drive) {
        x *= drive;
        // Asymmetric waveshaper - positive side clips harder
        if (x > 0.0f) {
            // Harder positive clipping (tube-like)
            return std::tanh(x * 1.2f);
        } else {
            // Softer negative clipping
            return std::tanh(x * 0.9f);
        }
    };

    float satL = saturate(left, preGain);
    float satR = saturate(right, preGain);

    // Tone control: low-pass filter (darker when tone is low)
    // Higher tone = more highs preserved
    float lpCoeff = 0.1f + tone_ * 0.85f;  // 0.1 to 0.95
    lpStateL_ += lpCoeff * (satL - lpStateL_);
    lpStateR_ += lpCoeff * (satR - lpStateR_);

    // High-pass to remove DC offset from asymmetric clipping
    float hpCoeff = 0.995f;
    hpStateL_ = hpCoeff * (hpStateL_ + satL - lpStateL_);
    hpStateR_ = hpCoeff * (hpStateR_ + satR - lpStateR_);

    // Mix based on tone: low tone = more filtered, high tone = more original saturation
    float toneBlend = tone_ * 0.7f + 0.3f;  // 0.3 to 1.0
    left = lpStateL_ * (1.0f - toneBlend) + satL * toneBlend;
    right = lpStateR_ * (1.0f - toneBlend) + satR * toneBlend;

    // Aggressive makeup gain to maintain consistent perceived loudness
    // tanh saturation increases RMS, so we compensate more heavily
    // At low gain: minimal compensation, at high gain: significant reduction
    float makeupGain = 1.0f / (1.0f + gain_ * 1.5f);

    // Additional compensation based on input level (louder inputs get more saturation boost)
    // This helps maintain consistent output regardless of input level
    left *= makeupGain;
    right *= makeupGain;
}

// ============ SIDECHAIN ============

void Sidechain::init(double sampleRate)
{
    sampleRate_ = sampleRate;
    envelope_ = 0.0f;
}

void Sidechain::setParams(float attack, float release, float ratio)
{
    attack_ = std::clamp(attack, 0.001f, 0.05f);
    release_ = std::clamp(release, 0.05f, 1.0f);
    ratio_ = std::clamp(ratio, 0.0f, 1.0f);
}

void Sidechain::feedSource(float sourceLevel)
{
    // Envelope follower - follow the source audio level
    float target = std::abs(sourceLevel);

    // Use different coefficients for attack and release
    float coeff = (target > envelope_) ? attack_ : release_;

    // Smooth envelope following
    float alpha = 1.0f - std::exp(-1.0f / (coeff * static_cast<float>(sampleRate_)));
    envelope_ = envelope_ + alpha * (target - envelope_);
}

void Sidechain::process(float& left, float& right)
{
    // Apply ducking based on envelope and ratio
    // ratio_ controls how much to duck (0 = no ducking, 1 = full duck)
    float gain = 1.0f - envelope_ * ratio_;
    gain = std::max(0.0f, gain);  // Clamp to non-negative

    left *= gain;
    right *= gain;
}

// ============ EFFECTS PROCESSOR ============

void EffectsProcessor::init(double sampleRate)
{
    reverb.init(sampleRate);
    delay.init(sampleRate);
    chorus.init(sampleRate);
    drive.init(sampleRate);
    sidechain.init(sampleRate);
    djFilter.init(sampleRate);
    limiter.init(sampleRate);

    // Default settings - good starting points
    // Note: mix parameter is ignored now since effects output 100% wet
    reverb.setParams(0.7f, 0.3f, 1.0f);   // Larger room, less damping for natural sound
    delay.setParams(0.65f, 0.55f, 1.0f);  // Dotted crotchet, feedback for 3+ repeats
    chorus.setParams(0.4f, 0.6f, 1.0f);   // Moderate rate, good depth
    drive.setParams(0.3f, 0.6f);          // Moderate drive, brighter tone
    djFilter.setPosition(0.0f);           // Center = bypass
    limiter.setParams(0.95f, 0.1f);       // Just below clipping, 100ms release
}

void EffectsProcessor::setTempo(float bpm)
{
    delay.setTempo(bpm);
}

void EffectsProcessor::process(float& left, float& right,
                               float reverbSend, float delaySend, float chorusSend)
{
    // Send-based mixing: dry signal preserved, wet signal added on top
    // Full send (1.0) = 100% wet mixed with dry
    float dryL = left;
    float dryR = right;

    // Accumulate wet signals from all effects
    float wetL = 0.0f;
    float wetR = 0.0f;

    // Reverb send
    if (reverbSend > 0.001f)
    {
        float tempL = dryL;
        float tempR = dryR;
        reverb.process(tempL, tempR);  // Returns 100% wet
        wetL += tempL * reverbSend;
        wetR += tempR * reverbSend;
    }

    // Delay send
    if (delaySend > 0.001f)
    {
        float tempL = dryL;
        float tempR = dryR;
        delay.process(tempL, tempR);  // Returns 100% wet
        wetL += tempL * delaySend;
        wetR += tempR * delaySend;
    }

    // Chorus send
    if (chorusSend > 0.001f)
    {
        float tempL = dryL;
        float tempR = dryR;
        chorus.process(tempL, tempR);  // Returns 100% wet
        wetL += tempL * chorusSend;
        wetR += tempR * chorusSend;
    }

    // Mix dry + wet
    left = dryL + wetL;
    right = dryR + wetR;

    // Note: Drive is now per-instrument in ChannelStrip, not a master bus send
    // Note: Sidechain is now handled separately in AudioEngine
    // Call sidechain.feedSource() with source instrument audio
    // Then sidechain.process() to apply ducking
}

void EffectsProcessor::processMaster(float& left, float& right)
{
    // Apply master bus effects: DJ Filter then Limiter
    djFilter.process(left, right);
    limiter.process(left, right);
}

// ============ DJ FILTER ============

void DJFilter::init(double sampleRate)
{
    sampleRate_ = sampleRate;
    position_ = 0.0f;

    // Clear filter states
    for (int i = 0; i < 2; ++i) {
        lpStateL_[i] = lpStateR_[i] = 0.0f;
        hpStateL_[i] = hpStateR_[i] = 0.0f;
    }
}

void DJFilter::setPosition(float position)
{
    position_ = std::clamp(position, -1.0f, 1.0f);
}

void DJFilter::process(float& left, float& right)
{
    // If near center, bypass for efficiency
    if (std::abs(position_) < 0.02f) {
        return;
    }

    constexpr float PI = 3.14159265f;

    if (position_ < 0.0f) {
        // Low-pass filter (negative position)
        // Map -1.0 to 0.0 -> cutoff from 200Hz to 20kHz
        float t = 1.0f + position_;  // 0 to 1
        float cutoff = 200.0f + t * t * t * 19800.0f;  // Cubic response

        // 2-pole Butterworth low-pass
        float omega = 2.0f * PI * cutoff / static_cast<float>(sampleRate_);
        float sn = std::sin(omega);
        float cs = std::cos(omega);
        float alpha = sn / (2.0f * 0.707f);  // Q = 0.707 for Butterworth

        float b0 = (1.0f - cs) / 2.0f;
        float b1 = 1.0f - cs;
        float b2 = (1.0f - cs) / 2.0f;
        float a0 = 1.0f + alpha;
        float a1 = -2.0f * cs;
        float a2 = 1.0f - alpha;

        // Normalize
        b0 /= a0; b1 /= a0; b2 /= a0;
        a1 /= a0; a2 /= a0;

        // Apply filter (direct form II)
        float inL = left;
        float inR = right;

        float outL = b0 * inL + lpStateL_[0];
        lpStateL_[0] = b1 * inL - a1 * outL + lpStateL_[1];
        lpStateL_[1] = b2 * inL - a2 * outL;

        float outR = b0 * inR + lpStateR_[0];
        lpStateR_[0] = b1 * inR - a1 * outR + lpStateR_[1];
        lpStateR_[1] = b2 * inR - a2 * outR;

        left = outL;
        right = outR;
    }
    else {
        // High-pass filter (positive position)
        // Map 0.0 to 1.0 -> cutoff from 20Hz to 5kHz
        float t = position_;
        float cutoff = 20.0f + t * t * t * 4980.0f;  // Cubic response

        // 2-pole Butterworth high-pass
        float omega = 2.0f * PI * cutoff / static_cast<float>(sampleRate_);
        float sn = std::sin(omega);
        float cs = std::cos(omega);
        float alpha = sn / (2.0f * 0.707f);

        float b0 = (1.0f + cs) / 2.0f;
        float b1 = -(1.0f + cs);
        float b2 = (1.0f + cs) / 2.0f;
        float a0 = 1.0f + alpha;
        float a1 = -2.0f * cs;
        float a2 = 1.0f - alpha;

        // Normalize
        b0 /= a0; b1 /= a0; b2 /= a0;
        a1 /= a0; a2 /= a0;

        // Apply filter
        float inL = left;
        float inR = right;

        float outL = b0 * inL + hpStateL_[0];
        hpStateL_[0] = b1 * inL - a1 * outL + hpStateL_[1];
        hpStateL_[1] = b2 * inL - a2 * outL;

        float outR = b0 * inR + hpStateR_[0];
        hpStateR_[0] = b1 * inR - a1 * outR + hpStateR_[1];
        hpStateR_[1] = b2 * inR - a2 * outR;

        left = outL;
        right = outR;
    }
}

// ============ LIMITER ============

void Limiter::init(double sampleRate)
{
    sampleRate_ = sampleRate;
    envelope_ = 0.0f;
    gainReduction_ = 0.0f;
}

void Limiter::setParams(float threshold, float release)
{
    threshold_ = std::clamp(threshold, 0.1f, 1.0f);
    release_ = std::clamp(release, 0.01f, 1.0f);
}

void Limiter::process(float& left, float& right)
{
    // Get peak level
    float peak = std::max(std::abs(left), std::abs(right));

    // Calculate required gain reduction
    float targetGain = 1.0f;
    if (peak > threshold_) {
        targetGain = threshold_ / peak;
    }

    // Attack is instant (brickwall), release is smooth
    if (targetGain < envelope_) {
        envelope_ = targetGain;  // Instant attack
    } else {
        // Smooth release
        float releaseCoeff = std::exp(-1.0f / (release_ * static_cast<float>(sampleRate_)));
        envelope_ = envelope_ * releaseCoeff + targetGain * (1.0f - releaseCoeff);
    }

    // Apply gain
    left *= envelope_;
    right *= envelope_;

    // Store gain reduction for metering (in dB would be 20*log10(envelope_))
    gainReduction_ = 1.0f - envelope_;
}

} // namespace audio
