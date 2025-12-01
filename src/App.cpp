#include "App.h"
#include "ui/PatternScreen.h"

App::App()
{
    setSize(1280, 800);
    setWantsKeyboardFocus(true);
    addKeyListener(this);

    // Create key handler
    keyHandler_ = std::make_unique<input::KeyHandler>(modeManager_);

    // Set up key handler callbacks
    keyHandler_->onScreenSwitch = [this](int screen) { switchScreen(screen - 1); };
    keyHandler_->onPlayStop = [this]() { isPlaying_ = !isPlaying_; repaint(); };
    keyHandler_->onNavigate = [this](int dx, int dy) {
        if (screens_[currentScreen_])
            screens_[currentScreen_]->navigate(dx, dy);
    };

    // Create screens (only PatternScreen for now, others will be placeholders)
    screens_[3] = std::make_unique<ui::PatternScreen>(project_, modeManager_);

    // Add active screen as child
    if (screens_[currentScreen_])
    {
        addAndMakeVisible(screens_[currentScreen_].get());
    }

    // Mode change callback
    modeManager_.onModeChanged = [this](input::Mode) { repaint(); };
}

App::~App()
{
    removeKeyListener(this);
}

void App::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));

    // Draw placeholder for screens not yet implemented
    if (!screens_[currentScreen_])
    {
        g.setColour(juce::Colours::white);
        g.setFont(24.0f);

        static const char* screenNames[] = {"PROJECT", "SONG", "CHAIN", "PATTERN", "INSTRUMENT", "MIXER"};
        g.drawText(juce::String(screenNames[currentScreen_]) + " (Coming Soon)",
                   getLocalBounds().reduced(0, STATUS_BAR_HEIGHT),
                   juce::Justification::centred, true);
    }

    // Status bar at bottom
    drawStatusBar(g, getLocalBounds().removeFromBottom(STATUS_BAR_HEIGHT));
}

void App::resized()
{
    auto area = getLocalBounds();
    area.removeFromBottom(STATUS_BAR_HEIGHT);

    for (auto& screen : screens_)
    {
        if (screen)
            screen->setBounds(area);
    }
}

bool App::keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent)
{
    juce::ignoreUnused(originatingComponent);

    bool handled = keyHandler_->handleKey(key);
    if (handled) repaint();
    return handled;
}

void App::switchScreen(int screenIndex)
{
    if (screenIndex < 0 || screenIndex >= 6) return;
    if (screenIndex == currentScreen_) return;

    // Hide old screen
    if (screens_[currentScreen_])
    {
        screens_[currentScreen_]->onExit();
        screens_[currentScreen_]->setVisible(false);
    }

    currentScreen_ = screenIndex;

    // Show new screen
    if (screens_[currentScreen_])
    {
        screens_[currentScreen_]->onEnter();
        screens_[currentScreen_]->setVisible(true);
        addAndMakeVisible(screens_[currentScreen_].get());
    }

    resized();
    repaint();
}

void App::drawStatusBar(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(juce::Colour(0xff2a2a4e));
    g.fillRect(area);

    g.setFont(14.0f);

    // Mode indicator (left)
    g.setColour(juce::Colours::white);
    g.drawText("-- " + juce::String(modeManager_.getModeString()) + " --",
               area.removeFromLeft(200), juce::Justification::centredLeft, true);

    // Transport (center)
    g.setColour(isPlaying_ ? juce::Colours::lightgreen : juce::Colours::grey);
    g.drawText(isPlaying_ ? "PLAYING" : "STOPPED",
               area.reduced(100, 0), juce::Justification::centred, true);

    // Tempo (right)
    g.setColour(juce::Colours::white);
    g.drawText(juce::String(project_.getTempo(), 1) + " BPM",
               area.removeFromRight(100), juce::Justification::centredRight, true);

    // Screen indicator
    static const char* screenKeys[] = {"1:PRJ", "2:SNG", "3:CHN", "4:PAT", "5:INS", "6:MIX"};
    int x = area.getX();
    for (int i = 0; i < 6; ++i)
    {
        g.setColour(i == currentScreen_ ? juce::Colours::yellow : juce::Colours::grey);
        g.drawText(screenKeys[i], x, area.getY(), 50, area.getHeight(),
                   juce::Justification::centred, true);
        x += 55;
    }
}
