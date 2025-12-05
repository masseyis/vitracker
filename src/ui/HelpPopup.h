#pragma once

#include <JuceHeader.h>
#include <string>
#include <vector>

namespace ui {

// Help entry for a single key/action
struct HelpEntry {
    std::string key;
    std::string description;
};

// Section of help entries
struct HelpSection {
    std::string title;
    std::vector<HelpEntry> entries;
};

class HelpPopup : public juce::Component
{
public:
    HelpPopup();
    ~HelpPopup() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setContent(const std::vector<HelpSection>& sections);
    void setScreenTitle(const std::string& title) { screenTitle_ = title; }

    bool isShowing() const { return isVisible(); }
    void show();
    void hide();
    void toggle();

private:
    std::string screenTitle_;
    std::vector<HelpSection> sections_;

    // Layout constants
    static constexpr int kPanelWidth = 600;
    static constexpr int kPanelMaxHeight = 600;
    static constexpr int kPadding = 20;
    static constexpr int kLineHeight = 22;
    static constexpr int kSectionGap = 16;
    static constexpr int kKeyWidth = 120;

    // Colors (matching app theme)
    static inline const juce::Colour bgColor{0xff1a1a2e};
    static inline const juce::Colour panelColor{0xff2a2a4e};
    static inline const juce::Colour borderColor{0xff4a4a6e};
    static inline const juce::Colour titleColor{0xff7c7cff};
    static inline const juce::Colour keyColor{0xffffcc66};
    static inline const juce::Colour textColor{0xffeaeaea};
    static inline const juce::Colour dimColor{0xff888888};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HelpPopup)
};

} // namespace ui
