# Channel Strip Design

Date: 2025-12-07
Status: Approved
Branch: feature/channel-strip

## Overview

Add per-instrument channel strip processing with EQ, dynamics, and tone shaping. This introduces insert effects applied to each instrument before volume/pan and master bus processing.

## Signal Flow

### Current Flow
```
Instrument -> Volume/Pan -> Master Bus -> [Reverb/Delay/Chorus/Drive sends] -> Limiter -> Output
```

### New Flow
```
Instrument
    -> ChannelStrip [HPF -> LowShelf -> MidEQ -> HighShelf -> Drive -> Punch -> OTT]
    -> Volume/Pan
    -> Master Bus
    -> [Reverb/Delay/Chorus sends] -> Sidechain -> DJ Filter -> Limiter
    -> Output
```

### Key Changes
- **ChannelStrip** is a new per-instrument processor applied after instrument output, before volume/pan
- **Drive moved from master bus to per-instrument insert**
- **Sends** (Reverb/Delay/Chorus/Sidechain) remain on master bus
- **ChannelStrip instances match instrument count** (not fixed)

### Processing Order Rationale
1. HPF first - removes rumble before EQ boosts anything
2. EQ section (Low/Mid/High) - tonal shaping
3. Drive - saturation benefits from clean EQ'd input
4. Punch (transient shaper) - enhances dynamics after saturation
5. OTT last in dynamics chain - controls final dynamic range

## Data Model

### ChannelStripParams (new struct in model/Instrument.h)

```cpp
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
    float ottDepth = 0.0f;      // 0-1 (0 = bypass)
    float ottMix = 1.0f;        // 0-1 wet/dry
    float ottSmooth = 0.5f;     // 0-1 (faster to slower attack/release)
};
```

### Storage
- Each `Instrument` gets a `ChannelStripParams channelStrip_` member
- Serialized in project save/load alongside existing instrument params

## DSP Implementation

### ChannelStrip Class (audio/ChannelStrip.h/.cpp)

```cpp
class ChannelStrip {
public:
    void prepare(double sampleRate, int samplesPerBlock);
    void process(float* left, float* right, int numSamples);
    void updateParams(const ChannelStripParams& params);

private:
    // HPF - Butterworth, 2nd or 4th order
    BiquadFilter hpf_[2];  // Stereo, cascaded for 24dB

    // EQ - Direct Form II biquads
    BiquadFilter lowShelf_;
    BiquadFilter midPeak_;
    BiquadFilter highShelf_;

    // Drive - existing algorithm moved from Effects.cpp
    DriveProcessor drive_;

    // Punch - envelope follower + transient detection
    TransientShaper punch_;

    // OTT - 3-band crossover + per-band dynamics
    MultibandOTT ott_;

    double sampleRate_ = 44100.0;
};
```

### Effect Implementations

**BiquadFilter**: Standard RBJ cookbook biquads for HPF, shelf, and parametric EQ.

**TransientShaper (Punch)**: Dual envelope followers with different attack times. Fast follower tracks transients, slow follower tracks sustain. Difference between them = transient content. Amount parameter controls gain applied to transient portion.

**MultibandOTT**:
- Linkwitz-Riley crossovers at ~100Hz and ~3kHz
- Each band has upward compressor (boosts quiet) + downward compressor (squashes loud)
- Depth controls compression intensity across all bands
- Mix is wet/dry blend
- Smoothness adjusts attack/release times

### CPU Consideration
OTT is heaviest (6 envelope followers per band x 3 bands = 18 total). With typical instrument counts (8-12), should be well under budget on modern CPUs.

## UI Design

### New ChannelScreen (ui/ChannelScreen.h/.cpp)

Accessible via function key (F4) between Instrument (F3) and Mixer (F5).

```
+--------------------------------------------------+
|  CHANNEL                           01: Kick      |
|  ------------------------------------------------|
|  [h] HPF        ************....  80 Hz   24dB   |
|  ------------------------------------------------|
|  [l] LOW SHELF  ********.......  +3 dB   200 Hz  |
|  [m] MID EQ     *******.........  +2 dB   1.2k Q2|
|  [e] HIGH SHELF ................   0 dB   8k Hz  |
|  ------------------------------------------------|
|  [d] DRIVE      *********.....   35%     Tone:60%|
|  [p] PUNCH      ****............   20%           |
|  [o] OTT        ********......   Depth 40% Mix80%|
|  ------------------------------------------------|
|  [v] VOLUME     **********....   -3 dB           |
|  [n] PAN        .....*.......    C               |
|  ------------------------------------------------|
|  [r] REVERB     ***............   15%            |
|  [y] DELAY      **..............  10%            |
|  [c] CHORUS     ................   0%            |
|  [s] SIDECHAIN  *****...........  25%            |
+--------------------------------------------------+
```

### Navigation (using centralized KeyHandler)

- `j/k` or Up/Down: Move between rows
- `h/l` or Left/Right: Move between fields on multi-param rows
- `Alt+h/l` or `Alt+Left/Right`: Adjust parameter value (Edit2Inc/Edit2Dec)
- `Alt+j/k` or `Alt+Up/Down`: Adjust parameter value vertically (Edit1Inc/Edit1Dec)
- `Alt+Shift+*`: Coarse adjustment
- `[/]`: Switch instruments (PatternPrev/PatternNext actions)
- Jump keys: Direct row access (press letter to jump)

### Input Context
Uses `InputContext::RowParams` (same as InstrumentScreen for Plaits/Sampler).

## AudioEngine Integration

### Changes to AudioEngine.cpp

1. **Add ChannelStrip storage:**
```cpp
std::vector<std::unique_ptr<ChannelStrip>> channelStrips_;
```

2. **Lifecycle management:**
- `prepare()`: Create/resize channelStrips_ to match instrument count
- When instruments added/removed: Sync channelStrip count

3. **Processing insertion point** (in render loop):
```cpp
// After: instrument renders to tempBuffer
// NEW: apply channel strip
channelStrips_[i]->updateParams(instrument.getChannelStrip());
channelStrips_[i]->process(tempBufferL, tempBufferR, numSamples);
// Before: apply volume/pan and mix to master
```

4. **Remove Drive from master bus:**
- Delete `driveGain_`, `driveTone_` from MixerState
- Remove drive accumulation in send averaging
- Remove drive processing from master bus effect chain

5. **Remove SendLevels.drive** - no longer needed since Drive is per-instrument insert.

## Files to Create

| File | Purpose |
|------|---------|
| `src/audio/ChannelStrip.h/.cpp` | DSP processor class |
| `src/audio/BiquadFilter.h/.cpp` | Reusable biquad filter |
| `src/audio/TransientShaper.h/.cpp` | Punch effect |
| `src/audio/MultibandOTT.h/.cpp` | 3-band OTT |
| `src/ui/ChannelScreen.h/.cpp` | New UI screen |

## Files to Modify

| File | Changes |
|------|---------|
| `src/model/Instrument.h` | Add ChannelStripParams struct and member |
| `src/model/Project.h` | Remove driveGain, driveTone from MixerState |
| `src/model/Project.cpp` | Serialize ChannelStripParams, remove drive serialization |
| `src/audio/AudioEngine.h/.cpp` | Add channelStrips_ vector, integrate processing, remove master drive |
| `src/audio/Effects.h/.cpp` | Remove Drive from master bus (keep class for reuse) |
| `src/ui/MixerScreen.cpp` | Remove Drive panel from FX section |
| `src/ui/InstrumentScreen.cpp` | Remove drive send row |
| `src/App.cpp` | Register ChannelScreen, add F4 keybinding |

## Migration

Existing projects with drive send values will lose those values on load. This is acceptable since it's a major feature change and drive is being fundamentally restructured.

## Decisions Made

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Processing scope | Per-instrument (post-mix voices) | Lower CPU, standard DAW behavior |
| Drive location | Move to channel strip insert | Per-instrument saturation more useful |
| UI location | New dedicated screen | Too many params for existing screens |
| OTT implementation | Full 3-band | Character comes from band independence |
| Punch implementation | Transient shaper | Most intuitive "one-knob punch" |
