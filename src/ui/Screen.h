#pragma once

#include <JuceHeader.h>
#include "../model/Project.h"
#include "../input/ModeManager.h"

namespace ui {

class Screen : public juce::Component
{
public:
    Screen(model::Project& project, input::ModeManager& modeManager);
    ~Screen() override = default;

    virtual void onEnter() {}   // Called when screen becomes active
    virtual void onExit() {}    // Called when leaving screen

    virtual void navigate(int dx, int dy) { juce::ignoreUnused(dx, dy); }
    virtual void handleEdit(const juce::KeyPress& key) { juce::ignoreUnused(key); }

    virtual std::string getTitle() const = 0;

protected:
    model::Project& project_;
    input::ModeManager& modeManager_;

    // Common colors
    static inline const juce::Colour bgColor{0xff1a1a2e};
    static inline const juce::Colour fgColor{0xffeaeaea};
    static inline const juce::Colour highlightColor{0xff4a4a6e};
    static inline const juce::Colour cursorColor{0xff7c7cff};
    static inline const juce::Colour headerColor{0xff2a2a4e};
};

} // namespace ui
