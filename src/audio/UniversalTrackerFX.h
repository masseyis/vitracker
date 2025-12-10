#pragma once

#include "../model/Step.h"
#include <cstdint>
#include <functional>
#include <cmath>

namespace audio {

// Universal tracker FX system that works for ALL instrument types
// Handles both timing-based effects (DLY, RET, CUT, OFF) and
// continuous effects (ARP, POR, VIB) at the instrument level
class UniversalTrackerFX
{
public:
    static constexpr int TICKS_PER_ROW = 6;

    // Callbacks for instrument control
    using NoteCallback = std::function<void(int note, float velocity)>;
    using ReleaseCallback = std::function<void()>;

    void setSampleRate(double sampleRate) {
        sampleRate_ = sampleRate;
        updateTickLength();
    }

    void setTempo(float bpm) {
        tempo_ = bpm;
        updateTickLength();
    }

    // Called when a new note triggers with FX commands
    void triggerNote(int note, float velocity, const model::Step& step) {
        baseNote_ = note;
        currentNote_ = note;
        currentVelocity_ = velocity;
        tickCounter_ = 0;
        sampleCounter_ = 0;
        active_ = false;  // Will be activated after delay

        // Reset FX state
        noteDelay_ = 0;
        retriggerInterval_ = 0;
        retriggerCounter_ = 0;
        cutTick_ = -1;
        offTick_ = -1;

        // Arpeggio
        arpNotes_[0] = 0;
        arpNotes_[1] = 0;
        arpNotes_[2] = 0;
        arpIndex_ = 0;

        // Portamento
        portamentoSpeed_ = 0;
        portamentoTarget_ = note;
        currentPitch_ = static_cast<float>(note);

        // Vibrato
        vibratoSpeed_ = 0;
        vibratoDepth_ = 0;
        vibratoPhase_ = 0.0f;

        // Parse FX commands
        for (int i = 0; i < 3; ++i) {
            const auto* fx = (i == 0) ? &step.fx1 : (i == 1) ? &step.fx2 : &step.fx3;
            if (fx->type == model::FXType::None) continue;

            switch (fx->type) {
                case model::FXType::ARP:
                    arpNotes_[0] = 0;
                    arpNotes_[1] = (fx->value >> 4) & 0x0F;
                    arpNotes_[2] = fx->value & 0x0F;
                    break;

                case model::FXType::POR:
                    portamentoSpeed_ = fx->value;
                    break;

                case model::FXType::VIB:
                    vibratoSpeed_ = (fx->value >> 4) & 0x0F;
                    vibratoDepth_ = fx->value & 0x0F;
                    break;

                case model::FXType::DLY:
                    noteDelay_ = fx->value;
                    break;

                case model::FXType::RET:
                    retriggerInterval_ = fx->value > 0 ? fx->value : 1;
                    break;

                case model::FXType::CUT:
                    cutTick_ = fx->value;
                    break;

                case model::FXType::OFF:
                    offTick_ = fx->value;
                    break;

                default:
                    break;
            }
        }

        // If no delay, activate immediately
        if (noteDelay_ == 0) {
            active_ = true;
        }
    }

    // Process timing and modulation for one audio block
    // Returns: current pitch (MIDI note + fractional cents)
    float process(int numSamples, NoteCallback onNoteOn, ReleaseCallback onNoteOff) {
        for (int i = 0; i < numSamples; ++i) {
            sampleCounter_++;

            // Check if we've advanced a tick
            if (sampleCounter_ >= samplesPerTick_) {
                sampleCounter_ -= samplesPerTick_;
                tickCounter_++;

                // Note delay - activate note after delay
                if (!active_ && noteDelay_ > 0 && tickCounter_ >= noteDelay_) {
                    active_ = true;
                    if (onNoteOn) {
                        onNoteOn(currentNote_, currentVelocity_);
                    }
                }

                // Arpeggio - cycle through notes every 2 ticks (starting from tick 2)
                // This gives a more musical arpeggio speed at typical BPMs
                if (active_ && tickCounter_ > 0 && tickCounter_ % 2 == 0 && (arpNotes_[1] != 0 || arpNotes_[2] != 0)) {
                    arpIndex_ = (arpIndex_ + 1) % 3;
                    currentNote_ = baseNote_ + arpNotes_[arpIndex_];
                    // Always trigger on arpeggio tick, even if same note
                    // (we released the previous note, so need to retrigger)
                    if (onNoteOn) {
                        onNoteOn(currentNote_, currentVelocity_);
                    }
                }

                // Retrigger
                if (active_ && retriggerInterval_ > 0 &&
                    tickCounter_ % retriggerInterval_ == 0 && tickCounter_ > 0) {
                    if (onNoteOn) {
                        onNoteOn(currentNote_, currentVelocity_);
                    }
                }

                // Note cut
                if (active_ && cutTick_ >= 0 && tickCounter_ >= cutTick_) {
                    active_ = false;
                    if (onNoteOff) {
                        onNoteOff();
                    }
                    return -1.0f;  // Signal cut
                }

                // Note off
                if (active_ && offTick_ >= 0 && tickCounter_ >= offTick_) {
                    if (onNoteOff) {
                        onNoteOff();
                    }
                }
            }

            // Continuous modulation (per-sample)
            if (active_) {
                // Portamento - smooth pitch glide
                if (portamentoSpeed_ > 0) {
                    float glideSpeed = portamentoSpeed_ / 255.0f * 0.1f;  // Scale to reasonable rate
                    if (currentPitch_ < portamentoTarget_) {
                        currentPitch_ = std::min(currentPitch_ + glideSpeed, static_cast<float>(portamentoTarget_));
                    } else if (currentPitch_ > portamentoTarget_) {
                        currentPitch_ = std::max(currentPitch_ - glideSpeed, static_cast<float>(portamentoTarget_));
                    }
                }

                // Vibrato - LFO pitch modulation
                if (vibratoSpeed_ > 0 && vibratoDepth_ > 0) {
                    float lfoRate = vibratoSpeed_ / 16.0f * 8.0f;  // 0-8 Hz
                    vibratoPhase_ += (lfoRate * 2.0f * M_PI) / static_cast<float>(sampleRate_);
                    if (vibratoPhase_ > 2.0f * M_PI) vibratoPhase_ -= 2.0f * M_PI;

                    float vibrato = std::sin(vibratoPhase_) * (vibratoDepth_ / 16.0f);  // ±1 semitone max
                    return currentPitch_ + vibrato;
                }
            }
        }

        return active_ ? currentPitch_ : -1.0f;
    }

    bool isActive() const { return active_; }
    int getCurrentNote() const { return currentNote_; }

private:
    void updateTickLength() {
        if (sampleRate_ > 0 && tempo_ > 0) {
            double rowsPerSecond = (tempo_ / 60.0) * 4.0;  // 4 rows per beat
            double secondsPerRow = 1.0 / rowsPerSecond;
            double secondsPerTick = secondsPerRow / TICKS_PER_ROW;
            samplesPerTick_ = secondsPerTick * sampleRate_;
        }
    }

    double sampleRate_ = 48000.0;
    float tempo_ = 120.0f;
    double samplesPerTick_ = 4000.0;

    int baseNote_ = 60;
    int currentNote_ = 60;
    float currentVelocity_ = 1.0f;
    bool active_ = false;

    int tickCounter_ = 0;
    int sampleCounter_ = 0;

    // Timing FX
    int noteDelay_ = 0;
    int retriggerInterval_ = 0;
    int retriggerCounter_ = 0;
    int cutTick_ = -1;
    int offTick_ = -1;

    // Arpeggio
    int arpNotes_[3] = {0, 0, 0};
    int arpIndex_ = 0;

    // Portamento
    int portamentoSpeed_ = 0;
    int portamentoTarget_ = 60;
    float currentPitch_ = 60.0f;

    // Vibrato
    int vibratoSpeed_ = 0;
    int vibratoDepth_ = 0;
    float vibratoPhase_ = 0.0f;
};

} // namespace audio
