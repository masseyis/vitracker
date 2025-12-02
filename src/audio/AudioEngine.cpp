#include "AudioEngine.h"

namespace audio {

AudioEngine::AudioEngine()
{
    trackVoices_.fill(nullptr);
}

AudioEngine::~AudioEngine() = default;

void AudioEngine::play()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!playing_)
    {
        playing_ = true;
        currentRow_ = 0;
        samplesUntilNextRow_ = 0.0;
    }
}

void AudioEngine::stop()
{
    std::lock_guard<std::mutex> lock(mutex_);
    playing_ = false;

    // Release all voices
    for (auto& voice : voices_)
    {
        if (voice.isActive())
            voice.noteOff();
    }
    trackVoices_.fill(nullptr);
}

void AudioEngine::triggerNote(int track, int note, int instrumentIndex, float velocity)
{
    model::Step emptyStep;
    triggerNote(track, note, instrumentIndex, velocity, emptyStep);
}

void AudioEngine::triggerNote(int track, int note, int instrumentIndex, float velocity, const model::Step& step)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!project_) return;
    auto* instrument = project_->getInstrument(instrumentIndex);
    if (!instrument) return;

    // Release current voice on this track
    if (trackVoices_[track])
    {
        trackVoices_[track]->noteOff();
    }

    // Allocate new voice
    Voice* voice = allocateVoice(note);
    if (voice)
    {
        voice->setSampleRate(sampleRate_);
        voice->noteOn(note, velocity, *instrument);
        trackVoices_[track] = voice;

        // Apply FX commands from the step
        if (!step.fx1.isEmpty()) voice->setFX(step.fx1.type, step.fx1.value);
        if (!step.fx2.isEmpty()) voice->setFX(step.fx2.type, step.fx2.value);
        if (!step.fx3.isEmpty()) voice->setFX(step.fx3.type, step.fx3.value);

        // Trigger sidechain if this instrument has high sidechainDuck
        // (typically bass drums or similar trigger instruments)
        if (instrument->getSends().sidechainDuck > 0.5f)
        {
            effects_.sidechain.trigger();
        }
    }
}

void AudioEngine::releaseNote(int track)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (trackVoices_[track])
    {
        trackVoices_[track]->noteOff();
        trackVoices_[track] = nullptr;
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

    // Initialize all voices
    for (auto& voice : voices_)
    {
        voice.init();
    }

    // Initialize effects processor
    effects_.init(sampleRate);
    effects_.setTempo(project_ ? project_->getTempo() : 120.0f);
}

void AudioEngine::releaseResources()
{
    stop();
}

void AudioEngine::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();

    std::lock_guard<std::mutex> lock(mutex_);

    float* outL = bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
    float* outR = bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample);

    int numSamples = bufferToFill.numSamples;

    // Update tempo for effects
    if (project_)
        effects_.setTempo(project_->getTempo());

    // If playing, process pattern
    if (playing_ && project_)
    {
        auto* pattern = project_->getPattern(currentPattern_);
        if (pattern)
        {
            double samplesPerBeat = sampleRate_ * 60.0 / project_->getTempo();
            double samplesPerRow = samplesPerBeat / 4.0;  // 4 rows per beat

            int processed = 0;
            while (processed < numSamples)
            {
                if (samplesUntilNextRow_ <= 0.0)
                {
                    // Trigger notes on current row
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

                    // Advance row
                    currentRow_ = (currentRow_ + 1) % pattern->getLength();

                    // Apply groove timing from track 0's groove template
                    float grooveOffset = 0.0f;
                    int grooveIdx = project_->getTrackGroove(0);
                    const auto& groove = grooveManager_.getTemplate(grooveIdx);
                    grooveOffset = groove.timings[static_cast<size_t>(currentRow_ % 16)];

                    samplesUntilNextRow_ = samplesPerRow * (1.0 + static_cast<double>(grooveOffset));
                }

                int blockSamples = std::min(numSamples - processed,
                                            static_cast<int>(samplesUntilNextRow_));

                samplesUntilNextRow_ -= blockSamples;
                processed += blockSamples;
            }
        }
    }

    // Render all active voices and process FX
    for (auto& voice : voices_)
    {
        if (voice.isActive())
        {
            voice.processFX();  // Process per-voice FX (arp, portamento, vibrato, etc.)
            voice.render(outL, outR, numSamples);
        }
    }

    // Process effects per sample using average send levels from active voices
    float avgReverb = 0.0f, avgDelay = 0.0f, avgChorus = 0.0f;
    float avgDrive = 0.0f, avgSidechain = 0.0f;
    int activeCount = 0;

    for (const auto& voice : voices_)
    {
        if (voice.isActive())
        {
            avgReverb += voice.getReverbSend();
            avgDelay += voice.getDelaySend();
            avgChorus += voice.getChorusSend();
            avgDrive += voice.getDriveSend();
            avgSidechain += voice.getSidechainSend();
            activeCount++;
        }
    }

    if (activeCount > 0)
    {
        avgReverb /= static_cast<float>(activeCount);
        avgDelay /= static_cast<float>(activeCount);
        avgChorus /= static_cast<float>(activeCount);
        avgDrive /= static_cast<float>(activeCount);
        avgSidechain /= static_cast<float>(activeCount);
    }

    // Apply effects per sample
    for (int i = 0; i < numSamples; ++i)
    {
        effects_.process(outL[i], outR[i], avgReverb, avgDelay, avgChorus, avgDrive, avgSidechain);
    }

    // Apply master volume from mixer
    if (project_)
    {
        float masterVol = project_->getMixer().masterVolume;
        for (int i = 0; i < numSamples; ++i)
        {
            outL[i] *= masterVol;
            outR[i] *= masterVol;

            // Clip to prevent distortion
            outL[i] = std::clamp(outL[i], -1.0f, 1.0f);
            outR[i] = std::clamp(outR[i], -1.0f, 1.0f);
        }
    }
}

void AudioEngine::advancePlayhead()
{
    // Called from audio thread - advance to next row
    if (!project_) return;

    auto* pattern = project_->getPattern(currentPattern_);
    if (pattern)
    {
        currentRow_ = (currentRow_ + 1) % pattern->getLength();
    }
}

} // namespace audio
