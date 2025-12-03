# Instrument Presets Design

## Overview

Add a preset system to the Plaits instrument, allowing users to quickly load and save synth parameter configurations.

## Scope

**Included in presets**:
- Engine selection (0-15)
- Harmonics, Timbre, Morph
- Attack, Decay
- Filter (Cutoff, Resonance)
- LFO1 & LFO2 (Rate, Shape, Dest, Amount)
- ENV1 & ENV2 (Attack, Decay, Dest, Amount)

**Excluded from presets** (user controls separately):
- Send levels (Reverb, Delay, Chorus, Drive, Sidechain)
- Mixer settings (Volume, Pan, Mute, Solo)

## Data Model

### Preset Structure

```cpp
struct Preset {
    std::string name;
    int engine;              // 0-15
    PlaitsParams params;     // Reuse existing struct
};
```

### Storage

- **Factory presets**: Embedded in binary as C++ data
- **User presets**: `~/.vitracker/presets/{engine}_{name}.json`

### Organization

- Presets grouped by engine (0-15)
- Each engine has factory list + user list
- Display order: Factory first, then user alphabetically

## PresetManager Class

```cpp
class PresetManager {
public:
    void loadFactoryPresets();
    void scanUserPresets();

    std::vector<Preset> getPresetsForEngine(int engine);
    const Preset* getPreset(int engine, int index);
    int getPresetCount(int engine);

    bool saveUserPreset(const std::string& name, int engine, const PlaitsParams& params);
    bool deleteUserPreset(const std::string& name, int engine);
    bool isFactoryPreset(int engine, int index);

private:
    std::vector<Preset> factoryPresets_[16];  // Per engine
    std::vector<Preset> userPresets_[16];     // Per engine
};
```

## Factory Presets

| Engine | Presets |
|--------|---------|
| 0 - Virtual Analog | Init, Fat Saw, Square Lead, PWM Pad |
| 1 - Waveshaping | Init, Fold Bass, Gritty Lead |
| 2 - FM | Init, DX7 E-Piano, M1 Piano, M1 Organ, Lately Bass, Bell, Brass |
| 3 - Grain | Init, Texture, Shimmer |
| 4 - Additive | Init, Organ, Bright Pad |
| 5 - Wavetable | Init, Digital, Sweep |
| 6 - Chords | Init, Minor 7th, Major Stack |
| 7 - Speech | Init, Vowels, Robot |
| 8 - Swarm | Init, Unison, Detune Pad |
| 9 - Noise | Init, Wind, White |
| 10 - Particle | Init, Dust, Crackle |
| 11 - String | Init, Pluck, Bowed |
| 12 - Modal | Init, Marimba, Glass |
| 13 - Bass Drum | Init, Punchy, 808 |
| 14 - Snare | Init, Tight, Noise |
| 15 - Hi-Hat | Init, Closed, Open |

Each engine has an "Init" preset as a neutral starting point.

## UI Integration

### Instrument Screen

New **Preset row** at top of screen (above Engine):

```
Preset:    [FM] DX7 E-Piano
Engine:    FM
Harmonics: ...
```

### Display Format

- `[EngineName] PresetName` - Preset loaded
- `[EngineName] --` - No preset (manual settings)
- `[EngineName] PresetName*` - Preset modified (asterisk indicates user tweaked parameters)

### Navigation

- `Up/Down` moves cursor to preset row
- `Left/Right` cycles through presets for current engine
- Loading a preset applies all synth parameters immediately
- Changing engine resets to that engine's "Init" preset

## Commands

### Save Preset

```
:save-preset <name>
```

- Saves current instrument's synth params as user preset
- Automatically categorized under current engine
- Overwrites if name exists (with confirmation)

### Delete Preset

```
:delete-preset <name>
```

- Removes a user preset
- Factory presets cannot be deleted

### Error Messages

- Invalid name: "Invalid preset name"
- Delete factory: "Cannot delete factory preset"
- Overwrite: "Preset exists. Overwrite? (y/n)"

## Implementation Notes

1. PresetManager instantiated in MainComponent, passed to InstrumentScreen
2. User preset directory created on first save
3. JSON format for user presets matches existing instrument serialization
4. Factory presets defined in `src/model/FactoryPresets.cpp`
