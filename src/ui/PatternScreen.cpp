#include "PatternScreen.h"

namespace ui {

PatternScreen::PatternScreen(model::Project& project, input::ModeManager& modeManager)
    : Screen(project, modeManager)
{
}

void PatternScreen::paint(juce::Graphics& g)
{
    g.fillAll(bgColor);

    auto area = getLocalBounds();

    drawHeader(g, area.removeFromTop(HEADER_HEIGHT));
    drawTrackHeaders(g, area.removeFromTop(TRACK_HEADER_HEIGHT));
    drawGrid(g, area);
}

void PatternScreen::resized()
{
}

void PatternScreen::navigate(int dx, int dy)
{
    auto* pattern = project_.getPattern(currentPattern_);
    if (!pattern) return;

    if (dx != 0)
    {
        // Move between columns within track, then between tracks
        cursorColumn_ += dx;
        if (cursorColumn_ < 0)
        {
            cursorTrack_ = std::max(0, cursorTrack_ - 1);
            cursorColumn_ = 5;
        }
        else if (cursorColumn_ > 5)
        {
            cursorTrack_ = std::min(15, cursorTrack_ + 1);
            cursorColumn_ = 0;
        }
    }

    if (dy != 0)
    {
        cursorRow_ = std::clamp(cursorRow_ + dy, 0, pattern->getLength() - 1);
    }

    repaint();
}

void PatternScreen::handleEdit(const juce::KeyPress& key)
{
    // TODO: Implement value editing
    juce::ignoreUnused(key);
}

void PatternScreen::drawHeader(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(headerColor);
    g.fillRect(area);

    g.setColour(fgColor);
    g.setFont(18.0f);

    auto* pattern = project_.getPattern(currentPattern_);
    juce::String title = "PATTERN";
    if (pattern)
    {
        title += ": " + juce::String(pattern->getName());
        title += " [" + juce::String(pattern->getLength()) + " steps]";
    }

    g.drawText(title, area.reduced(10, 0), juce::Justification::centredLeft, true);
}

void PatternScreen::drawTrackHeaders(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(headerColor.darker(0.2f));
    g.fillRect(area);

    g.setFont(12.0f);

    int trackWidth = 0;
    for (int w : COLUMN_WIDTHS) trackWidth += w;
    trackWidth += 4;  // padding

    int x = 40;  // row number column
    for (int t = 0; t < 16 && x < area.getWidth(); ++t)
    {
        g.setColour(t == cursorTrack_ ? cursorColor : fgColor.darker(0.3f));
        g.drawText(juce::String(t + 1), x, area.getY(), trackWidth, area.getHeight(),
                   juce::Justification::centred, true);
        x += trackWidth;
    }
}

void PatternScreen::drawGrid(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto* pattern = project_.getPattern(currentPattern_);
    if (!pattern) return;

    int trackWidth = 0;
    for (int w : COLUMN_WIDTHS) trackWidth += w;
    trackWidth += 4;

    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));

    int y = area.getY();
    for (int row = 0; row < pattern->getLength() && y < area.getBottom(); ++row)
    {
        bool isCurrentRow = (row == cursorRow_);

        // Row number
        g.setColour(isCurrentRow ? cursorColor : fgColor.darker(0.5f));
        g.drawText(juce::String::toHexString(row).toUpperCase().paddedLeft('0', 2),
                   0, y, 36, ROW_HEIGHT, juce::Justification::centred, true);

        // Tracks
        int x = 40;
        for (int t = 0; t < 16 && x < area.getWidth(); ++t)
        {
            const auto& step = pattern->getStep(t, row);
            bool isCurrentCell = isCurrentRow && (t == cursorTrack_);

            juce::Rectangle<int> cellArea(x, y, trackWidth, ROW_HEIGHT);
            drawStep(g, cellArea, step, isCurrentRow, isCurrentCell);

            x += trackWidth;
        }

        y += ROW_HEIGHT;
    }
}

void PatternScreen::drawStep(juce::Graphics& g, juce::Rectangle<int> area,
                              const model::Step& step, bool isCurrentRow, bool isCurrentCell)
{
    // Background
    if (isCurrentCell)
    {
        g.setColour(highlightColor);
        g.fillRect(area);
    }
    else if (isCurrentRow)
    {
        g.setColour(bgColor.brighter(0.1f));
        g.fillRect(area);
    }

    int x = area.getX() + 2;
    int y = area.getY();
    int h = area.getHeight();

    // Note
    g.setColour(step.note >= 0 ? fgColor : fgColor.darker(0.6f));
    g.drawText(noteToString(step.note), x, y, COLUMN_WIDTHS[0], h, juce::Justification::centredLeft, true);
    x += COLUMN_WIDTHS[0];

    // Instrument
    g.setColour(step.instrument >= 0 ? fgColor : fgColor.darker(0.6f));
    juce::String instStr = step.instrument >= 0 ? juce::String::toHexString(step.instrument).toUpperCase().paddedLeft('0', 2) : "..";
    g.drawText(instStr, x, y, COLUMN_WIDTHS[1], h, juce::Justification::centredLeft, true);
    x += COLUMN_WIDTHS[1];

    // Volume
    g.setColour(step.volume < 0xFF ? fgColor : fgColor.darker(0.6f));
    juce::String volStr = step.volume < 0xFF ? juce::String::toHexString(step.volume).toUpperCase().paddedLeft('0', 2) : "..";
    g.drawText(volStr, x, y, COLUMN_WIDTHS[2], h, juce::Justification::centredLeft, true);
    x += COLUMN_WIDTHS[2];

    // FX columns (simplified for now)
    for (int fx = 0; fx < 3; ++fx)
    {
        g.setColour(fgColor.darker(0.6f));
        g.drawText("....", x, y, COLUMN_WIDTHS[3 + fx], h, juce::Justification::centredLeft, true);
        x += COLUMN_WIDTHS[3 + fx];
    }
}

std::string PatternScreen::noteToString(int8_t note) const
{
    if (note == model::Step::NOTE_EMPTY) return "...";
    if (note == model::Step::NOTE_OFF) return "OFF";

    static const char* noteNames[] = {"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"};
    int octave = note / 12;
    int noteName = note % 12;
    return std::string(noteNames[noteName]) + std::to_string(octave);
}

} // namespace ui
