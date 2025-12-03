#pragma once

#include "Instrument.h"
#include <string>
#include <vector>
#include <juce_core/juce_core.h>

namespace model {

struct Preset
{
    std::string name;
    bool isFactory = false;
    PlaitsParams params;
};

class PresetManager
{
public:
    PresetManager();
    ~PresetManager() = default;

    // Initialize - call on startup
    void initialize();

    // Get presets for an engine
    const std::vector<Preset>& getPresetsForEngine(int engine) const;
    int getPresetCount(int engine) const;
    const Preset* getPreset(int engine, int index) const;

    // Find preset index by name
    int findPresetIndex(int engine, const std::string& name) const;

    // User preset management
    bool saveUserPreset(const std::string& name, int engine, const PlaitsParams& params);
    bool deleteUserPreset(const std::string& name, int engine);
    bool isFactoryPreset(int engine, int index) const;

    // Get user presets directory
    juce::File getUserPresetsDirectory() const;

    // Engine names for display
    static const char* getEngineName(int engine);

private:
    void loadFactoryPresets();
    void scanUserPresets();
    void rebuildPresetList(int engine);

    juce::File getPresetFile(int engine, const std::string& name) const;

    static constexpr int NUM_ENGINES = 16;
    std::vector<Preset> factoryPresets_[NUM_ENGINES];
    std::vector<Preset> userPresets_[NUM_ENGINES];
    std::vector<Preset> allPresets_[NUM_ENGINES];  // Combined, factory first
};

} // namespace model
