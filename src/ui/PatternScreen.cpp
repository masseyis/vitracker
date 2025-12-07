#include "PatternScreen.h"
#include "HelpPopup.h"
#include "../audio/AudioEngine.h"
#include "../input/KeyHandler.h"
#include <algorithm>

namespace ui {

PatternScreen::PatternScreen(model::Project& project, input::ModeManager& modeManager)
    : Screen(project, modeManager)
{
    chordPopup_ = std::make_unique<ChordPopup>();
    addChildComponent(chordPopup_.get());
}

void PatternScreen::paint(juce::Graphics& g)
{
    g.fillAll(bgColor);

    auto area = getLocalBounds();

    // Pattern selector tabs at top
    drawPatternTabs(g, area.removeFromTop(30));
    drawHeader(g, area.removeFromTop(HEADER_HEIGHT - 10));
    drawTrackHeaders(g, area.removeFromTop(TRACK_HEADER_HEIGHT));
    drawGrid(g, area);
}

void PatternScreen::drawPatternTabs(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setFont(12.0f);

    int numPatterns = project_.getPatternCount();
    if (numPatterns == 0)
    {
        g.setColour(fgColor.darker(0.5f));
        g.drawText("No patterns - press Ctrl+N to create one", area.reduced(10, 0), juce::Justification::centredLeft);
        return;
    }

    g.setColour(fgColor.darker(0.5f));
    g.drawText("[", area.removeFromLeft(20), juce::Justification::centred);

    int tabWidth = 80;
    int maxTabs = std::min(numPatterns, 12);
    int startIdx = std::max(0, currentPattern_ - 5);

    for (int i = startIdx; i < startIdx + maxTabs && i < numPatterns; ++i)
    {
        auto* pattern = project_.getPattern(i);
        bool isSelected = (i == currentPattern_);

        auto tabArea = area.removeFromLeft(tabWidth);

        if (isSelected)
        {
            g.setColour(cursorColor.withAlpha(0.3f));
            g.fillRect(tabArea.reduced(2));
        }

        g.setColour(isSelected ? cursorColor : fgColor.darker(0.3f));
        juce::String text = juce::String::toHexString(i).toUpperCase().paddedLeft('0', 2);
        if (pattern && !pattern->getName().empty())
            text += ":" + juce::String(pattern->getName()).substring(0, 5);
        g.drawText(text, tabArea, juce::Justification::centred);
    }

    g.setColour(fgColor.darker(0.5f));
    g.drawText("]", area.removeFromLeft(20), juce::Justification::centred);
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
            if (cursorTrack_ > 0)
            {
                cursorTrack_--;
                cursorColumn_ = 5;
            }
            else
            {
                // At leftmost edge - switch to previous pattern
                int numPatterns = project_.getPatternCount();
                if (numPatterns > 1)
                {
                    currentPattern_ = (currentPattern_ - 1 + numPatterns) % numPatterns;
                    cursorTrack_ = 15;  // Go to rightmost track
                    cursorColumn_ = 5;
                }
                else
                {
                    cursorColumn_ = 0;  // Stay put
                }
            }
        }
        else if (cursorColumn_ > 5)
        {
            if (cursorTrack_ < 15)
            {
                cursorTrack_++;
                cursorColumn_ = 0;
            }
            else
            {
                // At rightmost edge - switch to next pattern
                int numPatterns = project_.getPatternCount();
                if (numPatterns > 1)
                {
                    currentPattern_ = (currentPattern_ + 1) % numPatterns;
                    cursorTrack_ = 0;  // Go to leftmost track
                    cursorColumn_ = 0;
                }
                else
                {
                    cursorColumn_ = 5;  // Stay put
                }
            }
        }
    }

    if (dy != 0)
    {
        cursorRow_ = std::clamp(cursorRow_ + dy, 0, pattern->getLength() - 1);
    }

    repaint();
}

bool PatternScreen::handleEdit(const juce::KeyPress& key)
{
    // Forward to handleEditKey
    return handleEditKey(key);
}

bool PatternScreen::handleEditKey(const juce::KeyPress& key)
{
    auto textChar = key.getTextCharacter();  // Still used for hex input (0-9, a-f)
    bool shiftHeld = key.getModifiers().isShiftDown();

    // Use translateKey to detect Alt+hjkl for edit operations (macOS Alt produces special chars)
    auto action = input::KeyHandler::translateKey(key, input::InputContext::Grid,
                                                   modeManager_.getMode() == input::Mode::Visual);
    // Edit actions from translateKey - direction-based (Edit1=vertical, Edit2=horizontal)
    bool isEdit1Inc = (action.action == input::KeyAction::Edit1Inc ||
                       action.action == input::KeyAction::ShiftEdit1Inc);
    bool isEdit1Dec = (action.action == input::KeyAction::Edit1Dec ||
                       action.action == input::KeyAction::ShiftEdit1Dec);
    bool isEdit2Inc = (action.action == input::KeyAction::Edit2Inc ||
                       action.action == input::KeyAction::ShiftEdit2Inc);
    bool isEdit2Dec = (action.action == input::KeyAction::Edit2Dec ||
                       action.action == input::KeyAction::ShiftEdit2Dec);

    // Combined edit flags (for code that doesn't care about direction)
    bool isEditInc = isEdit1Inc || isEdit2Inc;
    bool isEditDec = isEdit1Dec || isEdit2Dec;

    // For FX columns: Edit1 = vertical (type cycling), Edit2 = horizontal (value adjust)
    bool isVerticalEditInc = isEdit1Inc;
    bool isVerticalEditDec = isEdit1Dec;
    bool isHorizontalEditInc = isEdit2Inc;
    bool isHorizontalEditDec = isEdit2Dec;

    // Coarse adjustment detection (Shift variants)
    bool isCoarseEdit = (action.action == input::KeyAction::ShiftEdit1Inc ||
                         action.action == input::KeyAction::ShiftEdit1Dec ||
                         action.action == input::KeyAction::ShiftEdit2Inc ||
                         action.action == input::KeyAction::ShiftEdit2Dec);

    // Handle name editing mode first
    if (editingName_)
    {
        auto* pattern = project_.getPattern(currentPattern_);
        if (!pattern)
        {
            editingName_ = false;
            repaint();
            return true;  // Consumed
        }

        switch (action.action)
        {
            case input::KeyAction::TextAccept:
                pattern->setName(nameBuffer_);
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

    // Tab: move to next track (stay on same column type)
    // Shift+Tab: move to previous track
    if (action.action == input::KeyAction::TabNext)
    {
        if (cursorTrack_ < 15) cursorTrack_++;
        repaint();
        return true;
    }
    if (action.action == input::KeyAction::TabPrev)
    {
        if (cursorTrack_ > 0) cursorTrack_--;
        repaint();
        return true;
    }

    // Visual mode special keys: 'f' for fill, 's' for shuffle
    // Must be in Visual mode, not just have a selection
    if (modeManager_.getMode() == input::Mode::Visual && hasSelection_ &&
        (action.action == input::KeyAction::Fill || action.action == input::KeyAction::Shuffle))
    {
        auto* pattern = project_.getPattern(currentPattern_);
        if (pattern)
        {
            int minT = selection_.minTrack();
            int maxT = selection_.maxTrack();
            int minR = selection_.minRow();
            int maxR = std::min(selection_.maxRow(), pattern->getLength() - 1);

            if (action.action == input::KeyAction::Fill)
            {
                // Fill following established pattern
                // For each track, collect populated values and repeat the sequence
                for (int track = minT; track <= maxT; ++track)
                {
                    // Collect populated values with their relative positions
                    std::vector<std::pair<int, model::Step>> populated;  // relative row, step
                    for (int row = minR; row <= maxR; ++row)
                    {
                        const auto& step = pattern->getStep(track, row);
                        bool hasData = false;
                        if (cursorColumn_ == 0 && step.note > 0 && step.note <= 127) hasData = true;
                        else if (cursorColumn_ == 1 && step.instrument >= 0) hasData = true;
                        else if (cursorColumn_ == 2 && step.volume != 0xFF) hasData = true;
                        else if (cursorColumn_ >= 3)
                        {
                            const model::FXCommand* fx = (cursorColumn_ == 3) ? &step.fx1 :
                                                         (cursorColumn_ == 4) ? &step.fx2 : &step.fx3;
                            if (fx->type != model::FXType::None) hasData = true;
                        }
                        if (hasData) populated.push_back({row - minR, step});  // Store relative position
                    }

                    if (populated.empty()) continue;

                    // Calculate the pattern period (distance from first to just after last populated)
                    int patternPeriod = populated.back().first + 1;
                    if (populated.size() >= 2)
                    {
                        // If we have multiple values, use the span as the period
                        patternPeriod = populated.back().first - populated.front().first + 1;
                    }

                    // Detect note intervals for melodic continuation
                    std::vector<int> noteIntervals;
                    if (cursorColumn_ == 0 && populated.size() >= 2)
                    {
                        for (size_t i = 1; i < populated.size(); ++i)
                        {
                            noteIntervals.push_back(populated[i].second.note - populated[i-1].second.note);
                        }
                    }

                    // Fill the entire selection by repeating the pattern
                    for (int row = minR; row <= maxR; ++row)
                    {
                        auto& step = pattern->getStep(track, row);
                        int relRow = row - minR;

                        // Check if this row needs filling
                        bool isEmpty = false;
                        if (cursorColumn_ == 0) isEmpty = (step.note <= 0 || step.note > 127);
                        else if (cursorColumn_ == 1) isEmpty = (step.instrument < 0);
                        else if (cursorColumn_ == 2) isEmpty = (step.volume == 0xFF);
                        else if (cursorColumn_ >= 3)
                        {
                            const model::FXCommand* fx = (cursorColumn_ == 3) ? &step.fx1 :
                                                         (cursorColumn_ == 4) ? &step.fx2 : &step.fx3;
                            isEmpty = (fx->type == model::FXType::None);
                        }

                        if (isEmpty)
                        {
                            // Find which pattern element should go here
                            // by matching relative position within the pattern period
                            int posInPeriod = relRow % patternPeriod;
                            int repetition = relRow / patternPeriod;

                            // Find the source element at this position in the pattern
                            for (size_t i = 0; i < populated.size(); ++i)
                            {
                                int srcRelPos = populated[i].first - populated.front().first;
                                if (srcRelPos == posInPeriod)
                                {
                                    const auto& srcStep = populated[i].second;

                                    if (cursorColumn_ == 0)
                                    {
                                        // For notes, apply cumulative interval for continuation
                                        if (!noteIntervals.empty() && repetition > 0)
                                        {
                                            // Calculate total interval shift for this repetition
                                            int totalShift = 0;
                                            for (int rep = 0; rep < repetition; ++rep)
                                            {
                                                for (int interval : noteIntervals)
                                                    totalShift += interval;
                                            }
                                            // Add intervals up to current position
                                            for (size_t j = 0; j < i && j < noteIntervals.size(); ++j)
                                                totalShift += noteIntervals[j];

                                            int newNote = populated.front().second.note + totalShift;
                                            step.note = static_cast<int8_t>(std::clamp(newNote, 1, 127));
                                        }
                                        else
                                        {
                                            step.note = srcStep.note;
                                        }
                                        if (step.instrument < 0 && srcStep.instrument >= 0)
                                            step.instrument = srcStep.instrument;
                                    }
                                    else if (cursorColumn_ == 1)
                                    {
                                        step.instrument = srcStep.instrument;
                                    }
                                    else if (cursorColumn_ == 2)
                                    {
                                        step.volume = srcStep.volume;
                                    }
                                    else if (cursorColumn_ >= 3)
                                    {
                                        model::FXCommand* dstFx = (cursorColumn_ == 3) ? &step.fx1 :
                                                                  (cursorColumn_ == 4) ? &step.fx2 : &step.fx3;
                                        const model::FXCommand* srcFx = (cursorColumn_ == 3) ? &srcStep.fx1 :
                                                                        (cursorColumn_ == 4) ? &srcStep.fx2 : &srcStep.fx3;
                                        *dstFx = *srcFx;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            else if (action.action == input::KeyAction::Shuffle)
            {
                // Randomize populated values with valid random values
                std::string scaleLock = getScaleLockForPattern(currentPattern_);
                int numInstruments = project_.getInstrumentCount();

                // Build scale note array for random note selection
                std::vector<int> scaleNotes;
                // Parse scale (simplified - assumes format like "C Major" or "A Minor")
                int rootNote = 0;  // C
                bool isMinor = (scaleLock.find("Minor") != std::string::npos);

                static const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
                for (int i = 0; i < 12; ++i)
                {
                    if (scaleLock.find(noteNames[i]) == 0)
                    {
                        rootNote = i;
                        break;
                    }
                }

                // Generate scale notes across MIDI range
                static const int majorIntervals[] = {0, 2, 4, 5, 7, 9, 11};
                static const int minorIntervals[] = {0, 2, 3, 5, 7, 8, 10};
                const int* intervals = isMinor ? minorIntervals : majorIntervals;

                for (int octave = 0; octave < 11; ++octave)
                {
                    for (int i = 0; i < 7; ++i)
                    {
                        int note = octave * 12 + rootNote + intervals[i];
                        if (note >= 1 && note <= 127)
                            scaleNotes.push_back(note);
                    }
                }

                juce::Random random;

                for (int track = minT; track <= maxT; ++track)
                {
                    for (int row = minR; row <= maxR; ++row)
                    {
                        auto& step = pattern->getStep(track, row);

                        if (cursorColumn_ == 0)
                        {
                            // Randomize notes (only if populated)
                            if (step.note > 0 && step.note <= 127 && !scaleNotes.empty())
                            {
                                // Keep roughly in same octave range
                                int currentOctave = step.note / 12;
                                std::vector<int> nearbyNotes;
                                for (int n : scaleNotes)
                                {
                                    if (std::abs(n / 12 - currentOctave) <= 1)
                                        nearbyNotes.push_back(n);
                                }
                                if (!nearbyNotes.empty())
                                    step.note = static_cast<int8_t>(nearbyNotes[random.nextInt(static_cast<int>(nearbyNotes.size()))]);
                            }
                        }
                        else if (cursorColumn_ == 1)
                        {
                            // Randomize instruments (only if populated)
                            if (step.instrument >= 0 && numInstruments > 0)
                            {
                                step.instrument = static_cast<int8_t>(random.nextInt(numInstruments));
                            }
                        }
                        else if (cursorColumn_ == 2)
                        {
                            // Randomize volume (only if populated)
                            if (step.volume != 0xFF)
                            {
                                // Keep in reasonable range (0x40-0xFF for audible)
                                step.volume = static_cast<uint8_t>(0x40 + random.nextInt(0xBF));
                            }
                        }
                        else if (cursorColumn_ >= 3)
                        {
                            // Randomize FX (only if populated)
                            model::FXCommand* fx = (cursorColumn_ == 3) ? &step.fx1 :
                                                   (cursorColumn_ == 4) ? &step.fx2 : &step.fx3;
                            if (fx->type != model::FXType::None)
                            {
                                // Keep same FX type, randomize value
                                fx->value = static_cast<uint8_t>(random.nextInt(256));
                            }
                        }
                    }
                }
            }

            repaint();
            return true;
        }
    }

    // Alt+Up/Down (or Alt+j/k) in Visual mode: batch edit all populated values in selection
    if (modeManager_.getMode() == input::Mode::Visual && hasSelection_ &&
        (isEditInc || isEditDec))
    {
        auto* pattern = project_.getPattern(currentPattern_);
        if (pattern)
        {
            int direction = isEditInc ? 1 : -1;
            int minT = selection_.minTrack();
            int maxT = selection_.maxTrack();
            int minR = selection_.minRow();
            int maxR = std::min(selection_.maxRow(), pattern->getLength() - 1);

            bool shiftHeld = key.getModifiers().isShiftDown();

            for (int track = minT; track <= maxT; ++track)
            {
                for (int row = minR; row <= maxR; ++row)
                {
                    auto& step = pattern->getStep(track, row);

                    if (cursorColumn_ == 0)
                    {
                        // Note column: transpose in scale (or by octave with Shift)
                        if (step.note > 0 && step.note <= 127)
                        {
                            if (shiftHeld)
                            {
                                // Octave jump
                                int newNote = step.note + direction * 12;
                                if (newNote >= 0 && newNote <= 127)
                                    step.note = static_cast<int8_t>(newNote);
                            }
                            else
                            {
                                // Scale-aware movement
                                std::string scaleLock = getScaleLockForPattern(currentPattern_);
                                step.note = static_cast<int8_t>(moveNoteInScale(step.note, direction, scaleLock));
                            }
                        }
                    }
                    else if (cursorColumn_ == 1)
                    {
                        // Instrument column: cycle through instruments
                        int numInstruments = project_.getInstrumentCount();
                        if (numInstruments > 0 && step.instrument >= 0)
                        {
                            step.instrument = static_cast<int8_t>((step.instrument + direction + numInstruments) % numInstruments);
                        }
                    }
                    else if (cursorColumn_ == 2)
                    {
                        // Volume column: adjust volume
                        if (step.volume != 0xFF)  // Only adjust if volume is set
                        {
                            int delta = direction * 16;  // Coarse adjustment
                            step.volume = static_cast<uint8_t>(std::clamp(static_cast<int>(step.volume) + delta, 0, 0xFE));
                        }
                    }
                    else
                    {
                        // FX columns: cycle FX type
                        model::FXCommand* fx = nullptr;
                        if (cursorColumn_ == 3) fx = &step.fx1;
                        else if (cursorColumn_ == 4) fx = &step.fx2;
                        else if (cursorColumn_ == 5) fx = &step.fx3;

                        if (fx && fx->type != model::FXType::None)
                        {
                            int currentType = static_cast<int>(fx->type);
                            int numTypes = 7;  // None + 6 types
                            int newType = (currentType + direction + numTypes) % numTypes;
                            if (newType == 0) newType = (direction > 0) ? 1 : numTypes - 1;  // Skip None
                            fx->type = static_cast<model::FXType>(newType);
                        }
                    }
                }
            }
            repaint();
            return true;
        }
    }

    // Pattern switching
    if (action.action == input::KeyAction::PatternPrev)
    {
        int numPatterns = project_.getPatternCount();
        if (numPatterns > 0)
            currentPattern_ = (currentPattern_ - 1 + numPatterns) % numPatterns;
        repaint();
        return true;
    }
    if (action.action == input::KeyAction::PatternNext)
    {
        int numPatterns = project_.getPatternCount();
        if (numPatterns > 0)
            currentPattern_ = (currentPattern_ + 1) % numPatterns;
        repaint();
        return true;
    }

    // Chord popup ('c' is passed as TextChar from translateKey)
    if (action.action == input::KeyAction::TextChar && action.charData == 'c' && !shiftHeld)
    {
        showChordPopup();
        return true;
    }

    // Zoom actions for pattern length
    if (action.action == input::KeyAction::ZoomIn)
    {
        auto* pat = project_.getPattern(currentPattern_);
        if (pat && pat->getLength() < model::Pattern::MAX_LENGTH)
        {
            pat->setLength(pat->getLength() + 1);
            repaint();
        }
        return true;
    }
    if (action.action == input::KeyAction::ZoomOut)
    {
        auto* pat = project_.getPattern(currentPattern_);
        if (pat && pat->getLength() > 1)
        {
            pat->setLength(pat->getLength() - 1);
            if (cursorRow_ >= pat->getLength())
                cursorRow_ = pat->getLength() - 1;
            repaint();
        }
        return true;
    }

    // Shift+N creates new pattern and enters edit mode
    if (action.action == input::KeyAction::NewItem)
    {
        currentPattern_ = project_.addPattern("Pattern " + std::to_string(project_.getPatternCount() + 1));
        auto* pattern = project_.getPattern(currentPattern_);
        if (pattern)
        {
            editingName_ = true;
            nameBuffer_ = pattern->getName();
        }
        repaint();
        return false;
    }

    // 'r' starts renaming current pattern
    if (action.action == input::KeyAction::Rename)
    {
        auto* pattern = project_.getPattern(currentPattern_);
        if (pattern)
        {
            editingName_ = true;
            nameBuffer_ = pattern->getName();
            repaint();
            return true;
        }
        return false;
    }

    auto* pattern = project_.getPattern(currentPattern_);

    // Handle Ctrl+N to create pattern if none exists (legacy behavior for 'n')
    if (!pattern)
    {
        repaint();
        return false;
    }

    auto& step = pattern->getStep(cursorTrack_, cursorRow_);

    // Column-specific edit behavior
    // Column 0 = Note, 1 = Instrument, 2 = Volume, 3-5 = FX

    if (cursorColumn_ == 0)
    {
        // NOTE COLUMN: Alt+Up/Down creates note or moves in scale, plain arrows navigate

        // Shift+Up/Down or Alt+Shift+j/k: octave stepping (ShiftEdit1 = coarse vertical)
        bool isOctaveUp = (action.action == input::KeyAction::ShiftEdit1Inc);
        bool isOctaveDown = (action.action == input::KeyAction::ShiftEdit1Dec);
        if (isOctaveUp || isOctaveDown)
        {
            int direction = isOctaveUp ? 12 : -12;

            // Create note if empty, copying from last non-empty row
            if (step.note <= 0 || step.note > 127)
            {
                const model::Step* prevStep = findLastNonEmptyRowAbove(cursorTrack_, cursorRow_);
                if (prevStep && prevStep->note >= 0)
                {
                    // Copy all columns from previous step
                    step = *prevStep;
                }
                else
                {
                    step.note = 60;  // C-4 default
                    if (step.instrument < 0) step.instrument = 0;
                }
            }
            else
            {
                // Move by octave
                int newNote = step.note + direction;
                if (newNote >= 0 && newNote <= 127)
                    step.note = static_cast<int8_t>(newNote);
            }

            if (onNotePreview) onNotePreview(step.note, step.instrument >= 0 ? step.instrument : 0);
            repaint();
            return true;  // Consumed
        }

        // Alt+Up/Down or Alt+j/k: create note if empty, then move in scale
        bool isScaleUp = (action.action == input::KeyAction::Edit1Inc);
        bool isScaleDown = (action.action == input::KeyAction::Edit1Dec);
        if (isScaleUp || isScaleDown)
        {
            int direction = isScaleUp ? 1 : -1;

            // Create note if empty, copying from last non-empty row
            if (step.note <= 0 || step.note > 127)
            {
                const model::Step* prevStep = findLastNonEmptyRowAbove(cursorTrack_, cursorRow_);
                if (prevStep && prevStep->note >= 0)
                {
                    // Copy all columns from previous step
                    step = *prevStep;
                }
                else
                {
                    step.note = 60;  // C-4 default
                    if (step.instrument < 0) step.instrument = 0;
                }
            }
            else
            {
                // Move in scale (or by semitone if no scale lock)
                std::string scaleLock = getScaleLockForPattern(currentPattern_);
                step.note = static_cast<int8_t>(moveNoteInScale(step.note, direction, scaleLock));
            }

            if (onNotePreview) onNotePreview(step.note, step.instrument >= 0 ? step.instrument : 0);
            repaint();
            return true;  // Consumed
        }

        // Plain Left/Right not consumed - let navigation handle
        // (handleEditKey is called before navigation for Left/Right)

        // 'n' (NewSelection) adds note, copying from last non-empty row above
        if (action.action == input::KeyAction::NewSelection)
        {
            const model::Step* prevStep = findLastNonEmptyRowAbove(cursorTrack_, cursorRow_);
            if (prevStep && prevStep->note >= 0)
            {
                // Copy all columns from previous step
                step = *prevStep;
            }
            else
            {
                step.note = 60;  // C-4 default
                if (step.instrument < 0) step.instrument = 0;
            }
            if (onNotePreview) onNotePreview(step.note, step.instrument >= 0 ? step.instrument : 0);
            cursorRow_ = std::min(cursorRow_ + 1, pattern->getLength() - 1);
            repaint();
            return true;
        }

        // Period (SecondaryNext = '.') for note off
        if (action.action == input::KeyAction::SecondaryNext)
        {
            step.note = model::Step::NOTE_OFF;
            cursorRow_ = std::min(cursorRow_ + 1, pattern->getLength() - 1);
            repaint();
            return false;
        }

    }
    else if (cursorColumn_ == 1)
    {
        // INSTRUMENT COLUMN: Alt+Up/Down or Alt+j/k cycles instruments, 'n' creates new

        if (isEditInc || isEditDec)
        {
            int delta = isEditDec ? 1 : -1;
            int numInstruments = project_.getInstrumentCount();

            if (numInstruments == 0)
            {
                if (delta > 0)
                {
                    project_.addInstrument("Inst " + std::to_string(project_.getInstrumentCount() + 1));
                    step.instrument = 0;
                }
            }
            else if (step.instrument < 0)
            {
                step.instrument = (delta > 0) ? 0 : static_cast<int8_t>(numInstruments - 1);
            }
            else
            {
                step.instrument = static_cast<int8_t>((step.instrument + delta + numInstruments) % numInstruments);
            }
            if (step.note >= 0 && onNotePreview) onNotePreview(step.note, step.instrument);
            repaint();
            return true;  // Consumed
        }

        // 'n' (NewSelection) creates new instrument
        if (action.action == input::KeyAction::NewSelection)
        {
            int newInst = project_.addInstrument("Inst " + std::to_string(project_.getInstrumentCount() + 1));
            step.instrument = static_cast<int8_t>(newInst);
            repaint();
            return true;  // Consumed
        }

        // Hex input for instrument number
        char lc = static_cast<char>(std::tolower(textChar));
        if ((textChar >= '0' && textChar <= '9') || (lc >= 'a' && lc <= 'f'))
        {
            int val = (textChar >= '0' && textChar <= '9') ? textChar - '0' : (lc - 'a' + 10);
            if (val < project_.getInstrumentCount())
            {
                step.instrument = static_cast<int8_t>(val);
                if (step.note >= 0 && onNotePreview) onNotePreview(step.note, step.instrument);
            }
            repaint();
            return true;  // Consumed
        }
    }
    else if (cursorColumn_ == 2)
    {
        // VOLUME COLUMN: Alt+Up/Down or Alt+j/k adjusts volume

        if (isEditInc || isEditDec)
        {
            int delta = isEditDec ? -16 : 16;  // Coarse adjustment
            if (step.volume == 0xFF) step.volume = 0x80;  // Default to mid if empty
            step.volume = static_cast<uint8_t>(std::clamp(static_cast<int>(step.volume) + delta, 0, 0xFE));
            repaint();
            return true;  // Consumed
        }

        // Hex input for volume
        char lc = static_cast<char>(std::tolower(textChar));
        if ((textChar >= '0' && textChar <= '9') || (lc >= 'a' && lc <= 'f'))
        {
            int val = (textChar >= '0' && textChar <= '9') ? textChar - '0' : (lc - 'a' + 10);
            step.volume = static_cast<uint8_t>(val * 16 + val);  // e.g., 'a' -> 0xAA
            repaint();
            return true;  // Consumed
        }
    }
    else
    {
        // FX COLUMNS (3-5): Alt+Up/Down or Alt+j/k cycles FX type, Alt+Left/Right or Alt+h/l adjusts value

        // Get the appropriate FX command
        model::FXCommand* fx = nullptr;
        if (cursorColumn_ == 3) fx = &step.fx1;
        else if (cursorColumn_ == 4) fx = &step.fx2;
        else if (cursorColumn_ == 5) fx = &step.fx3;

        if (fx)
        {
            // Alt+Up/Down or Alt+j/k cycles through FX types (vertical)
            if (isVerticalEditInc || isVerticalEditDec)
            {
                int delta = isVerticalEditInc ? 1 : -1;
                int currentType = static_cast<int>(fx->type);
                int numTypes = 7;  // None + 6 types
                int newType = (currentType + delta + numTypes) % numTypes;
                fx->type = static_cast<model::FXType>(newType);
                repaint();
                return true;  // Consumed
            }

            // Alt+Left/Right or Alt+h/l adjusts FX value (horizontal = Edit2)
            if (isHorizontalEditInc || isHorizontalEditDec)
            {
                int delta = isHorizontalEditInc ? 1 : -1;
                if (isCoarseEdit) delta *= 16;  // Coarse adjustment with Shift
                int newVal = static_cast<int>(fx->value) + delta;
                fx->value = static_cast<uint8_t>(std::clamp(newVal, 0, 255));
                // Default to ARP if no type set
                if (fx->type == model::FXType::None) fx->type = model::FXType::ARP;
                repaint();
                return true;  // Consumed
            }

            // Hex input for FX value
            char lc = static_cast<char>(std::tolower(textChar));
            if ((textChar >= '0' && textChar <= '9') || (lc >= 'a' && lc <= 'f'))
            {
                int val = (textChar >= '0' && textChar <= '9') ? textChar - '0' : (lc - 'a' + 10);
                // Shift existing value and add new digit
                fx->value = static_cast<uint8_t>(((fx->value & 0x0F) << 4) | val);
                if (fx->type == model::FXType::None) fx->type = model::FXType::ARP;  // Default to ARP if no type
                repaint();
                return true;  // Consumed
            }
        }
    }

    // Common: Delete action (Backspace/Delete/d) clears based on column
    if (action.action == input::KeyAction::Delete)
    {
        if (cursorColumn_ == 0) step.note = model::Step::NOTE_EMPTY;
        else if (cursorColumn_ == 1) step.instrument = -1;
        else if (cursorColumn_ == 2) step.volume = 0xFF;
        else if (cursorColumn_ == 3) step.fx1.clear();
        else if (cursorColumn_ == 4) step.fx2.clear();
        else if (cursorColumn_ == 5) step.fx3.clear();
        repaint();
        return true;  // Consumed
    }

    return false;  // Key not consumed
}

void PatternScreen::drawHeader(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(headerColor);
    g.fillRect(area);

    g.setColour(fgColor);
    g.setFont(18.0f);

    auto* pattern = project_.getPattern(currentPattern_);
    juce::String title = "PATTERN: ";
    if (editingName_)
        title += juce::String(nameBuffer_) + "_";
    else if (pattern)
        title += juce::String(pattern->getName());
    if (pattern)
        title += " [" + juce::String(pattern->getLength()) + " steps]";

    g.drawText(title, area.reduced(10, 0), juce::Justification::centredLeft, true);
}

void PatternScreen::drawTrackHeaders(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(headerColor.darker(0.2f));
    g.fillRect(area);

    int trackWidth = 0;
    for (int w : COLUMN_WIDTHS) trackWidth += w;
    trackWidth += 4;  // padding

    // Split header into two rows: track numbers and column labels
    int halfHeight = area.getHeight() / 2;

    // Track numbers row
    g.setFont(12.0f);
    int x = 40;  // row number column
    for (int t = 0; t < 16 && x < area.getWidth(); ++t)
    {
        g.setColour(t == cursorTrack_ ? cursorColor : fgColor.darker(0.3f));
        g.drawText(juce::String(t + 1), x, area.getY(), trackWidth, halfHeight,
                   juce::Justification::centred, true);
        x += trackWidth;
    }

    // Column labels row
    g.setFont(9.0f);
    x = 40;
    static const char* colLabels[] = {"NOT", "IN", "VL", "FX1", "FX2", "FX3"};
    for (int t = 0; t < 16 && x < area.getWidth(); ++t)
    {
        int colX = x + 2;
        for (int c = 0; c < 6; ++c)
        {
            bool isCurrentCol = (t == cursorTrack_ && c == cursorColumn_);
            g.setColour(isCurrentCol ? cursorColor : fgColor.darker(0.5f));
            g.drawText(colLabels[c], colX, area.getY() + halfHeight, COLUMN_WIDTHS[c], halfHeight,
                       juce::Justification::centredLeft, true);
            colX += COLUMN_WIDTHS[c];
        }
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

    // Check if this pattern is currently playing
    bool isPlaying = audioEngine_ && audioEngine_->isPlaying();
    int playheadRow = -1;
    if (isPlaying && audioEngine_->getCurrentPattern() == currentPattern_)
    {
        playheadRow = audioEngine_->getCurrentRow();
    }

    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));

    int y = area.getY();
    for (int row = 0; row < pattern->getLength() && y < area.getBottom(); ++row)
    {
        bool isCurrentRow = (row == cursorRow_);
        bool isPlayheadRow = (row == playheadRow);

        // Playhead indicator - green bar on left
        if (isPlayheadRow)
        {
            g.setColour(playheadColor);
            g.fillRect(0, y, 4, ROW_HEIGHT);
        }

        // Row number
        g.setColour(isPlayheadRow ? playheadColor : (isCurrentRow ? cursorColor : fgColor.darker(0.5f)));
        g.drawText(juce::String::toHexString(row).toUpperCase().paddedLeft('0', 2),
                   0, y, 36, ROW_HEIGHT, juce::Justification::centred, true);

        // Tracks
        int x = 40;
        for (int t = 0; t < 16 && x < area.getWidth(); ++t)
        {
            const auto& step = pattern->getStep(t, row);
            bool isCurrentCell = isCurrentRow && (t == cursorTrack_);

            // Check if this cell is in the visual selection
            bool isSelected = hasSelection_ &&
                              t >= selection_.minTrack() && t <= selection_.maxTrack() &&
                              row >= selection_.minRow() && row <= selection_.maxRow();

            juce::Rectangle<int> cellArea(x, y, trackWidth, ROW_HEIGHT);
            int columnToHighlight = isCurrentCell ? cursorColumn_ : -1;
            drawStep(g, cellArea, step, isCurrentRow, isCurrentCell, columnToHighlight, isSelected);

            x += trackWidth;
        }

        y += ROW_HEIGHT;
    }
}

void PatternScreen::drawStep(juce::Graphics& g, juce::Rectangle<int> area,
                              const model::Step& step, bool isCurrentRow, bool isCurrentCell,
                              int highlightColumn, bool isSelected)
{
    // Background
    if (isCurrentCell)
    {
        g.setColour(highlightColor);
        g.fillRect(area);
    }
    else if (isSelected)
    {
        // Visual mode selection highlight
        g.setColour(juce::Colours::cyan.withAlpha(0.25f));
        g.fillRect(area);
    }
    else if (isCurrentRow)
    {
        g.setColour(bgColor.brighter(0.1f));
        g.fillRect(area);
    }

    // Draw selection border if selected (on top of other backgrounds)
    if (isSelected && !isCurrentCell)
    {
        g.setColour(juce::Colours::cyan.withAlpha(0.6f));
        g.drawRect(area, 1);
    }

    int x = area.getX() + 2;
    int y = area.getY();
    int h = area.getHeight();

    // Helper to draw column highlight
    auto drawColumnHighlight = [&](int colX, int colWidth, int colIndex) {
        if (highlightColumn == colIndex)
        {
            g.setColour(cursorColor.withAlpha(0.4f));
            g.fillRect(colX - 1, y, colWidth + 1, h);
            // Draw a subtle border around the column
            g.setColour(cursorColor);
            g.drawRect(colX - 1, y, colWidth + 1, h, 1);
        }
    };

    // Note
    drawColumnHighlight(x, COLUMN_WIDTHS[0], 0);
    g.setColour(step.note >= 0 ? fgColor : fgColor.darker(0.6f));
    if (highlightColumn == 0) g.setColour(fgColor.brighter(0.2f));
    g.drawText(noteToString(step.note), x, y, COLUMN_WIDTHS[0], h, juce::Justification::centredLeft, true);
    x += COLUMN_WIDTHS[0];

    // Instrument
    drawColumnHighlight(x, COLUMN_WIDTHS[1], 1);
    g.setColour(step.instrument >= 0 ? fgColor : fgColor.darker(0.6f));
    if (highlightColumn == 1) g.setColour(fgColor.brighter(0.2f));
    juce::String instStr = step.instrument >= 0 ? juce::String::toHexString(step.instrument).toUpperCase().paddedLeft('0', 2) : "..";
    g.drawText(instStr, x, y, COLUMN_WIDTHS[1], h, juce::Justification::centredLeft, true);
    x += COLUMN_WIDTHS[1];

    // Volume
    drawColumnHighlight(x, COLUMN_WIDTHS[2], 2);
    g.setColour(step.volume < 0xFF ? fgColor : fgColor.darker(0.6f));
    if (highlightColumn == 2) g.setColour(fgColor.brighter(0.2f));
    juce::String volStr = step.volume < 0xFF ? juce::String::toHexString(step.volume).toUpperCase().paddedLeft('0', 2) : "..";
    g.drawText(volStr, x, y, COLUMN_WIDTHS[2], h, juce::Justification::centredLeft, true);
    x += COLUMN_WIDTHS[2];

    // FX columns
    const model::FXCommand* fxCmds[] = {&step.fx1, &step.fx2, &step.fx3};
    for (int fx = 0; fx < 3; ++fx)
    {
        drawColumnHighlight(x, COLUMN_WIDTHS[3 + fx], 3 + fx);
        bool hasData = !fxCmds[fx]->isEmpty();
        g.setColour(hasData ? fgColor : fgColor.darker(0.6f));
        if (highlightColumn == 3 + fx) g.setColour(fgColor.brighter(0.2f));
        g.drawText(fxCmds[fx]->toString(), x, y, COLUMN_WIDTHS[3 + fx], h, juce::Justification::centredLeft, true);
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

const model::Step* PatternScreen::findLastNonEmptyRowAbove(int track, int startRow) const
{
    auto* pattern = project_.getPattern(currentPattern_);
    if (!pattern) return nullptr;

    // Search upwards from startRow-1 to find last playable note in this track
    // Skip over OFF notes to find a genuinely useful note to copy
    for (int r = startRow - 1; r >= 0; --r)
    {
        const auto& step = pattern->getStep(track, r);
        // Only return steps with actual playable notes (>= 0), skip OFF and empty
        if (step.note >= 0)
            return &step;
    }
    return nullptr;
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

void PatternScreen::interpolate()
{
    if (!hasSelection_) return;

    auto* pattern = project_.getPattern(currentPattern_);
    if (!pattern) return;

    // For each track in selection
    for (int t = selection_.minTrack(); t <= selection_.maxTrack(); ++t)
    {
        int minR = selection_.minRow();
        int maxR = selection_.maxRow();
        int range = maxR - minR;
        if (range <= 0) continue;

        auto& first = pattern->getStep(t, minR);
        auto& last = pattern->getStep(t, maxR);

        // Interpolate volume if both have values
        if (first.volume < 0xFF && last.volume < 0xFF)
        {
            for (int r = minR + 1; r < maxR; ++r)
            {
                auto& step = pattern->getStep(t, r);
                float t_factor = static_cast<float>(r - minR) / static_cast<float>(range);
                step.volume = static_cast<uint8_t>(
                    static_cast<float>(first.volume) +
                    (static_cast<float>(last.volume) - static_cast<float>(first.volume)) * t_factor
                );
            }
        }
    }

    repaint();
}

void PatternScreen::randomize(int percent)
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
            if (step.note > 0 && step.note <= 127)
            {
                int variance = (std::rand() % (percent * 2 + 1)) - percent;
                int semitoneVariance = variance * 12 / 100;
                step.note = static_cast<int8_t>(std::clamp(static_cast<int>(step.note) + semitoneVariance, 1, 127));
            }
        }
    }

    repaint();
}

void PatternScreen::doublePattern()
{
    auto* pattern = project_.getPattern(currentPattern_);
    if (!pattern) return;

    int oldLength = pattern->getLength();
    int newLength = std::min(oldLength * 2, 128);

    pattern->setLength(newLength);

    // Duplicate steps
    for (int t = 0; t < 16; ++t)
    {
        for (int r = 0; r < oldLength; ++r)
        {
            pattern->getStep(t, r + oldLength) = pattern->getStep(t, r);
        }
    }

    repaint();
}

void PatternScreen::halvePattern()
{
    auto* pattern = project_.getPattern(currentPattern_);
    if (!pattern) return;

    int newLength = std::max(pattern->getLength() / 2, 1);
    pattern->setLength(newLength);
    repaint();
}

void PatternScreen::showChordPopup()
{
    auto* pattern = project_.getPattern(currentPattern_);
    if (!pattern) return;

    const auto& step = pattern->getStep(cursorTrack_, cursorRow_);
    int existingNote = step.note >= 0 ? step.note : -1;
    int instrument = step.instrument >= 0 ? step.instrument : 0;

    // Get scale lock from chain (if pattern is in a chain)
    std::string scaleLock = getScaleLockForPattern(currentPattern_);

    chordPopup_->setBounds(getLocalBounds());
    chordPopup_->showAtCell(cursorTrack_, cursorRow_, existingNote, instrument, scaleLock);

    // Set up callbacks
    chordPopup_->onChordConfirmed = [this](const std::vector<int>& notes) {
        auto* pat = project_.getPattern(currentPattern_);
        if (!pat) return;

        const auto& currentStep = pat->getStep(cursorTrack_, cursorRow_);
        int instrument = currentStep.instrument >= 0 ? currentStep.instrument : 0;

        // Place notes across adjacent tracks
        for (size_t i = 0; i < notes.size() && cursorTrack_ + i < 16; ++i)
        {
            auto& step = pat->getStep(cursorTrack_ + static_cast<int>(i), cursorRow_);
            step.note = static_cast<int8_t>(notes[i]);
            step.instrument = static_cast<int16_t>(instrument);
        }
        repaint();
    };

    chordPopup_->onChordPreview = [this](const std::vector<int>& notes, int instrument) {
        if (onChordPreview)
            onChordPreview(notes, instrument);
    };
}

std::string PatternScreen::getScaleLockForPattern(int patternIndex) const
{
    // Find any chain that contains this pattern and return its scale lock
    int numChains = project_.getChainCount();
    for (int c = 0; c < numChains; ++c)
    {
        auto* chain = project_.getChain(c);
        if (!chain) continue;

        const auto& patterns = chain->getPatternIndices();
        for (int p : patterns)
        {
            if (p == patternIndex)
                return chain->getScaleLock();
        }
    }
    return "";  // No scale lock found
}

int PatternScreen::moveNoteInScale(int currentNote, int direction, const std::string& scaleLock) const
{
    // Scale intervals (semitones from root)
    // Major: W W H W W W H -> 0, 2, 4, 5, 7, 9, 11
    // Minor: W H W W H W W -> 0, 2, 3, 5, 7, 8, 10
    static const int majorScale[] = {0, 2, 4, 5, 7, 9, 11};
    static const int minorScale[] = {0, 2, 3, 5, 7, 8, 10};

    // If no scale lock, move by semitone
    if (scaleLock.empty() || scaleLock == "None")
        return std::clamp(currentNote + direction, 1, 127);

    // Parse scale name: "C Major", "C# Minor", etc.
    int rootNote = 0;  // C = 0
    bool isMinor = scaleLock.find("Minor") != std::string::npos;

    // Parse root note
    if (scaleLock.length() >= 1)
    {
        char root = scaleLock[0];
        switch (root)
        {
            case 'C': rootNote = 0; break;
            case 'D': rootNote = 2; break;
            case 'E': rootNote = 4; break;
            case 'F': rootNote = 5; break;
            case 'G': rootNote = 7; break;
            case 'A': rootNote = 9; break;
            case 'B': rootNote = 11; break;
        }
        // Check for sharp
        if (scaleLock.length() >= 2 && scaleLock[1] == '#')
            rootNote = (rootNote + 1) % 12;
    }

    const int* scale = isMinor ? minorScale : majorScale;
    const int scaleSize = 7;

    // Calculate note position relative to the scale root
    // This ensures octave boundaries align with the root note, not C
    int noteRelativeToRoot = currentNote - rootNote;

    // Find the scale octave (how many complete octaves from root)
    int scaleOctave = noteRelativeToRoot / 12;
    if (noteRelativeToRoot < 0 && noteRelativeToRoot % 12 != 0)
        scaleOctave--;  // Correct for negative modulo

    // Note position within current scale octave (0-11)
    int noteInScaleOctave = ((noteRelativeToRoot % 12) + 12) % 12;

    // Find closest scale degree
    int currentDegree = 0;
    int minDist = 12;
    for (int i = 0; i < scaleSize; ++i)
    {
        int dist = std::abs(scale[i] - noteInScaleOctave);
        if (dist < minDist)
        {
            minDist = dist;
            currentDegree = i;
        }
    }

    // Move to next/previous degree
    int newDegree = currentDegree + direction;
    int octaveShift = 0;

    if (newDegree >= scaleSize)
    {
        newDegree = 0;
        octaveShift = 1;
    }
    else if (newDegree < 0)
    {
        newDegree = scaleSize - 1;
        octaveShift = -1;
    }

    // Calculate new note relative to root, then add root back
    int newNote = rootNote + (scaleOctave + octaveShift) * 12 + scale[newDegree];
    return std::clamp(newNote, 1, 127);
}

int PatternScreen::getInstrumentAtCursor() const
{
    auto* pattern = project_.getPattern(currentPattern_);
    if (!pattern) return -1;

    const auto& step = pattern->getStep(cursorTrack_, cursorRow_);
    return step.instrument;
}

std::vector<HelpSection> PatternScreen::getHelpContent() const
{
    return {
        {"Navigation", {
            {"Up/Down", "Move cursor vertically"},
            {"Left/Right", "Move between columns"},
            {"Tab / Shift+Tab", "Next/previous track"},
            {"[  ]", "Previous/Next pattern"},
            {"Enter", "Jump to instrument at cursor"},
        }},
        {"Note Column", {
            {"n", "Add note (copy from row above)"},
            {"c", "Open chord entry popup"},
            {"Alt+Up/Down", "Create note / move in scale"},
            {"Shift+Up/Down", "Move note by octave"},
            {".", "Note off"},
            {"Delete/d", "Clear note"},
        }},
        {"Inst/Vol/FX Columns", {
            {"Alt+Up/Down", "Adjust value / cycle type"},
            {"0-9, a-f", "Hex input for value"},
            {"Alt+L/R", "Adjust FX value (FX columns)"},
            {"n", "Create new instrument (inst column)"},
            {"Delete/d", "Clear column"},
        }},
        {"Pattern Length", {
            {"+", "Add row to pattern"},
            {"-", "Remove row from pattern"},
            {"Shift+N", "Create new pattern"},
            {"r", "Rename pattern"},
        }},
        {"Selection (v = Visual)", {
            {"v", "Start selection"},
            {"y", "Yank (copy)"},
            {"d", "Delete"},
            {"p", "Paste"},
            {"f", "Fill following pattern"},
            {"s", "Randomize populated values"},
            {"Alt+Up/Down", "Batch edit selection"},
        }},
    };
}

} // namespace ui
