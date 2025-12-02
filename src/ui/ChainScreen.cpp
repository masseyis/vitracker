#include "ChainScreen.h"

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

    auto area = getLocalBounds().reduced(20);

    // Header with chain name
    auto* chain = project_.getChain(currentChain_);
    g.setFont(20.0f);
    g.setColour(cursorRow_ == 0 ? cursorColor : fgColor);

    juce::String title = "CHAIN " + juce::String(currentChain_ + 1) + ": ";
    if (editingName_ && cursorRow_ == 0)
        title += juce::String(nameBuffer_) + "_";
    else if (chain)
        title += juce::String(chain->getName());
    else
        title += "---";

    g.drawText(title, area.removeFromTop(40), juce::Justification::centredLeft);

    area.removeFromTop(10);

    // Scale lock
    drawScaleLock(g, area.removeFromTop(30));

    area.removeFromTop(20);

    // Two columns: chain list (left), pattern list (right)
    auto leftCol = area.removeFromLeft(150);
    area.removeFromLeft(20);

    drawChainList(g, leftCol);
    drawPatternList(g, area);
}

void ChainScreen::drawChainList(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setFont(14.0f);
    g.setColour(fgColor);
    g.drawText("CHAINS", area.removeFromTop(25), juce::Justification::centredLeft);

    int numChains = project_.getChainCount();
    for (int i = 0; i < numChains && i < 20; ++i)
    {
        auto* chain = project_.getChain(i);
        bool isSelected = (i == currentChain_);

        g.setColour(isSelected ? cursorColor : fgColor.darker(0.3f));
        juce::String text = juce::String(i + 1) + ". " + (chain ? chain->getName() : "---");
        g.drawText(text, area.removeFromTop(20), juce::Justification::centredLeft);
    }
}

void ChainScreen::drawScaleLock(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto* chain = project_.getChain(currentChain_);
    if (!chain) return;

    g.setFont(14.0f);
    g.setColour(cursorRow_ == 1 ? cursorColor : fgColor);

    const std::string& scaleLock = chain->getScaleLock();
    juce::String scaleText = "Scale Lock: " + (scaleLock.empty() ? juce::String("None") : juce::String(scaleLock));
    g.drawText(scaleText, area, juce::Justification::centredLeft);
}

void ChainScreen::drawPatternList(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto* chain = project_.getChain(currentChain_);
    if (!chain) return;

    g.setFont(14.0f);
    g.setColour(fgColor);
    g.drawText("PATTERNS IN CHAIN", area.removeFromTop(25), juce::Justification::centredLeft);

    const auto& patternIndices = chain->getPatternIndices();
    for (size_t i = 0; i < patternIndices.size() && i < 32; ++i)
    {
        bool isSelected = (cursorRow_ == static_cast<int>(i) + 2);
        int patternIdx = patternIndices[i];

        g.setColour(isSelected ? cursorColor : fgColor);
        juce::String patternName = "---";
        if (auto* pattern = project_.getPattern(patternIdx))
            patternName = pattern->getName();

        juce::String text = juce::String(i + 1) + ". " + patternName;
        g.drawText(text, area.removeFromTop(20), juce::Justification::centredLeft);
    }

    // Add slot
    bool isAddSelected = (cursorRow_ == static_cast<int>(patternIndices.size()) + 2);
    g.setColour(isAddSelected ? cursorColor : fgColor.darker(0.5f));
    g.drawText("+ Add Pattern", area.removeFromTop(20), juce::Justification::centredLeft);
}

void ChainScreen::resized()
{
}

void ChainScreen::navigate(int dx, int dy)
{
    auto* chain = project_.getChain(currentChain_);

    if (dx != 0)
    {
        // Switch chains
        int numChains = project_.getChainCount();
        if (numChains > 0)
        {
            currentChain_ = (currentChain_ + dx + numChains) % numChains;
            cursorRow_ = std::min(cursorRow_, 2);  // Reset to safe position
        }
    }

    if (dy != 0)
    {
        int maxRow = 2;  // name, scale lock, first pattern slot
        if (chain)
            maxRow = chain->getPatternCount() + 2;

        cursorRow_ = std::clamp(cursorRow_ + dy, 0, maxRow);
    }

    repaint();
}

void ChainScreen::handleEdit(const juce::KeyPress& key)
{
    handleEditKey(key);
}

void ChainScreen::handleEditKey(const juce::KeyPress& key)
{
    auto* chain = project_.getChain(currentChain_);
    if (!chain) return;

    auto textChar = key.getTextCharacter();

    // Name editing (row 0)
    if (cursorRow_ == 0)
    {
        if (!editingName_)
        {
            editingName_ = true;
            nameBuffer_ = chain->getName();
        }

        if (key.getKeyCode() == juce::KeyPress::returnKey)
        {
            chain->setName(nameBuffer_);
            editingName_ = false;
        }
        else if (key.getKeyCode() == juce::KeyPress::backspaceKey && !nameBuffer_.empty())
        {
            nameBuffer_.pop_back();
        }
        else if (textChar >= ' ' && textChar <= '~' && nameBuffer_.length() < 16)
        {
            nameBuffer_ += static_cast<char>(textChar);
        }
    }
    // Scale lock (row 1)
    else if (cursorRow_ == 1)
    {
        int currentScale = getScaleIndex(chain->getScaleLock());
        if (textChar == '+' || textChar == '=' || key.getKeyCode() == juce::KeyPress::rightKey)
            chain->setScaleLock(getScaleName((currentScale + 1) % NUM_SCALES));
        else if (textChar == '-' || key.getKeyCode() == juce::KeyPress::leftKey)
            chain->setScaleLock(getScaleName((currentScale - 1 + NUM_SCALES) % NUM_SCALES));
    }
    // Pattern list (row 2+)
    else
    {
        int patternIdx = cursorRow_ - 2;
        int patternCount = chain->getPatternCount();

        if (patternIdx == patternCount)
        {
            // Add slot - Tab to add first available pattern
            if (key.getKeyCode() == juce::KeyPress::tabKey)
            {
                if (project_.getPatternCount() > 0)
                {
                    chain->addPattern(0);  // Add first pattern
                }
            }
        }
        else if (patternIdx < patternCount)
        {
            // Edit existing pattern reference
            if (key.getKeyCode() == juce::KeyPress::tabKey)
            {
                // Cycle through patterns
                int numPatterns = project_.getPatternCount();
                if (numPatterns > 0)
                {
                    const auto& indices = chain->getPatternIndices();
                    int currentPatIdx = indices[patternIdx];
                    int nextPatIdx = (currentPatIdx + 1) % numPatterns;
                    chain->setPattern(patternIdx, nextPatIdx);
                }
            }
            else if (textChar == 'd' || key.getKeyCode() == juce::KeyPress::deleteKey ||
                     key.getKeyCode() == juce::KeyPress::backspaceKey)
            {
                chain->removePattern(patternIdx);
                if (cursorRow_ > 2) cursorRow_--;
            }
        }
    }

    repaint();
}

} // namespace ui
