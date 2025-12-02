#include "SongScreen.h"

namespace ui {

SongScreen::SongScreen(model::Project& project, input::ModeManager& modeManager)
    : Screen(project, modeManager)
{
}

void SongScreen::paint(juce::Graphics& g)
{
    g.fillAll(bgColor);

    auto area = getLocalBounds();

    // Header
    g.setFont(20.0f);
    g.setColour(fgColor);
    g.drawText("SONG", area.removeFromTop(40).reduced(20, 0), juce::Justification::centredLeft);

    area.removeFromTop(10);
    drawGrid(g, area.reduced(10));
}

void SongScreen::drawGrid(juce::Graphics& g, juce::Rectangle<int> area)
{
    // Track headers
    int cellWidth = area.getWidth() / 16;
    int cellHeight = 24;
    auto headerArea = area.removeFromTop(cellHeight);

    g.setFont(12.0f);
    for (int t = 0; t < 16; ++t)
    {
        g.setColour(cursorTrack_ == t ? cursorColor : fgColor.darker(0.3f));
        g.drawText(juce::String(t + 1), headerArea.removeFromLeft(cellWidth),
                   juce::Justification::centred, true);
    }

    // Row numbers column
    area.removeFromLeft(30);

    // Grid rows
    for (int row = 0; row < VISIBLE_ROWS && (scrollOffset_ + row) < 64; ++row)
    {
        int songRow = scrollOffset_ + row;
        auto rowArea = area.removeFromTop(cellHeight);

        // Row number
        g.setColour(fgColor.darker(0.5f));
        g.setFont(10.0f);
        g.drawText(juce::String::toHexString(songRow).toUpperCase().paddedLeft('0', 2),
                   rowArea.getX() - 30, rowArea.getY(), 25, cellHeight,
                   juce::Justification::centredRight);

        for (int t = 0; t < 16; ++t)
        {
            auto cellArea = rowArea.removeFromLeft(cellWidth);
            drawCell(g, cellArea, t, songRow);
        }
    }
}

void SongScreen::drawCell(juce::Graphics& g, juce::Rectangle<int> area, int track, int row)
{
    bool isSelected = (cursorTrack_ == track && cursorRow_ == row);

    // Background
    if (isSelected)
    {
        g.setColour(cursorColor.withAlpha(0.3f));
        g.fillRect(area.reduced(1));
    }

    // Chain index
    int chainIdx = getChainAt(track, row);
    if (chainIdx >= 0)
    {
        auto* chain = project_.getChain(chainIdx);
        g.setColour(isSelected ? cursorColor : fgColor);
        g.setFont(10.0f);

        juce::String text = juce::String::toHexString(chainIdx).toUpperCase().paddedLeft('0', 2);
        if (chain)
            text = chain->getName().substr(0, 3);

        g.drawText(text, area, juce::Justification::centred, true);
    }
    else
    {
        g.setColour(highlightColor);
        g.setFont(10.0f);
        g.drawText("---", area, juce::Justification::centred, true);
    }

    // Cell border
    g.setColour(highlightColor);
    g.drawRect(area.reduced(1), 1);
}

int SongScreen::getChainAt(int track, int row) const
{
    const auto& trackData = project_.getSong().getTrack(track);
    if (row < static_cast<int>(trackData.size()))
        return trackData[row];
    return -1;
}

void SongScreen::setChainAt(int track, int row, int chainIndex)
{
    project_.getSong().setChain(track, row, chainIndex);
}

void SongScreen::resized()
{
}

void SongScreen::navigate(int dx, int dy)
{
    cursorTrack_ = std::clamp(cursorTrack_ + dx, 0, 15);
    cursorRow_ = std::clamp(cursorRow_ + dy, 0, 63);

    // Scroll if needed
    if (cursorRow_ < scrollOffset_)
        scrollOffset_ = cursorRow_;
    else if (cursorRow_ >= scrollOffset_ + VISIBLE_ROWS)
        scrollOffset_ = cursorRow_ - VISIBLE_ROWS + 1;

    repaint();
}

void SongScreen::handleEdit(const juce::KeyPress& key)
{
    handleEditKey(key);
}

void SongScreen::handleEditKey(const juce::KeyPress& key)
{
    auto textChar = key.getTextCharacter();

    // Tab cycles through chains
    if (key.getKeyCode() == juce::KeyPress::tabKey)
    {
        int chainCount = project_.getChainCount();
        if (chainCount == 0) return;

        int currentChain = getChainAt(cursorTrack_, cursorRow_);
        int nextChain = (currentChain + 1) % chainCount;
        if (currentChain < 0) nextChain = 0;

        setChainAt(cursorTrack_, cursorRow_, nextChain);
        repaint();
    }
    // Delete/clear with 'd' or Delete key
    else if (textChar == 'd' || key.getKeyCode() == juce::KeyPress::deleteKey ||
             key.getKeyCode() == juce::KeyPress::backspaceKey)
    {
        setChainAt(cursorTrack_, cursorRow_, -1);
        repaint();
    }
    // Number keys 0-9 for quick chain selection
    else if (textChar >= '0' && textChar <= '9')
    {
        int chainIdx = textChar - '0';
        if (chainIdx < project_.getChainCount())
        {
            setChainAt(cursorTrack_, cursorRow_, chainIdx);
        }
        repaint();
    }
}

} // namespace ui
