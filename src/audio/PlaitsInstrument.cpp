#include "PlaitsInstrument.h"
#include "Voice.h"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace audio {

std::unique_ptr<audio::Voice> PlaitsInstrument::createVoice() {
    // TODO: Implement PlaitsVoice class
    return nullptr;
}

void PlaitsInstrument::updateVoiceParameters(audio::Voice* voice) {
    // TODO: Implement parameter update for PlaitsVoice
    (void)voice;
}

static const char* kEngineNames[16] = {
    "Virtual Analog",
    "Waveshaping",
    "FM",
    "Grain",
    "Additive",
    "Wavetable",
    "Chord",
    "Speech",
    "Swarm",
    "Noise",
    "Particle",
    "String",
    "Modal",
    "Bass Drum",
    "Snare Drum",
    "Hi-Hat"
};

static const char* kParamNames[kNumParams] = {
    "Engine",
    "Harmonics",
    "Timbre",
    "Morph",
    "Attack",
    "Decay",
    "Polyphony",
    "Cutoff",
    "Resonance",
    "LFO1 Rate",
    "LFO1 Shape",
    "LFO1 Dest",
    "LFO1 Amount",
    "LFO2 Rate",
    "LFO2 Shape",
    "LFO2 Dest",
    "LFO2 Amount",
    "ENV1 Attack",
    "ENV1 Decay",
    "ENV1 Dest",
    "ENV1 Amount",
    "ENV2 Attack",
    "ENV2 Decay",
    "ENV2 Dest",
    "ENV2 Amount"
};

// Map 0-1 to milliseconds for attack (1ms to 2000ms, exponential)
static float mapAttack(float normalized)
{
    return 1.0f + std::pow(normalized, 2.0f) * 1999.0f;
}

// Map 0-1 to milliseconds for decay (1ms to 10000ms, exponential)
static float mapDecay(float normalized)
{
    return 1.0f + std::pow(normalized, 2.0f) * 9999.0f;
}

// Map 0-1 to Hz for cutoff (20Hz to 20000Hz, exponential)
static float mapCutoff(float normalized)
{
    return 20.0f * std::pow(1000.0f, normalized);
}

PlaitsInstrument::PlaitsInstrument()
{
    tempBufferL_.fill(0.0f);
    tempBufferR_.fill(0.0f);
}

void PlaitsInstrument::init(double sampleRate)
{
    sampleRate_ = sampleRate;
    voiceAllocator_.Init(sampleRate, polyphony_);
    modMatrix_.Init();
    filter_.Init(static_cast<float>(sampleRate));

    // Set initial parameter values
    voiceAllocator_.set_engine(engine_);
    voiceAllocator_.set_harmonics(harmonics_);
    voiceAllocator_.set_timbre(timbre_);
    voiceAllocator_.set_morph(morph_);

    filter_.SetCutoff(mapCutoff(cutoff_));
    filter_.SetResonance(resonance_);

    modMatrix_.SetTempo(tempo_);

    // Initialize tracker FX
    trackerFX_.setSampleRate(sampleRate);
    trackerFX_.setTempo(static_cast<float>(tempo_));
}

void PlaitsInstrument::setSampleRate(double sampleRate)
{
    sampleRate_ = sampleRate;
    voiceAllocator_.Init(sampleRate, polyphony_);
    filter_.SetSampleRate(static_cast<float>(sampleRate));
    trackerFX_.setSampleRate(sampleRate);
}

void PlaitsInstrument::noteOn(int note, float velocity)
{
    model::Step emptyStep;
    noteOnWithFX(note, velocity, emptyStep);
}

void PlaitsInstrument::noteOnWithFX(int note, float velocity, const model::Step& step)
{
    // If there's already pending FX, stop all voices first to avoid overlap
    if (hasPendingFX_) {
        voiceAllocator_.AllNotesOff();
        activeVoiceCount_ = 0;
    }

    // Trigger UniversalTrackerFX
    trackerFX_.triggerNote(note, velocity, step);
    hasPendingFX_ = true;
    lastArpNote_ = note;  // Initialize tracking with base note

    // If no delay, trigger note immediately
    // Otherwise, the process() method will trigger it after the delay
    bool hasDelay = false;
    for (int i = 0; i < 3; ++i) {
        const auto* fx = (i == 0) ? &step.fx1 : (i == 1) ? &step.fx2 : &step.fx3;
        if (fx->type == model::FXType::DLY) {
            hasDelay = true;
            break;
        }
    }

    if (!hasDelay) {
        float attackMs = mapAttack(attack_);
        float decayMs = mapDecay(decay_);

        voiceAllocator_.NoteOn(note, velocity, attackMs, decayMs);

        // Trigger modulation envelopes on first note
        int newActiveCount = voiceAllocator_.activeVoiceCount();
        if (activeVoiceCount_ == 0 && newActiveCount > 0)
        {
            modMatrix_.TriggerEnvelopes();
        }
        activeVoiceCount_ = newActiveCount;
    }
}

void PlaitsInstrument::noteOff(int note)
{
    voiceAllocator_.NoteOff(note);
    activeVoiceCount_ = voiceAllocator_.activeVoiceCount();
}

void PlaitsInstrument::allNotesOff()
{
    voiceAllocator_.AllNotesOff();
    activeVoiceCount_ = 0;
    hasPendingFX_ = false;  // Stop tracker FX processing
    lastArpNote_ = -1;  // Reset arpeggio tracking
}

void PlaitsInstrument::process(float* outL, float* outR, int numSamples)
{
    // Process tracker FX with callbacks
    if (hasPendingFX_) {
        auto onNoteOn = [this](int note, float velocity) {
            // For arpeggio, release previous note to keep it monophonic
            if (lastArpNote_ >= 0 && lastArpNote_ != note) {
                voiceAllocator_.NoteOff(lastArpNote_);
            }
            lastArpNote_ = note;

            float attackMs = mapAttack(attack_);
            float decayMs = mapDecay(decay_);
            voiceAllocator_.NoteOn(note, velocity, attackMs, decayMs);

            // Trigger modulation envelopes on first note
            int newActiveCount = voiceAllocator_.activeVoiceCount();
            if (activeVoiceCount_ == 0 && newActiveCount > 0) {
                modMatrix_.TriggerEnvelopes();
            }
            activeVoiceCount_ = newActiveCount;
        };

        auto onNoteOff = [this]() {
            // Only release the last triggered note, not all voices
            // This allows chords (multiple tracks with same instrument) to work
            if (lastArpNote_ >= 0) {
                voiceAllocator_.NoteOff(lastArpNote_);
                activeVoiceCount_ = voiceAllocator_.activeVoiceCount();
            }
            lastArpNote_ = -1;
        };

        // Process FX timing and modulation
        float currentPitch = trackerFX_.process(numSamples, onNoteOn, onNoteOff);

        // If FX becomes inactive (cut), clear pending flag
        if (currentPitch < 0.0f && !trackerFX_.isActive()) {
            hasPendingFX_ = false;
        }
    }

    // Process in chunks to avoid buffer overflow
    int samplesRemaining = numSamples;
    int offset = 0;

    while (samplesRemaining > 0)
    {
        int blockSize = std::min(samplesRemaining, kMaxBlockSize);

        // Clear temp buffers
        std::fill_n(tempBufferL_.begin(), blockSize, 0.0f);
        std::fill_n(tempBufferR_.begin(), blockSize, 0.0f);

        // Update modulation (once per block)
        updateModulationParams();
        modMatrix_.Process(static_cast<float>(sampleRate_), blockSize);
        applyModulation();

        // Render voices
        voiceAllocator_.Process(tempBufferL_.data(), tempBufferR_.data(), blockSize);

        // Apply filter if cutoff < 1.0 or resonance > 0
        if (cutoff_ < 0.99f || resonance_ > 0.01f)
        {
            float modCutoff = getModulatedCutoff();
            float modRes = getModulatedResonance();

            filter_.SetCutoff(mapCutoff(modCutoff));
            filter_.SetResonance(modRes);

            for (int i = 0; i < blockSize; ++i)
            {
                // Mono filter for now (sum to mono, filter, then back to stereo)
                float mono = (tempBufferL_[i] + tempBufferR_[i]) * 0.5f;
                float filtered = filter_.Process(mono);
                tempBufferL_[i] = filtered;
                tempBufferR_[i] = filtered;
            }
        }

        // Copy to output
        for (int i = 0; i < blockSize; ++i)
        {
            outL[offset + i] += tempBufferL_[i];
            outR[offset + i] += tempBufferR_[i];
        }

        offset += blockSize;
        samplesRemaining -= blockSize;
    }

    activeVoiceCount_ = voiceAllocator_.activeVoiceCount();
}

void PlaitsInstrument::updateModulationParams()
{
    // Update LFO1
    modMatrix_.GetLfo1().SetRate(static_cast<plaits::LfoRateDivision>(lfo1Rate_));
    modMatrix_.GetLfo1().SetShape(static_cast<plaits::LfoShape>(lfo1Shape_));
    modMatrix_.SetDestination(plaits::ModSource::Lfo1,
                              static_cast<plaits::ModDestination>(lfo1Dest_));
    modMatrix_.SetAmount(plaits::ModSource::Lfo1, static_cast<int8_t>(lfo1Amount_));

    // Update LFO2
    modMatrix_.GetLfo2().SetRate(static_cast<plaits::LfoRateDivision>(lfo2Rate_));
    modMatrix_.GetLfo2().SetShape(static_cast<plaits::LfoShape>(lfo2Shape_));
    modMatrix_.SetDestination(plaits::ModSource::Lfo2,
                              static_cast<plaits::ModDestination>(lfo2Dest_));
    modMatrix_.SetAmount(plaits::ModSource::Lfo2, static_cast<int8_t>(lfo2Amount_));

    // Update ENV1
    modMatrix_.GetEnv1().SetAttack(static_cast<uint16_t>(mapAttack(env1Attack_)));
    modMatrix_.GetEnv1().SetDecay(static_cast<uint16_t>(mapDecay(env1Decay_)));
    modMatrix_.SetDestination(plaits::ModSource::Env1,
                              static_cast<plaits::ModDestination>(env1Dest_));
    modMatrix_.SetAmount(plaits::ModSource::Env1, static_cast<int8_t>(env1Amount_));

    // Update ENV2
    modMatrix_.GetEnv2().SetAttack(static_cast<uint16_t>(mapAttack(env2Attack_)));
    modMatrix_.GetEnv2().SetDecay(static_cast<uint16_t>(mapDecay(env2Decay_)));
    modMatrix_.SetDestination(plaits::ModSource::Env2,
                              static_cast<plaits::ModDestination>(env2Dest_));
    modMatrix_.SetAmount(plaits::ModSource::Env2, static_cast<int8_t>(env2Amount_));

    modMatrix_.SetTempo(tempo_);
}

void PlaitsInstrument::applyModulation()
{
    // Apply modulated values to voice allocator
    voiceAllocator_.set_harmonics(getModulatedHarmonics());
    voiceAllocator_.set_timbre(getModulatedTimbre());
    voiceAllocator_.set_morph(getModulatedMorph());
}

float PlaitsInstrument::getModulatedHarmonics() const
{
    return modMatrix_.GetModulatedValue(plaits::ModDestination::Harmonics, harmonics_);
}

float PlaitsInstrument::getModulatedTimbre() const
{
    return modMatrix_.GetModulatedValue(plaits::ModDestination::Timbre, timbre_);
}

float PlaitsInstrument::getModulatedMorph() const
{
    return modMatrix_.GetModulatedValue(plaits::ModDestination::Morph, morph_);
}

float PlaitsInstrument::getModulatedCutoff() const
{
    return modMatrix_.GetModulatedValue(plaits::ModDestination::Cutoff, cutoff_);
}

float PlaitsInstrument::getModulatedResonance() const
{
    return modMatrix_.GetModulatedValue(plaits::ModDestination::Resonance, resonance_);
}

const char* PlaitsInstrument::getEngineName(int engine)
{
    if (engine >= 0 && engine < 16)
        return kEngineNames[engine];
    return "Unknown";
}

void PlaitsInstrument::setTempo(double bpm)
{
    tempo_ = bpm;
    modMatrix_.SetTempo(bpm);
    trackerFX_.setTempo(static_cast<float>(bpm));
}

const char* PlaitsInstrument::getParameterName(int index) const
{
    if (index >= 0 && index < kNumParams)
        return kParamNames[index];
    return "";
}

float PlaitsInstrument::getParameter(int index) const
{
    switch (index)
    {
        case kParamEngine:      return static_cast<float>(engine_) / 15.0f;
        case kParamHarmonics:   return harmonics_;
        case kParamTimbre:      return timbre_;
        case kParamMorph:       return morph_;
        case kParamAttack:      return attack_;
        case kParamDecay:       return decay_;
        case kParamPolyphony:   return static_cast<float>(polyphony_ - 1) / 15.0f;
        case kParamCutoff:      return cutoff_;
        case kParamResonance:   return resonance_;
        case kParamLfo1Rate:    return static_cast<float>(lfo1Rate_) / 9.0f;
        case kParamLfo1Shape:   return static_cast<float>(lfo1Shape_) / 3.0f;
        case kParamLfo1Dest:    return static_cast<float>(lfo1Dest_) / 9.0f;
        case kParamLfo1Amount:  return static_cast<float>(lfo1Amount_ + 64) / 127.0f;
        case kParamLfo2Rate:    return static_cast<float>(lfo2Rate_) / 9.0f;
        case kParamLfo2Shape:   return static_cast<float>(lfo2Shape_) / 3.0f;
        case kParamLfo2Dest:    return static_cast<float>(lfo2Dest_) / 9.0f;
        case kParamLfo2Amount:  return static_cast<float>(lfo2Amount_ + 64) / 127.0f;
        case kParamEnv1Attack:  return env1Attack_;
        case kParamEnv1Decay:   return env1Decay_;
        case kParamEnv1Dest:    return static_cast<float>(env1Dest_) / 9.0f;
        case kParamEnv1Amount:  return static_cast<float>(env1Amount_ + 64) / 127.0f;
        case kParamEnv2Attack:  return env2Attack_;
        case kParamEnv2Decay:   return env2Decay_;
        case kParamEnv2Dest:    return static_cast<float>(env2Dest_) / 9.0f;
        case kParamEnv2Amount:  return static_cast<float>(env2Amount_ + 64) / 127.0f;
        default: return 0.0f;
    }
}

void PlaitsInstrument::setParameter(int index, float value)
{
    value = std::clamp(value, 0.0f, 1.0f);

    switch (index)
    {
        case kParamEngine:
            engine_ = static_cast<int>(value * 15.0f + 0.5f);
            voiceAllocator_.set_engine(engine_);
            break;
        case kParamHarmonics:
            harmonics_ = value;
            break;
        case kParamTimbre:
            timbre_ = value;
            break;
        case kParamMorph:
            morph_ = value;
            break;
        case kParamAttack:
            attack_ = value;
            break;
        case kParamDecay:
            decay_ = value;
            voiceAllocator_.set_decay(value);
            break;
        case kParamPolyphony:
            polyphony_ = 1 + static_cast<int>(value * 15.0f + 0.5f);
            voiceAllocator_.setPolyphony(polyphony_);
            break;
        case kParamCutoff:
            cutoff_ = value;
            break;
        case kParamResonance:
            resonance_ = value;
            break;
        case kParamLfo1Rate:
            lfo1Rate_ = static_cast<int>(value * 9.0f + 0.5f);
            break;
        case kParamLfo1Shape:
            lfo1Shape_ = static_cast<int>(value * 3.0f + 0.5f);
            break;
        case kParamLfo1Dest:
            lfo1Dest_ = static_cast<int>(value * 9.0f + 0.5f);
            break;
        case kParamLfo1Amount:
            lfo1Amount_ = static_cast<int>(value * 127.0f + 0.5f) - 64;
            break;
        case kParamLfo2Rate:
            lfo2Rate_ = static_cast<int>(value * 9.0f + 0.5f);
            break;
        case kParamLfo2Shape:
            lfo2Shape_ = static_cast<int>(value * 3.0f + 0.5f);
            break;
        case kParamLfo2Dest:
            lfo2Dest_ = static_cast<int>(value * 9.0f + 0.5f);
            break;
        case kParamLfo2Amount:
            lfo2Amount_ = static_cast<int>(value * 127.0f + 0.5f) - 64;
            break;
        case kParamEnv1Attack:
            env1Attack_ = value;
            break;
        case kParamEnv1Decay:
            env1Decay_ = value;
            break;
        case kParamEnv1Dest:
            env1Dest_ = static_cast<int>(value * 9.0f + 0.5f);
            break;
        case kParamEnv1Amount:
            env1Amount_ = static_cast<int>(value * 127.0f + 0.5f) - 64;
            break;
        case kParamEnv2Attack:
            env2Attack_ = value;
            break;
        case kParamEnv2Decay:
            env2Decay_ = value;
            break;
        case kParamEnv2Dest:
            env2Dest_ = static_cast<int>(value * 9.0f + 0.5f);
            break;
        case kParamEnv2Amount:
            env2Amount_ = static_cast<int>(value * 127.0f + 0.5f) - 64;
            break;
    }
}

// State structure for serialization
struct PlaitsState {
    int engine;
    float harmonics;
    float timbre;
    float morph;
    float attack;
    float decay;
    int polyphony;
    float cutoff;
    float resonance;
    int lfo1Rate;
    int lfo1Shape;
    int lfo1Dest;
    int lfo1Amount;
    int lfo2Rate;
    int lfo2Shape;
    int lfo2Dest;
    int lfo2Amount;
    float env1Attack;
    float env1Decay;
    int env1Dest;
    int env1Amount;
    float env2Attack;
    float env2Decay;
    int env2Dest;
    int env2Amount;
};

void PlaitsInstrument::getState(void* data, int maxSize) const
{
    if (maxSize < static_cast<int>(sizeof(PlaitsState)))
        return;

    PlaitsState* state = static_cast<PlaitsState*>(data);
    state->engine = engine_;
    state->harmonics = harmonics_;
    state->timbre = timbre_;
    state->morph = morph_;
    state->attack = attack_;
    state->decay = decay_;
    state->polyphony = polyphony_;
    state->cutoff = cutoff_;
    state->resonance = resonance_;
    state->lfo1Rate = lfo1Rate_;
    state->lfo1Shape = lfo1Shape_;
    state->lfo1Dest = lfo1Dest_;
    state->lfo1Amount = lfo1Amount_;
    state->lfo2Rate = lfo2Rate_;
    state->lfo2Shape = lfo2Shape_;
    state->lfo2Dest = lfo2Dest_;
    state->lfo2Amount = lfo2Amount_;
    state->env1Attack = env1Attack_;
    state->env1Decay = env1Decay_;
    state->env1Dest = env1Dest_;
    state->env1Amount = env1Amount_;
    state->env2Attack = env2Attack_;
    state->env2Decay = env2Decay_;
    state->env2Dest = env2Dest_;
    state->env2Amount = env2Amount_;
}

void PlaitsInstrument::setState(const void* data, int size)
{
    if (size < static_cast<int>(sizeof(PlaitsState)))
        return;

    const PlaitsState* state = static_cast<const PlaitsState*>(data);
    engine_ = state->engine;
    harmonics_ = state->harmonics;
    timbre_ = state->timbre;
    morph_ = state->morph;
    attack_ = state->attack;
    decay_ = state->decay;
    polyphony_ = state->polyphony;
    cutoff_ = state->cutoff;
    resonance_ = state->resonance;
    lfo1Rate_ = state->lfo1Rate;
    lfo1Shape_ = state->lfo1Shape;
    lfo1Dest_ = state->lfo1Dest;
    lfo1Amount_ = state->lfo1Amount;
    lfo2Rate_ = state->lfo2Rate;
    lfo2Shape_ = state->lfo2Shape;
    lfo2Dest_ = state->lfo2Dest;
    lfo2Amount_ = state->lfo2Amount;
    env1Attack_ = state->env1Attack;
    env1Decay_ = state->env1Decay;
    env1Dest_ = state->env1Dest;
    env1Amount_ = state->env1Amount;
    env2Attack_ = state->env2Attack;
    env2Decay_ = state->env2Decay;
    env2Dest_ = state->env2Dest;
    env2Amount_ = state->env2Amount;

    // Apply to voice allocator
    voiceAllocator_.set_engine(engine_);
    voiceAllocator_.set_harmonics(harmonics_);
    voiceAllocator_.set_timbre(timbre_);
    voiceAllocator_.set_morph(morph_);
    voiceAllocator_.set_decay(decay_);
    voiceAllocator_.setPolyphony(polyphony_);
}

} // namespace audio
