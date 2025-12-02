#include "PatternScreen.h"
#include <map>
#include <cctype>
#include <algorithm>

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
    // Forward to handleEditKey
    handleEditKey(key);
}

void PatternScreen::handleEditKey(const juce::KeyPress& key)
{
    auto* pattern = project_.getPattern(currentPattern_);
    if (!pattern) return;

    auto& step = pattern->getStep(cursorTrack_, cursorRow_);
    auto textChar = key.getTextCharacter();

    // Note entry using keyboard layout (like a piano)
    // Lower row: Z=C, X=D, C=E, V=F, B=G, N=A, M=B
    // Upper row: Q=C, W=D, E=E, R=F, T=G, Y=A, U=B (one octave up)
    // S=C#, D=D#, G=F#, H=G#, J=A#, 2=C#, 3=D#, 5=F#, 6=G#, 7=A#

    static const std::map<char, int> noteMap = {
        // Lower octave (octave 4)
        {'z', 48}, {'s', 49}, {'x', 50}, {'d', 51}, {'c', 52},
        {'v', 53}, {'g', 54}, {'b', 55}, {'h', 56}, {'n', 57},
        {'j', 58}, {'m', 59}, {',', 60},
        // Upper octave (octave 5)
        {'q', 60}, {'2', 61}, {'w', 62}, {'3', 63}, {'e', 64},
        {'r', 65}, {'5', 66}, {'t', 67}, {'6', 68}, {'y', 69},
        {'7', 70}, {'u', 71}, {'i', 72},
    };

    char lowerChar = static_cast<char>(std::tolower(textChar));

    auto it = noteMap.find(lowerChar);
    if (it != noteMap.end())
    {
        step.note = static_cast<int8_t>(it->second);
        if (step.instrument < 0) step.instrument = 0;  // Default instrument

        // Preview the note
        if (onNotePreview) onNotePreview(step.note, step.instrument);

        // Move down to next row
        cursorRow_ = std::min(cursorRow_ + 1, pattern->getLength() - 1);
        repaint();
        return;
    }

    // Backspace/Delete clears cell
    if (key.getKeyCode() == juce::KeyPress::backspaceKey ||
        key.getKeyCode() == juce::KeyPress::deleteKey)
    {
        step.clear();
        repaint();
        return;
    }

    // Period for note off
    if (textChar == '.')
    {
        step.note = model::Step::NOTE_OFF;
        cursorRow_ = std::min(cursorRow_ + 1, pattern->getLength() - 1);
        repaint();
        return;
    }

    // +/- to change octave of current note
    if (textChar == '+' || textChar == '=')
    {
        if (step.note >= 0 && step.note < 120) step.note += 12;
        repaint();
        return;
    }
    if (textChar == '-')
    {
        if (step.note >= 12) step.note -= 12;
        repaint();
        return;
    }
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

void PatternScreen::startSelection()
{
    selection_.startTrack = cursorTrack_;
    selection_.startRow = cursorRow_;
    selection_.endTrack = cursorTrack_;
    selection_.endRow = cursorRow_;
    hasSelection_ = true;
}

void PatternScreen::updateSelection()
{
    if (hasSelection_)
    {
        selection_.endTrack = cursorTrack_;
        selection_.endRow = cursorRow_;
    }
}

void PatternScreen::yankSelection()
{
    if (!hasSelection_) return;

    auto* pattern = project_.getPattern(currentPattern_);
    if (!pattern) return;

    int minT = selection_.minTrack();
    int maxT = selection_.maxTrack();
    int minR = selection_.minRow();
    int maxR = selection_.maxRow();

    std::vector<std::vector<model::Step>> data;
    data.resize(static_cast<size_t>(maxT - minT + 1));

    for (int t = minT; t <= maxT; ++t)
    {
        for (int r = minR; r <= maxR; ++r)
        {
            data[static_cast<size_t>(t - minT)].push_back(pattern->getStep(t, r));
        }
    }

    model::Clipboard::instance().copy(data);
    hasSelection_ = false;
    repaint();
}

void PatternScreen::deleteSelection()
{
    if (!hasSelection_) return;

    auto* pattern = project_.getPattern(currentPattern_);
    if (!pattern) return;

    int minT = selection_.minTrack();
    int maxT = selection_.maxTrack();
    int minR = selection_.minRow();
    int maxR = selection_.maxRow();

    for (int t = minT; t <= maxT; ++t)
    {
        for (int r = minR; r <= maxR; ++r)
        {
            pattern->getStep(t, r) = model::Step{};
        }
    }

    hasSelection_ = false;
    repaint();
}

void PatternScreen::paste()
{
    auto& clipboard = model::Clipboard::instance();
    if (clipboard.isEmpty()) return;

    auto* pattern = project_.getPattern(currentPattern_);
    if (!pattern) return;

    const auto& data = clipboard.getData();
    int width = clipboard.getWidth();
    int height = clipboard.getHeight();

    for (int t = 0; t < width && (cursorTrack_ + t) < 16; ++t)
    {
        for (int r = 0; r < height && (cursorRow_ + r) < pattern->getLength(); ++r)
        {
            pattern->getStep(cursorTrack_ + t, cursorRow_ + r) = data[static_cast<size_t>(t)][static_cast<size_t>(r)];
        }
    }

    repaint();
}

void PatternScreen::transpose(int semitones)
{
    auto* pattern = project_.getPattern(currentPattern_);
    if (!pattern) return;

    int minT = hasSelection_ ? selection_.minTrack() : cursorTrack_;
    int maxT = hasSelection_ ? selection_.maxTrack() : cursorTrack_;
    int minR = hasSelection_ ? selection_.minRow() : cursorRow_;
    int maxR = hasSelection_ ? selection_.maxRow() : cursorRow_;

    for (int t = minT; t <= maxT; ++t)
    {
        for (int r = minR; r <= maxR; ++r)
        {
            auto& step = pattern->getStep(t, r);
            if (step.note > 0 && step.note <= 127)  // Valid MIDI note
            {
                step.note = static_cast<int8_t>(std::clamp(static_cast<int>(step.note) + semitones, 0, 127));
            }
        }
    }

    repaint();
}

} // namespace ui
