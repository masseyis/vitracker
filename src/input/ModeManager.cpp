#include "ModeManager.h"

namespace input {

ModeManager::ModeManager() {}

void ModeManager::setMode(Mode mode)
{
    if (mode != currentMode_)
    {
        currentMode_ = mode;
        if (mode != Mode::Command)
        {
            clearCommandBuffer();
        }
        if (onModeChanged)
        {
            onModeChanged(mode);
        }
    }
}

std::string ModeManager::getModeString() const
{
    switch (currentMode_)
    {
        case Mode::Normal: return "NORMAL";
        case Mode::Edit: return "EDIT";
        case Mode::Visual: return "VISUAL";
        case Mode::Command: return ":" + commandBuffer_;
    }
    return "";
}

void ModeManager::appendToCommandBuffer(char c)
{
    commandBuffer_ += c;
}

void ModeManager::backspaceCommandBuffer()
{
    if (!commandBuffer_.empty())
    {
        commandBuffer_.pop_back();
    }
}

void ModeManager::clearCommandBuffer()
{
    commandBuffer_.clear();
}

} // namespace input
