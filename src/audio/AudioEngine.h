#pragma once

#include "Voice.h"
#include "Effects.h"
#include "../model/Project.h"
#include "../model/Groove.h"
#include <JuceHeader.h>
#include <array>
#include <mutex>

namespace audio {

class AudioEngine : public juce::AudioSource
{
public:
    static constexpr int NUM_VOICES = 64;
    static constexpr int NUM_TRACKS = 16;

    enum class PlayMode { Pattern, Song };

    AudioEngine();
    ~AudioEngine() override;

    void setProject(model::Project* project) { project_ = project; }

    // Transport
    void play();
    void stop();
    bool isPlaying() const { return playing_; }

    // Play mode
    void setPlayMode(PlayMode mode) { playMode_ = mode; }
    PlayMode getPlayMode() const { return playMode_; }

    int getCurrentRow() const { return currentRow_; }
    int getCurrentPattern() const { return currentPattern_; }
    int getSongRow() const { return currentSongRow_; }
    int getChainPosition() const { return currentChainPosition_; }

    // Trigger a note
    void triggerNote(int track, int note, int instrumentIndex, float velocity);
    void triggerNote(int track, int note, int instrumentIndex, float velocity, const model::Step& step);
    void releaseNote(int track);

    // AudioSource interface
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

private:
    Voice* allocateVoice(int note);
    void advancePlayhead();
    void advanceChain();
    int getCurrentPatternIndex() const;

    model::Project* project_ = nullptr;

    std::array<Voice, NUM_VOICES> voices_;
    std::array<Voice*, NUM_TRACKS> trackVoices_;  // Current voice per track

    double sampleRate_ = 48000.0;
    int samplesPerBlock_ = 512;

    bool playing_ = false;
    int currentRow_ = 0;
    int currentPattern_ = 0;
    double samplesUntilNextRow_ = 0.0;

    // Song playback state
    PlayMode playMode_ = PlayMode::Pattern;
    int currentSongRow_ = 0;           // Position in song (which row of chains)
    int currentChainPosition_ = 0;     // Position within current chain (which pattern)
    std::array<int, NUM_TRACKS> trackChainPositions_;  // Per-track chain position

    std::mutex mutex_;

    EffectsProcessor effects_;
    model::GrooveManager grooveManager_;
};

} // namespace audio
