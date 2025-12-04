#include "AudioAnalysis.h"
#include <cmath>
#include <numeric>

namespace dsp {

float AudioAnalysis::detectBPM(const float* data, size_t numSamples, int sampleRate) {
    if (!data || numSamples < static_cast<size_t>(sampleRate)) {
        return 0.0f; // Need at least 1 second of audio
    }

    // Use a hop size of ~10ms for onset detection
    int hopSize = sampleRate / 100;

    // Compute onset envelope
    auto envelope = computeOnsetEnvelope(data, numSamples, hopSize);

    if (envelope.size() < 100) {
        return 0.0f; // Not enough data
    }

    // Find BPM via autocorrelation
    return autocorrelationBPM(envelope, sampleRate, hopSize);
}

std::vector<float> AudioAnalysis::computeOnsetEnvelope(const float* data, size_t numSamples, int hopSize) {
    std::vector<float> envelope;

    int windowSize = hopSize * 2;
    float prevEnergy = 0.0f;

    for (size_t i = 0; i + windowSize < numSamples; i += static_cast<size_t>(hopSize)) {
        // Compute energy in this frame
        float energy = 0.0f;
        for (int j = 0; j < windowSize; ++j) {
            float sample = data[i + static_cast<size_t>(j)];
            energy += sample * sample;
        }
        energy /= static_cast<float>(windowSize);

        // Onset is the positive difference in energy (half-wave rectified derivative)
        float onset = std::max(0.0f, energy - prevEnergy);
        envelope.push_back(onset);
        prevEnergy = energy;
    }

    return envelope;
}

float AudioAnalysis::autocorrelationBPM(const std::vector<float>& envelope, int sampleRate, int hopSize) {
    // BPM range: 60-180 BPM
    // At 60 BPM, beat period = 1 second = sampleRate/hopSize frames
    // At 180 BPM, beat period = 0.333 seconds

    float envelopeRate = static_cast<float>(sampleRate) / static_cast<float>(hopSize);
    int minLag = static_cast<int>(envelopeRate * 60.0f / 180.0f); // 180 BPM
    int maxLag = static_cast<int>(envelopeRate * 60.0f / 60.0f);  // 60 BPM

    // Limit search range based on available data
    maxLag = std::min(maxLag, static_cast<int>(envelope.size()) / 2);

    if (minLag >= maxLag) {
        return 0.0f;
    }

    // Find peak in autocorrelation
    float maxCorr = 0.0f;
    int bestLag = minLag;

    for (int lag = minLag; lag <= maxLag; ++lag) {
        float corr = 0.0f;
        int count = 0;

        for (size_t i = 0; i < envelope.size() - static_cast<size_t>(lag); ++i) {
            corr += envelope[i] * envelope[i + static_cast<size_t>(lag)];
            ++count;
        }

        if (count > 0) {
            corr /= static_cast<float>(count);
        }

        if (corr > maxCorr) {
            maxCorr = corr;
            bestLag = lag;
        }
    }

    // Convert lag to BPM
    float beatPeriodSec = static_cast<float>(bestLag) / envelopeRate;
    float bpm = 60.0f / beatPeriodSec;

    // Sanity check
    if (bpm < 60.0f || bpm > 180.0f) {
        return 0.0f;
    }

    return bpm;
}

float AudioAnalysis::detectPitch(const float* data, size_t numSamples, int sampleRate) {
    if (!data || numSamples < 2048) {
        return 0.0f;
    }

    return yinDetect(data, numSamples, sampleRate);
}

float AudioAnalysis::yinDetect(const float* data, size_t numSamples, int sampleRate) {
    // YIN algorithm for pitch detection
    // Analyze first ~50ms or available samples
    size_t analyzeLength = std::min(numSamples, static_cast<size_t>(sampleRate / 20));

    // Min and max periods (in samples)
    // C1 (~32Hz) to C7 (~2093Hz)
    int minPeriod = sampleRate / 2100;
    int maxPeriod = sampleRate / 30;

    // Ensure we have enough samples
    size_t requiredLength = static_cast<size_t>(maxPeriod * 2);
    if (analyzeLength < requiredLength) {
        analyzeLength = std::min(numSamples, requiredLength);
    }

    if (analyzeLength < requiredLength) {
        return 0.0f; // Not enough data
    }

    size_t windowSize = analyzeLength / 2;

    // Step 1 & 2: Difference function and cumulative mean normalized difference
    std::vector<float> yinBuffer(windowSize);

    // Compute difference function
    for (size_t tau = 0; tau < windowSize; ++tau) {
        float sum = 0.0f;
        for (size_t i = 0; i < windowSize; ++i) {
            float delta = data[i] - data[i + tau];
            sum += delta * delta;
        }
        yinBuffer[tau] = sum;
    }

    // Cumulative mean normalized difference function
    yinBuffer[0] = 1.0f;
    float runningSum = 0.0f;
    for (size_t tau = 1; tau < windowSize; ++tau) {
        runningSum += yinBuffer[tau];
        yinBuffer[tau] = yinBuffer[tau] * static_cast<float>(tau) / runningSum;
    }

    // Step 3: Absolute threshold
    float threshold = 0.1f;
    size_t tauEstimate = 0;

    for (size_t tau = static_cast<size_t>(minPeriod); tau < windowSize && tau < static_cast<size_t>(maxPeriod); ++tau) {
        if (yinBuffer[tau] < threshold) {
            // Find local minimum
            while (tau + 1 < windowSize && yinBuffer[tau + 1] < yinBuffer[tau]) {
                ++tau;
            }
            tauEstimate = tau;
            break;
        }
    }

    // If no pitch found with strict threshold, try relaxed threshold
    if (tauEstimate == 0) {
        threshold = 0.2f;
        for (size_t tau = static_cast<size_t>(minPeriod); tau < windowSize && tau < static_cast<size_t>(maxPeriod); ++tau) {
            if (yinBuffer[tau] < threshold) {
                while (tau + 1 < windowSize && yinBuffer[tau + 1] < yinBuffer[tau]) {
                    ++tau;
                }
                tauEstimate = tau;
                break;
            }
        }
    }

    if (tauEstimate == 0) {
        return 0.0f; // No pitch detected
    }

    // Step 4: Parabolic interpolation for better precision
    float betterTau = static_cast<float>(tauEstimate);
    if (tauEstimate > 0 && tauEstimate < windowSize - 1) {
        float s0 = yinBuffer[tauEstimate - 1];
        float s1 = yinBuffer[tauEstimate];
        float s2 = yinBuffer[tauEstimate + 1];
        float adjustment = (s2 - s0) / (2.0f * (2.0f * s1 - s2 - s0));
        if (std::abs(adjustment) < 1.0f) {
            betterTau = static_cast<float>(tauEstimate) + adjustment;
        }
    }

    // Convert period to frequency
    float frequency = static_cast<float>(sampleRate) / betterTau;

    // Sanity check (C1 to C8 range)
    if (frequency < 30.0f || frequency > 4200.0f) {
        return 0.0f;
    }

    return frequency;
}

int AudioAnalysis::frequencyToMidiNote(float frequency) {
    if (frequency <= 0.0f) {
        return -1;
    }
    // A4 = 440Hz = MIDI note 69
    // note = 69 + 12 * log2(freq / 440)
    float note = 69.0f + 12.0f * std::log2(frequency / 440.0f);
    return static_cast<int>(std::round(note));
}

float AudioAnalysis::midiNoteToFrequency(int note) {
    if (note < 0 || note > 127) {
        return 0.0f;
    }
    // freq = 440 * 2^((note - 69) / 12)
    return 440.0f * std::pow(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);
}

float AudioAnalysis::getPitchRatio(int fromNote, int toNote) {
    // Pitch ratio = 2^((toNote - fromNote) / 12)
    return std::pow(2.0f, static_cast<float>(toNote - fromNote) / 12.0f);
}

int AudioAnalysis::getNearestC(int midiNote) {
    // C notes: 12, 24, 36, 48, 60, 72, 84, 96, 108
    // Find nearest C by rounding to nearest multiple of 12
    int octave = (midiNote + 6) / 12; // +6 for rounding
    return octave * 12;
}

} // namespace dsp
