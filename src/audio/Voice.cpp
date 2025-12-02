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

void Voice::setFX(model::FXType type, uint8_t value)
{
    switch (type)
    {
        case model::FXType::ARP:
            arpNotes_[0] = 0;
            arpNotes_[1] = (value >> 4) & 0x0F;
            arpNotes_[2] = value & 0x0F;
            arpIndex_ = 0;
            arpTicks_ = 0;
            break;

        case model::FXType::POR:
            portamentoSpeed_ = value / 255.0f * 0.1f;  // Speed factor
            break;

        case model::FXType::VIB:
            vibratoSpeed_ = ((value >> 4) & 0x0F) / 15.0f * 10.0f;  // 0-10 Hz
            vibratoDepth_ = (value & 0x0F) / 15.0f * 0.5f;  // 0-0.5 semitones
            break;

        case model::FXType::VOL:
            // Volume slide - high nibble = up, low nibble = down
            volumeSlide_ = ((value >> 4) & 0x0F) - (value & 0x0F);
            break;

        case model::FXType::PAN:
            // Pan handled in mixer (not per-voice)
            break;

        case model::FXType::DLY:
            // Retrigger handled in AudioEngine
            break;

        default:
            break;
    }
}

void Voice::processFX()
{
    // Arpeggio
    if (arpNotes_[1] != 0 || arpNotes_[2] != 0)
    {
        arpTicks_++;
        if (arpTicks_ >= 6)  // Every 6 samples (fast arpeggio)
        {
            arpTicks_ = 0;
            arpIndex_ = (arpIndex_ + 1) % 3;
        }
        // Adjust pitch based on arp
        float arpOffset = static_cast<float>(arpNotes_[arpIndex_]);
        modulations_.note = arpOffset;
    }

    // Portamento
    if (portamentoSpeed_ > 0.0f && portamentoTarget_ != currentPitch_)
    {
        float diff = portamentoTarget_ - currentPitch_;
        if (std::abs(diff) > 0.01f)
        {
            currentPitch_ += diff * portamentoSpeed_;
            modulations_.note = currentPitch_ - static_cast<float>(currentNote_);
        }
    }

    // Vibrato
    if (vibratoDepth_ > 0.0f)
    {
        vibratoPhase_ += vibratoSpeed_ / static_cast<float>(sampleRate_);
        if (vibratoPhase_ >= 1.0f) vibratoPhase_ -= 1.0f;

        float vibrato = std::sin(vibratoPhase_ * 2.0f * 3.14159f) * vibratoDepth_;
        modulations_.note += vibrato;
    }

    // Volume slide (applied per tick, small increments)
    if (volumeSlide_ != 0.0f)
    {
        modulations_.level = std::clamp(modulations_.level + volumeSlide_ * 0.001f, 0.0f, 1.0f);
    }
}

} // namespace audio
