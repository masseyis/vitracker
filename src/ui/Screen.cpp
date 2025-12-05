#include "Screen.h"
#include "HelpPopup.h"

namespace ui {

Screen::Screen(model::Project& project, input::ModeManager& modeManager)
    : project_(project), modeManager_(modeManager)
{
}

std::vector<HelpSection> Screen::getHelpContent() const
{
    // Default help content - screens should override this
    return {
        {"Navigation", {
            {"Arrow keys", "Navigate"},
            {"1-6", "Switch screens"},
            {"{  }", "Previous/Next screen"},
        }},
        {"Actions", {
            {"Space", "Play/Stop"},
            {"Enter", "Confirm/Activate"},
            {":", "Command mode"},
            {"?", "Show this help"},
        }},
    };
}

} // namespace ui
