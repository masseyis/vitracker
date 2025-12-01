#include "Screen.h"

namespace ui {

Screen::Screen(model::Project& project, input::ModeManager& modeManager)
    : project_(project), modeManager_(modeManager)
{
}

} // namespace ui
