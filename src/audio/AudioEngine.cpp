#include "AudioEngine.h"

namespace audio {

AudioEngine::AudioEngine()
{
    trackVoices_.fill(nullptr);
    trackInstruments_.fill(-1);
    trackChainPositions_.fill(0);

    // Create instrument processors for all slots
    for (int i = 0; i < NUM_INSTRUMENTS; ++i)
    {
        instrumentProcessors_[i] = std::make_unique<PlaitsInstrument>();
        samplerProcessors_[i] = std::make_unique<SamplerInstrument>();
        slicerProcessors_[i] = std::make_unique<SlicerInstrument>();
        vaSynthProcessors_[i] = std::make_unique<VASynthInstrument>();
    }
}

AudioEngine::~AudioEngine() = default;

void AudioEngine::play()
{
    // Lock-free: signal to audio thread to start playback
    // The audio thread will handle state reset to avoid mutex contention
    if (!playing_.load(std::memory_order_relaxed))
    {
        pendingPlay_.store(true, std::memory_order_release);
    }
}

void AudioEngine::stop()
{
    // Lock-free: signal to audio thread to stop playback
    // The audio thread will handle cleanup to avoid mutex contention
    if (playing_.load(std::memory_order_relaxed))
    {
        pendingStop_.store(true, std::memory_order_release);
    }
}

void AudioEngine::triggerNote(int track, int note, int instrumentIndex, float velocity)
{
    model::Step emptyStep;
    triggerNote(track, note, instrumentIndex, velocity, emptyStep);
}

void AudioEngine::triggerNote(int track, int note, int instrumentIndex, float velocity, const model::Step& step)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (!project_) return;
    if (instrumentIndex < 0 || instrumentIndex >= NUM_INSTRUMENTS) return;

    auto* instrument = project_->getInstrument(instrumentIndex);
    if (!instrument) return;

    // Check instrument type and route to appropriate processor
    if (instrument->getType() == model::InstrumentType::Sampler)
    {
        // Handle Sampler instrument
        if (auto* sampler = getSamplerProcessor(instrumentIndex))
        {
            sampler->setInstrument(instrument);
            sampler->noteOn(note, velocity);
        }

        trackInstruments_[track] = instrumentIndex;
        return;
    }

    if (instrument->getType() == model::InstrumentType::Slicer)
    {
        // Handle Slicer instrument
        if (auto* slicer = getSlicerProcessor(instrumentIndex))
        {
            slicer->setInstrument(instrument);
            slicer->noteOn(note, velocity);
        }

        trackInstruments_[track] = instrumentIndex;
        return;
    }

    if (instrument->getType() == model::InstrumentType::VASynth)
    {
        // Handle VASynth instrument
        if (auto* vaSynth = getVASynthProcessor(instrumentIndex))
        {
            vaSynth->setInstrument(instrument);
            vaSynth->noteOn(note, velocity);
        }

        trackInstruments_[track] = instrumentIndex;
        return;
    }

    // Handle Plaits instrument (existing code)
    // Release previous note on this track if different instrument
    if (trackInstruments_[track] >= 0 && trackInstruments_[track] != instrumentIndex)
    {
        if (auto* prevProcessor = instrumentProcessors_[trackInstruments_[track]].get())
        {
            prevProcessor->noteOff(note);  // Release any note from previous instrument
        }
    }

    // Sync parameters from model::Instrument to processor
    syncInstrumentParams(instrumentIndex);

    // Trigger note on instrument processor
    if (auto* processor = instrumentProcessors_[instrumentIndex].get())
    {
        processor->noteOn(note, velocity);
    }

    trackInstruments_[track] = instrumentIndex;

    // Legacy voice for FX processing (optional - can be removed later)
    juce::ignoreUnused(step);
}

void AudioEngine::releaseNote(int track)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    // Release notes on the instrument that was playing on this track
    int instrumentIndex = trackInstruments_[track];
    if (instrumentIndex >= 0 && instrumentIndex < NUM_INSTRUMENTS)
    {
        // Check instrument type
        if (project_)
        {
            auto* instrument = project_->getInstrument(instrumentIndex);
            if (instrument && instrument->getType() == model::InstrumentType::Sampler)
            {
                // Handle Sampler instrument
                if (auto* sampler = getSamplerProcessor(instrumentIndex))
                {
                    sampler->allNotesOff();
                }
                trackInstruments_[track] = -1;
                trackVoices_[track] = nullptr;
                return;
            }

            if (instrument && instrument->getType() == model::InstrumentType::Slicer)
            {
                // Handle Slicer instrument
                if (auto* slicer = getSlicerProcessor(instrumentIndex))
                {
                    slicer->allNotesOff();
                }
                trackInstruments_[track] = -1;
                trackVoices_[track] = nullptr;
                return;
            }

            if (instrument && instrument->getType() == model::InstrumentType::VASynth)
            {
                // Handle VASynth instrument
                if (auto* vaSynth = getVASynthProcessor(instrumentIndex))
                {
                    vaSynth->allNotesOff();
                }
                trackInstruments_[track] = -1;
                trackVoices_[track] = nullptr;
                return;
            }
        }

        // Handle Plaits instrument (existing code)
        if (auto* processor = instrumentProcessors_[instrumentIndex].get())
        {
            // Note: we don't have the specific note, so we rely on the instrument's
            // internal tracking. For monophonic playback this works; for polyphonic
            // we may need to track notes per track.
            processor->allNotesOff();  // Simple approach for now
        }
    }
    trackInstruments_[track] = -1;

    // Legacy cleanup
    if (trackVoices_[track])
    {
        trackVoices_[track]->noteOff();
        trackVoices_[track] = nullptr;
    }
}

void AudioEngine::previewChord(const std::vector<int>& notes, int instrumentIndex)
{
    // Use tracks 0, 1, 2, etc. for chord preview
    int track = 0;
    for (int note : notes)
    {
        if (track >= NUM_TRACKS) break;
        triggerNote(track, note, instrumentIndex, 0.8f);
        track++;
    }
}

Voice* AudioEngine::allocateVoice(int note)
{
    juce::ignoreUnused(note);

    // First, look for inactive voice
    for (auto& voice : voices_)
    {
        if (!voice.isActive())
            return &voice;
    }

    // Voice stealing: find oldest voice
    Voice* oldest = &voices_[0];
    for (auto& voice : voices_)
    {
        if (voice.getStartTime() < oldest->getStartTime())
            oldest = &voice;
    }

    oldest->noteOff();
    return oldest;
}

void AudioEngine::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    sampleRate_ = sampleRate;
    samplesPerBlock_ = samplesPerBlockExpected;

    // Initialize all instrument processors
    double tempo = project_ ? project_->getTempo() : 120.0;
    for (auto& processor : instrumentProcessors_)
    {
        if (processor)
        {
            processor->init(sampleRate);
            processor->setTempo(tempo);
        }
    }

    // Initialize all sampler processors
    for (auto& sampler : samplerProcessors_)
    {
        if (sampler)
        {
            sampler->init(sampleRate);
        }
    }

    // Initialize all slicer processors
    for (auto& slicer : slicerProcessors_)
    {
        if (slicer)
        {
            slicer->init(sampleRate);
        }
    }

    // Initialize all VA synth processors
    for (auto& vaSynth : vaSynthProcessors_)
    {
        if (vaSynth)
        {
            vaSynth->init(sampleRate);
            vaSynth->setTempo(tempo);
        }
    }

    // Initialize legacy voices (can be removed later)
    for (auto& voice : voices_)
    {
        voice.init();
    }

    // Initialize effects processor
    effects_.init(sampleRate);
    effects_.setTempo(static_cast<float>(tempo));
}

void AudioEngine::releaseResources()
{
    stop();
}

void AudioEngine::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();

    // Handle pending transport commands (lock-free from UI thread)
    if (pendingStop_.load(std::memory_order_acquire))
    {
        pendingStop_.store(false, std::memory_order_relaxed);
        playing_.store(false, std::memory_order_relaxed);

        // Release all notes on all instrument processors
        for (auto& processor : instrumentProcessors_)
        {
            if (processor)
                processor->allNotesOff();
        }

        // Release all sampler processors
        for (auto& sampler : samplerProcessors_)
        {
            if (sampler)
                sampler->allNotesOff();
        }

        // Release all slicer processors
        for (auto& slicer : slicerProcessors_)
        {
            if (slicer)
                slicer->allNotesOff();
        }

        // Release all VA synth processors
        for (auto& vaSynth : vaSynthProcessors_)
        {
            if (vaSynth)
                vaSynth->allNotesOff();
        }

        // Legacy: Release all voices
        for (auto& voice : voices_)
        {
            if (voice.isActive())
                voice.noteOff();
        }
        trackVoices_.fill(nullptr);
        trackInstruments_.fill(-1);
    }

    if (pendingPlay_.load(std::memory_order_acquire))
    {
        pendingPlay_.store(false, std::memory_order_relaxed);
        playing_.store(true, std::memory_order_relaxed);
        currentRow_ = 0;
        samplesUntilNextRow_ = 0.0;

        // Reset song playback state
        currentSongRow_ = 0;
        currentChainPosition_ = 0;
        trackChainPositions_.fill(0);
    }

    std::lock_guard<std::recursive_mutex> lock(mutex_);

    float* outL = bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
    float* outR = bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample);

    int numSamples = bufferToFill.numSamples;

    // Update tempo for effects
    if (project_)
        effects_.setTempo(project_->getTempo());

    // If playing, process pattern(s)
    if (playing_.load(std::memory_order_relaxed) && project_)
    {
        double samplesPerBeat = sampleRate_ * 60.0 / project_->getTempo();
        double samplesPerRow = samplesPerBeat / 4.0;  // 4 rows per beat

        // Get pattern length from column 0's pattern (or current pattern in Pattern mode)
        int patternLength = 16;  // default
        if (playMode_ == PlayMode::Pattern)
        {
            auto* pattern = project_->getPattern(currentPattern_);
            if (pattern) patternLength = pattern->getLength();
        }
        else
        {
            // In Song mode, use the longest pattern across all columns
            for (int col = 0; col < NUM_TRACKS; ++col)
            {
                int patIdx = getPatternIndexForColumn(col);
                if (patIdx >= 0)
                {
                    auto* pat = project_->getPattern(patIdx);
                    if (pat && pat->getLength() > patternLength)
                        patternLength = pat->getLength();
                }
            }
        }

        int processed = 0;
        while (processed < numSamples)
        {
            if (samplesUntilNextRow_ <= 0.0)
            {
                if (playMode_ == PlayMode::Pattern)
                {
                    // Pattern mode: play single pattern on all 16 tracks
                    auto* pattern = project_->getPattern(currentPattern_);
                    if (pattern)
                    {
                        for (int track = 0; track < NUM_TRACKS; ++track)
                        {
                            const auto& step = pattern->getStep(track, currentRow_);
                            if (step.note == model::Step::NOTE_OFF)
                            {
                                releaseNote(track);
                            }
                            else if (step.note >= 0 && step.instrument >= 0)
                            {
                                float vel = step.volume < 0xFF ? step.volume / 255.0f : 1.0f;
                                triggerNote(track, step.note, step.instrument, vel, step);
                            }
                        }
                        patternLength = pattern->getLength();
                    }
                }
                else
                {
                    // Song mode: play patterns from ALL song columns
                    // Each pattern plays ALL its tracks (like Pattern mode)
                    // Use instrument index as audio track to avoid conflicts
                    const auto& song = project_->getSong();

                    for (int col = 0; col < NUM_TRACKS; ++col)
                    {
                        int patIdx = getPatternIndexForColumn(col);
                        if (patIdx < 0) continue;

                        auto* pattern = project_->getPattern(patIdx);
                        if (!pattern) continue;

                        // Get chain transpose and scale lock for this column
                        int chainTranspose = getChainTransposeForColumn(col);
                        std::string scaleLock;
                        const auto& songTrack = song.getTrack(col);
                        if (currentSongRow_ < static_cast<int>(songTrack.size()))
                        {
                            int chainIndex = songTrack[static_cast<size_t>(currentSongRow_)];
                            if (auto* chain = project_->getChain(chainIndex))
                            {
                                scaleLock = chain->getScaleLock();
                            }
                        }

                        // Play ALL tracks of this pattern (like Pattern mode)
                        if (currentRow_ < pattern->getLength())
                        {
                            for (int track = 0; track < NUM_TRACKS; ++track)
                            {
                                const auto& step = pattern->getStep(track, currentRow_);

                                if (step.note == model::Step::NOTE_OFF)
                                {
                                    // Use instrument as audio track for release
                                    if (step.instrument >= 0)
                                        releaseNote(step.instrument % NUM_TRACKS);
                                }
                                else if (step.note >= 0 && step.instrument >= 0)
                                {
                                    // Apply chain transpose (scale-degree based)
                                    int transposedNote = step.note;
                                    if (chainTranspose != 0 && !scaleLock.empty())
                                    {
                                        transposedNote = transposeNoteByScaleDegrees(step.note, chainTranspose, scaleLock);
                                    }

                                    float vel = step.volume < 0xFF ? step.volume / 255.0f : 1.0f;
                                    // Use instrument index as audio track to avoid conflicts
                                    // between patterns using same track with different instruments
                                    triggerNote(step.instrument % NUM_TRACKS, transposedNote, step.instrument, vel, step);
                                }
                            }
                        }
                    }
                }

                // Advance row within pattern
                currentRow_++;
                if (currentRow_ >= patternLength)
                {
                    currentRow_ = 0;

                    if (playMode_ == PlayMode::Song)
                    {
                        // Advance all song columns to their next patterns
                        advanceAllChains();
                    }
                    // In Pattern mode, just loop the same pattern
                }

                // Apply groove timing from track 0's groove template
                float grooveOffset = 0.0f;
                int grooveIdx = project_->getTrackGroove(0);
                const auto& groove = grooveManager_.getTemplate(grooveIdx);
                grooveOffset = groove.timings[static_cast<size_t>(currentRow_ % 16)];

                samplesUntilNextRow_ = samplesPerRow * (1.0 + static_cast<double>(grooveOffset));

                // Ensure samplesUntilNextRow_ is always positive to prevent infinite loop
                // This guards against extreme groove offsets near -1.0
                if (samplesUntilNextRow_ < 1.0)
                    samplesUntilNextRow_ = 1.0;
            }

            // Ensure we always advance by at least 1 sample to avoid infinite loop
            // This can happen at high BPM when samplesUntilNextRow_ < 1.0
            int blockSamples = std::min(numSamples - processed,
                                        std::max(1, static_cast<int>(samplesUntilNextRow_)));

            samplesUntilNextRow_ -= blockSamples;
            processed += blockSamples;
        }
    }

    // Check if any instrument is soloed
    bool anySoloed = false;
    if (project_)
    {
        for (int i = 0; i < project_->getInstrumentCount(); ++i)
        {
            if (auto* inst = project_->getInstrument(i))
            {
                if (inst->isSoloed())
                {
                    anySoloed = true;
                    break;
                }
            }
        }
    }

    // Temporary buffers for per-instrument rendering
    std::array<float, 512> tempL{}, tempR{};

    // Buffers for sidechain source capture
    std::array<float, 512> sidechainSourceL{}, sidechainSourceR{};
    std::fill(sidechainSourceL.begin(), sidechainSourceL.begin() + numSamples, 0.0f);
    std::fill(sidechainSourceR.begin(), sidechainSourceR.begin() + numSamples, 0.0f);

    // Get sidechain source instrument from mixer settings
    int sidechainSourceInst = project_ ? project_->getMixer().sidechainSource : -1;

    // Render all instrument processors with per-instrument mixing
    float avgReverb = 0.0f, avgDelay = 0.0f, avgChorus = 0.0f;
    float avgDrive = 0.0f;
    int activeCount = 0;

    for (int instIdx = 0; instIdx < NUM_INSTRUMENTS; ++instIdx)
    {
        auto& processor = instrumentProcessors_[instIdx];
        if (!processor) continue;

        // Get instrument model for mixer settings
        model::Instrument* instrument = project_ ? project_->getInstrument(instIdx) : nullptr;

        // Determine if this instrument should play
        bool shouldPlay = true;
        float volume = 1.0f;
        float pan = 0.0f;

        if (instrument)
        {
            // Check mute/solo
            if (anySoloed)
            {
                // In solo mode, only play soloed instruments
                shouldPlay = instrument->isSoloed();
            }
            else
            {
                // Normal mode, respect mute
                shouldPlay = !instrument->isMuted();
            }

            volume = instrument->getVolume();
            pan = instrument->getPan();
        }

        if (!shouldPlay)
        {
            // Still process to keep envelopes/LFOs running, but don't add to output
            processor->process(tempL.data(), tempR.data(), numSamples);
            continue;
        }

        // Clear temp buffers
        std::fill(tempL.begin(), tempL.begin() + numSamples, 0.0f);
        std::fill(tempR.begin(), tempR.begin() + numSamples, 0.0f);

        // Render instrument to temp buffers
        processor->process(tempL.data(), tempR.data(), numSamples);

        // Apply volume and pan, mix into output
        // Pan law: constant power (sqrt)
        float leftGain = volume * std::sqrt((1.0f - pan) / 2.0f);
        float rightGain = volume * std::sqrt((1.0f + pan) / 2.0f);

        for (int i = 0; i < numSamples; ++i)
        {
            // Simple pan: distribute mono sum based on pan
            float monoSample = (tempL[i] + tempR[i]) * 0.5f;
            outL[i] += monoSample * leftGain * 2.0f;  // *2 to compensate for mono sum
            outR[i] += monoSample * rightGain * 2.0f;

            // Capture sidechain source audio
            if (instIdx == sidechainSourceInst)
            {
                sidechainSourceL[i] += monoSample * leftGain * 2.0f;
                sidechainSourceR[i] += monoSample * rightGain * 2.0f;
            }
        }

        // Accumulate send levels from active instruments
        if (instrument && shouldPlay)
        {
            const auto& sends = instrument->getSends();
            avgReverb += sends.reverb * volume;
            avgDelay += sends.delay * volume;
            avgChorus += sends.chorus * volume;
            avgDrive += sends.drive * volume;
            activeCount++;
        }
    }

    // Process sampler instruments
    for (int instIdx = 0; instIdx < NUM_INSTRUMENTS; ++instIdx)
    {
        auto& sampler = samplerProcessors_[instIdx];
        if (!sampler) continue;

        // Get instrument model for mixer settings
        model::Instrument* instrument = project_ ? project_->getInstrument(instIdx) : nullptr;

        // Only process if this is a Sampler type instrument
        if (!instrument || instrument->getType() != model::InstrumentType::Sampler)
            continue;

        // Determine if this instrument should play
        bool shouldPlay = true;
        float volume = 1.0f;
        float pan = 0.0f;

        // Check mute/solo
        if (anySoloed)
        {
            shouldPlay = instrument->isSoloed();
        }
        else
        {
            shouldPlay = !instrument->isMuted();
        }

        if (!shouldPlay || !sampler->hasSample())
        {
            continue;
        }

        volume = instrument->getVolume();
        pan = instrument->getPan();

        // Clear temp buffers
        std::fill(tempL.begin(), tempL.begin() + numSamples, 0.0f);
        std::fill(tempR.begin(), tempR.begin() + numSamples, 0.0f);

        // Render sampler to temp buffers
        sampler->process(tempL.data(), tempR.data(), numSamples);

        // Apply volume and pan, mix into output
        float leftGain = volume * std::sqrt((1.0f - pan) / 2.0f);
        float rightGain = volume * std::sqrt((1.0f + pan) / 2.0f);

        for (int i = 0; i < numSamples; ++i)
        {
            float monoSample = (tempL[i] + tempR[i]) * 0.5f;
            outL[i] += monoSample * leftGain * 2.0f;
            outR[i] += monoSample * rightGain * 2.0f;

            // Capture sidechain source audio
            if (instIdx == sidechainSourceInst)
            {
                sidechainSourceL[i] += monoSample * leftGain * 2.0f;
                sidechainSourceR[i] += monoSample * rightGain * 2.0f;
            }
        }

        // Accumulate send levels
        const auto& sends = instrument->getSends();
        avgReverb += sends.reverb * volume;
        avgDelay += sends.delay * volume;
        avgChorus += sends.chorus * volume;
        avgDrive += sends.drive * volume;
        activeCount++;
    }

    // Process slicer instruments
    for (int instIdx = 0; instIdx < NUM_INSTRUMENTS; ++instIdx)
    {
        auto& slicer = slicerProcessors_[instIdx];
        if (!slicer) continue;

        // Get instrument model for mixer settings
        model::Instrument* instrument = project_ ? project_->getInstrument(instIdx) : nullptr;

        // Only process if this is a Slicer type instrument
        if (!instrument || instrument->getType() != model::InstrumentType::Slicer)
            continue;

        // Determine if this instrument should play
        bool shouldPlay = true;
        float volume = 1.0f;
        float pan = 0.0f;

        // Check mute/solo
        if (anySoloed)
        {
            shouldPlay = instrument->isSoloed();
        }
        else
        {
            shouldPlay = !instrument->isMuted();
        }

        if (!shouldPlay || !slicer->hasSample())
        {
            continue;
        }

        volume = instrument->getVolume();
        pan = instrument->getPan();

        // Clear temp buffers
        std::fill(tempL.begin(), tempL.begin() + numSamples, 0.0f);
        std::fill(tempR.begin(), tempR.begin() + numSamples, 0.0f);

        // Render slicer to temp buffers
        slicer->process(tempL.data(), tempR.data(), numSamples);

        // Apply volume and pan, mix into output
        float leftGain = volume * std::sqrt((1.0f - pan) / 2.0f);
        float rightGain = volume * std::sqrt((1.0f + pan) / 2.0f);

        for (int i = 0; i < numSamples; ++i)
        {
            float monoSample = (tempL[i] + tempR[i]) * 0.5f;
            outL[i] += monoSample * leftGain * 2.0f;
            outR[i] += monoSample * rightGain * 2.0f;

            // Capture sidechain source audio
            if (instIdx == sidechainSourceInst)
            {
                sidechainSourceL[i] += monoSample * leftGain * 2.0f;
                sidechainSourceR[i] += monoSample * rightGain * 2.0f;
            }
        }

        // Accumulate send levels
        const auto& sends = instrument->getSends();
        avgReverb += sends.reverb * volume;
        avgDelay += sends.delay * volume;
        avgChorus += sends.chorus * volume;
        avgDrive += sends.drive * volume;
        activeCount++;
    }

    // Process VA synth instruments
    for (int instIdx = 0; instIdx < NUM_INSTRUMENTS; ++instIdx)
    {
        auto& vaSynth = vaSynthProcessors_[instIdx];
        if (!vaSynth) continue;

        // Get instrument model for mixer settings
        model::Instrument* instrument = project_ ? project_->getInstrument(instIdx) : nullptr;

        // Only process if this is a VASynth type instrument
        if (!instrument || instrument->getType() != model::InstrumentType::VASynth)
            continue;

        // Determine if this instrument should play
        bool shouldPlay = true;
        float volume = 1.0f;
        float pan = 0.0f;

        // Check mute/solo
        if (anySoloed)
        {
            shouldPlay = instrument->isSoloed();
        }
        else
        {
            shouldPlay = !instrument->isMuted();
        }

        if (!shouldPlay)
        {
            continue;
        }

        volume = instrument->getVolume();
        pan = instrument->getPan();

        // Clear temp buffers
        std::fill(tempL.begin(), tempL.begin() + numSamples, 0.0f);
        std::fill(tempR.begin(), tempR.begin() + numSamples, 0.0f);

        // Render VA synth to temp buffers
        vaSynth->process(tempL.data(), tempR.data(), numSamples);

        // Apply volume and pan, mix into output
        float leftGain = volume * std::sqrt((1.0f - pan) / 2.0f);
        float rightGain = volume * std::sqrt((1.0f + pan) / 2.0f);

        for (int i = 0; i < numSamples; ++i)
        {
            float monoSample = (tempL[i] + tempR[i]) * 0.5f;
            outL[i] += monoSample * leftGain * 2.0f;
            outR[i] += monoSample * rightGain * 2.0f;

            // Capture sidechain source audio
            if (instIdx == sidechainSourceInst)
            {
                sidechainSourceL[i] += monoSample * leftGain * 2.0f;
                sidechainSourceR[i] += monoSample * rightGain * 2.0f;
            }
        }

        // Accumulate send levels
        const auto& sends = instrument->getSends();
        avgReverb += sends.reverb * volume;
        avgDelay += sends.delay * volume;
        avgChorus += sends.chorus * volume;
        avgDrive += sends.drive * volume;
        activeCount++;
    }

    if (activeCount > 0)
    {
        avgReverb /= static_cast<float>(activeCount);
        avgDelay /= static_cast<float>(activeCount);
        avgChorus /= static_cast<float>(activeCount);
        avgDrive /= static_cast<float>(activeCount);
    }

    // Update effect parameters from project settings
    if (project_)
    {
        const auto& mixer = project_->getMixer();
        effects_.reverb.setParams(mixer.reverbSize, mixer.reverbDamping, 1.0f);
        effects_.delay.setParams(mixer.delayTime, mixer.delayFeedback, 1.0f);
        effects_.chorus.setParams(mixer.chorusRate, mixer.chorusDepth, 1.0f);
        effects_.drive.setParams(mixer.driveGain, mixer.driveTone);
        effects_.sidechain.setParams(mixer.sidechainAttack, mixer.sidechainRelease, mixer.sidechainRatio);
        effects_.djFilter.setPosition(mixer.djFilterPosition);
        effects_.limiter.setParams(mixer.limiterThreshold, mixer.limiterRelease);
    }

    // Apply effects per sample
    for (int i = 0; i < numSamples; ++i)
    {
        // Feed sidechain envelope follower with source audio level
        if (sidechainSourceInst >= 0)
        {
            float sourceLevel = (sidechainSourceL[i] + sidechainSourceR[i]) * 0.5f;
            effects_.sidechain.feedSource(sourceLevel);
        }

        // Apply reverb, delay, chorus, drive
        effects_.process(outL[i], outR[i], avgReverb, avgDelay, avgChorus, avgDrive);

        // Apply sidechain ducking to entire output (all instruments except source are ducked)
        // The source instrument is NOT ducked - it triggers the ducking
        if (sidechainSourceInst >= 0)
        {
            effects_.sidechain.process(outL[i], outR[i]);
        }
    }

    // Apply master volume and master bus effects (DJ filter + limiter)
    if (project_)
    {
        float masterVol = project_->getMixer().masterVolume;
        for (int i = 0; i < numSamples; ++i)
        {
            // Apply master volume
            outL[i] *= masterVol;
            outR[i] *= masterVol;

            // Apply master bus effects (DJ filter then limiter)
            // Limiter replaces hard clipping for transparent peak control
            effects_.processMaster(outL[i], outR[i]);
        }
    }
}

void AudioEngine::advancePlayhead()
{
    // Called from audio thread - advance to next row
    if (!project_) return;

    auto* pattern = project_->getPattern(getCurrentPatternIndex());
    if (pattern)
    {
        currentRow_ = (currentRow_ + 1) % pattern->getLength();
    }
}

int AudioEngine::getCurrentPatternIndex() const
{
    if (playMode_ == PlayMode::Pattern)
    {
        // In pattern mode, just return the current pattern
        return currentPattern_;
    }

    // In song mode, get the pattern from the current chain position
    if (!project_) return currentPattern_;

    const auto& song = project_->getSong();

    // Get chain index from track 0 at current song row (track 0 drives timing)
    const auto& track0 = song.getTrack(0);
    if (currentSongRow_ >= static_cast<int>(track0.size()))
    {
        return currentPattern_;  // Fallback
    }

    int chainIndex = track0[static_cast<size_t>(currentSongRow_)];
    if (chainIndex < 0)
    {
        return currentPattern_;  // Empty slot
    }

    auto* chain = project_->getChain(chainIndex);
    if (!chain || chain->getPatternCount() == 0)
    {
        return currentPattern_;  // Empty chain
    }

    // Get pattern index from current position in chain
    if (currentChainPosition_ >= chain->getPatternCount())
    {
        return currentPattern_;
    }

    return chain->getPatternIndices()[static_cast<size_t>(currentChainPosition_)];
}

int AudioEngine::getPatternIndexForColumn(int songColumn) const
{
    if (playMode_ == PlayMode::Pattern)
    {
        // In pattern mode, only column 0 plays
        return (songColumn == 0) ? currentPattern_.load(std::memory_order_relaxed) : -1;
    }

    if (!project_) return -1;

    const auto& song = project_->getSong();
    const auto& track = song.getTrack(songColumn);

    if (currentSongRow_ >= static_cast<int>(track.size()))
    {
        return -1;  // No chain at this position
    }

    int chainIndex = track[static_cast<size_t>(currentSongRow_)];
    if (chainIndex < 0)
    {
        return -1;  // Empty slot
    }

    auto* chain = project_->getChain(chainIndex);
    if (!chain || chain->getPatternCount() == 0)
    {
        return -1;  // Empty chain
    }

    // Get pattern index from this column's position in its chain
    int chainPos = trackChainPositions_[static_cast<size_t>(songColumn)];
    if (chainPos >= chain->getPatternCount())
    {
        return -1;
    }

    return chain->getPatternIndices()[static_cast<size_t>(chainPos)];
}

void AudioEngine::advanceChain()
{
    // Called when we finish a pattern in song mode
    if (!project_) return;

    const auto& song = project_->getSong();
    const auto& track0 = song.getTrack(0);

    if (track0.empty() || song.getLength() == 0)
    {
        // No song data - stay on current pattern
        return;
    }

    // Get current chain
    if (currentSongRow_ >= static_cast<int>(track0.size()))
    {
        currentSongRow_ = 0;
        currentChainPosition_ = 0;
        return;
    }

    int chainIndex = track0[static_cast<size_t>(currentSongRow_)];
    auto* chain = (chainIndex >= 0) ? project_->getChain(chainIndex) : nullptr;

    if (chain && chain->getPatternCount() > 0)
    {
        // Advance to next pattern in chain
        currentChainPosition_++;

        if (currentChainPosition_ >= chain->getPatternCount())
        {
            // Finished this chain, move to next song row
            currentChainPosition_ = 0;
            currentSongRow_++;

            // Check for end of song
            if (currentSongRow_ >= song.getLength())
            {
                currentSongRow_ = 0;  // Loop back to beginning
            }
        }
    }
    else
    {
        // Empty chain slot - skip to next song row
        currentChainPosition_ = 0;
        currentSongRow_++;

        // Check for end of song
        if (currentSongRow_ >= song.getLength())
        {
            currentSongRow_ = 0;  // Loop back to beginning
        }
    }
}

void AudioEngine::advanceAllChains()
{
    // Advance all song columns' chain positions simultaneously
    if (!project_) return;

    const auto& song = project_->getSong();
    if (song.getLength() == 0) return;

    // Find the maximum pattern length across all active chains in this row
    // We need to know when ALL chains are done with their current pattern
    bool allChainsFinished = true;

    // Advance each song column's chain position
    for (int col = 0; col < NUM_TRACKS; ++col)
    {
        const auto& track = song.getTrack(col);
        if (currentSongRow_ >= static_cast<int>(track.size())) continue;

        int chainIndex = track[static_cast<size_t>(currentSongRow_)];
        if (chainIndex < 0) continue;

        auto* chain = project_->getChain(chainIndex);
        if (!chain || chain->getPatternCount() == 0) continue;

        // Advance this column's position in its chain
        trackChainPositions_[static_cast<size_t>(col)]++;

        // Check if this chain still has more patterns
        if (trackChainPositions_[static_cast<size_t>(col)] < chain->getPatternCount())
        {
            allChainsFinished = false;
        }
    }

    // If all chains are done (or empty), move to next song row
    if (allChainsFinished)
    {
        currentSongRow_++;
        trackChainPositions_.fill(0);  // Reset all chain positions

        // Loop back to beginning if at end of song
        if (currentSongRow_ >= song.getLength())
        {
            currentSongRow_ = 0;
        }
    }

    // Keep legacy currentChainPosition_ in sync with column 0 for backwards compatibility
    currentChainPosition_ = trackChainPositions_[0];
}

int AudioEngine::getCurrentChainTranspose() const
{
    if (playMode_ != PlayMode::Song || !project_) return 0;

    const auto& song = project_->getSong();
    const auto& track0 = song.getTrack(0);

    if (currentSongRow_ >= static_cast<int>(track0.size())) return 0;

    int chainIndex = track0[static_cast<size_t>(currentSongRow_)];
    if (chainIndex < 0) return 0;

    auto* chain = project_->getChain(chainIndex);
    if (!chain || currentChainPosition_ >= chain->getPatternCount()) return 0;

    return chain->getTranspose(currentChainPosition_);
}

int AudioEngine::getChainTransposeForColumn(int songColumn) const
{
    if (playMode_ != PlayMode::Song || !project_) return 0;

    const auto& song = project_->getSong();
    const auto& track = song.getTrack(songColumn);

    if (currentSongRow_ >= static_cast<int>(track.size())) return 0;

    int chainIndex = track[static_cast<size_t>(currentSongRow_)];
    if (chainIndex < 0) return 0;

    auto* chain = project_->getChain(chainIndex);
    int chainPos = trackChainPositions_[static_cast<size_t>(songColumn)];
    if (!chain || chainPos >= chain->getPatternCount()) return 0;

    return chain->getTranspose(chainPos);
}

int AudioEngine::transposeNoteByScaleDegrees(int note, int degrees, const std::string& scaleLock) const
{
    if (degrees == 0 || scaleLock.empty()) return note;

    // Parse scale lock (e.g., "C Minor", "A# Major")
    std::string rootStr;
    std::string scaleType = "Major";
    size_t spacePos = scaleLock.find(' ');
    if (spacePos != std::string::npos)
    {
        rootStr = scaleLock.substr(0, spacePos);
        scaleType = scaleLock.substr(spacePos + 1);
    }
    else
    {
        rootStr = scaleLock;
    }

    // Convert root note to MIDI note (C0 = 12, C1 = 24, etc.)
    static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    int rootNote = 0;  // C by default
    for (int i = 0; i < 12; ++i)
    {
        if (rootStr == noteNames[i])
        {
            rootNote = i;
            break;
        }
    }

    // Scale intervals (semitones from root)
    static const int majorScale[] = { 0, 2, 4, 5, 7, 9, 11 };
    static const int minorScale[] = { 0, 2, 3, 5, 7, 8, 10 };
    const int* scale = (scaleType == "Minor") ? minorScale : majorScale;

    // Calculate note position relative to the scale root
    int noteRelativeToRoot = note - rootNote;

    // Find the scale octave (how many complete octaves from root)
    int scaleOctave = noteRelativeToRoot / 12;
    if (noteRelativeToRoot < 0 && noteRelativeToRoot % 12 != 0)
        scaleOctave--;  // Correct for negative modulo

    // Note position within current scale octave (0-11)
    int noteInScaleOctave = ((noteRelativeToRoot % 12) + 12) % 12;

    // Find current scale degree
    int currentDegree = 0;
    int minDistance = 12;
    for (int i = 0; i < 7; ++i)
    {
        int dist = std::abs(noteInScaleOctave - scale[i]);
        if (dist < minDistance)
        {
            minDistance = dist;
            currentDegree = i;
        }
    }

    // Apply transpose in scale degrees
    int newDegree = currentDegree + degrees;
    int octaveShift = 0;

    // Handle wrapping
    while (newDegree >= 7)
    {
        newDegree -= 7;
        octaveShift++;
    }
    while (newDegree < 0)
    {
        newDegree += 7;
        octaveShift--;
    }

    // Calculate new note relative to root, then add root back
    int newNote = rootNote + (scaleOctave + octaveShift) * 12 + scale[newDegree];
    return std::clamp(newNote, 1, 127);
}

void AudioEngine::syncInstrumentParams(int instrumentIndex)
{
    if (!project_) return;
    if (instrumentIndex < 0 || instrumentIndex >= NUM_INSTRUMENTS) return;

    auto* instrument = project_->getInstrument(instrumentIndex);
    auto* processor = instrumentProcessors_[instrumentIndex].get();
    if (!instrument || !processor) return;

    // Sync basic parameters from model::Instrument to PlaitsInstrument
    const auto& params = instrument->getParams();
    processor->setParameter(kParamEngine, static_cast<float>(params.engine) / 15.0f);
    processor->setParameter(kParamHarmonics, params.harmonics);
    processor->setParameter(kParamTimbre, params.timbre);
    processor->setParameter(kParamMorph, params.morph);
    processor->setParameter(kParamAttack, params.attack);
    processor->setParameter(kParamDecay, params.decay);
    processor->setParameter(kParamPolyphony, static_cast<float>(params.polyphony) / 16.0f);

    // Sync filter parameters
    processor->setParameter(kParamCutoff, params.filter.cutoff);
    processor->setParameter(kParamResonance, params.filter.resonance);

    // Sync LFO1 parameters
    processor->setParameter(kParamLfo1Rate, static_cast<float>(params.lfo1.rate) / 15.0f);
    processor->setParameter(kParamLfo1Shape, static_cast<float>(params.lfo1.shape) / 4.0f);
    processor->setParameter(kParamLfo1Dest, static_cast<float>(params.lfo1.dest) / 8.0f);
    processor->setParameter(kParamLfo1Amount, static_cast<float>(params.lfo1.amount + 64) / 127.0f);

    // Sync LFO2 parameters
    processor->setParameter(kParamLfo2Rate, static_cast<float>(params.lfo2.rate) / 15.0f);
    processor->setParameter(kParamLfo2Shape, static_cast<float>(params.lfo2.shape) / 4.0f);
    processor->setParameter(kParamLfo2Dest, static_cast<float>(params.lfo2.dest) / 8.0f);
    processor->setParameter(kParamLfo2Amount, static_cast<float>(params.lfo2.amount + 64) / 127.0f);

    // Sync ENV1 parameters
    processor->setParameter(kParamEnv1Attack, params.env1.attack);
    processor->setParameter(kParamEnv1Decay, params.env1.decay);
    processor->setParameter(kParamEnv1Dest, static_cast<float>(params.env1.dest) / 8.0f);
    processor->setParameter(kParamEnv1Amount, static_cast<float>(params.env1.amount + 64) / 127.0f);

    // Sync ENV2 parameters
    processor->setParameter(kParamEnv2Attack, params.env2.attack);
    processor->setParameter(kParamEnv2Decay, params.env2.decay);
    processor->setParameter(kParamEnv2Dest, static_cast<float>(params.env2.dest) / 8.0f);
    processor->setParameter(kParamEnv2Amount, static_cast<float>(params.env2.amount + 64) / 127.0f);

    // Sync tempo
    processor->setTempo(project_->getTempo());
}

PlaitsInstrument* AudioEngine::getInstrumentProcessor(int index)
{
    if (index >= 0 && index < NUM_INSTRUMENTS)
        return instrumentProcessors_[index].get();
    return nullptr;
}

const PlaitsInstrument* AudioEngine::getInstrumentProcessor(int index) const
{
    if (index >= 0 && index < NUM_INSTRUMENTS)
        return instrumentProcessors_[index].get();
    return nullptr;
}

SamplerInstrument* AudioEngine::getSamplerProcessor(int index)
{
    if (index >= 0 && index < NUM_INSTRUMENTS)
        return samplerProcessors_[index].get();
    return nullptr;
}

SlicerInstrument* AudioEngine::getSlicerProcessor(int index)
{
    if (index >= 0 && index < NUM_INSTRUMENTS)
        return slicerProcessors_[index].get();
    return nullptr;
}

VASynthInstrument* AudioEngine::getVASynthProcessor(int index)
{
    if (index >= 0 && index < NUM_INSTRUMENTS)
        return vaSynthProcessors_[index].get();
    return nullptr;
}

} // namespace audio
