#pragma once

#include <cstdint>
#include <string>
#include <cstdio>

namespace model {

enum class FXType : uint8_t
{
    None = 0,
    ARP,   // Arpeggio - value = semitones (high nibble up, low nibble down)
    POR,   // Portamento - value = speed
    VIB,   // Vibrato - value = speed/depth
    VOL,   // Volume slide - value = up/down amount
    PAN,   // Pan - value = position
    DLY    // Retrigger/delay - value = ticks
};

struct FXCommand
{
    FXType type = FXType::None;
    uint8_t value = 0;

    bool isEmpty() const { return type == FXType::None; }
    void clear() { type = FXType::None; value = 0; }

    std::string toString() const {
        if (type == FXType::None) return "....";
        static const char* names[] = {"....", "ARP", "POR", "VIB", "VOL", "PAN", "DLY"};
        char buf[8];
        snprintf(buf, sizeof(buf), "%s%02X", names[static_cast<int>(type)], value);
        return buf;
    }
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
