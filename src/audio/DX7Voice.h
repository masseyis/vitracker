#pragma once

#include "Voice.h"
#include <cstdint>
#include <cstring>
#include <memory>

// Forward declarations to avoid including problematic headers (lfo.h has no
// include guards)
class Dx7Note;
class Lfo;
class FmCore;
class Controllers;
class TuningState;
std::shared_ptr<TuningState> createStandardTuning();

namespace audio {

// Size constants (matching DX7Instrument)
constexpr int DX7_VOICE_PATCH_SIZE = 156;

// Single-voice DX7 synthesizer implementing the Voice interface
// Each track owns one DX7Voice for voice-per-track architecture
class DX7Voice : public Voice {
public:
  DX7Voice();
  ~DX7Voice() override;

  // Voice interface
  void noteOn(int note, float velocity) override;
  void noteOff() override;
  void process(float *outL, float *outR, int numSamples, float pitchMod = 0.0f,
               float cutoffMod = 0.0f, float volumeMod = 1.0f,
               float panMod = 0.5f) override;
  bool isActive() const override { return active_; }
  int getCurrentNote() const override { return currentNote_; }
  void setSampleRate(double sampleRate) override;

  // DX7-specific parameter setters (called by
  // DX7Instrument::updateVoiceParameters)
  void loadPatch(const uint8_t *patchData);
  void setControllers(const Controllers *controllers) {
    parentControllers_ = controllers;
  }

private:
  double sampleRate_ = 44100.0;
  bool active_ = false;
  int currentNote_ = -1;
  float velocity_ = 0.0f;

  // DX7 synthesis components (pimpl-style to avoid header issues)
  std::unique_ptr<Dx7Note> dx7Note_;
  std::unique_ptr<Lfo> lfo_;
  std::unique_ptr<FmCore> fmCore_;
  std::unique_ptr<Controllers> controllers_;
  std::shared_ptr<TuningState> tuning_;
  uint8_t patch_[DX7_VOICE_PATCH_SIZE];

  // Pointer to parent instrument's controllers for shared state
  const Controllers *parentControllers_ = nullptr;
};

} // namespace audio
