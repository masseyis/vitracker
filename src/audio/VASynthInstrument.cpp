// VASynthInstrument implementation

#include "VASynthInstrument.h"
#include "Voice.h"
#include <cmath>
#include <algorithm>

namespace audio {

std::unique_ptr<Voice> VASynthInstrument::createVoice() {
    auto voice = std::make_unique<VASynthVoice>();
    voice->setSampleRate(sampleRate_);
    updateVoiceParameters(voice.get());
    return voice;
}

void VASynthInstrument::updateVoiceParameters(Voice* voice) {
    if (!instrument_) return;
    auto* vaVoice = static_cast<VASynthVoice*>(voice);
    vaVoice->updateParameters(instrument_->getVAParams());
}

// Map 0-1 to milliseconds for attack (1ms to 2000ms, exponential)
static float mapAttackMs(float normalized) {
    return 1.0f + std::pow(normalized, 2.0f) * 1999.0f;
}

// Map 0-1 to milliseconds for decay (1ms to 10000ms, exponential)
static float mapDecayMs(float normalized) {
    return 1.0f + std::pow(normalized, 2.0f) * 9999.0f;
}

VASynthInstrument::VASynthInstrument() {
    tempBufferL_.fill(0.0f);
    tempBufferR_.fill(0.0f);
}

void VASynthInstrument::init(double sampleRate) {
    sampleRate_ = sampleRate;
    // Legacy polyphony code disabled (will be removed in Task 11)
    // Voices are now created per-track via createVoice()
    modMatrix_.init();
    modMatrix_.setTempo(tempo_);
}

void VASynthInstrument::setSampleRate(double sampleRate) {
    sampleRate_ = sampleRate;
    // Legacy polyphony code disabled (will be removed in Task 11)
    // Sample rate is set per-voice when created
}

void VASynthInstrument::noteOn(int note, float velocity) {
    model::Step emptyStep;
    noteOnWithFX(note, velocity, emptyStep);
}

void VASynthInstrument::noteOnWithFX(int note, float velocity, const model::Step& step) {
    // STUB: Legacy polyphony code disabled
    // This will be implemented properly in Task 7 using Track-owned voices
    // For now, modulation matrix is triggered but no voice is played
    (void)note;
    (void)velocity;
    (void)step;

    if (activeVoiceCount_ == 0) {
        modMatrix_.triggerEnvelopes();
        activeVoiceCount_ = 1;  // Fake active voice to prevent retriggering
    }
}

void VASynthInstrument::noteOff(int note) {
    // STUB: Will be implemented in Task 7
    (void)note;
    activeVoiceCount_ = 0;
}

void VASynthInstrument::allNotesOff() {
    // STUB: Will be implemented in Task 7
    activeVoiceCount_ = 0;
}

void VASynthInstrument::process(float* outL, float* outR, int numSamples) {
    // STUB: Legacy polyphony code disabled
    // This will be implemented properly in Task 7 using Track-owned voices
    // For now, just clear output and process modulation
    std::fill(outL, outL + numSamples, 0.0f);
    std::fill(outR, outR + numSamples, 0.0f);

    if (!instrument_) return;

    // Update and process modulation (for UI display even if no audio)
    updateModulationParams();
    modMatrix_.process(static_cast<float>(sampleRate_), numSamples);
}

VASynthVoice* VASynthInstrument::findFreeVoice() {
    // STUB: Will be removed in Task 11
    return nullptr;
}

VASynthVoice* VASynthInstrument::findVoiceToSteal() {
    // STUB: Will be removed in Task 11
    return nullptr;
}

int64_t VASynthInstrument::getPlayheadPosition() const {
    // VA synth doesn't have a playhead, return -1
    return -1;
}

void VASynthInstrument::setTempo(double bpm) {
    tempo_ = bpm;
    modMatrix_.setTempo(bpm);
    // Legacy polyphony code removed - tempo will be set on Track-owned voices
}

void VASynthInstrument::updateModulationParams() {
    if (!instrument_) return;

    const auto& mod = instrument_->getVAParams().modulation;

    // Update LFO1
    modMatrix_.getLfo1().SetRate(static_cast<plaits::LfoRateDivision>(mod.lfo1.rateIndex));
    modMatrix_.getLfo1().SetShape(static_cast<plaits::LfoShape>(mod.lfo1.shapeIndex));
    modMatrix_.setDestination(0, mod.lfo1.destIndex);
    modMatrix_.setAmount(0, mod.lfo1.amount);

    // Update LFO2
    modMatrix_.getLfo2().SetRate(static_cast<plaits::LfoRateDivision>(mod.lfo2.rateIndex));
    modMatrix_.getLfo2().SetShape(static_cast<plaits::LfoShape>(mod.lfo2.shapeIndex));
    modMatrix_.setDestination(1, mod.lfo2.destIndex);
    modMatrix_.setAmount(1, mod.lfo2.amount);

    // Update ENV1
    modMatrix_.getEnv1().SetAttack(static_cast<uint16_t>(mapAttackMs(mod.env1.attack)));
    modMatrix_.getEnv1().SetDecay(static_cast<uint16_t>(mapDecayMs(mod.env1.decay)));
    modMatrix_.setDestination(2, mod.env1.destIndex);
    modMatrix_.setAmount(2, mod.env1.amount);

    // Update ENV2
    modMatrix_.getEnv2().SetAttack(static_cast<uint16_t>(mapAttackMs(mod.env2.attack)));
    modMatrix_.getEnv2().SetDecay(static_cast<uint16_t>(mapDecayMs(mod.env2.decay)));
    modMatrix_.setDestination(3, mod.env2.destIndex);
    modMatrix_.setAmount(3, mod.env2.amount);

    modMatrix_.setTempo(tempo_);
}

void VASynthInstrument::applyModulation(float* outL, float* outR, int numSamples) {
    // SamplerModDest indices: Volume=0, Pitch=1, Cutoff=2, Resonance=3, Pan=4
    // Get modulated volume (base 1.0)
    float volumeMod = modMatrix_.getModulation(0);  // Volume
    float volume = std::clamp(1.0f + volumeMod, 0.0f, 2.0f);

    // Get modulated pan (-1 to +1 becomes left/right balance)
    float panMod = modMatrix_.getModulation(4);  // Pan
    float panL = std::clamp(1.0f - panMod, 0.0f, 1.0f);
    float panR = std::clamp(1.0f + panMod, 0.0f, 1.0f);

    // Apply volume and pan
    for (int i = 0; i < numSamples; ++i) {
        outL[i] *= volume * panL;
        outR[i] *= volume * panR;
    }
}

float VASynthInstrument::getModulatedVolume() const {
    float volumeMod = modMatrix_.getModulation(0);  // Volume
    return std::clamp(1.0f + volumeMod, 0.0f, 2.0f);
}

float VASynthInstrument::getModulatedCutoff() const {
    if (!instrument_) return 1.0f;
    float baseCutoff = instrument_->getVAParams().filter.cutoff;
    return modMatrix_.getModulatedValue(2, baseCutoff);  // Cutoff
}

float VASynthInstrument::getModulatedResonance() const {
    if (!instrument_) return 0.0f;
    float baseRes = instrument_->getVAParams().filter.resonance;
    return modMatrix_.getModulatedValue(3, baseRes);  // Resonance
}

} // namespace audio
