#include "SamplerInstrument.h"

namespace audio {

SamplerInstrument::SamplerInstrument() {
    formatManager_.registerBasicFormats();
}

void SamplerInstrument::init(double sampleRate) {
    sampleRate_ = sampleRate;
    for (auto& voice : voices_) {
        voice.setSampleRate(sampleRate);
    }
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

    // Temp buffers for voice mixing
    std::vector<float> tempL(numSamples, 0.0f);
    std::vector<float> tempR(numSamples, 0.0f);

    for (auto& voice : voices_) {
        if (voice.isActive()) {
            voice.render(tempL.data(), tempR.data(), numSamples);

            for (int i = 0; i < numSamples; ++i) {
                outL[i] += tempL[i];
                outR[i] += tempR[i];
            }

            // Clear temp for next voice
            std::fill(tempL.begin(), tempL.end(), 0.0f);
            std::fill(tempR.begin(), tempR.end(), 0.0f);
        }
    }
}

void SamplerInstrument::noteOn(int midiNote, float velocity) {
    if (!hasSample() || !instrument_) return;

    auto* voice = findFreeVoice();
    if (!voice) {
        voice = findVoiceToSteal();
    }

    if (voice) {
        const auto& params = instrument_->getSamplerParams();
        voice->setSampleData(
            sampleBuffer_.getReadPointer(0),
            sampleBuffer_.getNumChannels(),
            static_cast<size_t>(sampleBuffer_.getNumSamples()),
            loadedSampleRate_
        );
        voice->trigger(midiNote, velocity, params);
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

    // Update instrument's sample ref
    if (instrument_) {
        auto& sampleRef = instrument_->getSamplerParams().sample;
        sampleRef.path = file.getFullPathName().toStdString();
        sampleRef.numChannels = numChannels;
        sampleRef.sampleRate = loadedSampleRate_;
        sampleRef.numSamples = static_cast<size_t>(numSamples);
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

} // namespace audio
