#pragma once

#include <string>
#include <array>
#include <algorithm>
#include "FilterParams.h"
#include "SamplerParams.h"
#include "SlicerParams.h"
#include "VAParams.h"
#include "DXParams.h"

namespace model {

enum class InstrumentType {
    Plaits,
    Sampler,
    Slicer,
    VASynth,
    DXPreset
};

// LFO parameter structure
struct LfoParams
{
    int rate = 4;        // LfoRateDivision index (0-15)
    int shape = 0;       // LfoShape index (0-4: Sine, Triangle, Saw, Square, S&H)
    int dest = 0;        // ModDestination index (0-8)
    int amount = 0;      // -64 to +63
};

// Envelope modulator parameter structure
struct EnvModParams
{
    float attack = 0.01f;   // 0.0-1.0
    float decay = 0.2f;     // 0.0-1.0
    int dest = 0;           // ModDestination index
    int amount = 0;         // -64 to +63
};

struct PlaitsParams
{
    int engine = 0;           // 0-15 engine type
    float harmonics = 0.5f;   // 0.0-1.0
    float timbre = 0.5f;      // 0.0-1.0
    float morph = 0.5f;       // 0.0-1.0
    float attack = 0.0f;      // 0.0-1.0
    float decay = 0.5f;       // 0.0-1.0
    float lpgColour = 0.5f;   // 0.0-1.0 (deprecated, kept for compatibility)
    int polyphony = 8;        // 1-16
    FilterParams filter;
    LfoParams lfo1;
    LfoParams lfo2;
    EnvModParams env1;
    EnvModParams env2;
};

struct SendLevels
{
    float reverb = 0.0f;
    float delay = 0.0f;
    float chorus = 0.0f;
    float sidechainDuck = 0.0f;
};

// Channel strip parameters (per-instrument insert processing)
struct ChannelStripParams {
    // HPF (High-Pass Filter / Low Cut)
    float hpfFreq = 20.0f;      // 20-500 Hz
    int hpfSlope = 0;           // 0=off, 1=12dB/oct, 2=24dB/oct

    // Low Shelf EQ
    float lowShelfGain = 0.0f;  // -12 to +12 dB
    float lowShelfFreq = 200.0f; // 50-500 Hz

    // Mid Parametric EQ
    float midFreq = 1000.0f;    // 200-8000 Hz
    float midGain = 0.0f;       // -12 to +12 dB
    float midQ = 1.0f;          // 0.5-8.0

    // High Shelf EQ
    float highShelfGain = 0.0f; // -12 to +12 dB
    float highShelfFreq = 8000.0f; // 2000-16000 Hz

    // Drive (moved from master bus send)
    float driveAmount = 0.0f;   // 0-1 (0 = bypass)
    float driveTone = 0.5f;     // 0-1 (dark to bright)

    // Punch (transient shaper)
    float punchAmount = 0.0f;   // 0-1 (0 = bypass)

    // OTT (3-band multiband dynamics)
    float ottLowDepth = 0.0f;   // 0-1 low band depth (0 = bypass)
    float ottMidDepth = 0.0f;   // 0-1 mid band depth (0 = bypass)
    float ottHighDepth = 0.0f;  // 0-1 high band depth (0 = bypass)
    float ottMix = 1.0f;        // 0-1 wet/dry
};

class Instrument
{
public:
    Instrument();
    explicit Instrument(const std::string& name);

    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

    PlaitsParams& getParams() { return params_; }
    const PlaitsParams& getParams() const { return params_; }

    SendLevels& getSends() { return sends_; }
    const SendLevels& getSends() const { return sends_; }

    SamplerParams& getSamplerParams() { return samplerParams_; }
    const SamplerParams& getSamplerParams() const { return samplerParams_; }

    SlicerParams& getSlicerParams() { return slicerParams_; }
    const SlicerParams& getSlicerParams() const { return slicerParams_; }

    VAParams& getVAParams() { return vaParams_; }
    const VAParams& getVAParams() const { return vaParams_; }

    DXParams& getDXParams() { return dxParams_; }
    const DXParams& getDXParams() const { return dxParams_; }

    ChannelStripParams& getChannelStrip() { return channelStrip_; }
    const ChannelStripParams& getChannelStrip() const { return channelStrip_; }

    // Mixer controls (per-instrument)
    float getVolume() const { return volume_; }
    void setVolume(float v) { volume_ = std::max(0.0f, std::min(1.0f, v)); }

    float getPan() const { return pan_; }
    void setPan(float p) { pan_ = std::max(-1.0f, std::min(1.0f, p)); }

    bool isMuted() const { return muted_; }
    void setMuted(bool m) { muted_ = m; }

    bool isSoloed() const { return soloed_; }
    void setSoloed(bool s) { soloed_ = s; }

    InstrumentType getType() const { return type_; }
    void setType(InstrumentType type) { type_ = type; }

private:
    std::string name_;
    PlaitsParams params_;
    SendLevels sends_;
    SamplerParams samplerParams_;
    SlicerParams slicerParams_;
    VAParams vaParams_;
    DXParams dxParams_;
    ChannelStripParams channelStrip_;

    // Mixer state
    float volume_ = 1.0f;    // 0.0-1.0
    float pan_ = 0.0f;       // -1.0 (left) to +1.0 (right)
    bool muted_ = false;
    bool soloed_ = false;

    InstrumentType type_ = InstrumentType::Plaits;
};

} // namespace model
