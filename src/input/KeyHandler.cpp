#include "KeyHandler.h"
#include <iostream>  // For debug output in Release builds

namespace input {

KeyHandler::KeyHandler(ModeManager& modeManager) : modeManager_(modeManager) {}

bool KeyHandler::handleKey(const juce::KeyPress& key)
{
    switch (modeManager_.getMode())
    {
        case Mode::Normal: return handleNormalMode(key);
        case Mode::Edit: return handleEditMode(key);
        case Mode::Visual: return handleVisualMode(key);
        case Mode::Command: return handleCommandMode(key);
    }
    return false;
}

bool KeyHandler::handleNormalMode(const juce::KeyPress& key)
{
    auto keyCode = key.getKeyCode();
    auto textChar = key.getTextCharacter();

    // Screen switching: 1-6 (Project screen removed)
    // But first, let the screen handle it (e.g., for name editing mode)
    if (textChar >= '1' && textChar <= '6')
    {
        // Give screen a chance to consume the key first (e.g., text input)
        if (onEditKey && onEditKey(key)) return true;
        // Screen didn't consume it, so switch screens
        if (onScreenSwitch) onScreenSwitch(textChar - '0');
        return true;
    }

    // Alt+Arrow/hjkl = temporary edit mode (adjust values without switching modes)
    // Note: On macOS, Alt+letter produces special characters, so we check keyCode directly
    // Using kVK_* constants from KeyAction.h for correct macOS virtual key codes
    // Also check ASCII codes as fallback since JUCE may return either depending on context
    if (key.getModifiers().isAltDown())
    {
        // DEBUG: Log what JUCE reports for Alt+key combinations
        std::cerr << "Alt+key: keyCode=" << keyCode << " (0x" << std::hex << keyCode << std::dec << ")"
                  << " textChar=" << (int)textChar << " ('" << (char)textChar << "')"
                  << " kVK_J=" << kVK_J << " kASCII_J=" << kASCII_J << std::endl;

        if (keyCode == juce::KeyPress::upKey || keyCode == juce::KeyPress::downKey ||
            keyCode == juce::KeyPress::leftKey || keyCode == juce::KeyPress::rightKey ||
            keyCode == kVK_H || keyCode == kVK_J || keyCode == kVK_K || keyCode == kVK_L ||
            keyCode == kASCII_H || keyCode == kASCII_J || keyCode == kASCII_K || keyCode == kASCII_L ||
            keyCode == kASCII_h || keyCode == kASCII_j || keyCode == kASCII_k || keyCode == kASCII_l)
        {
            std::cerr << "  -> MATCHED Alt+hjkl, forwarding to onEditKey" << std::endl;
            if (onEditKey && onEditKey(key)) return true;
            return true;  // Still consume Alt+arrows/hjkl even if not handled
        }
        else
        {
            std::cerr << "  -> NOT matched as hjkl" << std::endl;
        }
    }

    // Shift+N creates new item (chain, pattern, etc.) - forward to screen
    if (key.getModifiers().isShiftDown() && (textChar == 'N'))
    {
        if (onEditKey) onEditKey(key);
        return true;
    }

    // 'r' for rename - forward to screen
    if (textChar == 'r' || textChar == 'R')
    {
        if (onEditKey && onEditKey(key)) return true;
        // If not consumed (not in a renameable context), fall through
    }

    // '[' and ']' for quick navigation (patterns, etc.) - forward to screen
    if (textChar == '[' || textChar == ']')
    {
        if (onEditKey) onEditKey(key);
        return true;
    }

    // 'd' for delete - forward to screen in Normal mode too
    if (textChar == 'd')
    {
        if (onEditKey && onEditKey(key)) return true;
        // If not consumed, fall through to normal 'd' handling (if any)
    }

    // Backspace/Delete for clearing - forward to screen
    if (keyCode == juce::KeyPress::backspaceKey || keyCode == juce::KeyPress::deleteKey)
    {
        if (onEditKey && onEditKey(key)) return true;
    }

    // Enter and Escape need to reach screens for name editing mode
    // Forward them to onEditKey so screens can handle them if needed
    if (keyCode == juce::KeyPress::returnKey || keyCode == juce::KeyPress::escapeKey)
    {
        if (onEditKey && onEditKey(key)) return true;
        // If not consumed, continue with normal handling
    }

    // '{' and '}' (Shift+[]) for screen switching
    if (textChar == '{')
    {
        if (onScreenSwitch)
        {
            // Get current screen and go to previous
            // This will be handled by MainComponent
            onScreenSwitch(-1);  // Use negative for previous
        }
        return true;
    }
    if (textChar == '}')
    {
        if (onScreenSwitch)
        {
            onScreenSwitch(-2);  // Use -2 for next
        }
        return true;
    }

    // Forward Tab key to screen for type switching etc.
    if (keyCode == juce::KeyPress::tabKey)
    {
        if (onEditKey && onEditKey(key)) return true;
    }

    // Forward ALL printable keys to screen first (for name editing, note entry, etc.)
    // Screen returns true if it consumed the key (e.g., in name editing mode)
    // Only fall through to mode switches/actions if screen doesn't consume
    if ((textChar >= 'a' && textChar <= 'z') ||
        (textChar >= 'A' && textChar <= 'Z') ||
        (textChar >= '0' && textChar <= '9') ||
        textChar == '.' || textChar == ',' ||
        textChar == '+' || textChar == '=' || textChar == '-' ||
        textChar == ':')
    {
        if (onEditKey && onEditKey(key)) return true;
        // Fall through to mode switches/actions below if not consumed
    }

    // Left/Right: forward to screen first for non-grid value adjustment
    // (e.g., scale lock in chains, non-mod params in instruments)
    // Screen returns true if it consumed the key
    if (keyCode == juce::KeyPress::leftKey || keyCode == juce::KeyPress::rightKey)
    {
        if (onEditKey && onEditKey(key)) return true;
        // Not consumed - do navigation
        if (keyCode == juce::KeyPress::leftKey)
        {
            if (onNavigate) onNavigate(-1, 0);
            return true;
        }
        if (keyCode == juce::KeyPress::rightKey)
        {
            if (onNavigate) onNavigate(1, 0);
            return true;
        }
    }

    // Alt+Up/Down: forward to screen for value editing
    if (key.getModifiers().isAltDown() &&
        (keyCode == juce::KeyPress::upKey || keyCode == juce::KeyPress::downKey))
    {
        if (onEditKey && onEditKey(key)) return true;
        // Not consumed - fall through to navigation
    }

    // Up/Down: navigate (arrows and hjkl)
    if (keyCode == juce::KeyPress::upKey || textChar == 'k')
    {
        if (onNavigate) onNavigate(0, -1);
        return true;
    }
    if (keyCode == juce::KeyPress::downKey || textChar == 'j')
    {
        if (onNavigate) onNavigate(0, 1);
        return true;
    }
    // Left/Right vim keys (h/l) - only if not consumed by screen above
    if (textChar == 'h')
    {
        if (onNavigate) onNavigate(-1, 0);
        return true;
    }
    if (textChar == 'l')
    {
        if (onNavigate) onNavigate(1, 0);
        return true;
    }

    // Mode switches (only reached if screen didn't consume the key)
    // Note: 'i' no longer switches to Edit mode - use Alt+arrows for value editing
    if (textChar == 'v')
    {
        if (onStartSelection) onStartSelection();
        modeManager_.setMode(Mode::Visual);
        return true;
    }
    if (textChar == ':')
    {
        modeManager_.setMode(Mode::Command);
        return true;
    }

    // Space: Forward to screen first for text editing, then play/stop
    if (keyCode == juce::KeyPress::spaceKey)
    {
        // Screen can consume space if in TextEdit context
        if (onEditKey && onEditKey(key)) return true;
        // Not consumed by screen, use for play/stop
        if (onPlayStop) onPlayStop();
        return true;
    }
    if (textChar == 'y')
    {
        if (onYank) onYank();
        return true;
    }
    if (textChar == 'p')
    {
        if (onPaste) onPaste();
        return true;
    }
    if (textChar == 'd')
    {
        if (onDelete) onDelete();
        return true;
    }
    if (textChar == 'u')
    {
        if (onUndo) onUndo();
        return true;
    }
    if (key.getModifiers().isCtrlDown() && textChar == 'r')
    {
        if (onRedo) onRedo();
        return true;
    }

    // Enter confirms/activates current selection
    if (keyCode == juce::KeyPress::returnKey)
    {
        if (onConfirm) onConfirm();
        return true;
    }

    return false;
}

bool KeyHandler::handleEditMode(const juce::KeyPress& key)
{
    // Edit mode has been removed - redirect to Normal mode behavior
    // Value editing is now done via Alt+arrows in Normal mode
    modeManager_.setMode(Mode::Normal);
    return handleNormalMode(key);
}

bool KeyHandler::handleVisualMode(const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        modeManager_.setMode(Mode::Normal);
        return true;
    }

    // Navigation extends selection (arrows and hjkl)
    auto keyCode = key.getKeyCode();
    auto textChar = key.getTextCharacter();
    if (keyCode == juce::KeyPress::upKey || keyCode == juce::KeyPress::downKey ||
        keyCode == juce::KeyPress::leftKey || keyCode == juce::KeyPress::rightKey ||
        textChar == 'h' || textChar == 'j' || textChar == 'k' || textChar == 'l')
    {
        // Navigate and update selection
        bool result = handleNormalMode(key);
        if (onUpdateSelection) onUpdateSelection();
        return result;
    }

    if (textChar == 'y')
    {
        if (onYank) onYank();
        modeManager_.setMode(Mode::Normal);
        return true;
    }
    if (textChar == 'd')
    {
        if (onDelete) onDelete();
        modeManager_.setMode(Mode::Normal);
        return true;
    }

    // Transpose up/down
    if (textChar == '>')
    {
        if (onTranspose) onTranspose(1);
        return true;
    }
    if (textChar == '<')
    {
        if (onTranspose) onTranspose(-1);
        return true;
    }

    // Transpose by octave with Shift+</> or +/-
    if (textChar == '+' || textChar == '=')
    {
        if (onTranspose) onTranspose(12);
        return true;
    }
    if (textChar == '-')
    {
        if (onTranspose) onTranspose(-12);
        return true;
    }

    // 'f' for fill, 's' for randomize - forward to screen
    if (textChar == 'f' || textChar == 's')
    {
        if (onEditKey && onEditKey(key)) return true;
    }

    // Alt+arrows/hjkl for batch editing - forward to screen
    // Check both macOS virtual keycodes and ASCII keycodes (both upper and lowercase)
    if (key.getModifiers().isAltDown() &&
        (keyCode == juce::KeyPress::upKey || keyCode == juce::KeyPress::downKey ||
         keyCode == juce::KeyPress::leftKey || keyCode == juce::KeyPress::rightKey ||
         keyCode == kVK_H || keyCode == kVK_J || keyCode == kVK_K || keyCode == kVK_L ||
         keyCode == kASCII_H || keyCode == kASCII_J || keyCode == kASCII_K || keyCode == kASCII_L ||
         keyCode == kASCII_h || keyCode == kASCII_j || keyCode == kASCII_k || keyCode == kASCII_l))
    {
        if (onEditKey && onEditKey(key)) return true;
    }

    return false;
}

bool KeyHandler::handleCommandMode(const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        modeManager_.setMode(Mode::Normal);
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::returnKey)
    {
        executeCommand(modeManager_.getCommandBuffer());
        modeManager_.setMode(Mode::Normal);
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::backspaceKey)
    {
        modeManager_.backspaceCommandBuffer();
        return true;
    }

    auto textChar = key.getTextCharacter();
    if (textChar >= ' ' && textChar <= '~')
    {
        modeManager_.appendToCommandBuffer(static_cast<char>(textChar));
        return true;
    }

    return false;
}

void KeyHandler::executeCommand(const std::string& command)
{
    DBG("Executing command: " << command);

    if (command == "w")
    {
        if (onSave) onSave("");
    }
    else if (command.length() > 2 && command.substr(0, 2) == "w ")
    {
        if (onSave) onSave(command.substr(2));
    }
    else if (command == "e")
    {
        if (onLoad) onLoad("");
    }
    else if (command.length() > 2 && command.substr(0, 2) == "e ")
    {
        if (onLoad) onLoad(command.substr(2));
    }
    else if (command == "new")
    {
        if (onNew) onNew();
    }
    else if (command == "q")
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
    else if (command.length() >= 10 && command.substr(0, 10) == "transpose ")
    {
        try {
            int semitones = std::stoi(command.substr(10));
            if (onTranspose) onTranspose(semitones);
        } catch (...) {}
    }
    else if (command == "interpolate")
    {
        if (onInterpolate) onInterpolate();
    }
    else if (command.length() >= 10 && command.substr(0, 10) == "randomize ")
    {
        try {
            int percent = std::stoi(command.substr(10));
            if (onRandomize) onRandomize(percent);
        } catch (...) {}
    }
    else if (command == "double")
    {
        if (onDouble) onDouble();
    }
    else if (command == "halve")
    {
        if (onHalve) onHalve();
    }
    else if (command.length() > 12 && command.substr(0, 12) == "save-preset ")
    {
        if (onSavePreset) onSavePreset(command.substr(12));
    }
    else if (command.length() > 14 && command.substr(0, 14) == "delete-preset ")
    {
        if (onDeletePreset) onDeletePreset(command.substr(14));
    }
    else if (command == "sampler")
    {
        if (onCreateSampler) onCreateSampler();
    }
    else if (command == "slicer")
    {
        if (onCreateSlicer) onCreateSlicer();
    }
    else if (command.substr(0, 4) == "chop")
    {
        // Parse :chop N
        int divisions = 8;  // default
        if (command.length() > 5) {
            try {
                divisions = std::stoi(command.substr(5));
            } catch (...) {
                divisions = 8;
            }
        }
        if (onChop) onChop(divisions);
    }

    if (onCommand) onCommand(command);
}

bool KeyHandler::isReservedGlobalKey(const juce::KeyPress& key)
{
    auto keyCode = key.getKeyCode();
    auto textChar = key.getTextCharacter();
    auto mods = key.getModifiers();

    // Screen switching: 1-5
    if (textChar >= '1' && textChar <= '5' && !mods.isAltDown() && !mods.isCtrlDown())
        return true;

    // Screen switching: { and }
    if (textChar == '{' || textChar == '}')
        return true;

    // Play/Stop: Space
    if (keyCode == juce::KeyPress::spaceKey)
        return true;

    // Undo: u
    if (textChar == 'u' && !mods.isAltDown() && !mods.isCtrlDown() && !mods.isShiftDown())
        return true;

    // Redo: Ctrl+r
    if (mods.isCtrlDown() && (textChar == 'r' || textChar == 'R'))
        return true;

    // Command mode: :
    if (textChar == ':')
        return true;

    // Visual mode: v
    if (textChar == 'v' && !mods.isAltDown() && !mods.isCtrlDown() && !mods.isShiftDown())
        return true;

    // Help: ?
    if (textChar == '?')
        return true;

    return false;
}

KeyActionResult KeyHandler::translateKey(const juce::KeyPress& key, InputContext context, bool visualMode)
{
    auto keyCode = key.getKeyCode();
    auto textChar = key.getTextCharacter();
    auto mods = key.getModifiers();
    bool alt = mods.isAltDown();
    bool shift = mods.isShiftDown();

    // Text editing context - special handling (checked BEFORE reserved global keys
    // so that space and other printable chars can be entered as text)
    if (context == InputContext::TextEdit)
    {
        if (keyCode == juce::KeyPress::returnKey)
            return KeyActionResult(KeyAction::TextAccept);
        if (keyCode == juce::KeyPress::escapeKey)
            return KeyActionResult(KeyAction::TextReject);
        if (keyCode == juce::KeyPress::backspaceKey)
            return KeyActionResult(KeyAction::TextBackspace);
        // Printable characters (includes space)
        if (textChar >= ' ' && textChar <= '~')
            return KeyActionResult(KeyAction::TextChar, static_cast<char>(textChar));
        return KeyActionResult(KeyAction::None);
    }

    // Reserved global keys return None - they're handled by KeyHandler directly
    if (isReservedGlobalKey(key))
        return KeyActionResult(KeyAction::None);

    // Tab key handling (all contexts)
    if (keyCode == juce::KeyPress::tabKey)
    {
        if (shift)
            return KeyActionResult(KeyAction::TabPrev);
        return KeyActionResult(KeyAction::TabNext);
    }

    // Universal actions: Enter and Escape (outside TextEdit context)
    if (keyCode == juce::KeyPress::returnKey)
        return KeyActionResult(KeyAction::Confirm);
    if (keyCode == juce::KeyPress::escapeKey)
        return KeyActionResult(KeyAction::Cancel);

    // Backspace/Delete for clearing
    if (keyCode == juce::KeyPress::backspaceKey || keyCode == juce::KeyPress::deleteKey)
        return KeyActionResult(KeyAction::Delete);

    // Pattern navigation brackets
    if (textChar == '[')
        return KeyActionResult(KeyAction::PatternPrev);
    if (textChar == ']')
        return KeyActionResult(KeyAction::PatternNext);

    // Zoom: +/- (not in visual mode where they're transpose)
    if (!visualMode)
    {
        if (textChar == '+' || textChar == '=')
            return KeyActionResult(KeyAction::ZoomIn);
        if (textChar == '-')
            return KeyActionResult(KeyAction::ZoomOut);
    }

    // Secondary navigation: , and .
    if (textChar == ',')
        return KeyActionResult(KeyAction::SecondaryPrev);
    if (textChar == '.')
        return KeyActionResult(KeyAction::SecondaryNext);

    // Item operations
    if (textChar == 'n' && !alt && !shift)
        return KeyActionResult(KeyAction::NewSelection);
    if (textChar == 'N' && shift)
        return KeyActionResult(KeyAction::NewItem);
    if (textChar == 'd' && !alt)
        return KeyActionResult(KeyAction::Delete);
    if (textChar == 'r' && !alt && !shift)
        return KeyActionResult(KeyAction::Rename);
    if (textChar == 'o' && !alt && !shift)
        return KeyActionResult(KeyAction::Open);

    // Clipboard
    if (textChar == 'y' && !alt)
        return KeyActionResult(KeyAction::Yank);
    if (textChar == 'p' && !alt)
        return KeyActionResult(KeyAction::Paste);

    // Visual mode operations
    if (visualMode)
    {
        if (textChar == 'f')
            return KeyActionResult(KeyAction::Fill);
        if (textChar == 's')
            return KeyActionResult(KeyAction::Shuffle);
        // Transpose in visual mode: +/- are octave, Alt+arrows/hjkl are semitone
        if (textChar == '+' || textChar == '=')
            return KeyActionResult(KeyAction::Edit2Inc);  // Octave up
        if (textChar == '-')
            return KeyActionResult(KeyAction::Edit2Dec);  // Octave down
    }

    // Mixer-specific toggles (outside visual mode where 's' is Shuffle)
    if (!visualMode)
    {
        if (textChar == 'm' && !alt)
            return KeyActionResult(KeyAction::ToggleMute);
        if (textChar == 's' && !alt)
            return KeyActionResult(KeyAction::ToggleSolo);
    }

    // Helper to check for hjkl by keyCode (needed because Alt+letter produces special chars on macOS)
    // Check both macOS virtual key codes and ASCII codes since JUCE may return either
    auto isH = [&]() { return keyCode == kVK_H || keyCode == kASCII_H || textChar == 'h'; };
    auto isJ = [&]() { return keyCode == kVK_J || keyCode == kASCII_J || textChar == 'j'; };
    auto isK = [&]() { return keyCode == kVK_K || keyCode == kASCII_K || textChar == 'k'; };
    auto isL = [&]() { return keyCode == kVK_L || keyCode == kASCII_L || textChar == 'l'; };

    // Context-specific key translation
    switch (context)
    {
        case InputContext::Grid:
        {
            // Grid: hjkl and arrows for navigation, Alt variants for editing
            if (!alt)
            {
                // Navigation
                if (isH() || keyCode == juce::KeyPress::leftKey)
                    return KeyActionResult(KeyAction::NavLeft);
                if (isL() || keyCode == juce::KeyPress::rightKey)
                    return KeyActionResult(KeyAction::NavRight);
                if (isK() || keyCode == juce::KeyPress::upKey)
                    return KeyActionResult(KeyAction::NavUp);
                if (isJ() || keyCode == juce::KeyPress::downKey)
                    return KeyActionResult(KeyAction::NavDown);
            }
            else
            {
                // Alt + direction = edit (direction determines Edit1 vs Edit2, Shift = coarse)
                // Vertical (j/k/up/down) = Edit1, Horizontal (h/l/left/right) = Edit2
                if (isH() || keyCode == juce::KeyPress::leftKey)
                    return KeyActionResult(shift ? KeyAction::ShiftEdit2Dec : KeyAction::Edit2Dec);
                if (isL() || keyCode == juce::KeyPress::rightKey)
                    return KeyActionResult(shift ? KeyAction::ShiftEdit2Inc : KeyAction::Edit2Inc);
                if (isK() || keyCode == juce::KeyPress::upKey)
                    return KeyActionResult(shift ? KeyAction::ShiftEdit1Inc : KeyAction::Edit1Inc);
                if (isJ() || keyCode == juce::KeyPress::downKey)
                    return KeyActionResult(shift ? KeyAction::ShiftEdit1Dec : KeyAction::Edit1Dec);
            }
            break;
        }

        case InputContext::RowParams:
        {
            // RowParams: j/k/arrows for nav up/down, h/l/left/right for edit
            // Edit2 = horizontal (h/l), Edit1 = vertical (j/k when with Alt)
            if (!alt)
            {
                // Vertical navigation
                if (isK() || keyCode == juce::KeyPress::upKey)
                    return KeyActionResult(KeyAction::NavUp);
                if (isJ() || keyCode == juce::KeyPress::downKey)
                    return KeyActionResult(KeyAction::NavDown);
                // Horizontal = edit (Edit2)
                if (isH() || keyCode == juce::KeyPress::leftKey)
                    return KeyActionResult(shift ? KeyAction::ShiftEdit2Dec : KeyAction::Edit2Dec);
                if (isL() || keyCode == juce::KeyPress::rightKey)
                    return KeyActionResult(shift ? KeyAction::ShiftEdit2Inc : KeyAction::Edit2Inc);
            }
            else
            {
                // Alt + direction = edit (horizontal=Edit2, vertical=Edit1, Shift=coarse)
                if (isH() || keyCode == juce::KeyPress::leftKey)
                    return KeyActionResult(shift ? KeyAction::ShiftEdit2Dec : KeyAction::Edit2Dec);
                if (isL() || keyCode == juce::KeyPress::rightKey)
                    return KeyActionResult(shift ? KeyAction::ShiftEdit2Inc : KeyAction::Edit2Inc);
                if (isK() || keyCode == juce::KeyPress::upKey)
                    return KeyActionResult(shift ? KeyAction::ShiftEdit1Inc : KeyAction::Edit1Inc);
                if (isJ() || keyCode == juce::KeyPress::downKey)
                    return KeyActionResult(shift ? KeyAction::ShiftEdit1Dec : KeyAction::Edit1Dec);
            }
            break;
        }

        case InputContext::ColumnParams:
        {
            // ColumnParams: h/l/arrows for nav left/right, j/k for edit
            // Edit1 = vertical (j/k), Edit2 = horizontal (h/l when with Alt)
            if (!alt)
            {
                // Horizontal navigation
                if (isH() || keyCode == juce::KeyPress::leftKey)
                    return KeyActionResult(KeyAction::NavLeft);
                if (isL() || keyCode == juce::KeyPress::rightKey)
                    return KeyActionResult(KeyAction::NavRight);
                // Vertical = edit (Edit1)
                if (isK() || keyCode == juce::KeyPress::upKey)
                    return KeyActionResult(shift ? KeyAction::ShiftEdit1Inc : KeyAction::Edit1Inc);
                if (isJ() || keyCode == juce::KeyPress::downKey)
                    return KeyActionResult(shift ? KeyAction::ShiftEdit1Dec : KeyAction::Edit1Dec);
            }
            else
            {
                // Alt + direction = edit (vertical=Edit1, horizontal=Edit2, Shift=coarse)
                if (isK() || keyCode == juce::KeyPress::upKey)
                    return KeyActionResult(shift ? KeyAction::ShiftEdit1Inc : KeyAction::Edit1Inc);
                if (isJ() || keyCode == juce::KeyPress::downKey)
                    return KeyActionResult(shift ? KeyAction::ShiftEdit1Dec : KeyAction::Edit1Dec);
                if (isH() || keyCode == juce::KeyPress::leftKey)
                    return KeyActionResult(shift ? KeyAction::ShiftEdit2Dec : KeyAction::Edit2Dec);
                if (isL() || keyCode == juce::KeyPress::rightKey)
                    return KeyActionResult(shift ? KeyAction::ShiftEdit2Inc : KeyAction::Edit2Inc);
            }
            break;
        }

        case InputContext::TextEdit:
            // Already handled above
            break;
    }

    // Shortcut keys (a-z that aren't reserved) - return as Jump
    if (textChar >= 'a' && textChar <= 'z' && !alt)
    {
        // Skip navigation keys and keys with explicit actions
        if (context == InputContext::Grid || context == InputContext::RowParams || context == InputContext::ColumnParams)
        {
            if (textChar != 'h' && textChar != 'j' && textChar != 'k' && textChar != 'l' &&
                textChar != 'n' && textChar != 'd' && textChar != 'r' && textChar != 'o' &&
                textChar != 'y' && textChar != 'p' && textChar != 'f' && textChar != 's' &&
                textChar != 'm' && textChar != 'v' && textChar != 'u')
            {
                return KeyActionResult(KeyAction::Jump, static_cast<char>(textChar));
            }
        }
    }

    return KeyActionResult(KeyAction::None);
}

} // namespace input
