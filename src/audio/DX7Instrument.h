#pragma once

#include "InstrumentProcessor.h"
#include "../dsp/msfa/dx7note.h"
#include "../dsp/msfa/lfo.h"
#include "../dsp/msfa/fm_core.h"
#include "../dsp/msfa/controllers.h"
#include "../dsp/msfa/tuning.h"
#include "../dsp/msfa/synth.h"
#include "../dsp/msfa/freqlut.h"
#include "../dsp/msfa/exp2.h"
#include "../dsp/msfa/sin.h"
#include <memory>
#include <array>
#include <cstdint>
#include <cstring>

namespace audio {

// DX7 patch size constants
constexpr int DX7_PATCH_SIZE_PACKED = 128;    // Size of packed patch in sysex bank
constexpr int DX7_PATCH_SIZE_UNPACKED = 156;  // Size of unpacked patch for Dx7Note
constexpr int DX7_MAX_POLYPHONY = 16;

class DX7Instrument : public InstrumentProcessor
{
public:
    DX7Instrument();
    ~DX7Instrument() override;

    // InstrumentProcessor interface
    void init(double sampleRate) override;
    void setSampleRate(double sampleRate) override;
    void noteOn(int note, float velocity) override;
    void noteOnWithFX(int note, float velocity, const model::Step& step) override;
    void noteOff(int note) override;
    void allNotesOff() override;
    void process(float* outL, float* outR, int numSamples) override;
    const char* getTypeName() const override { return "DX7"; }
    int getNumParameters() const override { return 0; }  // No UI parameters - preset-only
    const char* getParameterName(int index) const override { return ""; }
    float getParameter(int index) const override { return 0.0f; }
    void setParameter(int index, float value) override {}
    void getState(void* data, int maxSize) const override;
    void setState(const void* data, int size) override;

    // DX7-specific methods
    void loadPatch(const uint8_t* patchData);  // Load 156-byte unpacked patch
    void loadPackedPatch(const uint8_t* packedData);  // Load 128-byte packed patch
    const char* getPatchName() const;
    void setPolyphony(int voices);
    int getPolyphony() const { return polyphony_; }

    // Unpack a 128-byte packed patch to 156-byte unpacked format
    static void unpackPatch(const uint8_t* packed, uint8_t* unpacked);

private:
    struct Voice {
        std::unique_ptr<Dx7Note> note;
        int midiNote = -1;
        bool active = false;
        uint64_t age = 0;  // For voice stealing
    };

    int findFreeVoice();
    int findVoiceForNote(int note);
    void processBlock(int32_t* buffer, int numSamples);

    double sampleRate_ = 44100.0;
    uint8_t currentPatch_[DX7_PATCH_SIZE_UNPACKED];
    char patchName_[11];  // 10 chars + null terminator

    std::array<Voice, DX7_MAX_POLYPHONY> voices_;
    uint64_t voiceCounter_ = 0;
    int polyphony_ = DX7_MAX_POLYPHONY;

    Lfo lfo_;
    FmCore fmCore_;
    Controllers controllers_;
    std::shared_ptr<TuningState> tuning_;
};

} // namespace audio
