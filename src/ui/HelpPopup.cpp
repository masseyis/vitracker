#include "HelpPopup.h"

namespace ui {

HelpPopup::HelpPopup()
{
    setVisible(false);
    setAlwaysOnTop(true);
}

int HelpPopup::calculateContentHeight() const
{
    int height = 0;
    for (const auto& section : sections_)
    {
        height += kLineHeight;  // Section title
        height += static_cast<int>(section.entries.size()) * kLineHeight;
        height += kSectionGap;
    }
    return height;
}

bool HelpPopup::needsTwoColumns() const
{
    int availableHeight = kPanelMaxHeight - kPadding * 2 - 40 - kSectionGap - kLineHeight;
    return calculateContentHeight() > availableHeight;
}

void HelpPopup::paint(juce::Graphics& g)
{
    // Semi-transparent overlay background
    g.fillAll(juce::Colours::black.withAlpha(0.7f));

    bool twoColumns = needsTwoColumns();
    int panelWidth = twoColumns ? kTwoColumnWidth : kSingleColumnWidth;

    // Calculate panel dimensions
    int contentHeight = kPadding * 2;  // Top and bottom padding
    contentHeight += 40;  // Title
    contentHeight += kSectionGap;

    if (twoColumns)
    {
        // For two columns, find optimal split point
        int totalSectionHeight = calculateContentHeight();
        int targetColumnHeight = totalSectionHeight / 2;

        int leftHeight = 0;
        int rightHeight = 0;
        int splitIndex = 0;

        // Find best split point
        int runningHeight = 0;
        for (size_t i = 0; i < sections_.size(); ++i)
        {
            int sectionHeight = kLineHeight + static_cast<int>(sections_[i].entries.size()) * kLineHeight + kSectionGap;
            if (runningHeight + sectionHeight / 2 < targetColumnHeight)
            {
                runningHeight += sectionHeight;
                splitIndex = static_cast<int>(i) + 1;
            }
        }

        // Calculate column heights
        for (int i = 0; i < splitIndex && i < static_cast<int>(sections_.size()); ++i)
        {
            leftHeight += kLineHeight + static_cast<int>(sections_[i].entries.size()) * kLineHeight + kSectionGap;
        }
        for (int i = splitIndex; i < static_cast<int>(sections_.size()); ++i)
        {
            rightHeight += kLineHeight + static_cast<int>(sections_[i].entries.size()) * kLineHeight + kSectionGap;
        }

        contentHeight += std::max(leftHeight, rightHeight);
    }
    else
    {
        contentHeight += calculateContentHeight();
    }

    contentHeight += kLineHeight;  // "Press ? or Esc to close"

    int panelHeight = std::min(contentHeight, kPanelMaxHeight);
    int panelX = (getWidth() - panelWidth) / 2;
    int panelY = (getHeight() - panelHeight) / 2;

    auto panelBounds = juce::Rectangle<int>(panelX, panelY, panelWidth, panelHeight);

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
    g.setFont(juce::Font(18.0f).boldened());
    g.drawText(screenTitle_ + " - Help", contentArea.getX(), y, contentArea.getWidth(), 30,
               juce::Justification::centred);
    y += 40;

    if (twoColumns)
    {
        // Two-column layout
        int columnWidth = (contentArea.getWidth() - kColumnGap) / 2;

        // Find split point
        int totalSectionHeight = calculateContentHeight();
        int targetColumnHeight = totalSectionHeight / 2;
        int runningHeight = 0;
        size_t splitIndex = 0;

        for (size_t i = 0; i < sections_.size(); ++i)
        {
            int sectionHeight = kLineHeight + static_cast<int>(sections_[i].entries.size()) * kLineHeight + kSectionGap;
            if (runningHeight + sectionHeight / 2 < targetColumnHeight)
            {
                runningHeight += sectionHeight;
                splitIndex = i + 1;
            }
        }

        // Draw left column
        int leftY = y;
        for (size_t i = 0; i < splitIndex && i < sections_.size(); ++i)
        {
            const auto& section = sections_[i];

            // Section title
            g.setColour(dimColor);
            g.setFont(juce::Font(13.0f).boldened());
            g.drawText(section.title, contentArea.getX(), leftY, columnWidth, kLineHeight,
                       juce::Justification::centredLeft);
            leftY += kLineHeight;

            // Entries
            g.setFont(13.0f);
            for (const auto& entry : section.entries)
            {
                g.setColour(keyColor);
                g.drawText(entry.key, contentArea.getX(), leftY, kKeyWidth, kLineHeight,
                           juce::Justification::centredLeft);

                g.setColour(textColor);
                g.drawText(entry.description, contentArea.getX() + kKeyWidth, leftY,
                           columnWidth - kKeyWidth, kLineHeight,
                           juce::Justification::centredLeft);
                leftY += kLineHeight;
            }
            leftY += kSectionGap;
        }

        // Draw right column
        int rightX = contentArea.getX() + columnWidth + kColumnGap;
        int rightY = y;
        for (size_t i = splitIndex; i < sections_.size(); ++i)
        {
            const auto& section = sections_[i];

            // Section title
            g.setColour(dimColor);
            g.setFont(juce::Font(13.0f).boldened());
            g.drawText(section.title, rightX, rightY, columnWidth, kLineHeight,
                       juce::Justification::centredLeft);
            rightY += kLineHeight;

            // Entries
            g.setFont(13.0f);
            for (const auto& entry : section.entries)
            {
                g.setColour(keyColor);
                g.drawText(entry.key, rightX, rightY, kKeyWidth, kLineHeight,
                           juce::Justification::centredLeft);

                g.setColour(textColor);
                g.drawText(entry.description, rightX + kKeyWidth, rightY,
                           columnWidth - kKeyWidth, kLineHeight,
                           juce::Justification::centredLeft);
                rightY += kLineHeight;
            }
            rightY += kSectionGap;
        }
    }
    else
    {
        // Single column layout
        for (const auto& section : sections_)
        {
            // Section title
            g.setColour(dimColor);
            g.setFont(juce::Font(13.0f).boldened());
            g.drawText(section.title, contentArea.getX(), y, contentArea.getWidth(), kLineHeight,
                       juce::Justification::centredLeft);
            y += kLineHeight;

            // Entries
            g.setFont(13.0f);
            for (const auto& entry : section.entries)
            {
                g.setColour(keyColor);
                g.drawText(entry.key, contentArea.getX(), y, kKeyWidth, kLineHeight,
                           juce::Justification::centredLeft);

                g.setColour(textColor);
                g.drawText(entry.description, contentArea.getX() + kKeyWidth, y,
                           contentArea.getWidth() - kKeyWidth, kLineHeight,
                           juce::Justification::centredLeft);
                y += kLineHeight;
            }
            y += kSectionGap;
        }
    }

    // Footer
    g.setColour(dimColor);
    g.setFont(12.0f);
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
