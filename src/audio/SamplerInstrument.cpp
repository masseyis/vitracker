#include "SamplerInstrument.h"
#include "../dsp/AudioAnalysis.h"
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

SamplerInstrument::SamplerInstrument() {
    formatManager_.registerBasicFormats();
    tempBufferL_.fill(0.0f);
    tempBufferR_.fill(0.0f);
}

void SamplerInstrument::init(double sampleRate) {
    sampleRate_ = sampleRate;
    for (auto& voice : voices_) {
        voice.setSampleRate(sampleRate);
    }
    modMatrix_.init();
    modMatrix_.setTempo(tempo_);
}

void SamplerInstrument::setSampleRate(double sampleRate) {
    sampleRate_ = sampleRate;
    for (auto& voice : voices_) {
        voice.setSampleRate(sampleRate);
    }
}

void SamplerInstrument::process(float* outL, float* outR, int numSamples) {
    // Clear output buffers
    std::fill(outL, outL + numSamples, 0.0f);
    std::fill(outR, outR + numSamples, 0.0f);

    if (!hasSample() || !instrument_) return;

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

        // Render all voices into temp buffers
        std::vector<float> voiceTempL(blockSize, 0.0f);
        std::vector<float> voiceTempR(blockSize, 0.0f);

        for (auto& voice : voices_) {
            if (voice.isActive()) {
                voice.render(voiceTempL.data(), voiceTempR.data(), blockSize);

                for (int i = 0; i < blockSize; ++i) {
                    tempBufferL_[i] += voiceTempL[i];
                    tempBufferR_[i] += voiceTempR[i];
                }

                // Clear voice temp for next voice
                std::fill(voiceTempL.begin(), voiceTempL.end(), 0.0f);
                std::fill(voiceTempR.begin(), voiceTempR.end(), 0.0f);
            }
        }

        // Apply modulation (volume, pan, etc.)
        applyModulation(tempBufferL_.data(), tempBufferR_.data(), blockSize);

        // Copy to output
        for (int i = 0; i < blockSize; ++i) {
            outL[offset + i] = tempBufferL_[i];
            outR[offset + i] = tempBufferR_[i];
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

void SamplerInstrument::noteOn(int midiNote, float velocity) {
    model::Step emptyStep;
    noteOnWithFX(midiNote, velocity, emptyStep);
}

void SamplerInstrument::noteOnWithFX(int midiNote, float velocity, const model::Step& step) {
    if (!hasSample() || !instrument_) return;

    auto* voice = findFreeVoice();
    if (!voice) {
        voice = findVoiceToSteal();
    }

    if (voice) {
        const auto& params = instrument_->getSamplerParams();
        // Pass both channel pointers (JUCE uses planar format)
        const float* leftData = sampleBuffer_.getReadPointer(0);
        const float* rightData = sampleBuffer_.getNumChannels() > 1
            ? sampleBuffer_.getReadPointer(1)
            : nullptr;
        voice->setSampleData(
            leftData,
            rightData,
            static_cast<size_t>(sampleBuffer_.getNumSamples()),
            loadedSampleRate_
        );
        voice->trigger(midiNote, velocity, params, step);

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
}

void SamplerInstrument::noteOff(int midiNote) {
    for (auto& voice : voices_) {
        if (voice.isActive() && voice.getNote() == midiNote) {
            voice.release();
        }
    }
}

void SamplerInstrument::allNotesOff() {
    for (auto& voice : voices_) {
        voice.release();
    }
}

SamplerVoice* SamplerInstrument::findFreeVoice() {
    for (auto& voice : voices_) {
        if (!voice.isActive()) {
            return &voice;
        }
    }
    return nullptr;
}

SamplerVoice* SamplerInstrument::findVoiceToSteal() {
    // Simple voice stealing: return first voice
    return &voices_[0];
}

int64_t SamplerInstrument::getPlayheadPosition() const {
    // Return the position of the first active voice
    for (const auto& voice : voices_) {
        if (voice.isActive()) {
            return static_cast<int64_t>(voice.getPlayPosition());
        }
    }
    return -1;  // No active voice
}

bool SamplerInstrument::loadSample(const juce::File& file) {
    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager_.createReaderFor(file)
    );

    if (!reader) {
        DBG("Failed to create reader for: " << file.getFullPathName());
        return false;
    }

    loadedSampleRate_ = static_cast<int>(reader->sampleRate);
    const int numChannels = static_cast<int>(reader->numChannels);
    const juce::int64 numSamples = reader->lengthInSamples;

    // Check file size (warn if > 50MB of audio data)
    const juce::int64 dataSize = numSamples * numChannels * sizeof(float);
    if (dataSize > 50 * 1024 * 1024) {
        DBG("Warning: Large sample file (" << (dataSize / (1024 * 1024)) << " MB)");
    }

    sampleBuffer_.setSize(numChannels, static_cast<int>(numSamples));
    reader->read(&sampleBuffer_, 0, static_cast<int>(numSamples), 0, true, true);

    // Update instrument's sample ref and detect pitch
    if (instrument_) {
        auto& params = instrument_->getSamplerParams();
        auto& sampleRef = params.sample;
        sampleRef.path = file.getFullPathName().toStdString();
        sampleRef.numChannels = numChannels;
        sampleRef.sampleRate = loadedSampleRate_;
        sampleRef.numSamples = static_cast<size_t>(numSamples);

        // Detect pitch for auto-repitch
        float detectedPitch = dsp::AudioAnalysis::detectPitch(
            sampleBuffer_.getReadPointer(0),
            static_cast<size_t>(numSamples),
            loadedSampleRate_
        );

        if (detectedPitch > 0.0f) {
            params.detectedPitchHz = detectedPitch;
            params.detectedMidiNote = dsp::AudioAnalysis::frequencyToMidiNote(detectedPitch);

            // Calculate cents deviation from the detected MIDI note
            float exactNote = 69.0f + 12.0f * std::log2(detectedPitch / 440.0f);
            params.detectedPitchCents = (exactNote - static_cast<float>(params.detectedMidiNote)) * 100.0f;

            // Find nearest C note for auto-repitch target
            params.targetRootNote = dsp::AudioAnalysis::getNearestC(params.detectedMidiNote);

            // Calculate pitch ratio to transpose to target C
            if (params.autoRepitchEnabled) {
                params.pitchRatio = dsp::AudioAnalysis::getPitchRatio(
                    params.detectedMidiNote, params.targetRootNote
                );
            } else {
                params.pitchRatio = 1.0f;
            }

            DBG("Detected pitch: " << detectedPitch << " Hz (MIDI " << params.detectedMidiNote
                << ", " << params.detectedPitchCents << " cents), target C: " << params.targetRootNote
                << ", ratio: " << params.pitchRatio);
        } else {
            // No pitch detected - reset to defaults
            params.detectedPitchHz = 0.0f;
            params.detectedMidiNote = -1;
            params.detectedPitchCents = 0.0f;
            params.targetRootNote = 60;
            params.pitchRatio = 1.0f;
            DBG("No pitch detected in sample");
        }
    }

    // Update all voices with new sample data
    for (auto& voice : voices_) {
        voice.setSampleRate(sampleRate_);
    }

    DBG("Loaded sample: " << file.getFileName()
        << " (" << numChannels << " ch, "
        << loadedSampleRate_ << " Hz, "
        << numSamples << " samples)");

    return true;
}

void SamplerInstrument::setTempo(double bpm) {
    tempo_ = bpm;
    modMatrix_.setTempo(bpm);
    // Update tempo for all voices for tracker FX timing
    for (auto& voice : voices_) {
        voice.setTempo(static_cast<float>(bpm));
    }
}

void SamplerInstrument::updateModulationParams() {
    if (!instrument_) return;

    const auto& mod = instrument_->getSamplerParams().modulation;

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

void SamplerInstrument::applyModulation(float* outL, float* outR, int numSamples) {
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

float SamplerInstrument::getModulatedVolume() const {
    float volumeMod = modMatrix_.getModulation(0);  // Volume
    return std::clamp(1.0f + volumeMod, 0.0f, 2.0f);
}

float SamplerInstrument::getModulatedCutoff() const {
    if (!instrument_) return 1.0f;
    float baseCutoff = instrument_->getSamplerParams().filter.cutoff;
    return modMatrix_.getModulatedValue(2, baseCutoff);  // Cutoff
}

float SamplerInstrument::getModulatedResonance() const {
    if (!instrument_) return 0.0f;
    float baseRes = instrument_->getSamplerParams().filter.resonance;
    return modMatrix_.getModulatedValue(3, baseRes);  // Resonance
}

} // namespace audio
