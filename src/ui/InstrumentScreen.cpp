#include "InstrumentScreen.h"
#include "../audio/AudioEngine.h"

namespace ui {

const char* InstrumentScreen::engineNames_[16] = {
    "Virtual Analog", "Waveshaping", "FM", "Grain",
    "Additive", "Wavetable", "Chords", "Vowel/Speech",
    "Swarm", "Noise", "Particle", "String",
    "Modal", "Bass Drum", "Snare Drum", "Hi-Hat"
};

// Per-engine parameter labels [engine][harmonics, timbre, morph]
const char* InstrumentScreen::engineParamLabels_[16][3] = {
    {"DETUNE", "SQUARE", "SAW"},           // VA
    {"WAVEFORM", "FOLD", "ASYMMETRY"},     // Waveshaper
    {"RATIO", "MOD IDX", "FEEDBACK"},      // FM
    {"FORMNT 2", "FORMANT", "WIDTH"},      // Grain
    {"BUMPS", "HARMONIC", "SHAPE"},        // Additive
    {"BANK", "ROW", "COLUMN"},             // Wavetable
    {"CHORD", "INVERS", "WAVEFORM"},       // Chord
    {"TYPE", "SPECIES", "PHONEME"},        // Speech
    {"PITCH", "DENSITY", "OVERLAP"},       // Swarm
    {"FILTER", "CLOCK", "RESONAN"},        // Noise
    {"FREQ RND", "DENSITY", "REVERB"},     // Particle
    {"INHARM", "EXCITER", "DECAY"},        // String
    {"MATERIAL", "EXCITER", "DECAY"},      // Modal
    {"ATTACK", "TONE", "DECAY"},           // Bass Drum
    {"NOISE", "MODES", "DECAY"},           // Snare
    {"METAL", "HIGHPASS", "DECAY"},        // Hi-Hat
};

static const char* kLfoRateNames[] = {"1/16", "1/16T", "1/8", "1/8T", "1/4", "1/4T", "1/2", "1BAR", "2BAR", "4BAR"};
static const char* kLfoShapeNames[] = {"TRI", "SAW", "SQR", "S&H"};
static const char* kModDestNames[] = {"HARM", "TIMB", "MRPH", "CUTF", "RESO", "LFO1R", "LFO1A", "LFO2R", "LFO2A"};

InstrumentScreen::InstrumentScreen(model::Project& project, input::ModeManager& modeManager)
    : Screen(project, modeManager)
{
    sliceWaveformDisplay_ = std::make_unique<SliceWaveformDisplay>();
    addChildComponent(sliceWaveformDisplay_.get());

    sliceWaveformDisplay_->onAddSlice = [this](size_t pos) {
        if (audioEngine_) {
            if (auto* slicer = audioEngine_->getSlicerProcessor(currentInstrument_)) {
                slicer->addSliceAtPosition(pos);
                updateSlicerDisplay();
            }
        }
    };

    sliceWaveformDisplay_->onDeleteSlice = [this](int index) {
        if (audioEngine_) {
            if (auto* slicer = audioEngine_->getSlicerProcessor(currentInstrument_)) {
                slicer->removeSlice(index);
                updateSlicerDisplay();
            }
        }
    };
}

void InstrumentScreen::setAudioEngine(audio::AudioEngine* engine)
{
    audioEngine_ = engine;
}

void InstrumentScreen::drawInstrumentTabs(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setFont(12.0f);

    int numInstruments = project_.getInstrumentCount();
    if (numInstruments == 0)
    {
        g.setColour(fgColor.darker(0.5f));
        g.drawText("No instruments - press Ctrl+N to create one", area.reduced(10, 0), juce::Justification::centredLeft);
        return;
    }

    g.setColour(fgColor.darker(0.5f));
    g.drawText("[", area.removeFromLeft(20), juce::Justification::centred);

    int tabWidth = 80;
    int maxTabs = std::min(numInstruments, 12);
    int startIdx = std::max(0, currentInstrument_ - 5);

    for (int i = startIdx; i < startIdx + maxTabs && i < numInstruments; ++i)
    {
        auto* instrument = project_.getInstrument(i);
        bool isSelected = (i == currentInstrument_);

        auto tabArea = area.removeFromLeft(tabWidth);

        if (isSelected)
        {
            g.setColour(cursorColor.withAlpha(0.3f));
            g.fillRect(tabArea.reduced(2));
        }

        g.setColour(isSelected ? cursorColor : fgColor.darker(0.3f));
        juce::String text = juce::String::toHexString(i).toUpperCase().paddedLeft('0', 2);
        if (instrument && !instrument->getName().empty())
            text += ":" + juce::String(instrument->getName()).substring(0, 5);
        g.drawText(text, tabArea, juce::Justification::centred);
    }

    g.setColour(fgColor.darker(0.5f));
    g.drawText("]", area.removeFromLeft(20), juce::Justification::centred);
}

void InstrumentScreen::paint(juce::Graphics& g)
{
    g.fillAll(bgColor);

    auto area = getLocalBounds();

    // Instrument selector tabs at top
    drawInstrumentTabs(g, area.removeFromTop(30));

    // Header bar (matches Pattern screen style)
    auto headerArea = area.removeFromTop(30);
    g.setColour(headerColor);
    g.fillRect(headerArea);

    auto* instrument = project_.getInstrument(currentInstrument_);

    // Check if this is a sampler instrument and render sampler UI
    if (instrument && instrument->getType() == model::InstrumentType::Sampler) {
        // Paint sampler-specific UI
        g.setColour(fgColor);
        g.setFont(18.0f);
        juce::String title = "SAMPLER: ";
        if (editingName_)
            title += juce::String(nameBuffer_) + "_";
        else
            title += juce::String(instrument->getName());
        g.drawText(title, headerArea.reduced(10, 0), juce::Justification::centredLeft, true);

        area = area.reduced(16);
        area.removeFromTop(8);

        // Show sample info
        const auto& samplerParams = instrument->getSamplerParams();
        g.setFont(14.0f);
        g.setColour(fgColor);

        int yPos = area.getY() + 20;

        if (samplerParams.sample.path.empty()) {
            g.setFont(16.0f);
            g.setColour(fgColor.darker(0.3f));
            g.drawText("No sample loaded", area.getX(), yPos, area.getWidth(), 30, juce::Justification::centredLeft);
            yPos += 40;

            g.setFont(14.0f);
            g.setColour(cursorColor);
            g.drawText("Press 'o' to load a sample", area.getX(), yPos, area.getWidth(), 30, juce::Justification::centredLeft);
        } else {
            // Show sample filename
            juce::File sampleFile(samplerParams.sample.path);
            g.setColour(cursorColor);
            g.drawText("Sample: " + sampleFile.getFileName(), area.getX(), yPos, area.getWidth(), 30, juce::Justification::centredLeft);
            yPos += 30;

            // Show sample info
            g.setColour(fgColor);
            juce::String info = juce::String::formatted("Channels: %d  Sample Rate: %d Hz  Samples: %d",
                samplerParams.sample.numChannels,
                samplerParams.sample.sampleRate,
                static_cast<int>(samplerParams.sample.numSamples));
            g.drawText(info, area.getX(), yPos, area.getWidth(), 30, juce::Justification::centredLeft);
            yPos += 30;

            // Show root note
            static const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
            int note = samplerParams.rootNote;
            juce::String rootNoteStr = juce::String(noteNames[note % 12]) + juce::String(note / 12 - 1);
            g.drawText("Root Note: " + rootNoteStr, area.getX(), yPos, area.getWidth(), 30, juce::Justification::centredLeft);
            yPos += 50;

            // Instructions
            g.setColour(fgColor.darker(0.3f));
            g.drawText("Press 'o' to load a different sample", area.getX(), yPos, area.getWidth(), 30, juce::Justification::centredLeft);
        }
        return;
    }

    // Check if this is a slicer instrument and render slicer UI
    if (instrument && instrument->getType() == model::InstrumentType::Slicer) {
        paintSlicerUI(g);
        return;
    }

    if (!instrument)
    {
        g.setColour(fgColor.darker(0.5f));
        g.drawText("No instruments - press Ctrl+N to create one", area.reduced(20), juce::Justification::centred);
        return;
    }

    // Header text
    g.setColour(fgColor);
    g.setFont(18.0f);
    juce::String title = "INSTRUMENT: ";
    if (editingName_)
        title += juce::String(nameBuffer_) + "_";
    else
        title += juce::String(instrument->getName());
    title += " [" + juce::String(engineNames_[instrument->getParams().engine]) + "]";
    g.drawText(title, headerArea.reduced(10, 0), juce::Justification::centredLeft, true);

    area = area.reduced(16);
    area.removeFromTop(8);

    // Get processor for modulated values
    audio::PlaitsInstrument* processor = nullptr;
    if (audioEngine_)
        processor = audioEngine_->getInstrumentProcessor(currentInstrument_);

    // Draw all rows
    int y = area.getY();
    for (int row = 0; row < kNumRows; ++row)
    {
        juce::Rectangle<int> rowArea(area.getX(), y, area.getWidth(), kRowHeight);
        drawRow(g, rowArea, row, cursorRow_ == row);
        y += kRowHeight + 2;
    }
}

bool InstrumentScreen::isModRow(int row) const
{
    auto type = static_cast<InstrumentRowType>(row);
    return type == InstrumentRowType::Lfo1 || type == InstrumentRowType::Lfo2 ||
           type == InstrumentRowType::Env1 || type == InstrumentRowType::Env2;
}

int InstrumentScreen::getNumFieldsForRow(int row) const
{
    return isModRow(row) ? 4 : 1;
}

void InstrumentScreen::drawRow(juce::Graphics& g, juce::Rectangle<int> area, int row, bool selected)
{
    auto type = static_cast<InstrumentRowType>(row);

    // Get model instrument for basic params
    auto* instrument = project_.getInstrument(currentInstrument_);
    if (!instrument) return;

    // Get processor for extended params
    audio::PlaitsInstrument* processor = nullptr;
    if (audioEngine_)
        processor = audioEngine_->getInstrumentProcessor(currentInstrument_);

    const auto& params = instrument->getParams();
    const auto& sends = instrument->getSends();
    int engine = params.engine;

    // Background
    if (selected)
    {
        g.setColour(highlightColor.withAlpha(0.3f));
        g.fillRect(area);
    }

    g.setFont(14.0f);
    g.setColour(selected ? cursorColor : fgColor);

    // Handle modulation rows specially
    if (isModRow(row))
    {
        drawModRow(g, area, row, selected);
        return;
    }

    // Get label and value based on row type
    const char* label = "";
    float value = 0.0f;
    float modValue = -1.0f;  // -1 means no modulation display
    juce::String valueText;

    switch (type)
    {
        case InstrumentRowType::Preset:
        {
            label = "PRESET";
            value = 0.0f;  // No slider for preset
            if (presetManager_)
            {
                int presetCount = presetManager_->getPresetCount(engine);
                if (presetCount > 0 && currentPresetIndex_ >= 0 && currentPresetIndex_ < presetCount)
                {
                    auto* preset = presetManager_->getPreset(engine, currentPresetIndex_);
                    if (preset)
                    {
                        valueText = "[" + juce::String(engineNames_[engine]) + "] " + juce::String(preset->name);
                        if (presetModified_)
                            valueText += "*";
                    }
                }
                else
                {
                    valueText = "[" + juce::String(engineNames_[engine]) + "] --";
                }
            }
            else
            {
                valueText = "[" + juce::String(engineNames_[engine]) + "] --";
            }

            // Draw preset row without slider bar
            g.drawText(label, area.getX(), area.getY(), kLabelWidth, kRowHeight, juce::Justification::centredLeft);
            g.drawText(valueText, area.getX() + kLabelWidth, area.getY(), area.getWidth() - kLabelWidth, kRowHeight, juce::Justification::centredLeft);
            return;  // Early return - no slider bar for preset row
        }

        case InstrumentRowType::Engine:
            label = "ENGINE";
            value = static_cast<float>(engine) / 15.0f;
            valueText = engineNames_[engine];
            break;

        case InstrumentRowType::Harmonics:
            label = engineParamLabels_[engine][0];
            value = params.harmonics;
            if (processor) modValue = processor->getModulatedHarmonics();
            valueText = juce::String(static_cast<int>(value * 127));
            break;

        case InstrumentRowType::Timbre:
            label = engineParamLabels_[engine][1];
            value = params.timbre;
            if (processor) modValue = processor->getModulatedTimbre();
            valueText = juce::String(static_cast<int>(value * 127));
            break;

        case InstrumentRowType::Morph:
            label = engineParamLabels_[engine][2];
            value = params.morph;
            if (processor) modValue = processor->getModulatedMorph();
            valueText = juce::String(static_cast<int>(value * 127));
            break;

        case InstrumentRowType::Attack:
            label = "ATTACK";
            value = params.attack;
            {
                float ms = 1.0f + value * value * 1999.0f;
                valueText = juce::String(static_cast<int>(ms)) + "ms";
            }
            break;

        case InstrumentRowType::Decay:
            label = "DECAY";
            value = params.decay;
            {
                float ms = 1.0f + value * value * 9999.0f;
                valueText = juce::String(static_cast<int>(ms)) + "ms";
            }
            break;

        case InstrumentRowType::Polyphony:
            label = "VOICES";
            {
                int poly = params.polyphony;
                value = static_cast<float>(poly - 1) / 15.0f;
                valueText = juce::String(poly);
            }
            break;

        case InstrumentRowType::Cutoff:
            label = "CUTOFF";
            {
                value = params.filter.cutoff;
                if (processor) modValue = processor->getModulatedCutoff();
                float hz = 20.0f * std::pow(1000.0f, value);
                valueText = juce::String(static_cast<int>(hz)) + "Hz";
            }
            break;

        case InstrumentRowType::Resonance:
            label = "RESO";
            {
                value = params.filter.resonance;
                if (processor) modValue = processor->getModulatedResonance();
                valueText = juce::String(static_cast<int>(value * 127));
            }
            break;

        case InstrumentRowType::Reverb:
            label = "REVERB";
            value = sends.reverb;
            valueText = juce::String(static_cast<int>(value * 100)) + "%";
            break;

        case InstrumentRowType::Delay:
            label = "DELAY";
            value = sends.delay;
            valueText = juce::String(static_cast<int>(value * 100)) + "%";
            break;

        case InstrumentRowType::Chorus:
            label = "CHORUS";
            value = sends.chorus;
            valueText = juce::String(static_cast<int>(value * 100)) + "%";
            break;

        case InstrumentRowType::Drive:
            label = "DRIVE";
            value = sends.drive;
            valueText = juce::String(static_cast<int>(value * 100)) + "%";
            break;

        case InstrumentRowType::Sidechain:
            label = "SIDECHAIN";
            value = sends.sidechainDuck;
            valueText = juce::String(static_cast<int>(value * 100)) + "%";
            break;

        case InstrumentRowType::Volume:
            label = "VOLUME";
            value = instrument->getVolume();
            valueText = juce::String(static_cast<int>(value * 100)) + "%";
            break;

        case InstrumentRowType::Pan:
            label = "PAN";
            {
                float pan = instrument->getPan();
                value = (pan + 1.0f) / 2.0f;  // Convert -1..1 to 0..1 for display
                if (pan < -0.01f)
                    valueText = "L" + juce::String(static_cast<int>(-pan * 100));
                else if (pan > 0.01f)
                    valueText = "R" + juce::String(static_cast<int>(pan * 100));
                else
                    valueText = "C";
            }
            break;

        default:
            break;
    }

    // Draw label
    g.drawText(label, area.getX(), area.getY(), kLabelWidth, kRowHeight, juce::Justification::centredLeft);

    // Draw slider bar
    int barX = area.getX() + kLabelWidth;
    int barY = area.getY() + (kRowHeight - 10) / 2;

    // Background bar
    g.setColour(juce::Colour(0xff303030));
    g.fillRect(barX, barY, kBarWidth, 10);

    // Value bar
    g.setColour(selected ? cursorColor : juce::Colour(0xff4a9090));
    g.fillRect(barX, barY, static_cast<int>(kBarWidth * value), 10);

    // Modulation overlay
    if (modValue >= 0.0f && modValue != value)
    {
        g.setColour(juce::Colour(0x80ffffff));
        int modX = barX + static_cast<int>(kBarWidth * std::min(value, modValue));
        int modWidth = static_cast<int>(kBarWidth * std::abs(modValue - value));
        g.fillRect(modX, barY, modWidth, 10);
    }

    // Value text
    g.setColour(selected ? cursorColor : fgColor);
    g.drawText(valueText, barX + kBarWidth + 10, area.getY(), 100, kRowHeight, juce::Justification::centredLeft);
}

void InstrumentScreen::drawModRow(juce::Graphics& g, juce::Rectangle<int> area, int row, bool selected)
{
    auto type = static_cast<InstrumentRowType>(row);

    auto* instrument = project_.getInstrument(currentInstrument_);
    if (!instrument) return;

    const auto& params = instrument->getParams();

    // Determine which LFO/ENV
    const char* label = "";
    int rate = 4, shape = 0, dest = 0, amount = 0;

    switch (type)
    {
        case InstrumentRowType::Lfo1:
            label = "LFO1";
            rate = params.lfo1.rate;
            shape = params.lfo1.shape;
            dest = params.lfo1.dest;
            amount = params.lfo1.amount;
            break;
        case InstrumentRowType::Lfo2:
            label = "LFO2";
            rate = params.lfo2.rate;
            shape = params.lfo2.shape;
            dest = params.lfo2.dest;
            amount = params.lfo2.amount;
            break;
        case InstrumentRowType::Env1:
            label = "ENV1";
            // For ENVs, convert attack/decay to ms for display
            rate = static_cast<int>(params.env1.attack * 500.0f + 0.5f);  // ms
            shape = static_cast<int>(params.env1.decay * 2000.0f + 0.5f);  // ms
            dest = params.env1.dest;
            amount = params.env1.amount;
            break;
        case InstrumentRowType::Env2:
            label = "ENV2";
            rate = static_cast<int>(params.env2.attack * 500.0f + 0.5f);
            shape = static_cast<int>(params.env2.decay * 2000.0f + 0.5f);
            dest = params.env2.dest;
            amount = params.env2.amount;
            break;
        default:
            break;
    }

    // Draw label
    g.setColour(selected ? cursorColor : fgColor);
    g.drawText(label, area.getX(), area.getY(), kLabelWidth, kRowHeight, juce::Justification::centredLeft);

    // Draw 4 fields: rate/attack, shape/decay, dest, amount
    int fieldX = area.getX() + kLabelWidth;
    int fieldWidth = 50;
    int fieldGap = 8;

    bool isEnv = (type == InstrumentRowType::Env1 || type == InstrumentRowType::Env2);

    // Field 0: Rate (LFO) or Attack (ENV)
    bool field0Selected = selected && cursorField_ == 0;
    g.setColour(field0Selected ? cursorColor : juce::Colour(0xff606060));
    g.fillRect(fieldX, area.getY() + 4, fieldWidth, kRowHeight - 8);
    g.setColour(field0Selected ? juce::Colours::black : fgColor);
    if (isEnv)
        g.drawText(juce::String(rate) + "ms", fieldX, area.getY(), fieldWidth, kRowHeight, juce::Justification::centred);
    else
        g.drawText(getLfoRateName(rate), fieldX, area.getY(), fieldWidth, kRowHeight, juce::Justification::centred);

    fieldX += fieldWidth + fieldGap;

    // Field 1: Shape (LFO) or Decay (ENV)
    bool field1Selected = selected && cursorField_ == 1;
    g.setColour(field1Selected ? cursorColor : juce::Colour(0xff606060));
    g.fillRect(fieldX, area.getY() + 4, fieldWidth, kRowHeight - 8);
    g.setColour(field1Selected ? juce::Colours::black : fgColor);
    if (isEnv)
        g.drawText(juce::String(shape) + "ms", fieldX, area.getY(), fieldWidth, kRowHeight, juce::Justification::centred);
    else
        g.drawText(getLfoShapeName(shape), fieldX, area.getY(), fieldWidth, kRowHeight, juce::Justification::centred);

    fieldX += fieldWidth + fieldGap;

    // Field 2: Destination
    bool field2Selected = selected && cursorField_ == 2;
    g.setColour(field2Selected ? cursorColor : juce::Colour(0xff606060));
    g.fillRect(fieldX, area.getY() + 4, fieldWidth, kRowHeight - 8);
    g.setColour(field2Selected ? juce::Colours::black : fgColor);
    g.drawText(getModDestName(dest), fieldX, area.getY(), fieldWidth, kRowHeight, juce::Justification::centred);

    fieldX += fieldWidth + fieldGap;

    // Field 3: Amount
    bool field3Selected = selected && cursorField_ == 3;
    g.setColour(field3Selected ? cursorColor : juce::Colour(0xff606060));
    g.fillRect(fieldX, area.getY() + 4, fieldWidth, kRowHeight - 8);
    g.setColour(field3Selected ? juce::Colours::black : fgColor);
    g.drawText((amount >= 0 ? "+" : "") + juce::String(amount), fieldX, area.getY(), fieldWidth, kRowHeight, juce::Justification::centred);
}

const char* InstrumentScreen::getLfoRateName(int rate)
{
    if (rate >= 0 && rate < 10)
        return kLfoRateNames[rate];
    return "???";
}

const char* InstrumentScreen::getLfoShapeName(int shape)
{
    if (shape >= 0 && shape < 4)
        return kLfoShapeNames[shape];
    return "???";
}

const char* InstrumentScreen::getModDestName(int dest)
{
    if (dest >= 0 && dest < 9)
        return kModDestNames[dest];
    return "???";
}

void InstrumentScreen::resized()
{
    if (sliceWaveformDisplay_) {
        auto bounds = getLocalBounds();
        bounds.removeFromTop(28);  // Title
        sliceWaveformDisplay_->setBounds(bounds.removeFromTop(100));
    }
}

void InstrumentScreen::navigate(int dx, int dy)
{
    if (dx != 0 && !isModRow(cursorRow_))
    {
        // Switch instruments
        int numInstruments = project_.getInstrumentCount();
        currentInstrument_ = (currentInstrument_ + dx + numInstruments) % numInstruments;
    }
    else if (dx != 0 && isModRow(cursorRow_))
    {
        // Move between fields in mod row
        cursorField_ = std::clamp(cursorField_ + dx, 0, 3);
    }

    if (dy != 0)
    {
        cursorRow_ = std::clamp(cursorRow_ + dy, 0, kNumRows - 1);
        // Reset field when entering/leaving mod rows
        if (!isModRow(cursorRow_))
            cursorField_ = 0;
    }

    repaint();
}

bool InstrumentScreen::handleEdit(const juce::KeyPress& key)
{
    return handleEditKey(key);
}

bool InstrumentScreen::handleEditKey(const juce::KeyPress& key)
{
    // Check if this is a sampler instrument and handle sampler keys
    auto* instrument = project_.getInstrument(currentInstrument_);
    if (instrument && instrument->getType() == model::InstrumentType::Sampler) {
        return handleSamplerKey(key, true);
    }

    // Check if this is a slicer instrument and handle slicer keys
    if (instrument && instrument->getType() == model::InstrumentType::Slicer) {
        return handleSlicerKey(key, true);
    }

    auto keyCode = key.getKeyCode();
    auto textChar = key.getTextCharacter();
    bool shiftHeld = key.getModifiers().isShiftDown();

    // Handle name editing mode
    if (editingName_)
    {
        auto* instrument = project_.getInstrument(currentInstrument_);
        if (!instrument)
        {
            editingName_ = false;
            repaint();
            return true;  // Consumed
        }

        if (keyCode == juce::KeyPress::returnKey || keyCode == juce::KeyPress::escapeKey)
        {
            if (keyCode == juce::KeyPress::returnKey)
                instrument->setName(nameBuffer_);
            editingName_ = false;
            repaint();
            return true;  // Consumed - important for Enter/Escape
        }
        if (keyCode == juce::KeyPress::backspaceKey && !nameBuffer_.empty())
        {
            nameBuffer_.pop_back();
            repaint();
            return true;  // Consumed
        }
        if (textChar >= ' ' && textChar <= '~' && nameBuffer_.length() < 16)
        {
            nameBuffer_ += static_cast<char>(textChar);
            repaint();
            return true;  // Consumed
        }
        return true;  // In name editing mode, consume all keys
    }

    // Shift+N creates new instrument and enters edit mode
    if (shiftHeld && textChar == 'N')
    {
        currentInstrument_ = project_.addInstrument("Inst " + std::to_string(project_.getInstrumentCount() + 1));
        auto* instrument = project_.getInstrument(currentInstrument_);
        if (instrument)
        {
            editingName_ = true;
            nameBuffer_ = instrument->getName();
        }
        repaint();
        return false;  // Let other handlers run too
    }

    // 'r' starts renaming current instrument
    if (textChar == 'r' || textChar == 'R')
    {
        auto* instrument = project_.getInstrument(currentInstrument_);
        if (instrument)
        {
            editingName_ = true;
            nameBuffer_ = instrument->getName();
            repaint();
            return true;  // Consumed - starting rename mode
        }
        return false;
    }

    // '[' and ']' switch instruments quickly
    if (textChar == '[')
    {
        int numInstruments = project_.getInstrumentCount();
        if (numInstruments > 0)
            currentInstrument_ = (currentInstrument_ - 1 + numInstruments) % numInstruments;
        repaint();
        return true;  // Consumed
    }
    if (textChar == ']')
    {
        int numInstruments = project_.getInstrumentCount();
        if (numInstruments > 0)
            currentInstrument_ = (currentInstrument_ + 1) % numInstruments;
        repaint();
        return true;  // Consumed
    }

    // For mod rows (grid), require Alt for editing, plain arrows navigate
    if (isModRow(cursorRow_))
    {
        bool altHeld = key.getModifiers().isAltDown();

        // Alt+Left/Right adjusts field value at edges, otherwise navigates fields
        if (altHeld && (keyCode == juce::KeyPress::leftKey || keyCode == juce::KeyPress::rightKey))
        {
            int delta = (keyCode == juce::KeyPress::rightKey) ? 1 : -1;
            adjustModField(cursorRow_, cursorField_, delta);
            presetModified_ = true;
            if (onNotePreview) onNotePreview(60, currentInstrument_);
            repaint();
            return true;  // Consumed
        }
        // Alt+Up/Down adjusts current field value
        if (altHeld && keyCode == juce::KeyPress::upKey)
        {
            adjustModField(cursorRow_, cursorField_, 1, shiftHeld);
            presetModified_ = true;
            if (onNotePreview) onNotePreview(60, currentInstrument_);
            repaint();
            return true;  // Consumed
        }
        if (altHeld && keyCode == juce::KeyPress::downKey)
        {
            adjustModField(cursorRow_, cursorField_, -1, shiftHeld);
            presetModified_ = true;
            if (onNotePreview) onNotePreview(60, currentInstrument_);
            repaint();
            return true;  // Consumed
        }
        // +/- always adjusts (no Alt needed)
        int modDelta = shiftHeld ? 1 : 1;  // Mod fields use int steps
        if (textChar == '+' || textChar == '=')
        {
            adjustModField(cursorRow_, cursorField_, modDelta, shiftHeld);
            presetModified_ = true;
            if (onNotePreview) onNotePreview(60, currentInstrument_);
            repaint();
            return true;  // Consumed
        }
        if (textChar == '-')
        {
            adjustModField(cursorRow_, cursorField_, -modDelta, shiftHeld);
            presetModified_ = true;
            if (onNotePreview) onNotePreview(60, currentInstrument_);
            repaint();
            return true;  // Consumed
        }
        // Plain arrows not consumed - let navigation handle field movement
        return false;
    }

    // Handle Preset row specially - Left/Right cycles presets
    if (static_cast<InstrumentRowType>(cursorRow_) == InstrumentRowType::Preset)
    {
        if (presetManager_ && (keyCode == juce::KeyPress::leftKey || keyCode == juce::KeyPress::rightKey))
        {
            auto* instrument = project_.getInstrument(currentInstrument_);
            if (instrument)
            {
                int engine = instrument->getParams().engine;
                int presetCount = presetManager_->getPresetCount(engine);
                if (presetCount > 0)
                {
                    int newIndex = currentPresetIndex_;
                    if (keyCode == juce::KeyPress::rightKey)
                        newIndex = (currentPresetIndex_ + 1) % presetCount;
                    else
                        newIndex = (currentPresetIndex_ - 1 + presetCount) % presetCount;
                    loadPreset(newIndex);
                }
            }
            return true;  // Consumed
        }
        return false;  // Let other keys pass through
    }

    // Standard (non-grid) value adjustment
    // Left/Right adjust values directly, Up/Down are for navigation (not consumed)
    float baseDelta = shiftHeld ? 0.01f : 0.05f;
    float delta = 0.0f;

    if (textChar == '+' || textChar == '=' || keyCode == juce::KeyPress::rightKey)
        delta = baseDelta;
    else if (textChar == '-' || keyCode == juce::KeyPress::leftKey)
        delta = -baseDelta;

    if (delta != 0.0f)
    {
        setRowValue(cursorRow_, delta);
        presetModified_ = true;  // Mark preset as modified
        if (onNotePreview) onNotePreview(60, currentInstrument_);
        repaint();
        return true;  // Consumed
    }

    return false;  // Key not consumed - let Up/Down navigate between rows
}

void InstrumentScreen::setRowValue(int row, float delta)
{
    auto type = static_cast<InstrumentRowType>(row);
    auto* instrument = project_.getInstrument(currentInstrument_);
    if (!instrument) return;

    auto& params = instrument->getParams();
    auto& sends = instrument->getSends();

    auto clamp01 = [](float& v, float d) { v = std::clamp(v + d, 0.0f, 1.0f); };
    auto clampInt = [](int& v, int d, int minVal, int maxVal) { v = std::clamp(v + d, minVal, maxVal); };

    switch (type)
    {
        case InstrumentRowType::Preset:
            // Preset changes are handled by handleEditKey directly
            return;
        case InstrumentRowType::Engine:
            {
                int newEngine = std::clamp(params.engine + (delta > 0 ? 1 : -1), 0, 15);
                if (newEngine != params.engine)
                {
                    params.engine = newEngine;
                    // Reset to Init preset for new engine
                    currentPresetIndex_ = 0;
                    presetModified_ = false;
                }
            }
            break;
        case InstrumentRowType::Harmonics:
            clamp01(params.harmonics, delta);
            break;
        case InstrumentRowType::Timbre:
            clamp01(params.timbre, delta);
            break;
        case InstrumentRowType::Morph:
            clamp01(params.morph, delta);
            break;
        case InstrumentRowType::Attack:
            clamp01(params.attack, delta);
            break;
        case InstrumentRowType::Decay:
            clamp01(params.decay, delta);
            break;
        case InstrumentRowType::Polyphony:
            clampInt(params.polyphony, delta > 0 ? 1 : -1, 1, 16);
            break;
        case InstrumentRowType::Cutoff:
            clamp01(params.filter.cutoff, delta);
            break;
        case InstrumentRowType::Resonance:
            clamp01(params.filter.resonance, delta);
            break;
        case InstrumentRowType::Reverb:
            clamp01(sends.reverb, delta);
            break;
        case InstrumentRowType::Delay:
            clamp01(sends.delay, delta);
            break;
        case InstrumentRowType::Chorus:
            clamp01(sends.chorus, delta);
            break;
        case InstrumentRowType::Drive:
            clamp01(sends.drive, delta);
            break;
        case InstrumentRowType::Sidechain:
            clamp01(sends.sidechainDuck, delta);
            break;
        case InstrumentRowType::Volume:
            instrument->setVolume(instrument->getVolume() + delta);
            break;
        case InstrumentRowType::Pan:
            instrument->setPan(instrument->getPan() + delta);
            break;
        default:
            break;
    }

    // Sync changes to processor immediately for audio feedback
    if (audioEngine_)
    {
        if (auto* processor = audioEngine_->getInstrumentProcessor(currentInstrument_))
        {
            // Update the relevant processor parameter
            switch (type)
            {
                case InstrumentRowType::Engine:
                    processor->setParameter(audio::kParamEngine, params.engine / 15.0f);
                    break;
                case InstrumentRowType::Harmonics:
                    processor->setParameter(audio::kParamHarmonics, params.harmonics);
                    break;
                case InstrumentRowType::Timbre:
                    processor->setParameter(audio::kParamTimbre, params.timbre);
                    break;
                case InstrumentRowType::Morph:
                    processor->setParameter(audio::kParamMorph, params.morph);
                    break;
                case InstrumentRowType::Attack:
                    processor->setParameter(audio::kParamAttack, params.attack);
                    break;
                case InstrumentRowType::Decay:
                    processor->setParameter(audio::kParamDecay, params.decay);
                    break;
                case InstrumentRowType::Polyphony:
                    processor->setParameter(audio::kParamPolyphony, (params.polyphony - 1) / 15.0f);
                    break;
                case InstrumentRowType::Cutoff:
                    processor->setParameter(audio::kParamCutoff, params.filter.cutoff);
                    break;
                case InstrumentRowType::Resonance:
                    processor->setParameter(audio::kParamResonance, params.filter.resonance);
                    break;
                default:
                    break;
            }
        }
    }
}

void InstrumentScreen::adjustModField(int row, int field, int delta, bool fineAdjust)
{
    auto type = static_cast<InstrumentRowType>(row);

    auto* instrument = project_.getInstrument(currentInstrument_);
    if (!instrument) return;

    auto& params = instrument->getParams();

    // Get references to the appropriate modulation params
    model::LfoParams* lfoParams = nullptr;
    model::EnvModParams* envParams = nullptr;
    bool isEnv = false;

    switch (type)
    {
        case InstrumentRowType::Lfo1:
            lfoParams = &params.lfo1;
            break;
        case InstrumentRowType::Lfo2:
            lfoParams = &params.lfo2;
            break;
        case InstrumentRowType::Env1:
            envParams = &params.env1;
            isEnv = true;
            break;
        case InstrumentRowType::Env2:
            envParams = &params.env2;
            isEnv = true;
            break;
        default:
            return;
    }

    // Step sizes - smaller with fineAdjust (shift held)
    float envAttackStep = fineAdjust ? 0.004f : 0.02f;   // 2ms vs 10ms at 500ms range
    float envDecayStep = fineAdjust ? 0.005f : 0.025f;   // 10ms vs 50ms at 2000ms range
    int amountStep = fineAdjust ? 1 : 4;                  // 1 vs 4 for -64..+63

    // Update the model::Instrument parameter
    switch (field)
    {
        case 0:  // Rate or Attack
            if (isEnv && envParams)
            {
                envParams->attack = std::clamp(envParams->attack + delta * envAttackStep, 0.0f, 1.0f);
            }
            else if (lfoParams)
            {
                // Rate: 0-15 discrete values (no fine adjust, always step by 1)
                lfoParams->rate = std::clamp(lfoParams->rate + delta, 0, 15);
            }
            break;
        case 1:  // Shape or Decay
            if (isEnv && envParams)
            {
                envParams->decay = std::clamp(envParams->decay + delta * envDecayStep, 0.0f, 1.0f);
            }
            else if (lfoParams)
            {
                // Shape: 0-4 discrete values (no fine adjust)
                lfoParams->shape = std::clamp(lfoParams->shape + delta, 0, 4);
            }
            break;
        case 2:  // Destination
            if (isEnv && envParams)
            {
                envParams->dest = std::clamp(envParams->dest + delta, 0, 8);
            }
            else if (lfoParams)
            {
                lfoParams->dest = std::clamp(lfoParams->dest + delta, 0, 8);
            }
            break;
        case 3:  // Amount
            if (isEnv && envParams)
            {
                envParams->amount = std::clamp(envParams->amount + delta * amountStep, -64, 63);
            }
            else if (lfoParams)
            {
                lfoParams->amount = std::clamp(lfoParams->amount + delta * amountStep, -64, 63);
            }
            break;
    }

    // Sync changes to processor immediately for audio feedback
    if (audioEngine_)
    {
        if (auto* processor = audioEngine_->getInstrumentProcessor(currentInstrument_))
        {
            // Determine parameter indices based on row type
            int rateParam = 0, shapeParam = 0, destParam = 0, amountParam = 0;

            switch (type)
            {
                case InstrumentRowType::Lfo1:
                    rateParam = audio::kParamLfo1Rate;
                    shapeParam = audio::kParamLfo1Shape;
                    destParam = audio::kParamLfo1Dest;
                    amountParam = audio::kParamLfo1Amount;
                    break;
                case InstrumentRowType::Lfo2:
                    rateParam = audio::kParamLfo2Rate;
                    shapeParam = audio::kParamLfo2Shape;
                    destParam = audio::kParamLfo2Dest;
                    amountParam = audio::kParamLfo2Amount;
                    break;
                case InstrumentRowType::Env1:
                    rateParam = audio::kParamEnv1Attack;
                    shapeParam = audio::kParamEnv1Decay;
                    destParam = audio::kParamEnv1Dest;
                    amountParam = audio::kParamEnv1Amount;
                    break;
                case InstrumentRowType::Env2:
                    rateParam = audio::kParamEnv2Attack;
                    shapeParam = audio::kParamEnv2Decay;
                    destParam = audio::kParamEnv2Dest;
                    amountParam = audio::kParamEnv2Amount;
                    break;
                default:
                    return;
            }

            // Update the appropriate processor parameter
            if (isEnv && envParams)
            {
                processor->setParameter(rateParam, envParams->attack);
                processor->setParameter(shapeParam, envParams->decay);
                processor->setParameter(destParam, envParams->dest / 8.0f);
                processor->setParameter(amountParam, (envParams->amount + 64) / 127.0f);
            }
            else if (lfoParams)
            {
                processor->setParameter(rateParam, lfoParams->rate / 15.0f);
                processor->setParameter(shapeParam, lfoParams->shape / 4.0f);
                processor->setParameter(destParam, lfoParams->dest / 8.0f);
                processor->setParameter(amountParam, (lfoParams->amount + 64) / 127.0f);
            }
        }
    }
}

void InstrumentScreen::loadPreset(int presetIndex)
{
    if (!presetManager_)
        return;

    auto* instrument = project_.getInstrument(currentInstrument_);
    if (!instrument)
        return;

    int engine = instrument->getParams().engine;
    auto* preset = presetManager_->getPreset(engine, presetIndex);
    if (!preset)
        return;

    // Apply preset parameters to instrument
    auto& params = instrument->getParams();
    params.harmonics = preset->params.harmonics;
    params.timbre = preset->params.timbre;
    params.morph = preset->params.morph;
    params.attack = preset->params.attack;
    params.decay = preset->params.decay;
    params.polyphony = preset->params.polyphony;
    params.filter = preset->params.filter;
    params.lfo1 = preset->params.lfo1;
    params.lfo2 = preset->params.lfo2;
    params.env1 = preset->params.env1;
    params.env2 = preset->params.env2;

    // If the preset has a different engine, change engine too
    if (preset->params.engine != engine)
    {
        params.engine = preset->params.engine;
    }

    currentPresetIndex_ = presetIndex;
    presetModified_ = false;

    // Sync all parameters to audio processor
    if (audioEngine_)
    {
        if (auto* processor = audioEngine_->getInstrumentProcessor(currentInstrument_))
        {
            processor->setParameter(audio::kParamEngine, params.engine / 15.0f);
            processor->setParameter(audio::kParamHarmonics, params.harmonics);
            processor->setParameter(audio::kParamTimbre, params.timbre);
            processor->setParameter(audio::kParamMorph, params.morph);
            processor->setParameter(audio::kParamAttack, params.attack);
            processor->setParameter(audio::kParamDecay, params.decay);
            processor->setParameter(audio::kParamPolyphony, (params.polyphony - 1) / 15.0f);
            processor->setParameter(audio::kParamCutoff, params.filter.cutoff);
            processor->setParameter(audio::kParamResonance, params.filter.resonance);

            // LFO1
            processor->setParameter(audio::kParamLfo1Rate, params.lfo1.rate / 15.0f);
            processor->setParameter(audio::kParamLfo1Shape, params.lfo1.shape / 4.0f);
            processor->setParameter(audio::kParamLfo1Dest, params.lfo1.dest / 8.0f);
            processor->setParameter(audio::kParamLfo1Amount, (params.lfo1.amount + 64) / 127.0f);

            // LFO2
            processor->setParameter(audio::kParamLfo2Rate, params.lfo2.rate / 15.0f);
            processor->setParameter(audio::kParamLfo2Shape, params.lfo2.shape / 4.0f);
            processor->setParameter(audio::kParamLfo2Dest, params.lfo2.dest / 8.0f);
            processor->setParameter(audio::kParamLfo2Amount, (params.lfo2.amount + 64) / 127.0f);

            // ENV1
            processor->setParameter(audio::kParamEnv1Attack, params.env1.attack);
            processor->setParameter(audio::kParamEnv1Decay, params.env1.decay);
            processor->setParameter(audio::kParamEnv1Dest, params.env1.dest / 8.0f);
            processor->setParameter(audio::kParamEnv1Amount, (params.env1.amount + 64) / 127.0f);

            // ENV2
            processor->setParameter(audio::kParamEnv2Attack, params.env2.attack);
            processor->setParameter(audio::kParamEnv2Decay, params.env2.decay);
            processor->setParameter(audio::kParamEnv2Dest, params.env2.dest / 8.0f);
            processor->setParameter(audio::kParamEnv2Amount, (params.env2.amount + 64) / 127.0f);
        }
    }

    // Preview the sound
    if (onNotePreview)
        onNotePreview(60, currentInstrument_);

    repaint();
}

bool InstrumentScreen::handleSamplerKey(const juce::KeyPress& key, bool /*isEditMode*/)
{
    auto keyCode = key.getKeyCode();
    auto textChar = key.getTextCharacter();

    // Handle name editing mode
    if (editingName_)
    {
        auto* instrument = project_.getInstrument(currentInstrument_);
        if (!instrument)
        {
            editingName_ = false;
            repaint();
            return true;
        }

        if (keyCode == juce::KeyPress::returnKey || keyCode == juce::KeyPress::escapeKey)
        {
            if (keyCode == juce::KeyPress::returnKey)
                instrument->setName(nameBuffer_);
            editingName_ = false;
            repaint();
            return true;
        }
        if (keyCode == juce::KeyPress::backspaceKey && !nameBuffer_.empty())
        {
            nameBuffer_.pop_back();
            repaint();
            return true;
        }
        if (textChar >= ' ' && textChar <= '~' && nameBuffer_.length() < 16)
        {
            nameBuffer_ += static_cast<char>(textChar);
            repaint();
            return true;
        }
        return true;
    }

    // 'r' starts renaming current instrument
    if (textChar == 'r' || textChar == 'R')
    {
        auto* instrument = project_.getInstrument(currentInstrument_);
        if (instrument)
        {
            editingName_ = true;
            nameBuffer_ = instrument->getName();
            repaint();
            return true;
        }
        return false;
    }

    // '[' and ']' switch instruments quickly
    if (textChar == '[')
    {
        int numInstruments = project_.getInstrumentCount();
        if (numInstruments > 0)
            currentInstrument_ = (currentInstrument_ - 1 + numInstruments) % numInstruments;
        repaint();
        return true;
    }
    if (textChar == ']')
    {
        int numInstruments = project_.getInstrumentCount();
        if (numInstruments > 0)
            currentInstrument_ = (currentInstrument_ + 1) % numInstruments;
        repaint();
        return true;
    }

    // 'o' to open file chooser and load sample
    if (textChar == 'o' || textChar == 'O')
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Load Sample",
            juce::File::getSpecialLocation(juce::File::userHomeDirectory),
            "*.wav;*.aiff;*.aif"
        );

        chooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc) {
                auto file = fc.getResult();
                if (file.existsAsFile() && audioEngine_)
                {
                    if (auto* sampler = audioEngine_->getSamplerProcessor(currentInstrument_))
                    {
                        auto* inst = project_.getInstrument(currentInstrument_);
                        sampler->setInstrument(inst);
                        if (sampler->loadSample(file))
                        {
                            repaint();
                        }
                    }
                }
            }
        );
        return true;
    }

    return false;
}

void InstrumentScreen::updateSlicerDisplay() {
    if (!audioEngine_) return;

    auto* inst = project_.getInstrument(currentInstrument_);
    if (!inst || inst->getType() != model::InstrumentType::Slicer) return;

    auto* slicer = audioEngine_->getSlicerProcessor(currentInstrument_);
    if (!slicer) return;

    sliceWaveformDisplay_->setAudioData(&slicer->getSampleBuffer(), slicer->getSampleRate());
    sliceWaveformDisplay_->setSlicePoints(inst->getSlicerParams().slicePoints);
    sliceWaveformDisplay_->setCurrentSlice(inst->getSlicerParams().currentSlice);
}

void InstrumentScreen::paintSlicerUI(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    g.fillAll(juce::Colour(0xFF1E1E1E));

    g.setColour(juce::Colours::white);
    g.setFont(16.0f);

    auto* inst = project_.getInstrument(currentInstrument_);
    if (!inst) return;

    g.drawText("SLICER - " + juce::String(inst->getName()),
               bounds.removeFromTop(28), juce::Justification::centredLeft);

    // Waveform takes top portion - handled by child component
    bounds.removeFromTop(100);

    g.setFont(14.0f);
    g.setColour(juce::Colours::grey);

    const auto& params = inst->getSlicerParams();
    juce::String sampleName = params.sample.path.empty() ? "(no sample)"
        : juce::File(params.sample.path).getFileName();

    g.drawText("Sample: " + sampleName, bounds.removeFromTop(24), juce::Justification::centredLeft);
    g.drawText("Slices: " + juce::String(static_cast<int>(params.slicePoints.size())),
               bounds.removeFromTop(24), juce::Justification::centredLeft);
    g.drawText("Current: " + juce::String(params.currentSlice + 1),
               bounds.removeFromTop(24), juce::Justification::centredLeft);

    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.drawText("Press 'o' to load sample, h/l to navigate slices, s to add, d to delete",
               bounds.removeFromTop(24), juce::Justification::centredLeft);
}

bool InstrumentScreen::handleSlicerKey(const juce::KeyPress& key, bool /*isEditMode*/) {
    auto textChar = key.getTextCharacter();

    // Load sample
    if (textChar == 'o' || textChar == 'O') {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Load Sample",
            juce::File::getSpecialLocation(juce::File::userHomeDirectory),
            "*.wav;*.aiff;*.aif"
        );

        chooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc) {
                auto file = fc.getResult();
                if (file.existsAsFile() && audioEngine_) {
                    if (auto* slicer = audioEngine_->getSlicerProcessor(currentInstrument_)) {
                        slicer->setInstrument(project_.getInstrument(currentInstrument_));
                        slicer->loadSample(file);
                        updateSlicerDisplay();
                        repaint();
                    }
                }
            }
        );
        return true;
    }

    // Navigate slices: h = previous, l = next
    if (textChar == 'h' || textChar == 'H') {
        if (sliceWaveformDisplay_) {
            sliceWaveformDisplay_->previousSlice();
            auto* inst = project_.getInstrument(currentInstrument_);
            if (inst) {
                inst->getSlicerParams().currentSlice = sliceWaveformDisplay_->getCurrentSlice();
            }
            repaint();
        }
        return true;
    }

    if (textChar == 'l' || textChar == 'L') {
        if (sliceWaveformDisplay_) {
            sliceWaveformDisplay_->nextSlice();
            auto* inst = project_.getInstrument(currentInstrument_);
            if (inst) {
                inst->getSlicerParams().currentSlice = sliceWaveformDisplay_->getCurrentSlice();
            }
            repaint();
        }
        return true;
    }

    // Add slice at cursor: s
    if (textChar == 's') {
        if (sliceWaveformDisplay_) {
            sliceWaveformDisplay_->addSliceAtCursor();
            repaint();
        }
        return true;
    }

    // Delete current slice: d
    if (textChar == 'd') {
        if (sliceWaveformDisplay_) {
            sliceWaveformDisplay_->deleteCurrentSlice();
            repaint();
        }
        return true;
    }

    // Zoom: +/-
    if (textChar == '+' || textChar == '=') {
        if (sliceWaveformDisplay_) sliceWaveformDisplay_->zoomIn();
        return true;
    }
    if (textChar == '-') {
        if (sliceWaveformDisplay_) sliceWaveformDisplay_->zoomOut();
        return true;
    }

    // Scroll: Shift+h/l
    if (key.getModifiers().isShiftDown()) {
        if (textChar == 'H' || textChar == 'h') {
            if (sliceWaveformDisplay_) sliceWaveformDisplay_->scrollLeft();
            return true;
        }
        if (textChar == 'L' || textChar == 'l') {
            if (sliceWaveformDisplay_) sliceWaveformDisplay_->scrollRight();
            return true;
        }
    }

    return false;
}

void InstrumentScreen::setCurrentInstrument(int index) {
    currentInstrument_ = index;

    auto* inst = project_.getInstrument(index);
    if (inst) {
        auto type = inst->getType();
        if (sliceWaveformDisplay_) {
            sliceWaveformDisplay_->setVisible(type == model::InstrumentType::Slicer);
            if (type == model::InstrumentType::Slicer) {
                updateSlicerDisplay();
            }
        }
    }
    repaint();
}

} // namespace ui
