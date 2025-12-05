#pragma once

#include "Instrument.h"
#include "Pattern.h"
#include "Chain.h"
#include "Song.h"
#include <string>
#include <vector>
#include <memory>
#include <array>

namespace model {

class Project
{
public:
    static constexpr int MAX_INSTRUMENTS = 128;
    static constexpr int MAX_PATTERNS = 256;
    static constexpr int MAX_CHAINS = 128;

    Project();
    explicit Project(const std::string& name);

    // Project metadata
    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

    float getTempo() const { return tempo_; }
    void setTempo(float bpm) { tempo_ = bpm; }

    const std::string& getGrooveTemplate() const { return grooveTemplate_; }
    void setGrooveTemplate(const std::string& groove) { grooveTemplate_ = groove; }

    // Instruments
    int addInstrument(const std::string& name = "Untitled");
    Instrument* getInstrument(int index);
    const Instrument* getInstrument(int index) const;
    int getInstrumentCount() const { return static_cast<int>(instruments_.size()); }

    // Patterns
    int addPattern(const std::string& name = "Untitled");
    Pattern* getPattern(int index);
    const Pattern* getPattern(int index) const;
    int getPatternCount() const { return static_cast<int>(patterns_.size()); }

    // Chains
    int addChain(const std::string& name = "Untitled");
    Chain* getChain(int index);
    const Chain* getChain(int index) const;
    int getChainCount() const { return static_cast<int>(chains_.size()); }

    // Song
    Song& getSong() { return song_; }
    const Song& getSong() const { return song_; }

    // Per-track groove
    int getTrackGroove(int track) const {
        if (track >= 0 && track < 16) return trackGrooves_[static_cast<size_t>(track)];
        return 0;
    }
    void setTrackGroove(int track, int grooveIndex) {
        if (track >= 0 && track < 16) trackGrooves_[static_cast<size_t>(track)] = grooveIndex;
    }

    // Mixer state
    struct MixerState
    {
        std::array<float, 16> trackVolumes;
        std::array<float, 16> trackPans;
        std::array<bool, 16> trackMutes;
        std::array<bool, 16> trackSolos;
        std::array<float, 5> busLevels;  // reverb, delay, chorus, drive, sidechain
        float masterVolume = 1.0f;

        // Effect parameters
        // Reverb
        float reverbSize = 0.7f;      // Room size (0-1)
        float reverbDamping = 0.3f;   // High frequency damping (0-1)

        // Delay
        float delayTime = 0.65f;      // Time (maps to note divisions, 0.65 = dotted crotchet)
        float delayFeedback = 0.55f;  // Feedback amount (0-1)

        // Chorus
        float chorusRate = 0.4f;      // LFO rate (0-1)
        float chorusDepth = 0.6f;     // Modulation depth (0-1)

        // Drive
        float driveGain = 0.3f;       // Saturation amount (0-1)
        float driveTone = 0.6f;       // Brightness (0-1)

        // Sidechain compressor
        int sidechainSource = -1;     // Instrument index (-1 = none)
        float sidechainAttack = 0.005f;   // Attack time (0.001-0.05)
        float sidechainRelease = 0.2f;    // Release time (0.05-1.0)
        float sidechainRatio = 0.7f;      // Compression amount (0-1, 0=off, 1=full duck)

        // DJ Filter (master bus) - bipolar: -1 = full LP, 0 = bypass, +1 = full HP
        float djFilterPosition = 0.0f;

        // Limiter (master bus)
        float limiterThreshold = 0.95f;   // 0.1-1.0, threshold level
        float limiterRelease = 0.1f;      // 0.01-1.0, release time

        MixerState();
    };

    MixerState& getMixer() { return mixer_; }
    const MixerState& getMixer() const { return mixer_; }

private:
    std::string name_;
    float tempo_ = 120.0f;
    std::string grooveTemplate_ = "None";
    std::array<int, 16> trackGrooves_ = {};  // Groove template index per track (0 = straight)

    std::vector<std::unique_ptr<Instrument>> instruments_;
    std::vector<std::unique_ptr<Pattern>> patterns_;
    std::vector<std::unique_ptr<Chain>> chains_;
    Song song_;
    MixerState mixer_;
};

} // namespace model
