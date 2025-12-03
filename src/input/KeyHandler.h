#pragma once

#include "ModeManager.h"
#include <JuceHeader.h>
#include <functional>

namespace input {

class KeyHandler
{
public:
    KeyHandler(ModeManager& modeManager);

    // Returns true if key was handled
    bool handleKey(const juce::KeyPress& key);

    // Callbacks for actions
    std::function<void(int)> onScreenSwitch;      // 1-6
    std::function<void()> onPlayStop;
    std::function<void(int, int)> onNavigate;     // dx, dy
    std::function<void()> onYank;
    std::function<void()> onPaste;
    std::function<void()> onDelete;
    std::function<void(int)> onTranspose;  // semitones
    std::function<void()> onStartSelection;
    std::function<void()> onUpdateSelection;
    std::function<void()> onUndo;
    std::function<void()> onRedo;
    std::function<void(const std::string&)> onCommand;  // Executed command
    std::function<bool(const juce::KeyPress&)> onEditKey;  // Forward edit keys to screen, returns true if consumed
    std::function<void(const std::string&)> onSave;  // :w [filename]
    std::function<void(const std::string&)> onLoad;  // :e [filename]
    std::function<void()> onNew;  // :new
    std::function<void()> onInterpolate;  // :interpolate
    std::function<void(int)> onRandomize;  // :randomize N
    std::function<void()> onDouble;  // :double
    std::function<void()> onHalve;  // :halve
    std::function<void()> onConfirm;  // Enter key in normal mode
    std::function<void(const std::string&)> onSavePreset;  // :save-preset name
    std::function<void(const std::string&)> onDeletePreset;  // :delete-preset name
    std::function<void()> onCreateSampler;  // :sampler
    std::function<void()> onCreateSlicer;  // :slicer
    std::function<void(int)> onChop;  // Takes number of divisions

private:
    bool handleNormalMode(const juce::KeyPress& key);
    bool handleEditMode(const juce::KeyPress& key);
    bool handleVisualMode(const juce::KeyPress& key);
    bool handleCommandMode(const juce::KeyPress& key);

    void executeCommand(const std::string& command);

    ModeManager& modeManager_;
};

} // namespace input
