#include "KeyHandler.h"

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

    // Alt+Arrow = temporary edit mode (adjust values without switching modes)
    if (key.getModifiers().isAltDown())
    {
        if (keyCode == juce::KeyPress::upKey || keyCode == juce::KeyPress::downKey ||
            keyCode == juce::KeyPress::leftKey || keyCode == juce::KeyPress::rightKey)
        {
            if (onEditKey && onEditKey(key)) return true;
            return true;  // Still consume Alt+arrows even if not handled
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

    // Up/Down: navigate
    if (keyCode == juce::KeyPress::upKey)
    {
        if (onNavigate) onNavigate(0, -1);
        return true;
    }
    if (keyCode == juce::KeyPress::downKey)
    {
        if (onNavigate) onNavigate(0, 1);
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

    // Actions
    if (keyCode == juce::KeyPress::spaceKey)
    {
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

    // Navigation extends selection
    auto keyCode = key.getKeyCode();
    if (keyCode == juce::KeyPress::upKey || keyCode == juce::KeyPress::downKey ||
        keyCode == juce::KeyPress::leftKey || keyCode == juce::KeyPress::rightKey)
    {
        // Navigate and update selection
        bool result = handleNormalMode(key);
        if (onUpdateSelection) onUpdateSelection();
        return result;
    }

    auto textChar = key.getTextCharacter();
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

    // Alt+Up/Down for batch editing - forward to screen
    if (key.getModifiers().isAltDown() &&
        (keyCode == juce::KeyPress::upKey || keyCode == juce::KeyPress::downKey))
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

} // namespace input
