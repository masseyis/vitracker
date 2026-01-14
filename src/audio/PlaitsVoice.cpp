#include "PlaitsVoice.h"
#include <algorithm>

namespace audio {

PlaitsVoice::PlaitsVoice() {
  // Wrapper will be initialized in setSampleRate
}

void PlaitsVoice::setSampleRate(double sampleRate) {
  sampleRate_ = sampleRate;
  plaitsVoiceWrapper_.Init(sampleRate);
}

void PlaitsVoice::noteOn(int note, float velocity) {
  currentNote_ = note;
  velocity_ = velocity;

  // Map decay parameter to attack/decay times (simplified for now)
  float attackMs = params_.attack * 2000.0f; // 0-2000ms
  float decayMs = params_.decay * 10000.0f;  // 0-10000ms

  plaitsVoiceWrapper_.NoteOn(note, velocity, attackMs, decayMs);
}

void PlaitsVoice::noteOff() { plaitsVoiceWrapper_.NoteOff(); }

void PlaitsVoice::process(float *outL, float *outR, int numSamples,
                          float pitchMod, float cutoffMod, float volumeMod,
                          float panMod) {
  if (!plaitsVoiceWrapper_.active()) {
    std::fill_n(outL, numSamples, 0.0f);
    std::fill_n(outR, numSamples, 0.0f);
    return;
  }

  // Update Plaits parameters before processing
  // Note: pitch modulation is handled by transposing the note in noteOn
  // For now, we'll skip real-time pitch modulation since it requires note
  // re-triggering
  plaitsVoiceWrapper_.set_engine(params_.engine);
  plaitsVoiceWrapper_.set_harmonics(
      std::clamp(params_.harmonics + cutoffMod, 0.0f, 1.0f));
  plaitsVoiceWrapper_.set_timbre(params_.timbre);
  plaitsVoiceWrapper_.set_morph(params_.morph);
  plaitsVoiceWrapper_.set_decay(params_.decay);
  plaitsVoiceWrapper_.set_lpg_colour(params_.lpgColour);

  // Process audio - wrapper handles stereo output
  // Need to zero buffers first since Process() mixes into them
  std::fill_n(outL, numSamples, 0.0f);
  std::fill_n(outR, numSamples, 0.0f);

  plaitsVoiceWrapper_.Process(outL, outR, numSamples);

  // Apply volume and pan modulation
  // Note: Plaits output is already relatively quiet compared to VA/DX7, no
  // headroom needed
  float leftGain = volumeMod * (1.0f - std::max(0.0f, panMod));
  float rightGain = volumeMod * (1.0f + std::min(0.0f, panMod));

  for (int i = 0; i < numSamples; ++i) {
    outL[i] *= leftGain;
    outR[i] *= rightGain;
  }
}

bool PlaitsVoice::isActive() const { return plaitsVoiceWrapper_.active(); }

int PlaitsVoice::getCurrentNote() const { return currentNote_; }

void PlaitsVoice::updateParameters(const model::PlaitsParams &params) {
  params_ = params;
}

} // namespace audio
