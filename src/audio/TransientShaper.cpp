#include "TransientShaper.h"
#include <cmath>
#include <algorithm>

namespace audio {

void TransientShaper::prepare(double sampleRate) {
    sampleRate_ = sampleRate;

    // Fast envelope: ~0.5ms attack, ~5ms release - very snappy for punch
    fastAttack_ = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * 0.0005f));
    fastRelease_ = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * 0.005f));

    // Slow envelope: ~30ms attack, ~300ms release - bigger difference for more punch
    slowAttack_ = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * 0.03f));
    slowRelease_ = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * 0.3f));

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

    // Amplify transient signal for more pronounced detection
    transientL = std::min(transientL * 2.0f, 1.0f);
    transientR = std::min(transientR * 2.0f, 1.0f);

    // Apply gain boost to transient portion (amount controls intensity)
    // Scale: amount=0 -> 1x, amount=1 -> 8x boost for heavy punch
    float boost = 1.0f + amount_ * 7.0f;
    float gainL = 1.0f + transientL * (boost - 1.0f);
    float gainR = 1.0f + transientR * (boost - 1.0f);

    left *= gainL;
    right *= gainR;
}

} // namespace audio
