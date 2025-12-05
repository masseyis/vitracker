#pragma once

#include <cmath>
#include <vector>
#include <algorithm>

namespace dsp {

class AudioAnalysis {
public:
    // BPM detection using onset detection + autocorrelation
    // Returns detected BPM in range 60-180, or 0 if detection fails
    static float detectBPM(const float* data, size_t numSamples, int sampleRate);

    // Pitch detection using YIN algorithm
    // Returns detected frequency in Hz, or 0 if detection fails
    static float detectPitch(const float* data, size_t numSamples, int sampleRate);

    // Convert frequency to MIDI note number (A4 = 440Hz = note 69)
    static int frequencyToMidiNote(float frequency);

    // Convert MIDI note number to frequency
    static float midiNoteToFrequency(int note);

    // Get pitch ratio to transpose from one note to another
    // e.g., to shift from detected note to C4 (note 60)
    static float getPitchRatio(int fromNote, int toNote);

    // Get the nearest C note (C1=24, C2=36, C3=48, C4=60, etc.)
    static int getNearestC(int midiNote);

    // Transient detection - returns sample positions of detected transients
    // sensitivity: 0.0 = less sensitive (fewer transients), 1.0 = more sensitive (more transients)
    static std::vector<size_t> detectTransients(const float* data, size_t numSamples,
                                                 int sampleRate, float sensitivity = 0.5f);

private:
    // Internal helpers for BPM detection
    static std::vector<float> computeOnsetEnvelope(const float* data, size_t numSamples, int hopSize);
    static float autocorrelationBPM(const std::vector<float>& envelope, int sampleRate, int hopSize);

    // Internal helpers for YIN pitch detection
    static float yinDetect(const float* data, size_t numSamples, int sampleRate);
};

} // namespace dsp
