#include "ChainScreen.h"
#include "HelpPopup.h"
#include "../audio/AudioEngine.h"
#include "../input/KeyHandler.h"

namespace ui {

static const char* scaleNames[] = {
    "None", "C Major", "C Minor", "C# Major", "C# Minor",
    "D Major", "D Minor", "D# Major", "D# Minor",
    "E Major", "E Minor", "F Major", "F Minor",
    "F# Major", "F# Minor", "G Major", "G Minor",
    "G# Major", "G# Minor", "A Major", "A Minor",
    "A# Major", "A# Minor", "B Major", "B Minor"
};

ChainScreen::ChainScreen(model::Project& project, input::ModeManager& modeManager)
    : Screen(project, modeManager)
{
}

int ChainScreen::getScaleIndex(const std::string& scale) const
{
    for (int i = 0; i < NUM_SCALES; ++i)
    {
        if (scale == scaleNames[i])
            return i;
    }
    return 0;  // None
}

std::string ChainScreen::getScaleName(int index) const
{
    if (index >= 0 && index < NUM_SCALES)
        return scaleNames[index];
    return scaleNames[0];
}

void ChainScreen::paint(juce::Graphics& g)
{
    g.fillAll(bgColor);

    auto area = getLocalBounds();

    // Chain selector (horizontal tabs)
    drawChainTabs(g, area.removeFromTop(30));

    // Header bar (matches Pattern screen style)
    auto headerArea = area.removeFromTop(30);
    g.setColour(headerColor);
    g.fillRect(headerArea);

    auto* chain = project_.getChain(currentChain_);
    if (!chain)
    {
        g.setColour(fgColor.darker(0.5f));
        g.drawText("No chains - press Ctrl+N to create one", area.reduced(20), juce::Justification::centred);
        return;
    }

    // Header text
    g.setColour(fgColor);
    g.setFont(18.0f);
    juce::String title = "CHAIN: ";
    if (editingName_)
        title += juce::String(nameBuffer_) + "_";
    else
        title += juce::String(chain->getName());
    title += juce::String(" [") + juce::String(chain->getPatternCount()) + " patterns]";
    g.drawText(title, headerArea.reduced(10, 0), juce::Justification::centredLeft, true);

    area = area.reduced(20);
    area.removeFromTop(10);

    // Scale lock
    drawScaleLock(g, area.removeFromTop(30));

    area.removeFromTop(15);

    // Pattern list (single column)
    drawPatternList(g, area);
}

void ChainScreen::drawChainTabs(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setFont(12.0f);

    int numChains = project_.getChainCount();
    if (numChains == 0)
    {
        g.setColour(fgColor.darker(0.5f));
        g.drawText("No chains - press Ctrl+N to create one", area, juce::Justification::centredLeft);
        return;
    }

    // Show chain tabs with bracket indicators
    g.setColour(fgColor.darker(0.5f));
    g.drawText("[", area.removeFromLeft(20), juce::Justification::centred);

    int tabWidth = 80;
    int maxTabs = std::min(numChains, 10);
    int startIdx = std::max(0, currentChain_ - 4);  // Keep current roughly centered

    for (int i = startIdx; i < startIdx + maxTabs && i < numChains; ++i)
    {
        auto* chain = project_.getChain(i);
        bool isSelected = (i == currentChain_);

        auto tabArea = area.removeFromLeft(tabWidth);

        if (isSelected)
        {
            g.setColour(cursorColor.withAlpha(0.3f));
            g.fillRect(tabArea.reduced(2));
        }

        g.setColour(isSelected ? cursorColor : fgColor.darker(0.3f));
        juce::String text = juce::String::toHexString(i).toUpperCase().paddedLeft('0', 2);
        if (isSelected && editingName_)
            text += ":" + juce::String(nameBuffer_).substring(0, 5) + "_";
        else if (chain && !chain->getName().empty())
            text += ":" + juce::String(chain->getName()).substring(0, 5);
        g.drawText(text, tabArea, juce::Justification::centred);
    }

    g.setColour(fgColor.darker(0.5f));
    g.drawText("]", area.removeFromLeft(20), juce::Justification::centred);
}

void ChainScreen::drawScaleLock(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto* chain = project_.getChain(currentChain_);
    if (!chain) return;

    g.setFont(14.0f);
    g.setColour(cursorRow_ == 0 ? cursorColor : fgColor);

    const std::string& scaleLock = chain->getScaleLock();
    juce::String scaleText = "Scale Lock: " + (scaleLock.empty() ? juce::String("None") : juce::String(scaleLock));
    g.drawText(scaleText, area, juce::Justification::centredLeft);
}

void ChainScreen::drawPatternList(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto* chain = project_.getChain(currentChain_);
    if (!chain) return;

    // Check if this chain is currently playing (in Song mode)
    int playheadPosition = -1;
    if (audioEngine_ && audioEngine_->isPlaying() &&
        audioEngine_->getPlayMode() == audio::AudioEngine::PlayMode::Song)
    {
        // Find if this chain is the one being played
        const auto& song = project_.getSong();
        int songRow = audioEngine_->getSongRow();
        if (songRow < song.getLength())
        {
            int playingChain = song.getTrack(0)[static_cast<size_t>(songRow)];
            if (playingChain == currentChain_)
            {
                playheadPosition = audioEngine_->getChainPosition();
            }
        }
    }

    // Column headers
    g.setFont(14.0f);
    g.setColour(fgColor);
    auto headerArea = area.removeFromTop(25);
    headerArea.removeFromLeft(30);  // Skip row number column
    g.drawText("PATTERN", headerArea.removeFromLeft(150), juce::Justification::centredLeft);
    g.drawText("TRANSPOSE", headerArea.removeFromLeft(80), juce::Justification::centred);

    int cellHeight = 24;
    int patternWidth = 150;
    int transposeWidth = 80;
    const auto& entries = chain->getEntries();

    for (size_t i = 0; i < entries.size() && i < 32; ++i)
    {
        bool isRowSelected = (cursorRow_ == static_cast<int>(i) + 1);  // Row 0 is scale lock
        bool isPlayhead = (static_cast<int>(i) == playheadPosition);
        const auto& entry = entries[i];

        auto rowArea = area.removeFromTop(cellHeight);

        // Playhead indicator
        if (isPlayhead)
        {
            g.setColour(playheadColor);
            g.fillRect(rowArea.getX() - 4, rowArea.getY(), 4, cellHeight);
        }

        // Row number
        g.setColour(isPlayhead ? playheadColor : fgColor.darker(0.5f));
        g.setFont(10.0f);
        g.drawText(juce::String(i + 1).paddedLeft('0', 2), rowArea.removeFromLeft(25),
                   juce::Justification::centredRight);
        rowArea.removeFromLeft(5);

        // Pattern cell
        auto patternCell = rowArea.removeFromLeft(patternWidth);
        bool isPatternSelected = isRowSelected && cursorColumn_ == 0;
        if (isPatternSelected)
        {
            g.setColour(cursorColor.withAlpha(0.3f));
            g.fillRect(patternCell.reduced(1));
        }

        g.setColour(isPlayhead ? playheadColor : (isPatternSelected ? cursorColor : fgColor));
        g.setFont(12.0f);
        juce::String text = juce::String::toHexString(entry.patternIndex).toUpperCase().paddedLeft('0', 2);
        if (auto* pattern = project_.getPattern(entry.patternIndex))
            text += ":" + juce::String(pattern->getName()).substring(0, 10);
        g.drawText(text, patternCell, juce::Justification::centredLeft);

        // Transpose cell
        auto transposeCell = rowArea.removeFromLeft(transposeWidth);
        bool isTransposeSelected = isRowSelected && cursorColumn_ == 1;
        if (isTransposeSelected)
        {
            g.setColour(cursorColor.withAlpha(0.3f));
            g.fillRect(transposeCell.reduced(1));
        }

        g.setColour(isPlayhead ? playheadColor : (isTransposeSelected ? cursorColor : fgColor.darker(0.2f)));
        g.setFont(12.0f);
        juce::String transposeText;
        if (entry.transpose > 0)
            transposeText = "+" + juce::String(entry.transpose);
        else if (entry.transpose < 0)
            transposeText = juce::String(entry.transpose);
        else
            transposeText = "0";
        g.drawText(transposeText, transposeCell, juce::Justification::centred);
    }

    // Empty slot for adding
    bool isAddSelected = (cursorRow_ == static_cast<int>(entries.size()) + 1);  // Row 0 is scale lock
    auto rowArea = area.removeFromTop(cellHeight);

    g.setColour(fgColor.darker(0.5f));
    g.setFont(10.0f);
    g.drawText(juce::String(entries.size() + 1).paddedLeft('0', 2),
               rowArea.removeFromLeft(25), juce::Justification::centredRight);
    rowArea.removeFromLeft(5);

    auto patternCell = rowArea.removeFromLeft(patternWidth);
    if (isAddSelected && cursorColumn_ == 0)
    {
        g.setColour(cursorColor.withAlpha(0.3f));
        g.fillRect(patternCell.reduced(1));
    }

    g.setColour(isAddSelected ? cursorColor : highlightColor);
    g.setFont(12.0f);
    g.drawText("---", patternCell, juce::Justification::centredLeft);
}

void ChainScreen::resized()
{
}

void ChainScreen::navigate(int dx, int dy)
{
    auto* chain = project_.getChain(currentChain_);

    if (dx != 0)
    {
        // On pattern rows (row >= 1), left/right switches between pattern and transpose columns
        if (cursorRow_ >= 1 && chain)
        {
            int slotIdx = cursorRow_ - 1;  // Row 0 is scale lock
            // Only allow column 1 (transpose) on existing entries, not empty slot
            if (slotIdx < chain->getPatternCount())
            {
                cursorColumn_ = std::clamp(cursorColumn_ + dx, 0, 1);
            }
            else
            {
                cursorColumn_ = 0;  // Empty slot only has pattern column
            }
        }
        else
        {
            // On scale lock row, left/right switches chains
            int numChains = project_.getChainCount();
            if (numChains > 0)
            {
                currentChain_ = (currentChain_ + dx + numChains) % numChains;
                cursorRow_ = std::min(cursorRow_, 1);  // Reset to safe position
            }
        }
    }

    if (dy != 0)
    {
        int maxRow = 1;  // scale lock, first pattern slot
        if (chain)
            maxRow = chain->getPatternCount() + 1;

        cursorRow_ = std::clamp(cursorRow_ + dy, 0, maxRow);

        // Reset column when moving to scale lock row
        if (cursorRow_ < 1)
            cursorColumn_ = 0;
    }

    repaint();
}

input::InputContext ChainScreen::getInputContext() const
{
    // Text editing mode takes priority
    if (editingName_)
        return input::InputContext::TextEdit;
    // Scale lock row uses horizontal params (left/right to adjust)
    if (cursorRow_ == 0)
        return input::InputContext::RowParams;
    // Pattern rows use grid context
    return input::InputContext::Grid;
}

bool ChainScreen::handleEdit(const juce::KeyPress& key)
{
    return handleEditKey(key);
}

bool ChainScreen::handleEditKey(const juce::KeyPress& key)
{
    // Use centralized key translation
    auto action = input::KeyHandler::translateKey(key, getInputContext(), false);

    // Handle name editing mode first
    if (editingName_)
    {
        auto* chain = project_.getChain(currentChain_);
        if (!chain)
        {
            editingName_ = false;
            repaint();
            return true;  // Consumed
        }

        switch (action.action)
        {
            case input::KeyAction::TextAccept:
                chain->setName(nameBuffer_);
                editingName_ = false;
                repaint();
                return true;

            case input::KeyAction::TextReject:
                editingName_ = false;
                repaint();
                return true;

            case input::KeyAction::TextBackspace:
                if (!nameBuffer_.empty())
                    nameBuffer_.pop_back();
                repaint();
                return true;

            case input::KeyAction::TextChar:
                if (nameBuffer_.length() < 16)
                    nameBuffer_ += action.charData;
                repaint();
                return true;

            default:
                return true;  // In name editing mode, consume all keys
        }
    }

    // Handle actions from translateKey()
    switch (action.action)
    {
        case input::KeyAction::PatternPrev:
            // '[' switch to previous chain
            {
                int numChains = project_.getChainCount();
                if (numChains > 0)
                    currentChain_ = (currentChain_ - 1 + numChains) % numChains;
                repaint();
            }
            return true;

        case input::KeyAction::PatternNext:
            // ']' switch to next chain
            {
                int numChains = project_.getChainCount();
                if (numChains > 0)
                    currentChain_ = (currentChain_ + 1) % numChains;
                repaint();
            }
            return true;

        case input::KeyAction::NewItem:
            // Shift+N creates new chain and enters edit mode
            {
                int newChain = project_.addChain("Chain " + std::to_string(project_.getChainCount() + 1));
                currentChain_ = newChain;
                auto* chain = project_.getChain(currentChain_);
                if (chain)
                {
                    editingName_ = true;
                    nameBuffer_ = chain->getName();
                }
                cursorRow_ = 0;
                repaint();
            }
            return true;

        case input::KeyAction::Rename:
            // 'r' starts renaming current chain
            {
                auto* chain = project_.getChain(currentChain_);
                if (chain)
                {
                    editingName_ = true;
                    nameBuffer_ = chain->getName();
                    repaint();
                    return true;
                }
            }
            return false;

        case input::KeyAction::NewSelection:
            // 'n' creates a new pattern and adds it to chain
            {
                auto* chain = project_.getChain(currentChain_);
                if (chain && cursorRow_ >= 1)
                {
                    int newPat = project_.addPattern("Pattern " + std::to_string(project_.getPatternCount() + 1));
                    chain->addPattern(newPat);
                    cursorRow_ = chain->getPatternCount();
                    cursorColumn_ = 0;
                    repaint();
                    return true;
                }
            }
            break;

        default:
            break;
    }

    auto* chain = project_.getChain(currentChain_);
    if (!chain) return false;

    // Scale lock (row 0) - h/l or left/right adjust scale
    if (cursorRow_ == 0)
    {
        int currentScale = getScaleIndex(chain->getScaleLock());
        // In RowParams context, h/l/left/right = Edit1Inc/Dec (Shift = coarse, skip 3)
        bool isEdit1Inc = (action.action == input::KeyAction::Edit1Inc ||
                           action.action == input::KeyAction::ShiftEdit1Inc);
        bool isEdit1Dec = (action.action == input::KeyAction::Edit1Dec ||
                           action.action == input::KeyAction::ShiftEdit1Dec);
        bool isCoarse = (action.action == input::KeyAction::ShiftEdit1Inc ||
                         action.action == input::KeyAction::ShiftEdit1Dec);
        int scaleDelta = isCoarse ? 3 : 1;

        if (isEdit1Inc)
        {
            chain->setScaleLock(getScaleName((currentScale + scaleDelta) % NUM_SCALES));
            repaint();
            return true;
        }
        else if (isEdit1Dec)
        {
            chain->setScaleLock(getScaleName((currentScale - scaleDelta + NUM_SCALES) % NUM_SCALES));
            repaint();
            return true;
        }
        // +/= (ZoomIn) and - (ZoomOut) for scale adjustment
        else if (action.action == input::KeyAction::ZoomIn)
        {
            chain->setScaleLock(getScaleName((currentScale + 1) % NUM_SCALES));
            repaint();
            return true;
        }
        else if (action.action == input::KeyAction::ZoomOut)
        {
            chain->setScaleLock(getScaleName((currentScale - 1 + NUM_SCALES) % NUM_SCALES));
            repaint();
            return true;
        }
        // Up/Down not consumed - let navigation handle moving to pattern rows
    }
    // Pattern list (row 1+)
    else
    {
        int slotIdx = cursorRow_ - 1;  // Row 0 is scale lock
        int patternCount = chain->getPatternCount();
        int numPatterns = project_.getPatternCount();

        // Enter (Confirm) jumps to the pattern screen (only from pattern column)
        if (action.action == input::KeyAction::Confirm && cursorColumn_ == 0)
        {
            int patternIdx = getPatternAtCursor();
            if (patternIdx >= 0 && onJumpToPattern)
            {
                onJumpToPattern(patternIdx);
                return true;  // Consumed when jumping
            }
            return false;  // Not consumed if no pattern to jump to
        }

        // Left/Right column switching is handled by navigate() - don't consume here

        // Transpose column (cursorColumn_ == 1)
        if (cursorColumn_ == 1 && slotIdx < patternCount)
        {
            // Alt+Up/Down (via translateKey actions) adjusts transpose
            // Shift variant = coarse (octave = 12 semitones)
            bool isTransposeInc = (action.action == input::KeyAction::Edit1Inc ||
                                   action.action == input::KeyAction::ShiftEdit1Inc);
            bool isTransposeDec = (action.action == input::KeyAction::Edit1Dec ||
                                   action.action == input::KeyAction::ShiftEdit1Dec);
            bool isCoarseTranspose = (action.action == input::KeyAction::ShiftEdit1Inc ||
                                      action.action == input::KeyAction::ShiftEdit1Dec);

            if (isTransposeInc || isTransposeDec)
            {
                int delta = isTransposeInc ? 1 : -1;
                if (isCoarseTranspose) delta *= 12;  // Octave jump
                int currentTranspose = chain->getTranspose(slotIdx);
                chain->setTranspose(slotIdx, std::clamp(currentTranspose + delta, -12, 12));
                repaint();
                return true;
            }

            // +/- (ZoomIn/ZoomOut) for transpose adjustment
            if (action.action == input::KeyAction::ZoomIn)
            {
                int currentTranspose = chain->getTranspose(slotIdx);
                chain->setTranspose(slotIdx, std::clamp(currentTranspose + 1, -12, 12));
                repaint();
                return true;
            }
            if (action.action == input::KeyAction::ZoomOut)
            {
                int currentTranspose = chain->getTranspose(slotIdx);
                chain->setTranspose(slotIdx, std::clamp(currentTranspose - 1, -12, 12));
                repaint();
                return true;
            }

            // '0' resets transpose to zero (via TextChar action)
            if ((action.action == input::KeyAction::TextChar || action.action == input::KeyAction::Jump) &&
                action.charData == '0')
            {
                chain->setTranspose(slotIdx, 0);
                repaint();
                return true;
            }

            // Delete resets transpose
            if (action.action == input::KeyAction::Delete)
            {
                chain->setTranspose(slotIdx, 0);
                repaint();
                return true;
            }
            return false;
        }

        // Pattern column (cursorColumn_ == 0)
        // Alt+Up/Down (via translateKey actions) cycles through available patterns
        // Shift variant = coarse (skip 4 patterns)
        bool isPatternInc = (action.action == input::KeyAction::Edit1Inc ||
                             action.action == input::KeyAction::ShiftEdit1Inc);
        bool isPatternDec = (action.action == input::KeyAction::Edit1Dec ||
                             action.action == input::KeyAction::ShiftEdit1Dec);
        bool isCoarsePattern = (action.action == input::KeyAction::ShiftEdit1Inc ||
                                action.action == input::KeyAction::ShiftEdit1Dec);

        if (isPatternInc || isPatternDec)
        {
            // In Grid context, Edit1Inc = up, Edit1Dec = down
            int delta = isPatternDec ? 1 : -1;
            if (isCoarsePattern) delta *= 4;  // Jump 4 patterns

            if (slotIdx < patternCount)
            {
                // Existing slot - cycle through patterns
                if (numPatterns > 0)
                {
                    const auto& indices = chain->getPatternIndices();
                    int currentPatIdx = indices[static_cast<size_t>(slotIdx)];
                    int nextPatIdx = (currentPatIdx + delta + numPatterns) % numPatterns;
                    chain->setPattern(slotIdx, nextPatIdx);
                }
            }
            else
            {
                // Empty slot - assign first/last pattern
                if (numPatterns > 0)
                {
                    int patIdx = (delta > 0) ? 0 : numPatterns - 1;
                    chain->addPattern(patIdx);
                }
                else if (delta > 0)
                {
                    // No patterns exist - create one
                    int newPat = project_.addPattern("Pattern " + std::to_string(project_.getPatternCount() + 1));
                    chain->addPattern(newPat);
                }
            }
            repaint();
            return true;
        }

        // Delete/clear (pattern column only)
        if (action.action == input::KeyAction::Delete)
        {
            if (slotIdx < patternCount)
            {
                chain->removePattern(slotIdx);
                if (cursorRow_ > 1) cursorRow_--;  // Row 0 is scale lock
                cursorColumn_ = 0;
            }
            repaint();
            return true;  // Consumed
        }

        // Number keys for quick pattern selection
        if (action.action == input::KeyAction::TextChar || action.action == input::KeyAction::Jump)
        {
            char c = action.charData;
            if (c >= '0' && c <= '9')
            {
                int patIdx = c - '0';
                if (patIdx < numPatterns)
                {
                    if (slotIdx < patternCount)
                        chain->setPattern(slotIdx, patIdx);
                    else
                        chain->addPattern(patIdx);
                }
                repaint();
                return true;  // Consumed
            }
        }
    }

    repaint();
    return false;  // Key not consumed
}

int ChainScreen::getPatternAtCursor() const
{
    if (cursorRow_ < 1) return -1;  // Not on pattern row (row 0 is scale lock)

    auto* chain = project_.getChain(currentChain_);
    if (!chain) return -1;

    int slotIdx = cursorRow_ - 1;  // Row 0 is scale lock
    const auto& indices = chain->getPatternIndices();
    if (slotIdx < static_cast<int>(indices.size()))
        return indices[static_cast<size_t>(slotIdx)];

    return -1;
}

std::vector<HelpSection> ChainScreen::getHelpContent() const
{
    return {
        {"Navigation", {
            {"Up/Down", "Move cursor vertically"},
            {"Left/Right", "Switch columns on pattern rows"},
            {"[  ]", "Previous/Next chain"},
            {"Enter", "Jump to pattern at cursor"},
        }},
        {"Scale Lock (row 0)", {
            {"Left/Right", "Change scale lock"},
            {"+  -", "Change scale lock"},
        }},
        {"Pattern Column", {
            {"Alt+Up/Down", "Cycle through patterns"},
            {"0-9", "Quick pattern selection"},
            {"n", "Create new pattern"},
            {"Delete/d", "Remove pattern from chain"},
        }},
        {"Transpose Column", {
            {"Alt+Up/Down", "Adjust transpose"},
            {"+  -", "Adjust transpose"},
            {"0", "Reset transpose to zero"},
            {"Delete/d", "Reset transpose to zero"},
        }},
        {"Chain Management", {
            {"Shift+N", "Create new chain"},
            {"r", "Rename chain"},
        }},
    };
}

} // namespace ui
