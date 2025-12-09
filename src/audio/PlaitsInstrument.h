#pragma once

#include "InstrumentProcessor.h"
#include "UniversalTrackerFX.h"
#include "../dsp/voice_allocator.h"
#include "../dsp/modulation_matrix.h"
#include "../dsp/moog_filter.h"
#include <array>
#include <string>

namespace audio {

// Parameter indices for PlaitsInstrument
enum PlaitsParam {
    kParamEngine = 0,
    kParamHarmonics,
    kParamTimbre,
    kParamMorph,
    kParamAttack,
    kParamDecay,
    kParamPolyphony,
    kParamCutoff,
    kParamResonance,
    // LFO1
    kParamLfo1Rate,
    kParamLfo1Shape,
    kParamLfo1Dest,
    kParamLfo1Amount,
    // LFO2
    kParamLfo2Rate,
    kParamLfo2Shape,
    kParamLfo2Dest,
    kParamLfo2Amount,
    // ENV1
    kParamEnv1Attack,
    kParamEnv1Decay,
    kParamEnv1Dest,
    kParamEnv1Amount,
    // ENV2
    kParamEnv2Attack,
    kParamEnv2Decay,
    kParamEnv2Dest,
    kParamEnv2Amount,
    kNumParams
};

// Full-featured Plaits instrument with modulation matrix and filter
class PlaitsInstrument : public InstrumentProcessor
{
public:
    PlaitsInstrument();
    ~PlaitsInstrument() override = default;

    // InstrumentProcessor interface
    void init(double sampleRate) override;
    void setSampleRate(double sampleRate) override;
    void noteOn(int note, float velocity) override;
    void noteOnWithFX(int note, float velocity, const model::Step& step) override;
    void noteOff(int note) override;
    void allNotesOff() override;
    void process(float* outL, float* outR, int numSamples) override;
    const char* getTypeName() const override { return "Plaits"; }

    int getNumParameters() const override { return kNumParams; }
    const char* getParameterName(int index) const override;
    float getParameter(int index) const override;
    void setParameter(int index, float value) override;

    void getState(void* data, int maxSize) const override;
    void setState(const void* data, int size) override;

    // Extended accessors for UI
    int getEngine() const { return engine_; }
    float getHarmonics() const { return harmonics_; }
    float getTimbre() const { return timbre_; }
    float getMorph() const { return morph_; }
    float getAttack() const { return attack_; }
    float getDecay() const { return decay_; }
    int getPolyphony() const { return polyphony_; }
    float getCutoff() const { return cutoff_; }
    float getResonance() const { return resonance_; }

    // Modulated values for UI visualization
    float getModulatedHarmonics() const;
    float getModulatedTimbre() const;
    float getModulatedMorph() const;
    float getModulatedCutoff() const;
    float getModulatedResonance() const;

    // Modulation access for UI
    const plaits::ModulationMatrix& getModMatrix() const { return modMatrix_; }

    // Engine names
    static const char* getEngineName(int engine);
    static constexpr int kNumEngines = 16;

    // Set tempo for tempo-synced LFOs
    void setTempo(double bpm);

private:
    void updateModulationParams();
    void applyModulation();

    VoiceAllocator voiceAllocator_;
    plaits::ModulationMatrix modMatrix_;
    plaits::MoogFilter filter_;

    double sampleRate_ = 48000.0;
    double tempo_ = 120.0;
    int activeVoiceCount_ = 0;

    // Core parameters (0-1 normalized unless noted)
    int engine_ = 0;           // 0-15
    float harmonics_ = 0.5f;
    float timbre_ = 0.5f;
    float morph_ = 0.5f;
    float attack_ = 0.01f;     // 1ms-2000ms mapped
    float decay_ = 0.3f;       // 1ms-10000ms mapped
    int polyphony_ = 8;        // 1-16
    float cutoff_ = 1.0f;      // 20Hz-20kHz mapped
    float resonance_ = 0.0f;   // 0-1

    // LFO1 params
    int lfo1Rate_ = 4;         // LfoRateDivision index
    int lfo1Shape_ = 0;        // LfoShape index
    int lfo1Dest_ = 2;         // ModDestination index
    int lfo1Amount_ = 0;       // -64 to +63

    // LFO2 params
    int lfo2Rate_ = 4;
    int lfo2Shape_ = 0;
    int lfo2Dest_ = 3;
    int lfo2Amount_ = 0;

    // ENV1 params
    float env1Attack_ = 0.01f;
    float env1Decay_ = 0.2f;
    int env1Dest_ = 0;
    int env1Amount_ = 0;

    // ENV2 params
    float env2Attack_ = 0.01f;
    float env2Decay_ = 0.5f;
    int env2Dest_ = 3;
    int env2Amount_ = 0;

    // Temporary buffers for processing
    static constexpr int kMaxBlockSize = 512;
    std::array<float, kMaxBlockSize> tempBufferL_;
    std::array<float, kMaxBlockSize> tempBufferR_;

    // Universal tracker FX
    UniversalTrackerFX trackerFX_;
    bool hasPendingFX_ = false;
};

} // namespace audio
