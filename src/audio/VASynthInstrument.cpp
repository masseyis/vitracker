// VASynthInstrument implementation

#include "VASynthInstrument.h"
#include <cmath>
#include <algorithm>

namespace audio {

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
    for (auto& voice : voices_) {
        voice.init(static_cast<float>(sampleRate));
    }
    modMatrix_.init();
    modMatrix_.setTempo(tempo_);
}

void VASynthInstrument::setSampleRate(double sampleRate) {
    sampleRate_ = sampleRate;
    for (auto& voice : voices_) {
        voice.setSampleRate(static_cast<float>(sampleRate));
    }
}

void VASynthInstrument::noteOn(int note, float velocity) {
    model::Step emptyStep;
    noteOnWithFX(note, velocity, emptyStep);
}

void VASynthInstrument::noteOnWithFX(int note, float velocity, const model::Step& step) {
    if (!instrument_) return;

    const auto& params = instrument_->getVAParams();

    // Handle mono mode
    if (params.monoMode) {
        // In mono mode, retrigger existing voice or use first voice
        auto* voice = &voices_[0];
        if (voice->isActive()) {
            // Legato: don't reset envelopes
            voice->trigger(note, velocity, params, step);
        } else {
            voice->trigger(note, velocity, params, step);
        }
    } else {
        // Polyphonic mode
        auto* voice = findFreeVoice();
        if (!voice) {
            voice = findVoiceToSteal();
        }

        if (voice) {
            voice->trigger(note, velocity, params, step);
        }
    }

    // Trigger modulation envelopes on first note after silence
    int newActiveCount = 0;
    for (const auto& v : voices_) {
        if (v.isActive()) newActiveCount++;
    }
    if (activeVoiceCount_ == 0 && newActiveCount > 0) {
        modMatrix_.triggerEnvelopes();
    }
    activeVoiceCount_ = newActiveCount;
}

void VASynthInstrument::noteOff(int note) {
    for (auto& voice : voices_) {
        if (voice.isActive() && voice.getNote() == note) {
            voice.release();
        }
    }
}

void VASynthInstrument::allNotesOff() {
    for (auto& voice : voices_) {
        voice.release();
    }
}

void VASynthInstrument::process(float* outL, float* outR, int numSamples) {
    // Clear output buffers
    std::fill(outL, outL + numSamples, 0.0f);
    std::fill(outR, outR + numSamples, 0.0f);

    if (!instrument_) return;

    const auto& params = instrument_->getVAParams();

    // Process in chunks to avoid buffer overflow
    int samplesRemaining = numSamples;
    int offset = 0;

    while (samplesRemaining > 0) {
        int blockSize = std::min(samplesRemaining, kMaxBlockSize);

        // Clear temp buffers
        std::fill_n(tempBufferL_.begin(), blockSize, 0.0f);
        std::fill_n(tempBufferR_.begin(), blockSize, 0.0f);

        // Update modulation (once per block)
        updateModulationParams();
        modMatrix_.process(static_cast<float>(sampleRate_), blockSize);

        // Render all voices into temp buffer (mono, we'll duplicate for stereo)
        for (auto& voice : voices_) {
            if (voice.isActive()) {
                for (int i = 0; i < blockSize; ++i) {
                    float sample = voice.process(params);
                    tempBufferL_[static_cast<size_t>(i)] += sample;
                    tempBufferR_[static_cast<size_t>(i)] += sample;
                }
            }
        }

        // Apply modulation (volume, pan, etc.)
        applyModulation(tempBufferL_.data(), tempBufferR_.data(), blockSize);

        // Copy to output
        for (int i = 0; i < blockSize; ++i) {
            outL[offset + i] = tempBufferL_[static_cast<size_t>(i)];
            outR[offset + i] = tempBufferR_[static_cast<size_t>(i)];
        }

        offset += blockSize;
        samplesRemaining -= blockSize;
    }

    // Update active voice count
    activeVoiceCount_ = 0;
    for (const auto& voice : voices_) {
        if (voice.isActive()) activeVoiceCount_++;
    }
}

VASynthVoice* VASynthInstrument::findFreeVoice() {
    // Respect polyphony setting from instrument params
    int maxVoices = NUM_VOICES;
    if (instrument_) {
        maxVoices = std::clamp(instrument_->getVAParams().polyphony, 1, NUM_VOICES);
    }

    for (int i = 0; i < maxVoices; ++i) {
        if (!voices_[static_cast<size_t>(i)].isActive()) {
            return &voices_[static_cast<size_t>(i)];
        }
    }
    return nullptr;
}

VASynthVoice* VASynthInstrument::findVoiceToSteal() {
    // Steal oldest voice within polyphony limit
    int maxVoices = NUM_VOICES;
    if (instrument_) {
        maxVoices = std::clamp(instrument_->getVAParams().polyphony, 1, NUM_VOICES);
    }
    // Simple: steal the first voice within polyphony limit
    // More sophisticated: could track voice ages and steal oldest
    (void)maxVoices;  // Used for polyphony limit, but simple impl just steals voice 0
    return &voices_[0];
}

int64_t VASynthInstrument::getPlayheadPosition() const {
    // VA synth doesn't have a playhead, return -1
    return -1;
}

void VASynthInstrument::setTempo(double bpm) {
    tempo_ = bpm;
    modMatrix_.setTempo(bpm);
    // Update tempo for all voices for tracker FX timing
    for (auto& voice : voices_) {
        voice.setTempo(static_cast<float>(bpm));
    }
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
