#include <gtest/gtest.h>
#include "../src/audio/DX7Instrument.h"
#include <cmath>
#include <numeric>
#include <iostream>

using namespace audio;

class DX7InstrumentTest : public ::testing::Test {
protected:
    void SetUp() override {
        dx7.init(44100.0);
    }

    DX7Instrument dx7;
};

// Test that DX7 produces non-zero output when a note is triggered
TEST_F(DX7InstrumentTest, ProducesSoundOnNoteOn) {
    // Trigger a note (middle C, velocity 100)
    dx7.noteOn(60, 0.8f);

    // Process several blocks of audio
    constexpr int numBlocks = 10;
    constexpr int blockSize = 256;
    float outL[blockSize];
    float outR[blockSize];

    float maxAmplitude = 0.0f;
    float totalEnergy = 0.0f;

    for (int block = 0; block < numBlocks; ++block) {
        // Clear buffers
        std::fill(outL, outL + blockSize, 0.0f);
        std::fill(outR, outR + blockSize, 0.0f);

        // Process audio
        dx7.process(outL, outR, blockSize);

        // Calculate energy for this block
        for (int i = 0; i < blockSize; ++i) {
            float absL = std::abs(outL[i]);
            float absR = std::abs(outR[i]);
            maxAmplitude = std::max(maxAmplitude, std::max(absL, absR));
            totalEnergy += outL[i] * outL[i] + outR[i] * outR[i];
        }

        std::cout << "Block " << block << ": max=" << maxAmplitude
                  << ", energy=" << totalEnergy << std::endl;
    }

    // The DX7 should produce audible output
    EXPECT_GT(maxAmplitude, 0.001f) << "DX7 should produce non-zero output";
    EXPECT_GT(totalEnergy, 0.0f) << "DX7 should produce non-zero energy";
}

// Test that DX7 is silent when no note is playing
TEST_F(DX7InstrumentTest, SilentWhenNoNote) {
    // Process audio without triggering any note
    constexpr int blockSize = 256;
    float outL[blockSize];
    float outR[blockSize];

    std::fill(outL, outL + blockSize, 0.0f);
    std::fill(outR, outR + blockSize, 0.0f);

    dx7.process(outL, outR, blockSize);

    // Calculate total energy
    float totalEnergy = 0.0f;
    for (int i = 0; i < blockSize; ++i) {
        totalEnergy += outL[i] * outL[i] + outR[i] * outR[i];
    }

    EXPECT_FLOAT_EQ(totalEnergy, 0.0f) << "DX7 should be silent with no notes";
}

// Test that noteOff eventually leads to silence
TEST_F(DX7InstrumentTest, NoteOffLeadsToSilence) {
    // Trigger a note
    dx7.noteOn(60, 0.8f);

    // Process a few blocks to let sound start
    constexpr int blockSize = 256;
    float outL[blockSize];
    float outR[blockSize];

    for (int i = 0; i < 5; ++i) {
        dx7.process(outL, outR, blockSize);
    }

    // Release the note
    dx7.noteOff(60);

    // Process many more blocks to let sound decay
    float lastEnergy = 0.0f;
    for (int i = 0; i < 100; ++i) {
        std::fill(outL, outL + blockSize, 0.0f);
        std::fill(outR, outR + blockSize, 0.0f);

        dx7.process(outL, outR, blockSize);

        float energy = 0.0f;
        for (int j = 0; j < blockSize; ++j) {
            energy += outL[j] * outL[j] + outR[j] * outR[j];
        }
        lastEnergy = energy;
    }

    // After release and decay, energy should be very low
    EXPECT_LT(lastEnergy, 0.001f) << "DX7 should decay to silence after noteOff";
}

// Test loading a packed patch
TEST_F(DX7InstrumentTest, LoadPackedPatch) {
    // A simple packed patch (all zeros for simplicity, but with algorithm=31)
    uint8_t packedPatch[DX7_PATCH_SIZE_PACKED] = {0};

    // Set algorithm to 31 (all carriers) at byte 110
    packedPatch[110] = 31;

    // Set some operator output levels (byte 14 of each 17-byte operator)
    for (int op = 0; op < 6; ++op) {
        packedPatch[op * 17 + 14] = 99;  // Output level
    }

    // Load the patch
    dx7.loadPackedPatch(packedPatch);

    // Trigger a note
    dx7.noteOn(60, 0.8f);

    // Process audio
    constexpr int blockSize = 256;
    float outL[blockSize];
    float outR[blockSize];

    float maxAmplitude = 0.0f;
    for (int block = 0; block < 10; ++block) {
        std::fill(outL, outL + blockSize, 0.0f);
        std::fill(outR, outR + blockSize, 0.0f);

        dx7.process(outL, outR, blockSize);

        for (int i = 0; i < blockSize; ++i) {
            maxAmplitude = std::max(maxAmplitude, std::abs(outL[i]));
        }
    }

    std::cout << "After loading packed patch, max amplitude: " << maxAmplitude << std::endl;

    // Just verify it doesn't crash - sound may or may not be present
    // depending on the patch parameters
    SUCCEED();
}

// Test multiple voices
TEST_F(DX7InstrumentTest, MultipleVoices) {
    // Trigger multiple notes
    dx7.noteOn(60, 0.8f);  // C4
    dx7.noteOn(64, 0.8f);  // E4
    dx7.noteOn(67, 0.8f);  // G4

    constexpr int blockSize = 256;
    float outL[blockSize];
    float outR[blockSize];

    float totalEnergy = 0.0f;
    for (int block = 0; block < 10; ++block) {
        std::fill(outL, outL + blockSize, 0.0f);
        std::fill(outR, outR + blockSize, 0.0f);

        dx7.process(outL, outR, blockSize);

        for (int i = 0; i < blockSize; ++i) {
            totalEnergy += outL[i] * outL[i];
        }
    }

    std::cout << "Multiple voices total energy: " << totalEnergy << std::endl;

    EXPECT_GT(totalEnergy, 0.0f) << "Multiple voices should produce sound";
}

// Test init patch produces sound
TEST_F(DX7InstrumentTest, InitPatchProducesSound) {
    // The default INIT VOICE patch should produce a simple sine wave
    std::cout << "Patch name: " << dx7.getPatchName() << std::endl;

    dx7.noteOn(60, 1.0f);

    constexpr int blockSize = 512;
    float outL[blockSize];
    float outR[blockSize];

    std::fill(outL, outL + blockSize, 0.0f);
    std::fill(outR, outR + blockSize, 0.0f);

    dx7.process(outL, outR, blockSize);

    // Print some sample values
    std::cout << "First 10 samples: ";
    for (int i = 0; i < 10; ++i) {
        std::cout << outL[i] << " ";
    }
    std::cout << std::endl;

    // Calculate peak and RMS
    float peak = 0.0f;
    float sumSquares = 0.0f;
    for (int i = 0; i < blockSize; ++i) {
        peak = std::max(peak, std::abs(outL[i]));
        sumSquares += outL[i] * outL[i];
    }
    float rms = std::sqrt(sumSquares / blockSize);

    std::cout << "Peak: " << peak << ", RMS: " << rms << std::endl;

    EXPECT_GT(peak, 0.0f) << "Init patch should produce sound";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
