#include "DX7Instrument.h"
#include <algorithm>
#include <cmath>
#include <iostream>

// Debug logging for DX7
#define DX7_DEBUG 0
#if DX7_DEBUG
#define DX7_LOG(msg) std::cerr << "[DX7] " << msg << std::endl
#else
#define DX7_LOG(msg)
#endif

namespace audio {

// DX7 init patch - a simple sine wave (algorithm 32, single carrier)
// Structure: 6 ops × 21 bytes + 8 pitch EG + 9 global + 10 name = 156 bytes
// Per-operator format (per dx7note.cc):
//   [0-3]  R1-R4 (EG rates)
//   [4-7]  L1-L4 (EG levels)
//   [8]    BP (breakpoint)
//   [9]    LD (left depth)
//   [10]   RD (right depth)
//   [11]   LC (left curve)
//   [12]   RC (right curve)
//   [13]   RS (rate scaling)
//   [14]   AMS (amp mod sens)
//   [15]   KVS (key velocity sens)
//   [16]   OL (output level) <-- CRITICAL FOR SOUND!
//   [17]   PM (osc mode: 0=ratio, 1=fixed)
//   [18]   FC (freq coarse)
//   [19]   FF (freq fine)
//   [20]   DET (detune)
static const uint8_t initPatch[DX7_PATCH_SIZE_UNPACKED] = {
    // Operator 6 (carrier in algo 32) - OL=99 for sound!
    // R1   R2   R3   R4   L1   L2   L3   L4   BP   LD   RD   LC   RC   RS  AMS  KVS   OL   PM   FC   FF  DET
    99,  99,  99,  99,  99,  99,  99,   0,  39,   0,   0,   0,   0,   0,   0,   0,  99,   0,   1,   0,   7,
    // Operators 5-1 (silent: OL=0) - 21 bytes each
    99,  99,  99,  99,  99,  99,  99,   0,  39,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   0,   7,
    99,  99,  99,  99,  99,  99,  99,   0,  39,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   0,   7,
    99,  99,  99,  99,  99,  99,  99,   0,  39,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   0,   7,
    99,  99,  99,  99,  99,  99,  99,   0,  39,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   0,   7,
    99,  99,  99,  99,  99,  99,  99,   0,  39,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   0,   7,

    // Pitch EG (8 bytes: PR1-4, PL1-4)
    50, 50, 50, 50, 50, 50, 50, 50,

    // Algorithm, Feedback, OSC sync (3 bytes)
    31, 0, 1,

    // LFO (6 bytes: speed, delay, PMD, AMD, sync, wave)
    35, 0, 0, 0, 0, 1,

    // PMS, Transpose (2 bytes)
    0, 24,

    // Patch name (10 bytes)
    'I', 'N', 'I', 'T', ' ', 'V', 'O', 'I', 'C', 'E'
};

DX7Instrument::DX7Instrument()
{
    tuning_ = createStandardTuning();
    std::memcpy(currentPatch_, initPatch, DX7_PATCH_SIZE_UNPACKED);
    std::memcpy(patchName_, "INIT VOICE", 10);
    patchName_[10] = '\0';

    // Initialize Controllers - CRITICAL for sound generation!
    // Clear all MIDI controller values
    std::memset(controllers_.values_, 0, sizeof(controllers_.values_));

    // Set pitch bend to center (0x2000 = 8192)
    controllers_.values_[kControllerPitch] = 0x2000;

    // Set default pitch bend range (2 semitones up and down)
    controllers_.values_[kControllerPitchRangeUp] = 2;
    controllers_.values_[kControllerPitchRangeDn] = 2;
    controllers_.values_[kControllerPitchStep] = 0;

    // Initialize other controller values
    controllers_.masterTune = 0;
    controllers_.modwheel_cc = 0;
    controllers_.breath_cc = 0;
    controllers_.foot_cc = 0;
    controllers_.aftertouch_cc = 0;

    // Portamento settings
    controllers_.portamento_enable_cc = false;
    controllers_.portamento_cc = 0;
    controllers_.portamento_gliss_cc = false;

    // Set FmCore pointer
    controllers_.core = &fmCore_;
}

DX7Instrument::~DX7Instrument() = default;

void DX7Instrument::init(double sampleRate)
{
    DX7_LOG("init() called with sampleRate=" << sampleRate);
    sampleRate_ = sampleRate;

    // Initialize static sample rate for msfa components
    Env::init_sr(sampleRate);
    Lfo::init(sampleRate);

    // Initialize lookup tables (critical for sound generation!)
    Freqlut::init(sampleRate);
    Exp2::init();
    Tanh::init();
    Sin::init();  // CRITICAL: Initialize sine table for FM synthesis!
    DX7_LOG("Lookup tables initialized (including Sin)");

    // Initialize LFO with default patch
    lfo_.reset(&currentPatch_[137]);  // LFO params at offset 137

    // Pre-create voice objects
    for (auto& voice : voices_) {
        voice.note = std::make_unique<Dx7Note>(tuning_, nullptr);
        voice.midiNote = -1;
        voice.active = false;
        voice.age = 0;
    }

    // Initialize tracker FX
    trackerFX_.setSampleRate(sampleRate);
    trackerFX_.setTempo(120.0f);  // Default tempo

    DX7_LOG("init() complete - " << DX7_MAX_POLYPHONY << " voices created");
}

void DX7Instrument::setSampleRate(double sampleRate)
{
    if (sampleRate_ != sampleRate) {
        sampleRate_ = sampleRate;
        Env::init_sr(sampleRate);
        Lfo::init(sampleRate);
        trackerFX_.setSampleRate(sampleRate);
    }
}

int DX7Instrument::findFreeVoice()
{
    // First, look for an inactive voice
    for (int i = 0; i < DX7_MAX_POLYPHONY; ++i) {
        if (!voices_[i].active) {
            return i;
        }
    }

    // No free voice, steal the oldest one
    uint64_t oldest = UINT64_MAX;
    int oldestIdx = 0;
    for (int i = 0; i < DX7_MAX_POLYPHONY; ++i) {
        if (voices_[i].age < oldest) {
            oldest = voices_[i].age;
            oldestIdx = i;
        }
    }

    // Release the stolen voice
    if (voices_[oldestIdx].note) {
        voices_[oldestIdx].note->keyup();
    }

    return oldestIdx;
}

int DX7Instrument::findVoiceForNote(int note)
{
    for (int i = 0; i < DX7_MAX_POLYPHONY; ++i) {
        if (voices_[i].active && voices_[i].midiNote == note) {
            return i;
        }
    }
    return -1;
}

void DX7Instrument::noteOn(int note, float velocity)
{
    model::Step emptyStep;
    noteOnWithFX(note, velocity, emptyStep);
}

void DX7Instrument::noteOnWithFX(int note, float velocity, const model::Step& step)
{
    DX7_LOG("noteOnWithFX() called: note=" << note << ", velocity=" << velocity);

    // Trigger UniversalTrackerFX
    trackerFX_.triggerNote(note, velocity, step);
    hasPendingFX_ = true;
    lastArpNote_ = note;  // Initialize tracking with base note

    // Check if any FX has a delay
    bool hasDelay = false;
    for (int i = 0; i < 3; ++i) {
        const auto* fx = (i == 0) ? &step.fx1 : (i == 1) ? &step.fx2 : &step.fx3;
        if (fx->type == model::FXType::DLY) {
            hasDelay = true;
            break;
        }
    }

    // If no delay, trigger note immediately
    // Otherwise, the process() method will trigger it after the delay
    if (!hasDelay) {
        int velocityInt = static_cast<int>(velocity * 127.0f);
        velocityInt = std::clamp(velocityInt, 0, 127);
        DX7_LOG("  velocityInt=" << velocityInt);

        int voiceIdx = findFreeVoice();
        Voice& voice = voices_[voiceIdx];
        DX7_LOG("  using voice " << voiceIdx);

        // Guard against uninitialized voices
        if (!voice.note) {
            DX7_LOG("  ERROR: voice.note is null!");
            return;
        }

        // Dump patch data for debugging
        int algo = currentPatch_[134];
        int feedback = currentPatch_[135];
        DX7_LOG("  patch algorithm=" << algo << " feedback=" << feedback);

        // Dump all 6 operators' output levels and EG data
        for (int op = 0; op < 6; ++op) {
            int off = op * 21;
            DX7_LOG("  Op" << (6-op) << ": OL=" << (int)currentPatch_[off+16]
                    << " R1-4=" << (int)currentPatch_[off+0] << "," << (int)currentPatch_[off+1]
                    << "," << (int)currentPatch_[off+2] << "," << (int)currentPatch_[off+3]
                    << " L1-4=" << (int)currentPatch_[off+4] << "," << (int)currentPatch_[off+5]
                    << "," << (int)currentPatch_[off+6] << "," << (int)currentPatch_[off+7]
                    << " mode=" << (int)currentPatch_[off+17] << " coarse=" << (int)currentPatch_[off+18]
                    << " fine=" << (int)currentPatch_[off+19]);
        }

        // Initialize the voice with current patch
        DX7_LOG("  initializing voice with patch (algo=" << algo << ")");
        voice.note->init(currentPatch_, note, velocityInt, 0, &controllers_);
        voice.midiNote = note;
        voice.active = true;
        voice.age = voiceCounter_++;

        // Trigger LFO only on first note (when no other voices are playing)
        // The DX7 has a global LFO shared across all voices
        int activeVoices = 0;
        for (const auto& v : voices_) {
            if (v.active) activeVoices++;
        }
        if (activeVoices == 1) {  // Only this new voice is active
            lfo_.keydown();
            DX7_LOG("  LFO triggered (first voice)");
        }
        DX7_LOG("  noteOn complete, voice active");
    }
}

void DX7Instrument::noteOff(int note)
{
    int voiceIdx = findVoiceForNote(note);
    if (voiceIdx >= 0 && voices_[voiceIdx].note) {
        voices_[voiceIdx].note->keyup();
    }
}

void DX7Instrument::allNotesOff()
{
    for (auto& voice : voices_) {
        if (voice.active && voice.note) {
            voice.note->keyup();
        }
    }
}

void DX7Instrument::process(float* outL, float* outR, int numSamples)
{
    static int processCallCount = 0;
    static int activeVoiceCount = 0;
    static float maxSampleEver = 0.0f;

    // Process tracker FX with callbacks
    if (hasPendingFX_) {
        auto onNoteOn = [this](int note, float velocity) {
            // For arpeggio, release previous note to keep it monophonic
            if (lastArpNote_ >= 0 && lastArpNote_ != note) {
                noteOff(lastArpNote_);
            }
            lastArpNote_ = note;

            int velocityInt = static_cast<int>(velocity * 127.0f);
            velocityInt = std::clamp(velocityInt, 0, 127);

            int voiceIdx = findFreeVoice();
            Voice& voice = voices_[voiceIdx];

            if (!voice.note) {
                DX7_LOG("  ERROR: voice.note is null in callback!");
                return;
            }

            voice.note->init(currentPatch_, note, velocityInt, 0, &controllers_);
            voice.midiNote = note;
            voice.active = true;
            voice.age = voiceCounter_++;

            // Trigger LFO on first note
            int activeVoices = 0;
            for (const auto& v : voices_) {
                if (v.active) activeVoices++;
            }
            if (activeVoices == 1) {
                lfo_.keydown();
            }
        };

        auto onNoteOff = [this]() {
            allNotesOff();
            lastArpNote_ = -1;  // Reset tracking
        };

        // Process FX timing and modulation
        float currentPitch = trackerFX_.process(numSamples, onNoteOn, onNoteOff);

        // If FX becomes inactive (cut), clear pending flag
        if (currentPitch < 0.0f && !trackerFX_.isActive()) {
            hasPendingFX_ = false;
        }
    }

    // Process in N-sample blocks (N = 64 in msfa)
    constexpr int blockSize = N;
    int32_t buf[blockSize];

    // Count active voices for debugging
    int currentActiveVoices = 0;
    for (const auto& voice : voices_) {
        if (voice.active) currentActiveVoices++;
    }

    if (currentActiveVoices != activeVoiceCount) {
        DX7_LOG("process() active voices changed: " << activeVoiceCount << " -> " << currentActiveVoices);
        activeVoiceCount = currentActiveVoices;
    }

    int samplesProcessed = 0;
    float maxSampleThisCall = 0.0f;
    int32_t maxBufVal = 0;

    while (samplesProcessed < numSamples) {
        int samplesToProcess = std::min(blockSize, numSamples - samplesProcessed);

        // Clear buffer
        std::memset(buf, 0, sizeof(buf));

        // Get LFO values
        int32_t lfoVal = lfo_.getsample();
        int32_t lfoDelay = lfo_.getdelay();

        // Refresh controller modulation
        controllers_.refresh();

        // Render all active voices
        for (auto& voice : voices_) {
            if (voice.active && voice.note) {
                voice.note->compute(buf, lfoVal, lfoDelay, &controllers_);

                // Check if voice has finished
                if (!voice.note->isPlaying()) {
                    DX7_LOG("  voice finished playing (note=" << voice.midiNote << ")");
                    voice.active = false;
                    voice.midiNote = -1;
                }
            }
        }

        // Track max buffer value before conversion
        for (int i = 0; i < samplesToProcess; ++i) {
            if (std::abs(buf[i]) > maxBufVal) {
                maxBufVal = std::abs(buf[i]);
            }
        }

        // Convert int32 to float and copy to output (mono for now)
        // DX7 output is in Q24 format (24-bit fixed point)
        // Scale by max polyphony to prevent distortion when multiple voices play
        // Using sqrt provides a reasonable balance between volume loss and distortion
        constexpr float baseScale = 1.0f / (1 << 24);
        float voiceScale = (activeVoiceCount > 1) ? (1.0f / std::sqrt(static_cast<float>(activeVoiceCount))) : 1.0f;
        float scale = baseScale * voiceScale;
        for (int i = 0; i < samplesToProcess; ++i) {
            float sample = static_cast<float>(buf[i]) * scale;
            // Soft clip to prevent harsh clipping
            sample = std::tanh(sample);
            outL[samplesProcessed + i] = sample;
            outR[samplesProcessed + i] = sample;

            if (std::abs(sample) > maxSampleThisCall) {
                maxSampleThisCall = std::abs(sample);
            }
        }

        samplesProcessed += samplesToProcess;
    }

    // Log periodically when there are active voices
    processCallCount++;
    if (activeVoiceCount > 0 && (processCallCount % 100 == 0)) {
        DX7_LOG("process() #" << processCallCount << ": voices=" << activeVoiceCount
                << " maxBuf=" << maxBufVal << " maxSample=" << maxSampleThisCall);
    }

    if (maxSampleThisCall > maxSampleEver) {
        maxSampleEver = maxSampleThisCall;
        if (maxSampleEver > 0.0001f) {
            DX7_LOG("process() NEW MAX SAMPLE: " << maxSampleEver);
        }
    }
}

void DX7Instrument::loadPatch(const uint8_t* patchData)
{
    DX7_LOG("loadPatch() called");

    std::memcpy(currentPatch_, patchData, DX7_PATCH_SIZE_UNPACKED);

    // Extract patch name (last 10 bytes)
    std::memcpy(patchName_, &patchData[145], 10);
    patchName_[10] = '\0';

    // Clean up patch name (replace non-printable chars with space)
    for (int i = 0; i < 10; ++i) {
        if (patchName_[i] < 32 || patchName_[i] > 126) {
            patchName_[i] = ' ';
        }
    }

    DX7_LOG("  patch name: '" << patchName_ << "'");
    DX7_LOG("  algorithm: " << (int)currentPatch_[134]);
    DX7_LOG("  feedback: " << (int)currentPatch_[135]);

    // Log operator output levels (key for sound!)
    // OL is at offset 16 within each 21-byte operator block
    DX7_LOG("  op output levels (OL at offset 16): "
            << (int)currentPatch_[0*21+16] << ", "   // Op6
            << (int)currentPatch_[1*21+16] << ", "   // Op5
            << (int)currentPatch_[2*21+16] << ", "   // Op4
            << (int)currentPatch_[3*21+16] << ", "   // Op3
            << (int)currentPatch_[4*21+16] << ", "   // Op2
            << (int)currentPatch_[5*21+16]);         // Op1

    // Reset LFO with new patch parameters
    lfo_.reset(&currentPatch_[137]);

    // Update any currently playing voices
    int updatedVoices = 0;
    for (auto& voice : voices_) {
        if (voice.active && voice.note) {
            voice.note->update(currentPatch_, voice.midiNote, 100, 0);
            updatedVoices++;
        }
    }
    DX7_LOG("  updated " << updatedVoices << " active voices");
}

void DX7Instrument::unpackPatch(const uint8_t* packed, uint8_t* unpacked)
{
    // DX7 sysex packed format to unpacked format conversion
    // Based on Dexed's unpacking logic

    // Process 6 operators (packed format has them in reverse order: op6 first)
    for (int op = 0; op < 6; ++op) {
        const uint8_t* src = &packed[op * 17];  // Each packed operator is 17 bytes
        uint8_t* dst = &unpacked[op * 21];      // Each unpacked operator is 21 bytes

        // EG rates (4 bytes, direct copy)
        dst[0] = src[0];   // R1
        dst[1] = src[1];   // R2
        dst[2] = src[2];   // R3
        dst[3] = src[3];   // R4

        // EG levels (4 bytes, direct copy)
        dst[4] = src[4];   // L1
        dst[5] = src[5];   // L2
        dst[6] = src[6];   // L3
        dst[7] = src[7];   // L4

        // Keyboard level scaling
        dst[8] = src[8];                    // BP (breakpoint)
        dst[9] = src[9];                    // LD (left depth)
        dst[10] = src[10];                  // RD (right depth)
        dst[11] = src[11] & 0x03;           // LC (left curve)
        dst[12] = (src[11] >> 2) & 0x03;    // RC (right curve)

        // Rate scaling
        dst[13] = src[12] & 0x07;           // RS (rate scaling)

        // AMS/KVS - dx7note.cc reads AMS from [off+14], KVS from [off+15]
        dst[14] = src[13] & 0x03;           // AMS (amp mod sens)
        dst[15] = (src[13] >> 2) & 0x07;    // KVS (key velocity sens)

        // Output level - dx7note.cc reads OL from [off+16]
        dst[16] = src[14];                  // OL (output level)

        // Frequency - dx7note.cc reads mode from [off+17], coarse from [off+18], fine from [off+19]
        dst[17] = src[15] & 0x01;           // PM (osc mode: 0=ratio, 1=fixed)
        dst[18] = (src[15] >> 1) & 0x1F;    // FC (freq coarse)
        dst[19] = src[16];                  // FF (freq fine)

        // Detune - dx7note.cc reads detune from [off+20]
        dst[20] = (src[12] >> 3) & 0x0F;    // DET (detune, 0-14)
    }

    // Pitch EG (8 bytes starting at packed offset 102)
    const uint8_t* pitchSrc = &packed[102];
    uint8_t* pitchDst = &unpacked[126];
    for (int i = 0; i < 8; ++i) {
        pitchDst[i] = pitchSrc[i];
    }

    // Algorithm, feedback, osc sync (packed byte 110)
    unpacked[134] = packed[110] & 0x1F;           // Algorithm (0-31)
    unpacked[135] = packed[111] & 0x07;           // Feedback (0-7)
    unpacked[136] = (packed[111] >> 3) & 0x01;    // OSC key sync

    // LFO (6 bytes starting at packed offset 112)
    const uint8_t* lfoSrc = &packed[112];
    uint8_t* lfoDst = &unpacked[137];
    lfoDst[0] = lfoSrc[0];                        // LFO speed
    lfoDst[1] = lfoSrc[1];                        // LFO delay
    lfoDst[2] = lfoSrc[2];                        // LFO PMD
    lfoDst[3] = lfoSrc[3];                        // LFO AMD
    lfoDst[4] = lfoSrc[4] & 0x01;                 // LFO sync
    lfoDst[5] = (lfoSrc[4] >> 1) & 0x07;          // LFO wave

    // PMS
    unpacked[143] = (packed[116] >> 4) & 0x07;    // PMS (pitch mod sens)

    // Transpose
    unpacked[144] = packed[117];                   // Transpose

    // Patch name (10 bytes starting at packed offset 118)
    for (int i = 0; i < 10; ++i) {
        unpacked[145 + i] = packed[118 + i];
    }
}

void DX7Instrument::loadPackedPatch(const uint8_t* packedData)
{
    DX7_LOG("loadPackedPatch() called");
    uint8_t unpacked[DX7_PATCH_SIZE_UNPACKED];
    unpackPatch(packedData, unpacked);
    loadPatch(unpacked);
}

const char* DX7Instrument::getPatchName() const
{
    return patchName_;
}

void DX7Instrument::setPolyphony(int voices)
{
    polyphony_ = std::clamp(voices, 1, DX7_MAX_POLYPHONY);
}

void DX7Instrument::setTempo(double bpm)
{
    trackerFX_.setTempo(static_cast<float>(bpm));
}

void DX7Instrument::getState(void* data, int maxSize) const
{
    if (maxSize >= DX7_PATCH_SIZE_UNPACKED) {
        std::memcpy(data, currentPatch_, DX7_PATCH_SIZE_UNPACKED);
    }
}

void DX7Instrument::setState(const void* data, int size)
{
    if (size >= DX7_PATCH_SIZE_UNPACKED) {
        loadPatch(static_cast<const uint8_t*>(data));
    } else if (size >= DX7_PATCH_SIZE_PACKED) {
        loadPackedPatch(static_cast<const uint8_t*>(data));
    }
}

} // namespace audio
