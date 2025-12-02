#include "Voice.h"
#include <cstring>
#include <algorithm>

namespace audio {

uint64_t Voice::globalTime_ = 0;

Voice::Voice()
{
    std::memset(&patch_, 0, sizeof(patch_));
    std::memset(&modulations_, 0, sizeof(modulations_));
    std::memset(outBuffer_, 0, sizeof(outBuffer_));
    std::memset(auxBuffer_, 0, sizeof(auxBuffer_));
    std::memset(sharedBuffer_, 0, sizeof(sharedBuffer_));
}

Voice::~Voice() = default;

void Voice::init()
{
    stmlib::BufferAllocator allocator(sharedBuffer_, sizeof(sharedBuffer_));
    plaitsVoice_.Init(&allocator);
}

void Voice::noteOn(int note, float velocity, const model::Instrument& instrument)
{
    active_ = true;
    currentNote_ = note;
    startTime_ = ++globalTime_;

    // Set up patch from instrument
    const auto& params = instrument.getParams();
    patch_.engine = params.engine;
    patch_.note = static_cast<float>(note);
    patch_.harmonics = params.harmonics;
    patch_.timbre = params.timbre;
    patch_.morph = params.morph;
    patch_.lpg_colour = params.lpgColour;
    patch_.decay = params.decay;

    // Modulations
    modulations_.engine = 0.0f;
    modulations_.note = 0.0f;
    modulations_.frequency = 0.0f;
    modulations_.harmonics = 0.0f;
    modulations_.timbre = 0.0f;
    modulations_.morph = 0.0f;
    modulations_.trigger = velocity;
    modulations_.level = velocity;

    // Store send levels
    const auto& sends = instrument.getSends();
    reverbSend_ = sends.reverb;
    delaySend_ = sends.delay;
    chorusSend_ = sends.chorus;
    driveSend_ = sends.drive;
    sidechainSend_ = sends.sidechainDuck;
}

void Voice::noteOff()
{
    // For now, just deactivate (Plaits uses decay envelope)
    // In future, could trigger release phase
    active_ = false;
}

void Voice::render(float* outL, float* outR, int numSamples)
{
    if (!active_) return;

    // Plaits renders in blocks of 24 samples
    int processed = 0;
    while (processed < numSamples)
    {
        int blockSize = std::min(24, numSamples - processed);

        plaits::Voice::Frame frames[24];
        plaitsVoice_.Render(patch_, modulations_, frames, blockSize);

        // Clear trigger after first block
        modulations_.trigger = 0.0f;

        for (int i = 0; i < blockSize; ++i)
        {
            outL[processed + i] += frames[i].out / 32768.0f;
            outR[processed + i] += frames[i].aux / 32768.0f;
        }

        processed += blockSize;
    }
}

} // namespace audio
