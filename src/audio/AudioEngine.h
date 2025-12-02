#pragma once

#include "Voice.h"
#include "../model/Project.h"
#include <JuceHeader.h>
#include <array>
#include <mutex>

namespace audio {

class AudioEngine : public juce::AudioSource
{
public:
    static constexpr int NUM_VOICES = 64;
    static constexpr int NUM_TRACKS = 16;

    AudioEngine();
    ~AudioEngine() override;

    void setProject(model::Project* project) { project_ = project; }

    // Transport
    void play();
    void stop();
    bool isPlaying() const { return playing_; }

    int getCurrentRow() const { return currentRow_; }
    int getCurrentPattern() const { return currentPattern_; }

    // Trigger a note
    void triggerNote(int track, int note, int instrumentIndex, float velocity);
    void releaseNote(int track);

    // AudioSource interface
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

private:
    Voice* allocateVoice(int note);
    void advancePlayhead();

    model::Project* project_ = nullptr;

    std::array<Voice, NUM_VOICES> voices_;
    std::array<Voice*, NUM_TRACKS> trackVoices_;  // Current voice per track

    double sampleRate_ = 48000.0;
    int samplesPerBlock_ = 512;

    bool playing_ = false;
    int currentRow_ = 0;
    int currentPattern_ = 0;
    double samplesUntilNextRow_ = 0.0;

    std::mutex mutex_;
};

} // namespace audio
