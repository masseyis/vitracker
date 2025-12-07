#pragma once

#include <cstdint>

namespace input {

// macOS Carbon virtual key codes for hjkl (different from ASCII!)
// On Mac, keyCode is the hardware key code, not the character
// JUCE may return either the raw macOS keyCode OR normalized ASCII depending on context
#ifdef __APPLE__
constexpr int kVK_H = 0x04;  // macOS virtual key code
constexpr int kVK_J = 0x26;
constexpr int kVK_K = 0x28;
constexpr int kVK_L = 0x25;
#else
// On Windows/Linux, use ASCII (this may need adjustment for those platforms)
constexpr int kVK_H = 'H';
constexpr int kVK_J = 'J';
constexpr int kVK_K = 'K';
constexpr int kVK_L = 'L';
#endif

// ASCII codes as fallback (JUCE may return these instead of virtual key codes)
// JUCE reports UPPERCASE keycodes on macOS, so we check both cases
constexpr int kASCII_H = 'H';  // 72 - JUCE reports uppercase
constexpr int kASCII_J = 'J';  // 74 - JUCE reports uppercase
constexpr int kASCII_K = 'K';  // 75 - JUCE reports uppercase
constexpr int kASCII_L = 'L';  // 76 - JUCE reports uppercase
constexpr int kASCII_h = 'h';  // 104 - lowercase backup
constexpr int kASCII_j = 'j';  // 106 - lowercase backup
constexpr int kASCII_k = 'k';  // 107 - lowercase backup
constexpr int kASCII_l = 'l';  // 108 - lowercase backup

// Input context describes the UI layout the screen is currently displaying
enum class InputContext {
    Grid,           // 2D navigation (pattern, song, chain, VA Synth, Mixer)
    RowParams,      // Vertical param list, single column (Plaits, Sampler, Slicer)
    ColumnParams,   // Horizontal params, single row (chord chooser)
    TextEdit,       // Text input mode (name editing)
};

// Semantic actions returned from key translation
enum class KeyAction {
    None,           // Key consumed globally or invalid for context

    // Navigation
    NavLeft,
    NavRight,
    NavUp,
    NavDown,

    // Value editing (direction-based, not magnitude-based)
    Edit1Inc,       // Vertical edit up (Alt+k/up) - transpose, cycle FX type
    Edit1Dec,       // Vertical edit down (Alt+j/down)
    Edit2Inc,       // Horizontal edit right (Alt+l/right) - adjust FX value
    Edit2Dec,       // Horizontal edit left (Alt+h/left)
    ShiftEdit1Inc,  // Coarse vertical up (Alt+Shift+k/up) - octave up
    ShiftEdit1Dec,  // Coarse vertical down (Alt+Shift+j/down) - octave down
    ShiftEdit2Inc,  // Coarse horizontal right (Alt+Shift+l/right)
    ShiftEdit2Dec,  // Coarse horizontal left (Alt+Shift+h/left)

    // Item operations
    NewSelection,   // 'n' - create new at selection
    NewItem,        // 'N' - create new screen-level item
    Delete,         // 'd' - delete selection/item
    Rename,         // 'r' - rename current item
    Open,           // 'o' - open/load

    // Clipboard
    Yank,           // 'y' - copy selection
    Paste,          // 'p' - paste clipboard

    // Visual mode operations
    Fill,           // 'f' - fill selection
    Shuffle,        // 's' - shuffle/randomize selection

    // Zoom and secondary scroll
    ZoomIn,         // '+' or '='
    ZoomOut,        // '-'
    SecondaryPrev,  // ',' - secondary navigation (sample slices, etc.)
    SecondaryNext,  // '.' - secondary navigation

    // Text editing
    TextChar,       // Printable character input (char in actionData)
    TextBackspace,  // Backspace
    TextAccept,     // Enter - accept text
    TextReject,     // Escape - cancel text edit

    // Jump and tab
    Jump,           // Shortcut key press (char in actionData)
    TabNext,        // Tab key
    TabPrev,        // Shift+Tab

    // Pattern-specific quick navigation
    PatternPrev,    // '[' - previous pattern
    PatternNext,    // ']' - next pattern

    // Universal actions (Enter, Escape, Space outside text mode)
    Confirm,        // Enter - drill down, select, confirm
    Cancel,         // Escape - back, close, cancel
    PlayPause,      // Space - toggle play/pause

    // Mixer-specific toggles
    ToggleMute,     // 'm' - toggle mute
    ToggleSolo,     // 's' - toggle solo
};

// Result of key translation - includes action and optional data
struct KeyActionResult {
    KeyAction action = KeyAction::None;
    char charData = 0;      // For TextChar, Jump actions

    KeyActionResult() = default;
    explicit KeyActionResult(KeyAction a) : action(a) {}
    KeyActionResult(KeyAction a, char c) : action(a), charData(c) {}

    bool isNone() const { return action == KeyAction::None; }
    bool isNavigation() const {
        return action == KeyAction::NavLeft || action == KeyAction::NavRight ||
               action == KeyAction::NavUp || action == KeyAction::NavDown;
    }
    bool isEdit() const {
        return action == KeyAction::Edit1Inc || action == KeyAction::Edit1Dec ||
               action == KeyAction::Edit2Inc || action == KeyAction::Edit2Dec ||
               action == KeyAction::ShiftEdit1Inc || action == KeyAction::ShiftEdit1Dec ||
               action == KeyAction::ShiftEdit2Inc || action == KeyAction::ShiftEdit2Dec;
    }
};

} // namespace input
