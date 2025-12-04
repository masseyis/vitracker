#include "SlicerInstrument.h"
#include "../dsp/AudioAnalysis.h"
#include <algorithm>
#include <cmath>

namespace audio {

SlicerInstrument::SlicerInstrument() {
    formatManager_.registerBasicFormats();
}

void SlicerInstrument::init(double sampleRate) {
    setSampleRate(sampleRate);
}

void SlicerInstrument::setSampleRate(double sampleRate) {
    sampleRate_ = sampleRate;
    for (auto& voice : voices_) {
        voice.setSampleRate(sampleRate);
    }
}

void SlicerInstrument::process(float* outL, float* outR, int numSamples) {
    // Clear output
    std::fill(outL, outL + numSamples, 0.0f);
    std::fill(outR, outR + numSamples, 0.0f);

    if (!hasSample() || !instrument_) return;

    // Temp buffers for voice mixing
    std::vector<float> tempL(static_cast<size_t>(numSamples), 0.0f);
    std::vector<float> tempR(static_cast<size_t>(numSamples), 0.0f);

    for (auto& voice : voices_) {
        if (voice.isActive()) {
            voice.render(tempL.data(), tempR.data(), numSamples);

            for (size_t i = 0; i < static_cast<size_t>(numSamples); ++i) {
                outL[i] += tempL[i];
                outR[i] += tempR[i];
            }

            // Clear temp for next voice
            std::fill(tempL.begin(), tempL.end(), 0.0f);
            std::fill(tempR.begin(), tempR.end(), 0.0f);
        }
    }
}

int SlicerInstrument::midiNoteToSliceIndex(int midiNote) const {
    // C1 (36) = slice 0, C#1 (37) = slice 1, etc.
    return midiNote - BASE_NOTE;
}

void SlicerInstrument::noteOn(int midiNote, float velocity) {
    if (!hasSample() || !instrument_) return;

    int sliceIndex = midiNoteToSliceIndex(midiNote);
    if (sliceIndex < 0) return;  // Note below C1

    const auto& params = instrument_->getSlicerParams();

    // Check if slice exists
    if (sliceIndex >= static_cast<int>(params.slicePoints.size()) && !params.slicePoints.empty()) {
        return;  // Slice doesn't exist
    }

    // In choke mode (polyphony=1), kill all active voices first
    if (params.polyphony == 1) {
        for (auto& voice : voices_) {
            if (voice.isActive()) {
                voice.release();
            }
        }
    }

    // Find a voice to use
    auto* voice = findFreeVoice(params.polyphony);
    if (!voice) {
        voice = findVoiceToSteal();
    }

    if (voice) {
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
        voice->trigger(sliceIndex, velocity, params);
    }
}

void SlicerInstrument::noteOff(int midiNote) {
    int sliceIndex = midiNoteToSliceIndex(midiNote);
    for (auto& voice : voices_) {
        if (voice.isActive() && voice.getSliceIndex() == sliceIndex) {
            voice.release();
        }
    }
}

void SlicerInstrument::allNotesOff() {
    for (auto& voice : voices_) {
        voice.release();
    }
}

SlicerVoice* SlicerInstrument::findFreeVoice(int maxVoices) {
    // Only search within the polyphony limit
    int limit = std::min(maxVoices, static_cast<int>(voices_.size()));
    for (int i = 0; i < limit; ++i) {
        if (!voices_[static_cast<size_t>(i)].isActive()) {
            return &voices_[static_cast<size_t>(i)];
        }
    }
    return nullptr;
}

SlicerVoice* SlicerInstrument::findVoiceToSteal() {
    // Simple voice stealing: return first voice
    return &voices_[0];
}

bool SlicerInstrument::loadSample(const juce::File& file) {
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
    const size_t dataSize = static_cast<size_t>(numSamples) * static_cast<size_t>(numChannels) * sizeof(float);
    if (dataSize > 50 * 1024 * 1024) {
        DBG("Warning: Large sample file (" << (dataSize / (1024 * 1024)) << " MB)");
    }

    sampleBuffer_.setSize(numChannels, static_cast<int>(numSamples));
    reader->read(&sampleBuffer_, 0, static_cast<int>(numSamples), 0, true, true);

    // Update instrument's sample ref and detect BPM
    if (instrument_) {
        auto& params = instrument_->getSlicerParams();
        auto& sampleRef = params.sample;
        sampleRef.path = file.getFullPathName().toStdString();
        sampleRef.numChannels = numChannels;
        sampleRef.sampleRate = loadedSampleRate_;
        sampleRef.numSamples = static_cast<size_t>(numSamples);

        // Detect BPM
        float detectedBPM = dsp::AudioAnalysis::detectBPM(
            sampleBuffer_.getReadPointer(0),
            static_cast<size_t>(numSamples),
            loadedSampleRate_
        );

        if (detectedBPM > 0.0f) {
            params.detectedBPM = detectedBPM;
            // Initialize speed to 1.0 and targetBPM to match detected (no stretching by default)
            params.speed = 1.0f;
            params.targetBPM = detectedBPM;

            // Calculate number of bars: (samples / sampleRate) / (60 / BPM * 4)
            // = sample_duration_seconds / seconds_per_bar
            float durationSec = static_cast<float>(numSamples) / loadedSampleRate_;
            float secondsPerBar = (60.0f / detectedBPM) * 4.0f;  // 4 beats per bar
            params.detectedBars = std::max(1, static_cast<int>(std::round(durationSec / secondsPerBar)));

            DBG("Detected BPM: " << detectedBPM << ", Bars: " << params.detectedBars);
        } else {
            params.detectedBPM = 0.0f;
            params.speed = 1.0f;
            params.targetBPM = 120.0f;  // Default target BPM when detection fails
            params.detectedBars = 0;
            DBG("No BPM detected in sample");
        }

        // Also detect pitch (optional, for display)
        float detectedPitch = dsp::AudioAnalysis::detectPitch(
            sampleBuffer_.getReadPointer(0),
            static_cast<size_t>(numSamples),
            loadedSampleRate_
        );

        if (detectedPitch > 0.0f) {
            params.pitchHz = detectedPitch;
            params.detectedRootNote = dsp::AudioAnalysis::frequencyToMidiNote(detectedPitch);
            DBG("Detected pitch: " << detectedPitch << " Hz (MIDI " << params.detectedRootNote << ")");
        } else {
            params.pitchHz = 0.0f;
            params.detectedRootNote = 60;
        }

        // Default chop into 8 divisions
        chopIntoDivisions(8);
    }

    // Update all voices with new sample data
    for (auto& voice : voices_) {
        voice.setSampleRate(sampleRate_);
    }

    DBG("Loaded sample for slicer: " << file.getFileName()
        << " (" << numChannels << " ch, "
        << loadedSampleRate_ << " Hz, "
        << numSamples << " samples)");

    return true;
}

void SlicerInstrument::chopIntoDivisions(int numDivisions) {
    if (!instrument_ || sampleBuffer_.getNumSamples() == 0) return;

    auto& params = instrument_->getSlicerParams();
    params.slicePoints.clear();
    params.numDivisions = numDivisions;
    params.chopMode = model::ChopMode::Divisions;

    size_t samplesPerSlice = static_cast<size_t>(sampleBuffer_.getNumSamples()) / static_cast<size_t>(numDivisions);

    for (int i = 0; i < numDivisions; ++i) {
        size_t position = static_cast<size_t>(i) * samplesPerSlice;
        // Snap to zero crossing
        position = findNearestZeroCrossing(position);
        params.slicePoints.push_back(position);
    }
}

void SlicerInstrument::addSliceAtPosition(size_t samplePosition) {
    if (!instrument_) return;

    auto& params = instrument_->getSlicerParams();

    // Snap to zero crossing
    size_t snappedPosition = findNearestZeroCrossing(samplePosition);

    // Insert in sorted order
    auto it = std::lower_bound(params.slicePoints.begin(), params.slicePoints.end(), snappedPosition);
    params.slicePoints.insert(it, snappedPosition);

    params.chopMode = model::ChopMode::Lazy;
}

void SlicerInstrument::removeSlice(int sliceIndex) {
    if (!instrument_) return;

    auto& params = instrument_->getSlicerParams();
    if (sliceIndex >= 0 && sliceIndex < static_cast<int>(params.slicePoints.size())) {
        params.slicePoints.erase(params.slicePoints.begin() + sliceIndex);
        params.chopMode = model::ChopMode::Lazy;
    }
}

void SlicerInstrument::clearSlices() {
    if (!instrument_) return;
    instrument_->getSlicerParams().slicePoints.clear();
}

int SlicerInstrument::getNumSlices() const {
    if (!instrument_) return 0;
    return static_cast<int>(instrument_->getSlicerParams().slicePoints.size());
}

size_t SlicerInstrument::findNearestZeroCrossing(size_t position) const {
    if (sampleBuffer_.getNumSamples() == 0) return position;

    const float* data = sampleBuffer_.getReadPointer(0);
    const size_t numSamples = static_cast<size_t>(sampleBuffer_.getNumSamples());

    // Search window: +/- 1024 samples
    const size_t searchRadius = 1024;
    size_t start = (position > searchRadius) ? position - searchRadius : 0;
    size_t end = std::min(position + searchRadius, numSamples - 1);

    size_t bestPosition = position;
    float bestDistance = std::abs(data[position]);

    for (size_t i = start; i < end; ++i) {
        // Check for sign change (zero crossing)
        if (i > 0 && data[i - 1] * data[i] <= 0.0f) {
            // Found a zero crossing
            size_t distance = (i > position) ? i - position : position - i;
            if (distance < (bestPosition > position ? bestPosition - position : position - bestPosition) ||
                std::abs(data[i]) < bestDistance) {
                bestPosition = i;
                bestDistance = std::abs(data[i]);
            }
        }
    }

    return bestPosition;
}

// ============================================================================
// Three-way dependency management
// ============================================================================

void SlicerInstrument::markEdited(model::SlicerLastEdited which) {
    if (!instrument_) return;
    auto& params = instrument_->getSlicerParams();

    // Don't duplicate if already most recent
    if (params.lastEdited1 == which) return;

    // Shift: current most recent becomes second, new becomes most recent
    params.lastEdited2 = params.lastEdited1;
    params.lastEdited1 = which;
}

void SlicerInstrument::recalculateDependencies() {
    if (!instrument_ || sampleBuffer_.getNumSamples() == 0) return;

    auto& params = instrument_->getSlicerParams();
    using LE = model::SlicerLastEdited;

    // Determine which value needs recalculating (the one NOT in lastEdited1 or lastEdited2)
    bool recalcOriginal = (params.lastEdited1 != LE::OriginalBPM && params.lastEdited2 != LE::OriginalBPM);
    bool recalcTarget = (params.lastEdited1 != LE::TargetBPM && params.lastEdited2 != LE::TargetBPM);
    bool recalcSpeed = (params.lastEdited1 != LE::Speed && params.lastEdited2 != LE::Speed);

    float effectiveOriginalBPM = params.getOriginalBPM();

    if (recalcSpeed && effectiveOriginalBPM > 0.0f) {
        // Speed = targetBPM / originalBPM
        params.speed = params.targetBPM / effectiveOriginalBPM;
    } else if (recalcTarget && effectiveOriginalBPM > 0.0f) {
        // targetBPM = originalBPM * speed
        params.targetBPM = effectiveOriginalBPM * params.speed;
    } else if (recalcOriginal && params.speed > 0.0f) {
        // originalBPM = targetBPM / speed
        float newOriginalBPM = params.targetBPM / params.speed;
        params.originalBPM = newOriginalBPM;
        // Update bars to match
        params.bars = calculateBarsFromBPM(newOriginalBPM);
    }

    // If repitch is enabled, sync pitch with speed
    if (params.repitch) {
        params.pitchSemitones = static_cast<int>(std::round(12.0 * std::log2(params.speed)));
    }
}

float SlicerInstrument::calculateBPMFromBars(int bars) const {
    if (bars <= 0 || sampleBuffer_.getNumSamples() == 0) return 0.0f;

    // BPM = (bars * 4 beats * 60 seconds) / sample_duration_seconds
    float durationSec = static_cast<float>(sampleBuffer_.getNumSamples()) / loadedSampleRate_;
    return (bars * 4.0f * 60.0f) / durationSec;
}

int SlicerInstrument::calculateBarsFromBPM(float bpm) const {
    if (bpm <= 0.0f || sampleBuffer_.getNumSamples() == 0) return 0;

    // bars = sample_duration_seconds / seconds_per_bar
    // seconds_per_bar = (60 / BPM) * 4 beats
    float durationSec = static_cast<float>(sampleBuffer_.getNumSamples()) / loadedSampleRate_;
    float secondsPerBar = (60.0f / bpm) * 4.0f;
    return std::max(1, static_cast<int>(std::round(durationSec / secondsPerBar)));
}

void SlicerInstrument::editOriginalBars(int newBars) {
    if (!instrument_ || newBars <= 0) return;

    auto& params = instrument_->getSlicerParams();
    params.bars = newBars;
    params.originalBPM = calculateBPMFromBars(newBars);

    markEdited(model::SlicerLastEdited::OriginalBPM);
    recalculateDependencies();
}

void SlicerInstrument::editOriginalBPM(float newBPM) {
    if (!instrument_ || newBPM <= 0.0f) return;

    auto& params = instrument_->getSlicerParams();
    params.originalBPM = newBPM;
    params.bars = calculateBarsFromBPM(newBPM);

    markEdited(model::SlicerLastEdited::OriginalBPM);
    recalculateDependencies();
}

void SlicerInstrument::editTargetBPM(float newBPM) {
    if (!instrument_ || newBPM <= 0.0f) return;

    auto& params = instrument_->getSlicerParams();
    params.targetBPM = newBPM;

    markEdited(model::SlicerLastEdited::TargetBPM);
    recalculateDependencies();
}

void SlicerInstrument::editSpeed(float newSpeed) {
    if (!instrument_ || newSpeed <= 0.0f) return;

    auto& params = instrument_->getSlicerParams();
    params.speed = newSpeed;

    markEdited(model::SlicerLastEdited::Speed);
    recalculateDependencies();
}

void SlicerInstrument::editPitch(int newSemitones) {
    if (!instrument_) return;

    auto& params = instrument_->getSlicerParams();
    if (!params.repitch) return;  // Only works when repitch is enabled

    params.pitchSemitones = newSemitones;
    // Calculate speed from pitch: speed = 2^(semitones/12)
    params.speed = std::pow(2.0f, newSemitones / 12.0f);

    markEdited(model::SlicerLastEdited::Speed);
    recalculateDependencies();
}

void SlicerInstrument::setRepitch(bool enabled) {
    if (!instrument_) return;

    auto& params = instrument_->getSlicerParams();
    params.repitch = enabled;

    // When enabling repitch, calculate pitch from current speed
    if (enabled) {
        params.pitchSemitones = static_cast<int>(std::round(12.0 * std::log2(params.speed)));
    }
}

} // namespace audio
