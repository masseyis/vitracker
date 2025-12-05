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
    // Engine 0: Virtual Analog - Two detuned oscillators with variable waveshapes
    // HARMONICS: detuning, TIMBRE: pulse width (square), MORPH: saw shape (tri to saw)
    factoryPresets_[0] = {
        makePreset("Init", 0, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f),
        // Supersaw: Heavy detuning, bright saw waveform
        makePreset("Supersaw", 0, 0.75f, 0.2f, 0.85f, 0.01f, 0.5f, 0.8f, 0.1f),
        // Classic pulse lead with medium pulse width
        makePreset("Pulse Lead", 0, 0.1f, 0.6f, 0.2f, 0.0f, 0.4f, 0.75f, 0.2f),
        // Thick detuned square bass
        makePreset("Square Bass", 0, 0.4f, 0.5f, 0.0f, 0.0f, 0.3f, 0.4f, 0.3f),
        // PWM pad with LFO on pulse width
        makePresetWithMod("PWM Pad", 0, 0.2f, 0.5f, 0.3f, 0.25f, 0.7f, 0.65f, 0.1f, 4,
            {5, 1, 2, 40}, {0, 0, 0, 0}, {0.01f, 0.2f, 0, 0}, {0.01f, 0.5f, 0, 0}),
        // Sync lead - hardsync with detuning
        makePreset("Sync Lead", 0, 0.5f, 0.9f, 0.5f, 0.0f, 0.35f, 0.85f, 0.15f)
    };

    // Engine 1: Waveshaping - Asymmetric triangle through waveshaping/folding
    // HARMONICS: waveshaper type, TIMBRE: fold amount, MORPH: waveform asymmetry
    factoryPresets_[1] = {
        makePreset("Init", 1, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f),
        // Deep fold bass with lots of harmonics
        makePreset("Fold Bass", 1, 0.25f, 0.75f, 0.3f, 0.0f, 0.35f, 0.5f, 0.25f),
        // Bright overtone-rich lead
        makePreset("Fold Lead", 1, 0.6f, 0.65f, 0.5f, 0.0f, 0.4f, 0.7f, 0.2f),
        // Gentle waveshaping, soft tone
        makePreset("Warm Fold", 1, 0.2f, 0.4f, 0.4f, 0.05f, 0.5f, 0.6f, 0.0f),
        // Aggressive with modulated fold amount
        makePresetWithMod("Growl", 1, 0.5f, 0.8f, 0.6f, 0.0f, 0.3f, 0.55f, 0.35f, 4,
            {0, 0, 0, 0}, {0, 0, 0, 0}, {0.0f, 0.2f, 2, 45}, {0.01f, 0.4f, 0, 0})
    };

    // Engine 2: FM (Phase Modulation) - Two sines modulating each other's phase
    // HARMONICS: frequency ratio, TIMBRE: mod index, MORPH: feedback (0=clean, 1=chaotic)
    factoryPresets_[2] = {
        makePreset("Init", 2, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f),
        // Clean bell - integer ratio, moderate mod index, no feedback
        makePresetWithMod("Bell", 2, 0.6f, 0.55f, 0.0f, 0.0f, 0.7f, 1.0f, 0.0f, 2,
            {0, 0, 0, 0}, {0, 0, 0, 0}, {0.0f, 0.5f, 1, 35}, {0.0f, 0.4f, 0, 0}),
        // Electric piano - attack brightness decay via env
        makePresetWithMod("E-Piano", 2, 0.42f, 0.5f, 0.1f, 0.0f, 0.45f, 0.9f, 0.0f, 4,
            {0, 0, 0, 0}, {0, 0, 0, 0}, {0.0f, 0.25f, 1, 50}, {0.0f, 0.3f, 0, 0}),
        // Metallic clang - high ratio, feedback
        makePreset("Metallic", 2, 0.85f, 0.6f, 0.4f, 0.0f, 0.5f, 1.0f, 0.0f),
        // FM bass - low ratio for sub harmonics
        makePreset("FM Bass", 2, 0.25f, 0.45f, 0.15f, 0.0f, 0.35f, 0.5f, 0.2f),
        // Harsh feedback lead
        makePreset("Feedback Lead", 2, 0.55f, 0.65f, 0.7f, 0.0f, 0.4f, 0.75f, 0.1f),
        // Plucky keys with env mod
        makePresetWithMod("Pluck Keys", 2, 0.5f, 0.6f, 0.05f, 0.0f, 0.3f, 0.85f, 0.0f, 4,
            {0, 0, 0, 0}, {0, 0, 0, 0}, {0.0f, 0.15f, 1, 60}, {0.0f, 0.2f, 0, 0})
    };

    // Engine 3: Formant - Simulation of formants through waveshaping
    // HARMONICS: formant freq ratio, TIMBRE: formant frequency, MORPH: formant width
    factoryPresets_[3] = {
        makePreset("Init", 3, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f),
        // Vowel-like with slow morph
        makePresetWithMod("Vowel Pad", 3, 0.5f, 0.5f, 0.5f, 0.2f, 0.65f, 0.8f, 0.0f, 4,
            {4, 1, 3, 25}, {0, 0, 0, 0}, {0.01f, 0.2f, 0, 0}, {0.01f, 0.5f, 0, 0}),
        // Nasal quality
        makePreset("Nasal", 3, 0.65f, 0.6f, 0.3f, 0.0f, 0.4f, 0.7f, 0.15f),
        // Open throat sound
        makePreset("Open", 3, 0.35f, 0.4f, 0.7f, 0.05f, 0.5f, 0.85f, 0.0f),
        // Filtered texture
        makePreset("Filtered", 3, 0.7f, 0.7f, 0.4f, 0.1f, 0.55f, 0.6f, 0.25f)
    };

    // Engine 4: Additive - Mixture of harmonic sine waves
    // HARMONICS: number of bumps, TIMBRE: prominent harmonic, MORPH: bump shape
    factoryPresets_[4] = {
        makePreset("Init", 4, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f),
        // Hammond-style organ (drawbar harmonics on AUX)
        makePreset("Organ", 4, 0.85f, 0.3f, 0.35f, 0.02f, 0.7f, 1.0f, 0.0f),
        // Bright harmonic pad
        makePresetWithMod("Harmonic Pad", 4, 0.5f, 0.65f, 0.6f, 0.25f, 0.7f, 0.8f, 0.0f, 4,
            {5, 1, 1, 20}, {0, 0, 0, 0}, {0.01f, 0.2f, 0, 0}, {0.01f, 0.5f, 0, 0}),
        // Few harmonics, pure tone
        makePreset("Pure", 4, 0.2f, 0.15f, 0.3f, 0.05f, 0.6f, 1.0f, 0.0f),
        // Rich spectrum
        makePreset("Rich", 4, 0.75f, 0.55f, 0.5f, 0.0f, 0.5f, 0.9f, 0.0f)
    };

    // Engine 5: Wavetable - 4 banks of 8x8 waveforms
    // HARMONICS: bank select, TIMBRE: row (brightness), MORPH: column
    factoryPresets_[5] = {
        makePreset("Init", 5, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f),
        // Digital pluck
        makePreset("Digital Pluck", 5, 0.2f, 0.7f, 0.4f, 0.0f, 0.3f, 0.9f, 0.1f),
        // Sweeping wavetable pad
        makePresetWithMod("WT Sweep", 5, 0.4f, 0.5f, 0.5f, 0.2f, 0.65f, 0.75f, 0.15f, 4,
            {5, 1, 3, 35}, {0, 0, 0, 0}, {0.01f, 0.2f, 0, 0}, {0.15f, 0.5f, 2, 25}),
        // Lofi 5-bit crunch (uses AUX)
        makePreset("Lofi", 5, 0.6f, 0.4f, 0.6f, 0.0f, 0.4f, 0.5f, 0.0f),
        // Evolving texture
        makePresetWithMod("Evolving", 5, 0.3f, 0.5f, 0.5f, 0.15f, 0.6f, 0.7f, 0.1f, 4,
            {6, 0, 2, 30}, {4, 0, 3, 20}, {0.01f, 0.2f, 0, 0}, {0.01f, 0.5f, 0, 0})
    };

    // Engine 6: Chords - Four-note chords via VA or wavetable
    // HARMONICS: chord type, TIMBRE: inversion/transpose, MORPH: waveform
    factoryPresets_[6] = {
        makePreset("Init", 6, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f),
        // Minor chord with warm saw
        makePreset("Minor", 6, 0.25f, 0.4f, 0.7f, 0.1f, 0.6f, 0.75f, 0.0f),
        // Major chord bright
        makePreset("Major", 6, 0.15f, 0.6f, 0.75f, 0.05f, 0.5f, 0.85f, 0.0f),
        // Seventh chord for jazz
        makePreset("Seventh", 6, 0.45f, 0.5f, 0.6f, 0.08f, 0.6f, 0.8f, 0.0f),
        // Suspended chord
        makePreset("Sus4", 6, 0.7f, 0.45f, 0.65f, 0.1f, 0.55f, 0.8f, 0.0f),
        // Spread voicing pad
        makePresetWithMod("Chord Pad", 6, 0.35f, 0.5f, 0.5f, 0.2f, 0.7f, 0.7f, 0.1f, 4,
            {6, 1, 1, 15}, {0, 0, 0, 0}, {0.01f, 0.2f, 0, 0}, {0.01f, 0.5f, 0, 0})
    };

    // Engine 7: Speech - Formant filtering, SAM, and LPC
    // HARMONICS: algorithm (formants->LPC), TIMBRE: species (daleks->chipmunks), MORPH: phoneme
    factoryPresets_[7] = {
        makePreset("Init", 7, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f),
        // Slow vowel morphing
        makePresetWithMod("Vowel Morph", 7, 0.15f, 0.5f, 0.5f, 0.05f, 0.6f, 1.0f, 0.0f, 4,
            {3, 1, 3, 40}, {0, 0, 0, 0}, {0.01f, 0.2f, 0, 0}, {0.01f, 0.5f, 0, 0}),
        // Dalek voice (low species)
        makePreset("Dalek", 7, 0.3f, 0.1f, 0.4f, 0.0f, 0.5f, 0.7f, 0.2f),
        // High chipmunk
        makePreset("Chipmunk", 7, 0.4f, 0.9f, 0.5f, 0.0f, 0.35f, 0.9f, 0.0f),
        // LPC words region
        makePreset("Words", 7, 0.85f, 0.5f, 0.5f, 0.0f, 0.4f, 0.8f, 0.0f),
        // Robotic speech
        makePreset("Robot", 7, 0.5f, 0.3f, 0.5f, 0.0f, 0.45f, 0.6f, 0.3f)
    };

    // Engine 8: Swarm - 8 enveloped saws with randomization (granular)
    // HARMONICS: pitch randomization, TIMBRE: grain density, MORPH: grain duration
    factoryPresets_[8] = {
        makePreset("Init", 8, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f),
        // Superunison - high density, low pitch random
        makePreset("Superunison", 8, 0.2f, 0.8f, 0.4f, 0.0f, 0.45f, 0.8f, 0.1f),
        // Sparse texture
        makePreset("Sparse", 8, 0.6f, 0.3f, 0.6f, 0.15f, 0.65f, 0.75f, 0.0f),
        // Detune pad with movement
        makePresetWithMod("Swarm Pad", 8, 0.45f, 0.6f, 0.55f, 0.25f, 0.7f, 0.7f, 0.1f, 4,
            {6, 0, 0, 20}, {0, 0, 0, 0}, {0.01f, 0.2f, 0, 0}, {0.01f, 0.5f, 0, 0}),
        // Dense cluster
        makePreset("Cluster", 8, 0.7f, 0.9f, 0.3f, 0.0f, 0.4f, 0.65f, 0.2f),
        // Sine variant (AUX) - softer
        makePreset("Soft Swarm", 8, 0.35f, 0.55f, 0.65f, 0.1f, 0.6f, 0.9f, 0.0f)
    };

    // Engine 9: Filtered Noise - Variable-clock noise through resonant filter
    // HARMONICS: filter type (LP->BP->HP), TIMBRE: clock freq, MORPH: resonance
    factoryPresets_[9] = {
        makePreset("Init", 9, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f),
        // Wind with filter sweep
        makePresetWithMod("Wind", 9, 0.2f, 0.4f, 0.3f, 0.2f, 0.7f, 0.6f, 0.0f, 4,
            {4, 1, 2, 30}, {0, 0, 0, 0}, {0.01f, 0.2f, 0, 0}, {0.01f, 0.5f, 0, 0}),
        // White noise burst
        makePreset("White Burst", 9, 0.5f, 0.9f, 0.1f, 0.0f, 0.2f, 1.0f, 0.0f),
        // Resonant sweep
        makePreset("Reso Sweep", 9, 0.35f, 0.6f, 0.85f, 0.0f, 0.4f, 0.8f, 0.0f),
        // Highpass hiss
        makePreset("Hiss", 9, 0.85f, 0.7f, 0.2f, 0.0f, 0.5f, 1.0f, 0.0f),
        // Bandpass whistle
        makePreset("Whistle", 9, 0.5f, 0.5f, 0.9f, 0.0f, 0.6f, 0.9f, 0.0f)
    };

    // Engine 10: Particle/Dust - Dust noise through allpass/bandpass networks
    // HARMONICS: freq randomization, TIMBRE: particle density, MORPH: filter type
    factoryPresets_[10] = {
        makePreset("Init", 10, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f),
        // Sparse dust
        makePreset("Dust", 10, 0.3f, 0.35f, 0.4f, 0.0f, 0.25f, 0.7f, 0.0f),
        // Vinyl crackle
        makePreset("Vinyl", 10, 0.5f, 0.5f, 0.3f, 0.0f, 0.15f, 0.5f, 0.0f),
        // Rain on roof
        makePreset("Rain", 10, 0.4f, 0.7f, 0.5f, 0.0f, 0.3f, 0.8f, 0.0f),
        // Dense resonant particles
        makePreset("Resonant", 10, 0.6f, 0.6f, 0.85f, 0.0f, 0.4f, 0.9f, 0.1f),
        // Fire crackling
        makePreset("Fire", 10, 0.7f, 0.45f, 0.35f, 0.0f, 0.2f, 0.65f, 0.0f)
    };

    // Engine 11: String - Modal resonator (Mini-Rings) for strings
    // HARMONICS: inharmonicity/material, TIMBRE: exciter brightness, MORPH: decay
    factoryPresets_[11] = {
        makePreset("Init", 11, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f),
        // Nylon guitar pluck
        makePreset("Nylon Pluck", 11, 0.25f, 0.45f, 0.35f, 0.0f, 0.4f, 0.85f, 0.0f),
        // Steel string bright
        makePreset("Steel String", 11, 0.3f, 0.7f, 0.4f, 0.0f, 0.45f, 0.9f, 0.0f),
        // Bowed string with sustain
        makePresetWithMod("Bowed", 11, 0.35f, 0.35f, 0.7f, 0.15f, 0.75f, 0.85f, 0.0f, 4,
            {5, 1, 1, 12}, {0, 0, 0, 0}, {0.01f, 0.2f, 0, 0}, {0.1f, 0.6f, 2, 15}),
        // Sitar-like (high inharmonicity)
        makePreset("Sitar", 11, 0.75f, 0.6f, 0.55f, 0.0f, 0.5f, 0.8f, 0.0f),
        // Harp
        makePreset("Harp", 11, 0.2f, 0.5f, 0.5f, 0.0f, 0.55f, 0.95f, 0.0f)
    };

    // Engine 12: Modal - Modal resonator for percussion/bells
    // HARMONICS: inharmonicity/material, TIMBRE: brightness/dust, MORPH: decay
    factoryPresets_[12] = {
        makePreset("Init", 12, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f),
        // Wooden marimba
        makePreset("Marimba", 12, 0.3f, 0.4f, 0.35f, 0.0f, 0.4f, 0.85f, 0.0f),
        // Glass/crystal bell
        makePreset("Glass", 12, 0.55f, 0.65f, 0.55f, 0.0f, 0.6f, 1.0f, 0.0f),
        // Metallic gong
        makePreset("Gong", 12, 0.7f, 0.5f, 0.75f, 0.0f, 0.8f, 0.9f, 0.0f),
        // Kalimba/thumb piano
        makePreset("Kalimba", 12, 0.4f, 0.55f, 0.4f, 0.0f, 0.45f, 0.9f, 0.0f),
        // Tubular bell
        makePreset("Tubular", 12, 0.6f, 0.6f, 0.6f, 0.0f, 0.65f, 1.0f, 0.0f)
    };

    // Engine 13: Bass Drum - Bridged T-network synthesis
    // HARMONICS: attack sharpness/overdrive, TIMBRE: brightness, MORPH: decay
    factoryPresets_[13] = {
        makePreset("Init", 13, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f, 1.0f, 0.0f, 1),
        // Punchy acoustic kick
        makePreset("Acoustic", 13, 0.45f, 0.55f, 0.35f, 0.0f, 0.35f, 0.9f, 0.0f, 1),
        // 808 sub kick
        makePreset("808", 13, 0.25f, 0.3f, 0.6f, 0.0f, 0.65f, 0.5f, 0.15f, 1),
        // 909 punchy
        makePreset("909", 13, 0.55f, 0.6f, 0.3f, 0.0f, 0.3f, 0.75f, 0.0f, 1),
        // Deep sub
        makePreset("Sub", 13, 0.2f, 0.25f, 0.7f, 0.0f, 0.75f, 0.4f, 0.2f, 1),
        // Hard distorted
        makePreset("Distorted", 13, 0.8f, 0.7f, 0.25f, 0.0f, 0.25f, 0.65f, 0.0f, 1)
    };

    // Engine 14: Snare - Multiple T-networks + bandpass noise
    // HARMONICS: body/noise balance, TIMBRE: tone, MORPH: decay
    factoryPresets_[14] = {
        makePreset("Init", 14, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f, 1.0f, 0.0f, 1),
        // Tight studio snare
        makePreset("Tight", 14, 0.45f, 0.55f, 0.3f, 0.0f, 0.25f, 0.9f, 0.0f, 1),
        // Fat backbeat
        makePreset("Fat", 14, 0.35f, 0.45f, 0.5f, 0.0f, 0.4f, 0.7f, 0.1f, 1),
        // 808 snare
        makePreset("808 Snare", 14, 0.55f, 0.5f, 0.35f, 0.0f, 0.3f, 0.8f, 0.0f, 1),
        // Rim shot
        makePreset("Rim", 14, 0.25f, 0.7f, 0.2f, 0.0f, 0.15f, 0.95f, 0.0f, 1),
        // Clap-like
        makePreset("Clap", 14, 0.7f, 0.6f, 0.4f, 0.0f, 0.35f, 0.85f, 0.0f, 1)
    };

    // Engine 15: Hi-Hat/Metallic - Square osc + clocked noise through filters
    // HARMONICS: metallic/noise balance, TIMBRE: highpass cutoff, MORPH: decay
    factoryPresets_[15] = {
        makePreset("Init", 15, 0.5f, 0.5f, 0.5f, 0.0f, 0.5f, 1.0f, 0.0f, 1),
        // Closed hi-hat
        makePreset("Closed HH", 15, 0.5f, 0.55f, 0.2f, 0.0f, 0.1f, 1.0f, 0.0f, 1),
        // Open hi-hat
        makePreset("Open HH", 15, 0.45f, 0.5f, 0.65f, 0.0f, 0.45f, 1.0f, 0.0f, 1),
        // Ride cymbal
        makePreset("Ride", 15, 0.35f, 0.4f, 0.7f, 0.0f, 0.6f, 0.95f, 0.0f, 1),
        // Crash cymbal
        makePreset("Crash", 15, 0.4f, 0.45f, 0.85f, 0.0f, 0.8f, 0.9f, 0.0f, 1),
        // 808 hat
        makePreset("808 Hat", 15, 0.6f, 0.6f, 0.25f, 0.0f, 0.15f, 1.0f, 0.0f, 1)
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
