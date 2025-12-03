#include "PresetManager.h"
#include <juce_core/juce_core.h>

namespace model {

namespace {

// Engine names
const char* engineNames[] = {
    "Virtual Analog",
    "Waveshaping",
    "FM",
    "Grain",
    "Additive",
    "Wavetable",
    "Chords",
    "Speech",
    "Swarm",
    "Noise",
    "Particle",
    "String",
    "Modal",
    "Bass Drum",
    "Snare",
    "Hi-Hat"
};

// Helper to create a preset with default modulation settings
Preset makePreset(const std::string& name, int engine,
                  float harmonics, float timbre, float morph,
                  float attack, float decay,
                  float cutoff = 1.0f, float resonance = 0.0f,
                  int polyphony = 8)
{
    Preset p;
    p.name = name;
    p.isFactory = true;
    p.params.engine = engine;
    p.params.harmonics = harmonics;
    p.params.timbre = timbre;
    p.params.morph = morph;
    p.params.attack = attack;
    p.params.decay = decay;
    p.params.filter.cutoff = cutoff;
    p.params.filter.resonance = resonance;
    p.params.polyphony = polyphony;
    // Default LFO/ENV settings
    p.params.lfo1 = {4, 0, 0, 0};
    p.params.lfo2 = {4, 0, 0, 0};
    p.params.env1 = {0.01f, 0.2f, 0, 0};
    p.params.env2 = {0.01f, 0.5f, 0, 0};
    return p;
}

// Extended helper for presets with modulation
Preset makePresetWithMod(const std::string& name, int engine,
                         float harmonics, float timbre, float morph,
                         float attack, float decay,
                         float cutoff, float resonance,
                         int polyphony,
                         LfoParams lfo1, LfoParams lfo2,
                         EnvModParams env1, EnvModParams env2)
{
    Preset p = makePreset(name, engine, harmonics, timbre, morph, attack, decay, cutoff, resonance, polyphony);
    p.params.lfo1 = lfo1;
    p.params.lfo2 = lfo2;
    p.params.env1 = env1;
    p.params.env2 = env2;
    return p;
}

} // anonymous namespace

PresetManager::PresetManager()
{
}

void PresetManager::initialize()
{
    loadFactoryPresets();
    scanUserPresets();
    for (int i = 0; i < NUM_ENGINES; ++i)
        rebuildPresetList(i);
}

const char* PresetManager::getEngineName(int engine)
{
    if (engine >= 0 && engine < 16)
        return engineNames[engine];
    return "Unknown";
}

void PresetManager::loadFactoryPresets()
{
    // Engine 0: Virtual Analog
    factoryPresets_[0] = {
        makePreset("Init", 0, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f),
        makePreset("Fat Saw", 0, 0.7f, 0.8f, 0.3f, 0.01f, 0.6f, 0.7f, 0.1f),
        makePreset("Square Lead", 0, 0.0f, 0.5f, 0.0f, 0.0f, 0.4f, 0.8f, 0.2f),
        makePresetWithMod("PWM Pad", 0, 0.0f, 0.5f, 0.5f, 0.3f, 0.7f, 0.6f, 0.1f, 4,
            {6, 0, 2, 32}, {0, 0, 0, 0}, {0.01f, 0.2f, 0, 0}, {0.01f, 0.5f, 0, 0})
    };

    // Engine 1: Waveshaping
    factoryPresets_[1] = {
        makePreset("Init", 1, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f),
        makePreset("Fold Bass", 1, 0.3f, 0.7f, 0.4f, 0.0f, 0.3f, 0.5f, 0.2f),
        makePreset("Gritty Lead", 1, 0.6f, 0.8f, 0.6f, 0.0f, 0.4f, 0.7f, 0.3f)
    };

    // Engine 2: FM - Classic sounds
    factoryPresets_[2] = {
        makePreset("Init", 2, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f),
        // DX7 E-Piano: Bright attack, quick decay, bell-like
        makePresetWithMod("DX7 E-Piano", 2, 0.4f, 0.6f, 0.3f, 0.0f, 0.35f, 0.85f, 0.0f, 4,
            {0, 0, 0, 0}, {0, 0, 0, 0}, {0.0f, 0.15f, 1, 40}, {0.0f, 0.25f, 3, 30}),
        // M1 Piano: Full, slightly detuned
        makePreset("M1 Piano", 2, 0.5f, 0.45f, 0.4f, 0.0f, 0.5f, 0.9f, 0.0f),
        // M1 Organ: Sustained, rich harmonics
        makePreset("M1 Organ", 2, 0.7f, 0.3f, 0.5f, 0.02f, 0.8f, 1.0f, 0.0f),
        // Lately Bass: Smooth, round FM bass
        makePreset("Lately Bass", 2, 0.35f, 0.5f, 0.25f, 0.0f, 0.4f, 0.45f, 0.15f),
        // Bell: High harmonics, long decay
        makePresetWithMod("Bell", 2, 0.8f, 0.7f, 0.2f, 0.0f, 0.7f, 1.0f, 0.0f, 2,
            {0, 0, 0, 0}, {0, 0, 0, 0}, {0.0f, 0.6f, 0, 20}, {0.0f, 0.4f, 0, 0}),
        // Brass: Attack modulation
        makePresetWithMod("Brass", 2, 0.6f, 0.5f, 0.5f, 0.1f, 0.5f, 0.75f, 0.1f, 4,
            {0, 0, 0, 0}, {0, 0, 0, 0}, {0.05f, 0.3f, 1, 50}, {0.0f, 0.2f, 3, 20})
    };

    // Engine 3: Grain
    factoryPresets_[3] = {
        makePreset("Init", 3, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f),
        makePresetWithMod("Texture", 3, 0.4f, 0.6f, 0.7f, 0.1f, 0.6f, 0.8f, 0.0f, 4,
            {5, 2, 2, 20}, {0, 0, 0, 0}, {0.01f, 0.2f, 0, 0}, {0.01f, 0.5f, 0, 0}),
        makePresetWithMod("Shimmer", 3, 0.7f, 0.4f, 0.8f, 0.2f, 0.7f, 0.9f, 0.0f, 4,
            {7, 0, 0, 15}, {6, 0, 2, 10}, {0.01f, 0.2f, 0, 0}, {0.01f, 0.5f, 0, 0})
    };

    // Engine 4: Additive
    factoryPresets_[4] = {
        makePreset("Init", 4, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f),
        makePreset("Organ", 4, 0.8f, 0.3f, 0.4f, 0.02f, 0.7f, 1.0f, 0.0f),
        makePresetWithMod("Bright Pad", 4, 0.6f, 0.7f, 0.5f, 0.25f, 0.65f, 0.85f, 0.0f, 4,
            {6, 0, 1, 20}, {0, 0, 0, 0}, {0.01f, 0.2f, 0, 0}, {0.01f, 0.5f, 0, 0})
    };

    // Engine 5: Wavetable
    factoryPresets_[5] = {
        makePreset("Init", 5, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f),
        makePreset("Digital", 5, 0.3f, 0.6f, 0.4f, 0.0f, 0.4f, 0.9f, 0.1f),
        makePresetWithMod("Sweep", 5, 0.5f, 0.5f, 0.5f, 0.15f, 0.6f, 0.7f, 0.2f, 4,
            {5, 0, 2, 40}, {0, 0, 0, 0}, {0.01f, 0.2f, 0, 0}, {0.2f, 0.5f, 3, 30})
    };

    // Engine 6: Chords
    factoryPresets_[6] = {
        makePreset("Init", 6, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f),
        makePreset("Minor 7th", 6, 0.3f, 0.5f, 0.4f, 0.1f, 0.6f, 0.8f, 0.0f),
        makePreset("Major Stack", 6, 0.7f, 0.4f, 0.6f, 0.05f, 0.5f, 0.9f, 0.0f)
    };

    // Engine 7: Speech
    factoryPresets_[7] = {
        makePreset("Init", 7, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f),
        makePresetWithMod("Vowels", 7, 0.5f, 0.5f, 0.5f, 0.02f, 0.5f, 1.0f, 0.0f, 4,
            {4, 0, 2, 30}, {0, 0, 0, 0}, {0.01f, 0.2f, 0, 0}, {0.01f, 0.5f, 0, 0}),
        makePreset("Robot", 7, 0.2f, 0.8f, 0.3f, 0.0f, 0.4f, 0.6f, 0.3f)
    };

    // Engine 8: Swarm
    factoryPresets_[8] = {
        makePreset("Init", 8, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f),
        makePreset("Unison", 8, 0.7f, 0.6f, 0.3f, 0.0f, 0.5f, 0.8f, 0.0f),
        makePresetWithMod("Detune Pad", 8, 0.5f, 0.7f, 0.6f, 0.3f, 0.7f, 0.7f, 0.1f, 4,
            {7, 0, 1, 15}, {0, 0, 0, 0}, {0.01f, 0.2f, 0, 0}, {0.01f, 0.5f, 0, 0})
    };

    // Engine 9: Noise
    factoryPresets_[9] = {
        makePreset("Init", 9, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f),
        makePreset("Wind", 9, 0.3f, 0.4f, 0.6f, 0.2f, 0.7f, 0.5f, 0.3f),
        makePreset("White", 9, 0.5f, 0.8f, 0.5f, 0.0f, 0.3f, 1.0f, 0.0f)
    };

    // Engine 10: Particle
    factoryPresets_[10] = {
        makePreset("Init", 10, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f),
        makePreset("Dust", 10, 0.3f, 0.6f, 0.4f, 0.0f, 0.2f, 0.7f, 0.0f),
        makePreset("Crackle", 10, 0.6f, 0.7f, 0.5f, 0.0f, 0.15f, 0.9f, 0.1f)
    };

    // Engine 11: String
    factoryPresets_[11] = {
        makePreset("Init", 11, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f),
        makePreset("Pluck", 11, 0.4f, 0.6f, 0.3f, 0.0f, 0.35f, 0.8f, 0.0f),
        makePresetWithMod("Bowed", 11, 0.5f, 0.4f, 0.6f, 0.15f, 0.7f, 0.9f, 0.0f, 4,
            {6, 0, 1, 10}, {0, 0, 0, 0}, {0.01f, 0.2f, 0, 0}, {0.1f, 0.6f, 0, 15})
    };

    // Engine 12: Modal
    factoryPresets_[12] = {
        makePreset("Init", 12, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f),
        makePreset("Marimba", 12, 0.4f, 0.5f, 0.3f, 0.0f, 0.4f, 0.85f, 0.0f),
        makePreset("Glass", 12, 0.7f, 0.6f, 0.5f, 0.0f, 0.55f, 1.0f, 0.0f)
    };

    // Engine 13: Bass Drum
    factoryPresets_[13] = {
        makePreset("Init", 13, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f, 1.0f, 0.0f, 1),
        makePreset("Punchy", 13, 0.4f, 0.6f, 0.3f, 0.0f, 0.35f, 0.8f, 0.0f, 1),
        makePreset("808", 13, 0.3f, 0.4f, 0.5f, 0.0f, 0.6f, 0.6f, 0.2f, 1)
    };

    // Engine 14: Snare
    factoryPresets_[14] = {
        makePreset("Init", 14, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f, 1.0f, 0.0f, 1),
        makePreset("Tight", 14, 0.5f, 0.6f, 0.4f, 0.0f, 0.25f, 0.9f, 0.0f, 1),
        makePreset("Noise", 14, 0.6f, 0.7f, 0.6f, 0.0f, 0.35f, 1.0f, 0.0f, 1)
    };

    // Engine 15: Hi-Hat
    factoryPresets_[15] = {
        makePreset("Init", 15, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f, 1.0f, 0.0f, 1),
        makePreset("Closed", 15, 0.5f, 0.5f, 0.3f, 0.0f, 0.15f, 1.0f, 0.0f, 1),
        makePreset("Open", 15, 0.5f, 0.5f, 0.7f, 0.0f, 0.5f, 1.0f, 0.0f, 1)
    };
}

juce::File PresetManager::getUserPresetsDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userHomeDirectory)
        .getChildFile(".vitracker")
        .getChildFile("presets");
}

juce::File PresetManager::getPresetFile(int engine, const std::string& name) const
{
    auto dir = getUserPresetsDirectory();
    juce::String filename = juce::String(engine) + "_" + juce::String(name) + ".json";
    return dir.getChildFile(filename);
}

void PresetManager::scanUserPresets()
{
    auto presetsDir = getUserPresetsDirectory();
    if (!presetsDir.exists())
        return;

    for (int engine = 0; engine < NUM_ENGINES; ++engine)
        userPresets_[engine].clear();

    for (const auto& file : presetsDir.findChildFiles(juce::File::findFiles, false, "*.json"))
    {
        auto filename = file.getFileNameWithoutExtension();
        int underscorePos = filename.indexOf("_");
        if (underscorePos <= 0)
            continue;

        int engine = filename.substring(0, underscorePos).getIntValue();
        if (engine < 0 || engine >= NUM_ENGINES)
            continue;

        auto name = filename.substring(underscorePos + 1).toStdString();

        // Parse JSON
        auto json = juce::JSON::parse(file);
        if (!json.isObject())
            continue;

        auto* obj = json.getDynamicObject();
        if (!obj)
            continue;

        Preset preset;
        preset.name = name;
        preset.isFactory = false;
        preset.params.engine = engine;
        preset.params.harmonics = static_cast<float>(obj->getProperty("harmonics"));
        preset.params.timbre = static_cast<float>(obj->getProperty("timbre"));
        preset.params.morph = static_cast<float>(obj->getProperty("morph"));
        preset.params.attack = static_cast<float>(obj->getProperty("attack"));
        preset.params.decay = static_cast<float>(obj->getProperty("decay"));
        preset.params.polyphony = static_cast<int>(obj->getProperty("polyphony"));

        if (obj->hasProperty("filter"))
        {
            auto* filter = obj->getProperty("filter").getDynamicObject();
            if (filter)
            {
                preset.params.filter.cutoff = static_cast<float>(filter->getProperty("cutoff"));
                preset.params.filter.resonance = static_cast<float>(filter->getProperty("resonance"));
            }
        }

        if (obj->hasProperty("lfo1"))
        {
            auto* lfo = obj->getProperty("lfo1").getDynamicObject();
            if (lfo)
            {
                preset.params.lfo1.rate = static_cast<int>(lfo->getProperty("rate"));
                preset.params.lfo1.shape = static_cast<int>(lfo->getProperty("shape"));
                preset.params.lfo1.dest = static_cast<int>(lfo->getProperty("dest"));
                preset.params.lfo1.amount = static_cast<int>(lfo->getProperty("amount"));
            }
        }

        if (obj->hasProperty("lfo2"))
        {
            auto* lfo = obj->getProperty("lfo2").getDynamicObject();
            if (lfo)
            {
                preset.params.lfo2.rate = static_cast<int>(lfo->getProperty("rate"));
                preset.params.lfo2.shape = static_cast<int>(lfo->getProperty("shape"));
                preset.params.lfo2.dest = static_cast<int>(lfo->getProperty("dest"));
                preset.params.lfo2.amount = static_cast<int>(lfo->getProperty("amount"));
            }
        }

        if (obj->hasProperty("env1"))
        {
            auto* env = obj->getProperty("env1").getDynamicObject();
            if (env)
            {
                preset.params.env1.attack = static_cast<float>(env->getProperty("attack"));
                preset.params.env1.decay = static_cast<float>(env->getProperty("decay"));
                preset.params.env1.dest = static_cast<int>(env->getProperty("dest"));
                preset.params.env1.amount = static_cast<int>(env->getProperty("amount"));
            }
        }

        if (obj->hasProperty("env2"))
        {
            auto* env = obj->getProperty("env2").getDynamicObject();
            if (env)
            {
                preset.params.env2.attack = static_cast<float>(env->getProperty("attack"));
                preset.params.env2.decay = static_cast<float>(env->getProperty("decay"));
                preset.params.env2.dest = static_cast<int>(env->getProperty("dest"));
                preset.params.env2.amount = static_cast<int>(env->getProperty("amount"));
            }
        }

        userPresets_[engine].push_back(preset);
    }

    // Sort user presets by name
    for (int engine = 0; engine < NUM_ENGINES; ++engine)
    {
        std::sort(userPresets_[engine].begin(), userPresets_[engine].end(),
            [](const Preset& a, const Preset& b) { return a.name < b.name; });
    }
}

void PresetManager::rebuildPresetList(int engine)
{
    if (engine < 0 || engine >= NUM_ENGINES)
        return;

    allPresets_[engine].clear();
    allPresets_[engine].insert(allPresets_[engine].end(),
        factoryPresets_[engine].begin(), factoryPresets_[engine].end());
    allPresets_[engine].insert(allPresets_[engine].end(),
        userPresets_[engine].begin(), userPresets_[engine].end());
}

const std::vector<Preset>& PresetManager::getPresetsForEngine(int engine) const
{
    static std::vector<Preset> empty;
    if (engine < 0 || engine >= NUM_ENGINES)
        return empty;
    return allPresets_[engine];
}

int PresetManager::getPresetCount(int engine) const
{
    if (engine < 0 || engine >= NUM_ENGINES)
        return 0;
    return static_cast<int>(allPresets_[engine].size());
}

const Preset* PresetManager::getPreset(int engine, int index) const
{
    if (engine < 0 || engine >= NUM_ENGINES)
        return nullptr;
    if (index < 0 || index >= static_cast<int>(allPresets_[engine].size()))
        return nullptr;
    return &allPresets_[engine][static_cast<size_t>(index)];
}

int PresetManager::findPresetIndex(int engine, const std::string& name) const
{
    if (engine < 0 || engine >= NUM_ENGINES)
        return -1;

    const auto& presets = allPresets_[engine];
    for (size_t i = 0; i < presets.size(); ++i)
    {
        if (presets[i].name == name)
            return static_cast<int>(i);
    }
    return -1;
}

bool PresetManager::isFactoryPreset(int engine, int index) const
{
    if (engine < 0 || engine >= NUM_ENGINES)
        return false;
    if (index < 0 || index >= static_cast<int>(allPresets_[engine].size()))
        return false;
    return allPresets_[engine][static_cast<size_t>(index)].isFactory;
}

bool PresetManager::saveUserPreset(const std::string& name, int engine, const PlaitsParams& params)
{
    if (name.empty() || engine < 0 || engine >= NUM_ENGINES)
        return false;

    // Validate name (no special characters)
    for (char c : name)
    {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != ' ' && c != '-' && c != '_')
            return false;
    }

    auto presetsDir = getUserPresetsDirectory();
    if (!presetsDir.exists())
        presetsDir.createDirectory();

    auto file = getPresetFile(engine, name);

    // Create JSON
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("harmonics", params.harmonics);
    obj->setProperty("timbre", params.timbre);
    obj->setProperty("morph", params.morph);
    obj->setProperty("attack", params.attack);
    obj->setProperty("decay", params.decay);
    obj->setProperty("polyphony", params.polyphony);

    juce::DynamicObject::Ptr filter = new juce::DynamicObject();
    filter->setProperty("cutoff", params.filter.cutoff);
    filter->setProperty("resonance", params.filter.resonance);
    obj->setProperty("filter", juce::var(filter.get()));

    juce::DynamicObject::Ptr lfo1 = new juce::DynamicObject();
    lfo1->setProperty("rate", params.lfo1.rate);
    lfo1->setProperty("shape", params.lfo1.shape);
    lfo1->setProperty("dest", params.lfo1.dest);
    lfo1->setProperty("amount", params.lfo1.amount);
    obj->setProperty("lfo1", juce::var(lfo1.get()));

    juce::DynamicObject::Ptr lfo2 = new juce::DynamicObject();
    lfo2->setProperty("rate", params.lfo2.rate);
    lfo2->setProperty("shape", params.lfo2.shape);
    lfo2->setProperty("dest", params.lfo2.dest);
    lfo2->setProperty("amount", params.lfo2.amount);
    obj->setProperty("lfo2", juce::var(lfo2.get()));

    juce::DynamicObject::Ptr env1 = new juce::DynamicObject();
    env1->setProperty("attack", params.env1.attack);
    env1->setProperty("decay", params.env1.decay);
    env1->setProperty("dest", params.env1.dest);
    env1->setProperty("amount", params.env1.amount);
    obj->setProperty("env1", juce::var(env1.get()));

    juce::DynamicObject::Ptr env2 = new juce::DynamicObject();
    env2->setProperty("attack", params.env2.attack);
    env2->setProperty("decay", params.env2.decay);
    env2->setProperty("dest", params.env2.dest);
    env2->setProperty("amount", params.env2.amount);
    obj->setProperty("env2", juce::var(env2.get()));

    auto jsonString = juce::JSON::toString(juce::var(obj.get()));
    if (!file.replaceWithText(jsonString))
        return false;

    // Reload user presets
    scanUserPresets();
    rebuildPresetList(engine);

    return true;
}

bool PresetManager::deleteUserPreset(const std::string& name, int engine)
{
    if (name.empty() || engine < 0 || engine >= NUM_ENGINES)
        return false;

    auto file = getPresetFile(engine, name);
    if (!file.exists())
        return false;

    if (!file.deleteFile())
        return false;

    // Reload user presets
    scanUserPresets();
    rebuildPresetList(engine);

    return true;
}

} // namespace model
