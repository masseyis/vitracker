#include "SongScreen.h"
#include "../audio/AudioEngine.h"

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
    // Track headers - use fixed width for better readability
    int cellWidth = 80;  // Fixed width for chain cells (fits "00:Chain 1")
    int cellHeight = 24;
    auto headerArea = area.removeFromTop(cellHeight);

    // Check if playing in Song mode
    int playheadRow = -1;
    if (audioEngine_ && audioEngine_->isPlaying() &&
        audioEngine_->getPlayMode() == audio::AudioEngine::PlayMode::Song)
    {
        playheadRow = audioEngine_->getSongRow();
    }

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
        bool isPlayheadRow = (songRow == playheadRow);
        auto rowArea = area.removeFromTop(cellHeight);

        // Playhead indicator
        if (isPlayheadRow)
        {
            g.setColour(playheadColor);
            g.fillRect(rowArea.getX() - 34, rowArea.getY(), 4, cellHeight);
        }

        // Row number
        g.setColour(isPlayheadRow ? playheadColor : fgColor.darker(0.5f));
        g.setFont(10.0f);
        g.drawText(juce::String::toHexString(songRow).toUpperCase().paddedLeft('0', 2),
                   rowArea.getX() - 30, rowArea.getY(), 25, cellHeight,
                   juce::Justification::centredRight);

        for (int t = 0; t < 16; ++t)
        {
            auto cellArea = rowArea.removeFromLeft(cellWidth);
            drawCell(g, cellArea, t, songRow, isPlayheadRow);
        }
    }
}

void SongScreen::drawCell(juce::Graphics& g, juce::Rectangle<int> area, int track, int row, bool isPlayheadRow)
{
    bool isSelected = (cursorTrack_ == track && cursorRow_ == row);

    // Background
    if (isSelected)
    {
        g.setColour(cursorColor.withAlpha(0.3f));
        g.fillRect(area.reduced(1));
    }
    else if (isPlayheadRow)
    {
        g.setColour(playheadColor.withAlpha(0.15f));
        g.fillRect(area.reduced(1));
    }

    // Chain index - show as hex with name preview
    int chainIdx = getChainAt(track, row);
    if (chainIdx >= 0)
    {
        auto* chain = project_.getChain(chainIdx);
        g.setColour(isPlayheadRow ? playheadColor : (isSelected ? cursorColor : fgColor));
        g.setFont(11.0f);

        // Show hex index followed by abbreviated name
        juce::String text = juce::String::toHexString(chainIdx).toUpperCase().paddedLeft('0', 2);
        if (chain && !chain->getName().empty())
            text += ":" + juce::String(chain->getName()).substring(0, 7);

        g.drawText(text, area, juce::Justification::centred, true);
    }
    else
    {
        g.setColour(highlightColor);
        g.setFont(10.0f);
        g.drawText("---", area, juce::Justification::centred, true);
    }

    // Cell border
    g.setColour(isPlayheadRow ? playheadColor.withAlpha(0.5f) : highlightColor);
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

bool SongScreen::handleEdit(const juce::KeyPress& key)
{
    return handleEditKey(key);
}

bool SongScreen::handleEditKey(const juce::KeyPress& key)
{
    auto keyCode = key.getKeyCode();
    auto textChar = key.getTextCharacter();

    // Enter jumps to the chain screen with the selected chain
    if (keyCode == juce::KeyPress::returnKey)
    {
        int chainIdx = getChainAt(cursorTrack_, cursorRow_);
        if (chainIdx >= 0 && onJumpToChain)
        {
            onJumpToChain(chainIdx);
        }
        return false;
    }

    // Up/Down cycle through chains (like scrolling a list)
    if (keyCode == juce::KeyPress::upKey || keyCode == juce::KeyPress::downKey)
    {
        int chainCount = project_.getChainCount();
        int currentChain = getChainAt(cursorTrack_, cursorRow_);
        int delta = (keyCode == juce::KeyPress::downKey) ? 1 : -1;

        if (chainCount == 0)
        {
            // No chains exist - create one on down arrow
            if (delta > 0)
            {
                int newChain = project_.addChain("Chain " + std::to_string(project_.getChainCount() + 1));
                setChainAt(cursorTrack_, cursorRow_, newChain);
            }
        }
        else if (currentChain < 0)
        {
            // Empty cell - select first or last chain
            currentChain = (delta > 0) ? 0 : chainCount - 1;
            setChainAt(cursorTrack_, cursorRow_, currentChain);
        }
        else
        {
            // Cycle through chains, wrapping around
            currentChain = (currentChain + delta + chainCount) % chainCount;
            setChainAt(cursorTrack_, cursorRow_, currentChain);
        }
        repaint();
        return false;
    }

    // Left/Right do nothing in edit mode (no navigation)

    // 'n' or '+' creates a new chain and assigns it
    if (textChar == 'n' || textChar == '+' || textChar == '=')
    {
        int newChain = project_.addChain("Chain " + std::to_string(project_.getChainCount() + 1));
        setChainAt(cursorTrack_, cursorRow_, newChain);
        repaint();
        return false;
    }

    // Tab cycles through chains (same as down)
    if (keyCode == juce::KeyPress::tabKey)
    {
        int chainCount = project_.getChainCount();
        if (chainCount == 0)
        {
            int newChain = project_.addChain("Chain " + std::to_string(project_.getChainCount() + 1));
            setChainAt(cursorTrack_, cursorRow_, newChain);
        }
        else
        {
            int currentChain = getChainAt(cursorTrack_, cursorRow_);
            int nextChain = (currentChain + 1) % chainCount;
            if (currentChain < 0) nextChain = 0;
            setChainAt(cursorTrack_, cursorRow_, nextChain);
        }
        repaint();
        return false;
    }

    // Delete/clear with 'd' or Delete key
    if (textChar == 'd' || keyCode == juce::KeyPress::deleteKey ||
        keyCode == juce::KeyPress::backspaceKey)
    {
        setChainAt(cursorTrack_, cursorRow_, -1);
        repaint();
        return false;
    }

    // Number keys 0-9 for quick chain selection
    if (textChar >= '0' && textChar <= '9')
    {
        int chainIdx = textChar - '0';
        if (chainIdx < project_.getChainCount())
        {
            setChainAt(cursorTrack_, cursorRow_, chainIdx);
        }
        repaint();
        return false;
    }

    return false;
}

} // namespace ui
