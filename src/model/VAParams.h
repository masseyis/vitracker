#pragma once

#include "FilterParams.h"
#include "ModulationParams.h"

namespace model {

// VA oscillator waveform types
enum class VAWaveform {
    Saw = 0,
    Square,
    Triangle,
    Pulse,      // Square with PWM
    Sine,
    NumWaveforms
};

inline const char* getVAWaveformName(VAWaveform wf) {
    switch (wf) {
        case VAWaveform::Saw: return "SAW";
        case VAWaveform::Square: return "SQR";
        case VAWaveform::Triangle: return "TRI";
        case VAWaveform::Pulse: return "PLS";
        case VAWaveform::Sine: return "SIN";
        default: return "???";
    }
}

// Per-oscillator parameters
struct VAOscParams {
    int waveform = 0;         // VAWaveform index
    int octave = 0;           // -2 to +2 octaves from base note
    int semitones = 0;        // -12 to +12 semitone detune
    float cents = 0.0f;       // -100 to +100 fine tune in cents
    float level = 1.0f;       // 0.0-1.0 oscillator mix level
    float pulseWidth = 0.5f;  // 0.0-1.0 for pulse waveform
    bool enabled = true;
};

// ADSR envelope parameters
struct VAEnvelopeParams {
    float attack = 0.01f;     // 0.0-1.0 mapped to time
    float decay = 0.2f;       // 0.0-1.0 mapped to time
    float sustain = 0.7f;     // 0.0-1.0 level
    float release = 0.3f;     // 0.0-1.0 mapped to time
};

// Moog-style ladder filter parameters
struct VAFilterParams {
    float cutoff = 0.8f;      // 0.0-1.0 mapped to frequency
    float resonance = 0.0f;   // 0.0-1.0 (self-oscillation around 0.9+)
    float envAmount = 0.5f;   // 0.0-1.0 filter envelope modulation depth
    float keyTrack = 0.5f;    // 0.0-1.0 keyboard tracking amount
    int filterType = 0;       // 0=24dB LP, 1=12dB LP, 2=24dB HP (optional)
};

// LFO for modulation
struct VALfoParams {
    int rateIndex = 4;        // Tempo-synced rate division
    int shapeIndex = 0;       // LFO shape (sin, tri, saw, sqr, s&h)
    float toPitch = 0.0f;     // 0.0-1.0 pitch modulation amount
    float toFilter = 0.0f;    // 0.0-1.0 filter modulation amount
    float toPW = 0.0f;        // 0.0-1.0 pulse width modulation
};

// Complete VA synth parameters (Mini Moog-style)
struct VAParams {
    // Three oscillators
    VAOscParams osc1;
    VAOscParams osc2;
    VAOscParams osc3;

    // Noise generator
    float noiseLevel = 0.0f;  // 0.0-1.0

    // Master settings
    float glide = 0.0f;       // 0.0-1.0 portamento time
    int polyphony = 8;        // 1-16 voices
    bool monoMode = false;    // True for mono with legato

    // Filter
    VAFilterParams filter;

    // Envelopes
    VAEnvelopeParams ampEnv;      // Amplitude envelope
    VAEnvelopeParams filterEnv;   // Filter envelope

    // LFO
    VALfoParams lfo;

    // Master volume and output
    float masterLevel = 0.8f;  // 0.0-1.0

    // Modulation matrix (same as Sampler/Slicer for consistency)
    SamplerModulationParams modulation;

    // Initialize with classic Mini Moog-like defaults
    void initDefaults() {
        // OSC1: Saw, base octave
        osc1.waveform = static_cast<int>(VAWaveform::Saw);
        osc1.octave = 0;
        osc1.level = 1.0f;
        osc1.enabled = true;

        // OSC2: Saw, -1 octave, slight detune
        osc2.waveform = static_cast<int>(VAWaveform::Saw);
        osc2.octave = -1;
        osc2.cents = 7.0f;
        osc2.level = 0.8f;
        osc2.enabled = true;

        // OSC3: Square, same octave, slight detune other direction
        osc3.waveform = static_cast<int>(VAWaveform::Square);
        osc3.octave = 0;
        osc3.cents = -5.0f;
        osc3.level = 0.6f;
        osc3.enabled = true;

        // Classic low filter with some resonance
        filter.cutoff = 0.4f;
        filter.resonance = 0.3f;
        filter.envAmount = 0.5f;
        filter.keyTrack = 0.5f;

        // Punchy amp envelope
        ampEnv.attack = 0.01f;
        ampEnv.decay = 0.3f;
        ampEnv.sustain = 0.7f;
        ampEnv.release = 0.2f;

        // Filter envelope with medium attack
        filterEnv.attack = 0.05f;
        filterEnv.decay = 0.4f;
        filterEnv.sustain = 0.3f;
        filterEnv.release = 0.3f;

        // Slow LFO for subtle vibrato
        lfo.rateIndex = 6;  // ~4 Hz
        lfo.shapeIndex = 0; // Sine
        lfo.toPitch = 0.0f;
        lfo.toFilter = 0.0f;
    }
};

} // namespace model
