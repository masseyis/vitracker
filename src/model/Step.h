#pragma once

#include <cstdint>
#include <string>

namespace model {

struct FXCommand
{
    uint8_t command = 0;   // 0 = none, 1 = ARP, 2 = POR, etc.
    uint8_t value = 0;

    bool isEmpty() const { return command == 0; }
    void clear() { command = 0; value = 0; }
};

struct Step
{
    int8_t note = -1;           // -1 = empty, 0-127 = MIDI note, -2 = OFF
    int16_t instrument = -1;    // -1 = empty, 0+ = instrument index
    uint8_t volume = 0xFF;      // 0x00-0xFF, 0xFF = default
    FXCommand fx1;
    FXCommand fx2;
    FXCommand fx3;

    static constexpr int8_t NOTE_EMPTY = -1;
    static constexpr int8_t NOTE_OFF = -2;

    bool isEmpty() const
    {
        return note == NOTE_EMPTY && instrument == -1 && volume == 0xFF
            && fx1.isEmpty() && fx2.isEmpty() && fx3.isEmpty();
    }

    void clear()
    {
        note = NOTE_EMPTY;
        instrument = -1;
        volume = 0xFF;
        fx1.clear();
        fx2.clear();
        fx3.clear();
    }
};

} // namespace model
