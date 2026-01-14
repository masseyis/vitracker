#include "DX7Voice.h"
#include "../dsp/msfa/controllers.h"
#include "../dsp/msfa/dx7note.h"
#include "../dsp/msfa/env.h"
#include "../dsp/msfa/exp2.h"
#include "../dsp/msfa/fm_core.h"
#include "../dsp/msfa/freqlut.h"
#include "../dsp/msfa/lfo.h"
#include "../dsp/msfa/sin.h"
#include "../dsp/msfa/tuning.h"
#include <algorithm>
#include <cmath>

namespace audio {

// Dx7Note block size (from msfa)
constexpr int N = 64;

DX7Voice::DX7Voice() {
  tuning_ = createStandardTuning();

  // Initialize patch to zeros (will be set via updateVoiceParameters)
  std::memset(patch_, 0, DX7_VOICE_PATCH_SIZE);

  // Create DSP components
  fmCore_ = std::make_unique<FmCore>();
  lfo_ = std::make_unique<Lfo>();
  controllers_ = std::make_unique<Controllers>();

  // Initialize Controllers
  std::memset(controllers_->values_, 0, sizeof(controllers_->values_));

  // Set pitch bend to center (0x2000 = 8192)
  controllers_->values_[kControllerPitch] = 0x2000;

  // Set default pitch bend range (2 semitones up and down)
  controllers_->values_[kControllerPitchRangeUp] = 2;
  controllers_->values_[kControllerPitchRangeDn] = 2;
  controllers_->values_[kControllerPitchStep] = 0;

  // Initialize other controller values
  controllers_->masterTune = 0;
  controllers_->modwheel_cc = 0;
  controllers_->breath_cc = 0;
  controllers_->foot_cc = 0;
  controllers_->aftertouch_cc = 0;

  // Portamento settings
  controllers_->portamento_enable_cc = false;
  controllers_->portamento_cc = 0;
  controllers_->portamento_gliss_cc = false;

  // Set FmCore pointer
  controllers_->core = fmCore_.get();

  // Create the Dx7Note (nullptr for MTSClient - not using microtuning)
  dx7Note_ = std::make_unique<Dx7Note>(tuning_, nullptr);
}

DX7Voice::~DX7Voice() = default;

void DX7Voice::setSampleRate(double sampleRate) {
  sampleRate_ = sampleRate;
  // Note: Env::init_sr and Lfo::init should only be called once globally
  // They are static init methods called from DX7Instrument::init()
}

void DX7Voice::loadPatch(const uint8_t *patchData) {
  std::memcpy(patch_, patchData, DX7_VOICE_PATCH_SIZE);
  // Reset LFO with new patch parameters (LFO params at offset 137)
  lfo_->reset(&patch_[137]);
}

void DX7Voice::noteOn(int note, float velocity) {
  currentNote_ = note;
  velocity_ = velocity;
  active_ = true;

  int velocityInt = static_cast<int>(velocity * 127.0f);
  velocityInt = std::clamp(velocityInt, 0, 127);

  // Initialize the Dx7Note with current patch
  dx7Note_->init(patch_, note, velocityInt, 0, controllers_.get());

  // Trigger LFO
  lfo_->keydown();
}

void DX7Voice::noteOff() {
  if (dx7Note_) {
    dx7Note_->keyup();
  }
}

void DX7Voice::process(float *outL, float *outR, int numSamples, float pitchMod,
                       float /* cutoffMod */, float volumeMod, float panMod) {
  if (!active_ || !dx7Note_) {
    // Don't fill with zeros - Voice::process adds to buffer
    return;
  }

  // Apply pitch modulation via pitch bend controller
  // pitchMod is in semitones, convert to pitch bend value
  // Pitch bend range is ±2 semitones by default, centered at 0x2000 (8192)
  if (std::abs(pitchMod) > 0.001f) {
    // Clamp to ±2 semitones range
    float clampedPitch = std::clamp(pitchMod, -2.0f, 2.0f);
    // Convert to 14-bit pitch bend value (0-16383, center is 8192)
    int pitchBend = static_cast<int>(8192 + (clampedPitch / 2.0f) * 8191);
    pitchBend = std::clamp(pitchBend, 0, 16383);
    controllers_->values_[kControllerPitch] = pitchBend;
  } else {
    controllers_->values_[kControllerPitch] = 0x2000; // Center
  }

  // Process in N-sample blocks (N = 64 in msfa)
  int32_t buf[N];
  int samplesProcessed = 0;

  while (samplesProcessed < numSamples) {
    int samplesToProcess = std::min(N, numSamples - samplesProcessed);

    // Clear buffer
    std::memset(buf, 0, sizeof(buf));

    // Get LFO values
    int32_t lfoVal = lfo_->getsample();
    int32_t lfoDelay = lfo_->getdelay();

    // Refresh controller modulation
    controllers_->refresh();

    // Render the voice
    dx7Note_->compute(buf, lfoVal, lfoDelay, controllers_.get());

    // Check if voice has finished
    if (!dx7Note_->isPlaying()) {
      active_ = false;
    }

    // Convert int32 Q24 to float and apply volume/pan
    // DX7 output is in Q24 format (24-bit fixed point)
    constexpr float baseScale = 1.0f / (1 << 24);
    // Apply headroom scaling
    constexpr float headroomScale = 0.5f;
    float scale = baseScale * headroomScale * volumeMod;

    // Calculate pan gains
    float leftGain = std::sqrt(1.0f - panMod);
    float rightGain = std::sqrt(panMod);

    for (int i = 0; i < samplesToProcess; ++i) {
      float sample = static_cast<float>(buf[i]) * scale;
      outL[samplesProcessed + i] += sample * leftGain;
      outR[samplesProcessed + i] += sample * rightGain;
    }

    samplesProcessed += samplesToProcess;
  }
}

} // namespace audio
