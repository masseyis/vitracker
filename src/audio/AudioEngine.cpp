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
        voice->noteOn(note, velocity, *instrument);
        trackVoices_[track] = voice;
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
                            triggerNote(track, step.note, step.instrument, vel);
                        }
                    }

                    // Advance row
                    currentRow_ = (currentRow_ + 1) % pattern->getLength();
                    samplesUntilNextRow_ = samplesPerRow;
                }

                int blockSamples = std::min(numSamples - processed,
                                            static_cast<int>(samplesUntilNextRow_));

                samplesUntilNextRow_ -= blockSamples;
                processed += blockSamples;
            }
        }
    }

    // Render all active voices
    for (auto& voice : voices_)
    {
        if (voice.isActive())
        {
            voice.render(outL, outR, numSamples);
        }
    }

    // Apply master volume from mixer
    if (project_)
    {
        float masterVol = project_->getMixer().masterVolume;
        for (int i = 0; i < numSamples; ++i)
        {
            outL[i] *= masterVol;
            outR[i] *= masterVol;
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
