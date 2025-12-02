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

    // Screen switching: 1-6
    if (textChar >= '1' && textChar <= '6')
    {
        if (onScreenSwitch) onScreenSwitch(textChar - '0');
        return true;
    }

    // Navigation
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

    // Mode switches
    if (textChar == 'i')
    {
        modeManager_.setMode(Mode::Edit);
        return true;
    }
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

    return false;
}

bool KeyHandler::handleEditMode(const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        modeManager_.setMode(Mode::Normal);
        return true;
    }

    // Navigation also works in edit mode
    auto keyCode = key.getKeyCode();
    if (keyCode == juce::KeyPress::upKey || keyCode == juce::KeyPress::downKey ||
        keyCode == juce::KeyPress::leftKey || keyCode == juce::KeyPress::rightKey)
    {
        return handleNormalMode(key);
    }

    // Forward to edit handler callback
    if (onEditKey)
    {
        onEditKey(key);
        return true;
    }

    return false;
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

    if (onCommand) onCommand(command);
}

} // namespace input
