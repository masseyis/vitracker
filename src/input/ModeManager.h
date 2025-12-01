#pragma once

#include <functional>
#include <string>

namespace input {

enum class Mode
{
    Normal,
    Edit,
    Visual,
    Command
};

class ModeManager
{
public:
    ModeManager();

    Mode getMode() const { return currentMode_; }
    void setMode(Mode mode);

    std::string getModeString() const;

    // Command mode buffer
    const std::string& getCommandBuffer() const { return commandBuffer_; }
    void appendToCommandBuffer(char c);
    void backspaceCommandBuffer();
    void clearCommandBuffer();

    // Callback when mode changes
    std::function<void(Mode)> onModeChanged;

private:
    Mode currentMode_ = Mode::Normal;
    std::string commandBuffer_;
};

} // namespace input
