#include "HelpPopup.h"

namespace ui {

HelpPopup::HelpPopup()
{
    setVisible(false);
    setAlwaysOnTop(true);
}

void HelpPopup::paint(juce::Graphics& g)
{
    // Semi-transparent overlay background
    g.fillAll(juce::Colours::black.withAlpha(0.7f));

    // Calculate panel dimensions
    int contentHeight = kPadding * 2;  // Top and bottom padding
    contentHeight += 40;  // Title
    contentHeight += kSectionGap;

    for (const auto& section : sections_)
    {
        contentHeight += kLineHeight;  // Section title
        contentHeight += static_cast<int>(section.entries.size()) * kLineHeight;
        contentHeight += kSectionGap;
    }

    contentHeight += kLineHeight;  // "Press ? or Esc to close"

    int panelHeight = std::min(contentHeight, kPanelMaxHeight);
    int panelX = (getWidth() - kPanelWidth) / 2;
    int panelY = (getHeight() - panelHeight) / 2;

    auto panelBounds = juce::Rectangle<int>(panelX, panelY, kPanelWidth, panelHeight);

    // Draw panel background with border
    g.setColour(panelColor);
    g.fillRoundedRectangle(panelBounds.toFloat(), 8.0f);
    g.setColour(borderColor);
    g.drawRoundedRectangle(panelBounds.toFloat(), 8.0f, 2.0f);

    // Content area
    auto contentArea = panelBounds.reduced(kPadding);
    int y = contentArea.getY();

    // Title
    g.setColour(titleColor);
    g.setFont(juce::Font(20.0f).boldened());
    g.drawText(screenTitle_ + " - Help", contentArea.getX(), y, contentArea.getWidth(), 30,
               juce::Justification::centred);
    y += 40;

    // Sections
    for (const auto& section : sections_)
    {
        // Section title
        g.setColour(dimColor);
        g.setFont(juce::Font(14.0f).boldened());
        g.drawText(section.title, contentArea.getX(), y, contentArea.getWidth(), kLineHeight,
                   juce::Justification::centredLeft);
        y += kLineHeight;

        // Entries
        g.setFont(14.0f);
        for (const auto& entry : section.entries)
        {
            // Key
            g.setColour(keyColor);
            g.drawText(entry.key, contentArea.getX(), y, kKeyWidth, kLineHeight,
                       juce::Justification::centredLeft);

            // Description
            g.setColour(textColor);
            g.drawText(entry.description, contentArea.getX() + kKeyWidth, y,
                       contentArea.getWidth() - kKeyWidth, kLineHeight,
                       juce::Justification::centredLeft);
            y += kLineHeight;
        }

        y += kSectionGap;
    }

    // Footer
    g.setColour(dimColor);
    g.setFont(13.0f);
    g.drawText("Press ? or Esc to close", contentArea.getX(), panelBounds.getBottom() - kPadding - kLineHeight,
               contentArea.getWidth(), kLineHeight, juce::Justification::centred);
}

void HelpPopup::resized()
{
    // Nothing special needed - we use getWidth()/getHeight() in paint
}

void HelpPopup::setContent(const std::vector<HelpSection>& sections)
{
    sections_ = sections;
    repaint();
}

void HelpPopup::show()
{
    setVisible(true);
    repaint();
}

void HelpPopup::hide()
{
    setVisible(false);
}

void HelpPopup::toggle()
{
    if (isVisible())
        hide();
    else
        show();
}

} // namespace ui
