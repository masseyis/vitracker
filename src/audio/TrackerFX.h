#pragma once

#include "../model/Step.h"
#include <cstdint>
#include <array>
#include <cmath>

namespace audio {

// Tracker FX processor - handles tick-based effects
// Ticks are subdivisions of a row (typically 6 ticks per row)
class TrackerFX
{
public:
    static constexpr int TICKS_PER_ROW = 6;

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
        currentNote_ = note;
        currentVelocity_ = velocity;
        tickCounter_ = 0;
        sampleCounter_ = 0;

        // Reset FX state
        arpNotes_[0] = 0;
        arpNotes_[1] = 0;
        arpNotes_[2] = 0;
        arpIndex_ = 0;

        noteDelay_ = 0;
        retriggerInterval_ = 0;
        retriggerCounter_ = 0;
        cutTick_ = -1;
        offTick_ = -1;

        // Process FX commands from step
        for (int i = 0; i < 3; ++i) {
            const auto* fx = (i == 0) ? &step.fx1 : (i == 1) ? &step.fx2 : &step.fx3;
            if (fx->type == model::FXType::None) continue;

            switch (fx->type) {
                case model::FXType::ARP:
                    arpNotes_[0] = 0;
                    arpNotes_[1] = (fx->value >> 4) & 0x0F;
                    arpNotes_[2] = fx->value & 0x0F;
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
    }

    // Process one sample - returns true if note should be active
    bool processSample() {
        sampleCounter_++;

        // Check if we've advanced a tick
        if (sampleCounter_ >= samplesPerTick_) {
            sampleCounter_ -= samplesPerTick_;
            tickCounter_++;

            // Arpeggio cycles through notes each tick
            if (arpNotes_[1] != 0 || arpNotes_[2] != 0) {
                arpIndex_ = (arpIndex_ + 1) % 3;
            }

            // Retrigger check
            if (retriggerInterval_ > 0 && tickCounter_ % retriggerInterval_ == 0 && tickCounter_ > 0) {
                shouldRetrigger_ = true;
            }

            // Note cut check
            if (cutTick_ >= 0 && tickCounter_ >= cutTick_) {
                return false;  // Note is cut
            }

            // Note off check
            if (offTick_ >= 0 && tickCounter_ >= offTick_) {
                shouldReleaseNote_ = true;
            }
        }

        // Note delay - don't play until delay ticks have passed
        if (noteDelay_ > 0 && tickCounter_ < noteDelay_) {
            return false;
        }

        return true;
    }

    // Get current arpeggio offset in semitones
    int getArpOffset() const {
        return arpNotes_[arpIndex_];
    }

    // Check if note should retrigger (and clear flag)
    bool shouldRetrigger() {
        if (shouldRetrigger_) {
            shouldRetrigger_ = false;
            return true;
        }
        return false;
    }

    // Check if note should release (and clear flag)
    bool shouldReleaseNote() {
        if (shouldReleaseNote_) {
            shouldReleaseNote_ = false;
            return true;
        }
        return false;
    }

private:
    void updateTickLength() {
        if (sampleRate_ > 0 && tempo_ > 0) {
            // Calculate samples per tick
            // At 120 BPM: 2 beats/second = 0.5 seconds/beat = 0.125 seconds/row (at 4 rows per beat)
            // With 6 ticks per row: 0.125 / 6 = 0.0208 seconds per tick
            double rowsPerSecond = (tempo_ / 60.0) * 4.0;  // 4 rows per beat
            double secondsPerRow = 1.0 / rowsPerSecond;
            double secondsPerTick = secondsPerRow / TICKS_PER_ROW;
            samplesPerTick_ = secondsPerTick * sampleRate_;
        }
    }

    double sampleRate_ = 48000.0;
    float tempo_ = 120.0f;
    double samplesPerTick_ = 4000.0;

    int currentNote_ = 60;
    float currentVelocity_ = 1.0f;

    int tickCounter_ = 0;
    int sampleCounter_ = 0;

    // Arpeggio
    std::array<int, 3> arpNotes_ = {0, 0, 0};
    int arpIndex_ = 0;

    // Note delay
    int noteDelay_ = 0;

    // Retrigger
    int retriggerInterval_ = 0;
    int retriggerCounter_ = 0;
    bool shouldRetrigger_ = false;

    // Note cut/off
    int cutTick_ = -1;
    int offTick_ = -1;
    bool shouldReleaseNote_ = false;
};

} // namespace audio
