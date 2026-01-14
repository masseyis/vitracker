#include "SlicerInstrument.h"
#include "Voice.h"
#include "../dsp/AudioAnalysis.h"
#include <rubberband/RubberBandStretcher.h>
#include <algorithm>
#include <cmath>

namespace audio {

std::unique_ptr<audio::Voice> SlicerInstrument::createVoice() {
    // TODO: Implement SlicerVoice as Voice subclass
    return nullptr;
}

void SlicerInstrument::updateVoiceParameters(audio::Voice* voice) {
    // TODO: Implement parameter update for SlicerVoice
    (void)voice;
}

SlicerInstrument::SlicerInstrument() {
    formatManager_.registerBasicFormats();
    tempBufferL_.fill(0.0f);
    tempBufferR_.fill(0.0f);
}

void SlicerInstrument::init(double sampleRate) {
    sampleRate_ = sampleRate;
    for (auto& voice : voices_) {
        voice.setSampleRate(sampleRate);
    }
    modMatrix_.init();
    modMatrix_.setTempo(tempo_);
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

    // Handle lazy chop preview playback
    if (lazyChopPlaying_) {
        const float* srcL = sampleBuffer_.getReadPointer(0);
        const float* srcR = sampleBuffer_.getNumChannels() > 1
            ? sampleBuffer_.getReadPointer(1)
            : srcL;  // Mono: use left for both

        size_t totalSamples = static_cast<size_t>(sampleBuffer_.getNumSamples());

        for (int i = 0; i < numSamples && lazyChopPlayhead_ < totalSamples; ++i) {
            outL[i] = srcL[lazyChopPlayhead_];
            outR[i] = srcR[lazyChopPlayhead_];
            lazyChopPlayhead_++;
        }

        // Stop when we reach the end
        if (lazyChopPlayhead_ >= totalSamples) {
            lazyChopPlaying_ = false;
        }
        return;  // Don't process regular voices during lazy chop
    }

    // Normal voice processing - process in blocks for modulation
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
        std::vector<float> voiceTempL(static_cast<size_t>(blockSize), 0.0f);
        std::vector<float> voiceTempR(static_cast<size_t>(blockSize), 0.0f);

        for (auto& voice : voices_) {
            if (voice.isActive()) {
                voice.render(voiceTempL.data(), voiceTempR.data(), blockSize);

                for (int i = 0; i < blockSize; ++i) {
                    tempBufferL_[static_cast<size_t>(i)] += voiceTempL[static_cast<size_t>(i)];
                    tempBufferR_[static_cast<size_t>(i)] += voiceTempR[static_cast<size_t>(i)];
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

int SlicerInstrument::midiNoteToSliceIndex(int midiNote) const {
    // C1 (36) = slice 0, C#1 (37) = slice 1, etc.
    return midiNote - BASE_NOTE;
}

void SlicerInstrument::noteOn(int midiNote, float velocity) {
    model::Step emptyStep;
    noteOnWithFX(midiNote, velocity, emptyStep);
}

void SlicerInstrument::noteOnWithFX(int midiNote, float velocity, const model::Step& step) {
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
        // Choose buffer based on repitch setting
        const juce::AudioBuffer<float>* bufferToUse = &sampleBuffer_;
        float playbackSpeed = params.speed;

        // If repitch is off and stretched buffer is ready, use it at 1x speed
        if (!params.repitch && stretchedBufferReady_ && stretchedBuffer_.getNumSamples() > 0) {
            bufferToUse = &stretchedBuffer_;
            playbackSpeed = 1.0f;  // Stretched buffer already at correct speed
        }

        // Pass both channel pointers (JUCE uses planar format)
        const float* leftData = bufferToUse->getReadPointer(0);
        const float* rightData = bufferToUse->getNumChannels() > 1
            ? bufferToUse->getReadPointer(1)
            : nullptr;
        voice->setSampleData(
            leftData,
            rightData,
            static_cast<size_t>(bufferToUse->getNumSamples()),
            loadedSampleRate_
        );
        voice->setPlaybackSpeed(playbackSpeed);
        voice->trigger(sliceIndex, velocity, params, step);

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

void SlicerInstrument::chopByTransients(float sensitivity) {
    if (!instrument_ || sampleBuffer_.getNumSamples() == 0) return;

    auto& params = instrument_->getSlicerParams();
    params.slicePoints.clear();
    params.transientSensitivity = sensitivity;
    params.chopMode = model::ChopMode::Transients;

    // Use the first channel for transient detection
    const float* data = sampleBuffer_.getReadPointer(0);
    size_t numSamples = static_cast<size_t>(sampleBuffer_.getNumSamples());

    // Detect transients
    auto transients = dsp::AudioAnalysis::detectTransients(
        data, numSamples, loadedSampleRate_, sensitivity
    );

    // Always include start position (0) as first slice
    params.slicePoints.push_back(0);

    // Add detected transients, snapping to zero crossings
    for (size_t transientPos : transients) {
        // Skip if too close to start
        if (transientPos < 1000) continue;

        size_t snappedPos = findNearestZeroCrossing(transientPos);
        params.slicePoints.push_back(snappedPos);
    }

    // Remove duplicates (from zero-crossing snapping) and sort
    std::sort(params.slicePoints.begin(), params.slicePoints.end());
    auto last = std::unique(params.slicePoints.begin(), params.slicePoints.end());
    params.slicePoints.erase(last, params.slicePoints.end());

    DBG("Transient detection found " << params.slicePoints.size() << " slices with sensitivity " << sensitivity);
}

int SlicerInstrument::addSliceAtPosition(size_t samplePosition) {
    if (!instrument_) return -1;

    auto& params = instrument_->getSlicerParams();

    // Snap to zero crossing
    size_t snappedPosition = findNearestZeroCrossing(samplePosition);

    // Insert in sorted order and get the index
    auto it = std::lower_bound(params.slicePoints.begin(), params.slicePoints.end(), snappedPosition);
    int newSliceIndex = static_cast<int>(it - params.slicePoints.begin());
    params.slicePoints.insert(it, snappedPosition);

    params.chopMode = model::ChopMode::Lazy;

    return newSliceIndex;
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
    auto& slicePoints = instrument_->getSlicerParams().slicePoints;
    slicePoints.clear();
    slicePoints.push_back(0);  // Always have one slice starting at 0
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
        params.speed = std::clamp(params.targetBPM / effectiveOriginalBPM, 0.1f, 4.0f);
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

    // Regenerate stretched buffer if repitch is off (speed may have changed)
    if (!params.repitch) {
        regenerateStretchedBuffer();
    }
}

void SlicerInstrument::editOriginalBPM(float newBPM) {
    if (!instrument_ || newBPM <= 0.0f) return;

    auto& params = instrument_->getSlicerParams();
    params.originalBPM = newBPM;
    params.bars = calculateBarsFromBPM(newBPM);

    markEdited(model::SlicerLastEdited::OriginalBPM);
    recalculateDependencies();

    // Regenerate stretched buffer if repitch is off (speed may have changed)
    if (!params.repitch) {
        regenerateStretchedBuffer();
    }
}

void SlicerInstrument::editTargetBPM(float newBPM) {
    if (!instrument_ || newBPM <= 0.0f) return;

    auto& params = instrument_->getSlicerParams();
    params.targetBPM = newBPM;

    markEdited(model::SlicerLastEdited::TargetBPM);
    recalculateDependencies();

    // Regenerate stretched buffer if repitch is off (speed may have changed)
    if (!params.repitch) {
        regenerateStretchedBuffer();
    }
}

void SlicerInstrument::editSpeed(float newSpeed) {
    if (!instrument_ || newSpeed <= 0.0f) return;

    auto& params = instrument_->getSlicerParams();
    params.speed = newSpeed;

    markEdited(model::SlicerLastEdited::Speed);
    recalculateDependencies();

    // Regenerate stretched buffer if repitch is off
    if (!params.repitch) {
        regenerateStretchedBuffer();
    }
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
        // Clear stretched buffer since we'll use variable speed playback
        stretchedBuffer_.setSize(0, 0);
        stretchedBufferReady_ = false;
    } else {
        // When disabling repitch, regenerate stretched buffer
        regenerateStretchedBuffer();
    }
}

// ============================================================================
// Lazy chop preview
// ============================================================================

void SlicerInstrument::startLazyChopPreview() {
    if (!hasSample()) return;

    // Stop any currently playing voices
    allNotesOff();

    lazyChopPlayhead_ = 0;
    lazyChopPlaying_ = true;
}

void SlicerInstrument::stopLazyChopPreview() {
    lazyChopPlaying_ = false;
}

void SlicerInstrument::addSliceAtPlayhead() {
    if (!lazyChopPlaying_ || !instrument_) return;

    // Add a slice at the current playhead position
    addSliceAtPosition(lazyChopPlayhead_);
}

int64_t SlicerInstrument::getPlayheadPosition() const {
    // During lazy chop preview, return the lazy chop playhead
    if (lazyChopPlaying_) {
        return static_cast<int64_t>(lazyChopPlayhead_);
    }

    // Otherwise, return the position of the first active voice
    for (const auto& voice : voices_) {
        if (voice.isActive()) {
            int64_t voicePos = static_cast<int64_t>(voice.getPlayPosition());

            // If using stretched buffer, convert position back to original sample coords
            if (!instrument_->getSlicerParams().repitch &&
                stretchedBufferReady_ && stretchedBuffer_.getNumSamples() > 0) {
                double scaleFactor = static_cast<double>(sampleBuffer_.getNumSamples()) /
                                     static_cast<double>(stretchedBuffer_.getNumSamples());
                voicePos = static_cast<int64_t>(voicePos * scaleFactor);
            }

            return voicePos;
        }
    }
    return -1;  // No active voice
}

// ============================================================================
// Time-stretching with RubberBand
// ============================================================================

void SlicerInstrument::regenerateStretchedBuffer() {
    if (!instrument_ || sampleBuffer_.getNumSamples() == 0) {
        stretchedBufferReady_ = false;
        return;
    }

    auto& params = instrument_->getSlicerParams();

    // If repitch is enabled, we don't need the stretched buffer
    if (params.repitch) {
        stretchedBuffer_.setSize(0, 0);
        stretchedBufferReady_ = false;
        lastStretchSpeed_ = params.speed;
        return;
    }

    // Skip if speed hasn't changed significantly
    if (std::abs(params.speed - lastStretchSpeed_) < 0.001f && stretchedBufferReady_) {
        return;
    }

    // Stop all voices before regenerating - they hold raw pointers to the buffer
    // which would become invalid after reallocation
    allNotesOff();

    stretchedBufferReady_ = false;
    lastStretchSpeed_ = params.speed;

    // Safety check: speed must be > 0 to avoid divide by zero
    if (params.speed <= 0.001f) {
        DBG("Invalid speed value: " << params.speed << ", using original buffer");
        stretchedBuffer_ = sampleBuffer_;
        stretchedBufferReady_ = true;
        return;
    }

    // Time ratio is inverse of speed (speed=2 means half duration = timeRatio 0.5)
    double timeRatio = 1.0 / static_cast<double>(params.speed);

    // For speed=1, just copy the original buffer
    if (std::abs(params.speed - 1.0f) < 0.01f) {
        stretchedBuffer_ = sampleBuffer_;
        stretchedBufferReady_ = true;
        DBG("Speed ~1.0, using original buffer");
        return;
    }

    int numChannels = sampleBuffer_.getNumChannels();
    size_t numInputSamples = static_cast<size_t>(sampleBuffer_.getNumSamples());

    // Estimate output size
    size_t estimatedOutputSamples = static_cast<size_t>(numInputSamples * timeRatio) + 1024;

    DBG("Regenerating stretched buffer: speed=" << params.speed
        << " timeRatio=" << timeRatio
        << " inputSamples=" << numInputSamples
        << " estimatedOutput=" << estimatedOutputSamples);

    // Create RubberBand stretcher in offline mode for best quality
    RubberBand::RubberBandStretcher stretcher(
        static_cast<size_t>(loadedSampleRate_),
        static_cast<size_t>(numChannels),
        RubberBand::RubberBandStretcher::OptionProcessOffline |
        RubberBand::RubberBandStretcher::OptionEngineFiner |
        RubberBand::RubberBandStretcher::OptionPitchHighQuality,
        timeRatio,
        1.0  // pitch scale = 1.0 (preserve pitch)
    );

    // Process block size
    const size_t blockSize = 1024;

    // Prepare input pointers for each channel
    std::vector<const float*> inputPtrs(static_cast<size_t>(numChannels));
    for (int ch = 0; ch < numChannels; ++ch) {
        inputPtrs[static_cast<size_t>(ch)] = sampleBuffer_.getReadPointer(ch);
    }

    // First pass: study
    size_t pos = 0;
    while (pos < numInputSamples) {
        size_t remaining = numInputSamples - pos;
        size_t thisBlock = std::min(blockSize, remaining);
        bool final = (pos + thisBlock >= numInputSamples);

        std::vector<const float*> blockPtrs(static_cast<size_t>(numChannels));
        for (int ch = 0; ch < numChannels; ++ch) {
            blockPtrs[static_cast<size_t>(ch)] = inputPtrs[static_cast<size_t>(ch)] + pos;
        }

        stretcher.study(blockPtrs.data(), thisBlock, final);
        pos += thisBlock;
    }

    // Prepare output buffer
    std::vector<std::vector<float>> outputChannels(static_cast<size_t>(numChannels));
    for (auto& ch : outputChannels) {
        ch.reserve(estimatedOutputSamples);
    }

    // Second pass: process and retrieve
    pos = 0;
    std::vector<float*> outputPtrs(static_cast<size_t>(numChannels));
    std::vector<std::vector<float>> retrieveBuffers(static_cast<size_t>(numChannels));
    for (auto& buf : retrieveBuffers) {
        buf.resize(blockSize * 4);  // Extra space for output
    }

    while (pos < numInputSamples) {
        size_t remaining = numInputSamples - pos;
        size_t thisBlock = std::min(blockSize, remaining);
        bool final = (pos + thisBlock >= numInputSamples);

        std::vector<const float*> blockPtrs(static_cast<size_t>(numChannels));
        for (int ch = 0; ch < numChannels; ++ch) {
            blockPtrs[static_cast<size_t>(ch)] = inputPtrs[static_cast<size_t>(ch)] + pos;
        }

        stretcher.process(blockPtrs.data(), thisBlock, final);
        pos += thisBlock;

        // Retrieve available output
        int available;
        while ((available = stretcher.available()) > 0) {
            size_t toRetrieve = std::min(static_cast<size_t>(available), retrieveBuffers[0].size());

            for (int ch = 0; ch < numChannels; ++ch) {
                outputPtrs[static_cast<size_t>(ch)] = retrieveBuffers[static_cast<size_t>(ch)].data();
            }

            size_t retrieved = stretcher.retrieve(outputPtrs.data(), toRetrieve);

            for (int ch = 0; ch < numChannels; ++ch) {
                outputChannels[static_cast<size_t>(ch)].insert(
                    outputChannels[static_cast<size_t>(ch)].end(),
                    retrieveBuffers[static_cast<size_t>(ch)].begin(),
                    retrieveBuffers[static_cast<size_t>(ch)].begin() + static_cast<ptrdiff_t>(retrieved)
                );
            }
        }
    }

    // Copy to JUCE buffer
    if (!outputChannels.empty() && !outputChannels[0].empty()) {
        int outputSamples = static_cast<int>(outputChannels[0].size());
        stretchedBuffer_.setSize(numChannels, outputSamples);

        for (int ch = 0; ch < numChannels; ++ch) {
            stretchedBuffer_.copyFrom(ch, 0, outputChannels[static_cast<size_t>(ch)].data(), outputSamples);
        }

        stretchedBufferReady_ = true;
        DBG("Stretched buffer ready: " << outputSamples << " samples (ratio: "
            << (static_cast<float>(outputSamples) / numInputSamples) << ")");
    } else {
        DBG("Failed to generate stretched buffer");
        stretchedBuffer_.setSize(0, 0);
        stretchedBufferReady_ = false;
    }
}

// ============================================================================
// Modulation helpers
// ============================================================================

// Map 0-1 to milliseconds for attack (1ms to 2000ms, exponential)
static float mapAttackMs(float normalized) {
    return 1.0f + std::pow(normalized, 2.0f) * 1999.0f;
}

// Map 0-1 to milliseconds for decay (1ms to 10000ms, exponential)
static float mapDecayMs(float normalized) {
    return 1.0f + std::pow(normalized, 2.0f) * 9999.0f;
}

void SlicerInstrument::setTempo(double bpm) {
    tempo_ = bpm;
    modMatrix_.setTempo(bpm);
    // Update tempo for all voices for tracker FX timing
    for (auto& voice : voices_) {
        voice.setTempo(static_cast<float>(bpm));
    }
}

void SlicerInstrument::updateModulationParams() {
    if (!instrument_) return;

    const auto& mod = instrument_->getSlicerParams().modulation;

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

void SlicerInstrument::applyModulation(float* outL, float* outR, int numSamples) {
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

float SlicerInstrument::getModulatedVolume() const {
    float volumeMod = modMatrix_.getModulation(0);  // Volume
    return std::clamp(1.0f + volumeMod, 0.0f, 2.0f);
}

float SlicerInstrument::getModulatedCutoff() const {
    if (!instrument_) return 1.0f;
    float baseCutoff = instrument_->getSlicerParams().filter.cutoff;
    return modMatrix_.getModulatedValue(2, baseCutoff);  // Cutoff
}

float SlicerInstrument::getModulatedResonance() const {
    if (!instrument_) return 0.0f;
    float baseRes = instrument_->getSlicerParams().filter.resonance;
    return modMatrix_.getModulatedValue(3, baseRes);  // Resonance
}

} // namespace audio
