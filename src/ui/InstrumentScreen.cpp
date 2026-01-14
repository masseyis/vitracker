#include "InstrumentScreen.h"
#include "HelpPopup.h"
#include "../audio/AudioEngine.h"
#include "../audio/SlicerInstrument.h"
#include "../input/KeyHandler.h"
#include <cstring>

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

// Jump key mappings per engine type
// Available keys: a,b,c,d,e,f,i,m,o,p,q,s,u,w,x,y,z (17 keys - r/v reserved)
// Sequential alphabetical, mod matrix = ONE jump point
// Navigation keys excluded: g,h,j,k,l,n,t

// Plaits: 21 rows, mod matrix (rows 10-13) = 1 jump point
// Jump points: 0-9 individual, 10=mod, 14-20 individual = 18 logical (17 keys covers 0-19)
static const std::pair<char, int> kPlaitsJumpMap[] = {
    {'a', 0},  {'b', 1},  {'c', 2},  {'d', 3},  {'e', 4},   // Preset, Engine, Harmonics, Timbre, Morph
    {'f', 5},  {'i', 6},  {'m', 7},  {'o', 8},  {'p', 9},   // Attack, Decay, Polyphony, Cutoff, Resonance
    {'q', 10},                                               // Mod matrix (LFO1/2, ENV1/2)
    {'s', 14}, {'u', 15}, {'w', 16}, {'x', 17}, {'y', 18}, {'z', 19},  // Volume, Pan, Reverb, Delay, Chorus, Drive
};

// Sampler: 20 rows, mod matrix (rows 9-12) = 1 jump point
// Jump points: 0-8 individual, 9=mod, 13-19 individual = 17 logical
static const std::pair<char, int> kSamplerJumpMap[] = {
    {'a', 0},  {'b', 1},  {'c', 2},  {'d', 3},  {'e', 4},   // RootNote, TargetNote, AutoRepitch, Attack, Decay
    {'f', 5},  {'i', 6},  {'m', 7},  {'o', 8},              // Sustain, Release, Cutoff, Resonance
    {'p', 9},                                               // Mod matrix (LFO1/2, ENV1/2)
    {'q', 13}, {'s', 14}, {'u', 15}, {'w', 16}, {'x', 17}, {'y', 18}, {'z', 19},  // Reverb, Delay, Chorus, Drive, Sidechain, Volume, Pan
};

// Slicer: 19 rows, mod matrix (rows 8-11) = 1 jump point
// Jump points: 0-7 individual, 8=mod, 12-18 individual = 16 logical
static const std::pair<char, int> kSlicerJumpMap[] = {
    {'a', 0},  {'b', 1},  {'c', 2},  {'d', 3},  {'e', 4},   // ChopMode, OrigBars, OrigBPM, TargetBPM, Speed
    {'f', 5},  {'i', 6},  {'m', 7},                         // Pitch, Repitch, Polyphony
    {'o', 8},                                               // Mod matrix (LFO1/2, ENV1/2)
    {'p', 12}, {'q', 13}, {'s', 14}, {'u', 15}, {'w', 16}, {'x', 17}, {'y', 18},  // Reverb, Delay, Chorus, Drive, Sidechain, Volume, Pan
};

// VA Synth: 36 rows, osc groups (3), envelope groups (2), mod matrix = grouped
// Jump points: Osc1(0), Osc2(3), Osc3(7), Noise(11), Cutoff(12), Reso(13), EnvAmt(14),
//              AmpAtk(15), FiltAtk(19), Glide(23), Poly(24), Mod(25), Reverb-Drive(29-32), Sidechain(33)
static const std::pair<char, int> kVASynthJumpMap[] = {
    {'a', 0},  {'b', 3},  {'c', 7},                         // Osc1, Osc2, Osc3 groups
    {'d', 11}, {'e', 12}, {'f', 13}, {'i', 14},             // Noise, Cutoff, Resonance, FilterEnvAmt
    {'m', 15}, {'o', 19},                                   // AmpAttack (amp env group), FilterAttack (filter env group)
    {'p', 23}, {'q', 24},                                   // Glide, Polyphony
    {'s', 25},                                              // Mod matrix (LFO1/2, ENV1/2)
    {'u', 29}, {'w', 30}, {'x', 31}, {'y', 32}, {'z', 33},  // Reverb, Delay, Chorus, Drive, Sidechain
};

// Helper: Check if a character is a valid jump key for ANY engine
static bool isJumpKey(char c)
{
    // Available: a,b,c,d,e,f,i,m,o,p,q,s,u,w,x,y,z
    // Excluded: g,h,j,k,l,n,r,t,v (nav + reserved)
    return (c >= 'a' && c <= 'z') &&
           c != 'g' && c != 'h' && c != 'j' && c != 'k' && c != 'l' &&
           c != 'n' && c != 'r' && c != 't' && c != 'v';
}

char InstrumentScreen::getJumpKeyForRow(model::InstrumentType type, int row)
{
    const std::pair<char, int>* map = nullptr;
    size_t mapSize = 0;

    switch (type) {
        case model::InstrumentType::Plaits:
            map = kPlaitsJumpMap;
            mapSize = sizeof(kPlaitsJumpMap) / sizeof(kPlaitsJumpMap[0]);
            break;
        case model::InstrumentType::Sampler:
            map = kSamplerJumpMap;
            mapSize = sizeof(kSamplerJumpMap) / sizeof(kSamplerJumpMap[0]);
            break;
        case model::InstrumentType::Slicer:
            map = kSlicerJumpMap;
            mapSize = sizeof(kSlicerJumpMap) / sizeof(kSlicerJumpMap[0]);
            break;
        case model::InstrumentType::VASynth:
            map = kVASynthJumpMap;
            mapSize = sizeof(kVASynthJumpMap) / sizeof(kVASynthJumpMap[0]);
            break;
    }

    if (map) {
        for (size_t i = 0; i < mapSize; ++i) {
            if (map[i].second == row)
                return map[i].first;
        }
    }
    return '\0';
}

int InstrumentScreen::getRowForJumpKey(model::InstrumentType type, char key)
{
    const std::pair<char, int>* map = nullptr;
    size_t mapSize = 0;

    switch (type) {
        case model::InstrumentType::Plaits:
            map = kPlaitsJumpMap;
            mapSize = sizeof(kPlaitsJumpMap) / sizeof(kPlaitsJumpMap[0]);
            break;
        case model::InstrumentType::Sampler:
            map = kSamplerJumpMap;
            mapSize = sizeof(kSamplerJumpMap) / sizeof(kSamplerJumpMap[0]);
            break;
        case model::InstrumentType::Slicer:
            map = kSlicerJumpMap;
            mapSize = sizeof(kSlicerJumpMap) / sizeof(kSlicerJumpMap[0]);
            break;
        case model::InstrumentType::VASynth:
            map = kVASynthJumpMap;
            mapSize = sizeof(kVASynthJumpMap) / sizeof(kVASynthJumpMap[0]);
            break;
    }

    if (map) {
        for (size_t i = 0; i < mapSize; ++i) {
            if (map[i].first == key)
                return map[i].second;
        }
    }
    return -1;
}

InstrumentScreen::InstrumentScreen(model::Project& project, input::ModeManager& modeManager)
    : Screen(project, modeManager)
{
    // Slicer waveform display
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

    // Sampler waveform display
    samplerWaveformDisplay_ = std::make_unique<WaveformDisplay>();
    addChildComponent(samplerWaveformDisplay_.get());

    // Load DX7 presets from bundled resources (or fall back to built-in presets)
    dxPresetBank_.loadBundledPresets();

    // Start playhead update timer (30Hz for smooth animation)
    startTimer(33);
}

void InstrumentScreen::setAudioEngine(audio::AudioEngine* engine)
{
    audioEngine_ = engine;
}

void InstrumentScreen::timerCallback()
{
    if (!audioEngine_) return;

    auto* instrument = project_.getInstrument(currentInstrument_);
    if (!instrument) return;

    if (instrument->getType() == model::InstrumentType::Slicer) {
        if (auto* slicer = audioEngine_->getSlicerProcessor(currentInstrument_)) {
            int64_t pos = slicer->getPlayheadPosition();
            if (sliceWaveformDisplay_) {
                sliceWaveformDisplay_->setPlayheadPosition(pos);
            }
        }
    } else if (instrument->getType() == model::InstrumentType::Sampler) {
        if (auto* sampler = audioEngine_->getSamplerProcessor(currentInstrument_)) {
            int64_t pos = sampler->getPlayheadPosition();
            if (samplerWaveformDisplay_) {
                samplerWaveformDisplay_->setPlayheadPosition(pos);
            }
        }
    }
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

void InstrumentScreen::drawTypeSelector(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto* instrument = project_.getInstrument(currentInstrument_);
    if (!instrument) return;

    auto currentType = instrument->getType();

    g.setFont(12.0f);
    g.setColour(fgColor.darker(0.5f));
    g.drawText("TYPE:", area.removeFromLeft(50), juce::Justification::centredLeft);

    const char* typeNames[] = {"Plaits", "Sampler", "Slicer", "VASynth", "DX7"};
    const model::InstrumentType types[] = {
        model::InstrumentType::Plaits,
        model::InstrumentType::Sampler,
        model::InstrumentType::Slicer,
        model::InstrumentType::VASynth,
        model::InstrumentType::DXPreset
    };

    for (int i = 0; i < 5; ++i) {
        auto buttonArea = area.removeFromLeft(60);
        bool isSelected = (types[i] == currentType);

        if (isSelected) {
            g.setColour(cursorColor.withAlpha(0.3f));
            g.fillRoundedRectangle(buttonArea.reduced(2).toFloat(), 4.0f);
        }

        g.setColour(isSelected ? cursorColor : fgColor.darker(0.3f));
        g.drawText(typeNames[i], buttonArea, juce::Justification::centred);

        area.removeFromLeft(4);  // Gap between buttons
    }

    // Hint for switching
    g.setColour(fgColor.darker(0.6f));
    g.drawText("[Tab to switch]", area, juce::Justification::centredLeft);
}

void InstrumentScreen::paint(juce::Graphics& g)
{
    g.fillAll(bgColor);

    auto area = getLocalBounds();

    // Instrument selector tabs at top
    drawInstrumentTabs(g, area.removeFromTop(30));

    // Type selector row
    auto typeSelectorArea = area.removeFromTop(26);
    g.setColour(headerColor.darker(0.3f));
    g.fillRect(typeSelectorArea);
    drawTypeSelector(g, typeSelectorArea.reduced(10, 2));

    // Header bar (matches Pattern screen style)
    auto headerArea = area.removeFromTop(30);
    g.setColour(headerColor);
    g.fillRect(headerArea);

    auto* instrument = project_.getInstrument(currentInstrument_);

    // Check if this is a sampler instrument and render sampler UI
    if (instrument && instrument->getType() == model::InstrumentType::Sampler) {
        // Paint header
        g.setColour(fgColor);
        g.setFont(18.0f);
        juce::String title = "SAMPLER: ";
        if (editingName_)
            title += juce::String(nameBuffer_) + "_";
        else
            title += juce::String(instrument->getName());
        g.drawText(title, headerArea.reduced(10, 0), juce::Justification::centredLeft, true);

        paintSamplerUI(g);
        return;
    }

    // Check if this is a slicer instrument and render slicer UI
    if (instrument && instrument->getType() == model::InstrumentType::Slicer) {
        paintSlicerUI(g);
        return;
    }

    // Check if this is a VA synth instrument and render VA synth UI
    if (instrument && instrument->getType() == model::InstrumentType::VASynth) {
        paintVASynthUI(g);
        return;
    }

    // Check if this is a DX Preset instrument and render DX preset UI
    if (instrument && instrument->getType() == model::InstrumentType::DXPreset) {
        paintDXPresetUI(g);
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

            // Draw shortcut key hint
            char shortcutKey = getJumpKeyForRow(model::InstrumentType::Plaits, row);
            if (shortcutKey != '\0')
            {
                g.setColour(fgColor.withAlpha(0.4f));
                g.setFont(11.0f);
                g.drawText(juce::String::charToString(shortcutKey), area.getX(), area.getY(), kShortcutWidth, kRowHeight, juce::Justification::centred);
                g.setFont(14.0f);
                g.setColour(selected ? cursorColor : fgColor);
            }

            // Draw preset row without slider bar
            int labelX = area.getX() + kShortcutWidth;
            g.drawText(label, labelX, area.getY(), kLabelWidth, kRowHeight, juce::Justification::centredLeft);
            g.drawText(valueText, labelX + kLabelWidth, area.getY(), area.getWidth() - kLabelWidth - kShortcutWidth, kRowHeight, juce::Justification::centredLeft);
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

        // Voice-per-track architecture: polyphony removed for Plaits, one voice per track
        // case InstrumentRowType::Polyphony:
        //     label = "VOICES";
        //     {
        //         int poly = params.polyphony;
        //         value = static_cast<float>(poly - 1) / 15.0f;
        //         valueText = juce::String(poly);
        //     }
        //     break;

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

        default:
            break;
    }

    // Draw shortcut key hint
    char shortcutKey = getJumpKeyForRow(model::InstrumentType::Plaits, row);
    if (shortcutKey != '\0')
    {
        g.setColour(fgColor.withAlpha(0.4f));
        g.setFont(11.0f);
        g.drawText(juce::String::charToString(shortcutKey), area.getX(), area.getY(), kShortcutWidth, kRowHeight, juce::Justification::centred);
        g.setFont(14.0f);
        g.setColour(selected ? cursorColor : fgColor);
    }

    // Draw label (offset by shortcut width)
    int labelX = area.getX() + kShortcutWidth;
    g.drawText(label, labelX, area.getY(), kLabelWidth, kRowHeight, juce::Justification::centredLeft);

    // Draw slider bar
    int barX = labelX + kLabelWidth;
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

    // Draw shortcut key hint
    char shortcutKey = getJumpKeyForRow(model::InstrumentType::Plaits, row);
    if (shortcutKey != '\0')
    {
        g.setColour(fgColor.withAlpha(0.4f));
        g.setFont(11.0f);
        g.drawText(juce::String::charToString(shortcutKey), area.getX(), area.getY(), kShortcutWidth, kRowHeight, juce::Justification::centred);
        g.setFont(14.0f);
    }

    // Draw label (offset by shortcut width)
    int labelX = area.getX() + kShortcutWidth;
    g.setColour(selected ? cursorColor : fgColor);
    g.drawText(label, labelX, area.getY(), kLabelWidth, kRowHeight, juce::Justification::centredLeft);

    // Draw 4 fields: rate/attack, shape/decay, dest, amount
    int fieldX = labelX + kLabelWidth;
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
    auto bounds = getLocalBounds();
    bounds.removeFromTop(30);  // Instrument tabs
    bounds.removeFromTop(26);  // Type selector
    bounds.removeFromTop(30);  // Header

    auto waveformArea = bounds.removeFromTop(100).reduced(16, 0);

    if (sliceWaveformDisplay_) {
        sliceWaveformDisplay_->setBounds(waveformArea);
    }
    if (samplerWaveformDisplay_) {
        samplerWaveformDisplay_->setBounds(waveformArea);
    }
}

void InstrumentScreen::navigate(int dx, int dy)
{
    auto* instrument = project_.getInstrument(currentInstrument_);

    // Handle Slicer navigation
    if (instrument && instrument->getType() == model::InstrumentType::Slicer) {
        if (dy != 0) {
            slicerCursorRow_ = std::clamp(slicerCursorRow_ + dy, 0, kNumSlicerRows - 1);
        }
        if (dx != 0) {
            // Left/Right switches instruments in Slicer mode
            int numInstruments = project_.getInstrumentCount();
            currentInstrument_ = (currentInstrument_ + dx + numInstruments) % numInstruments;
            setCurrentInstrument(currentInstrument_);
        }
        repaint();
        return;
    }

    // Handle Sampler navigation
    if (instrument && instrument->getType() == model::InstrumentType::Sampler) {
        if (dy != 0) {
            samplerCursorRow_ = std::clamp(samplerCursorRow_ + dy, 0, kNumSamplerRows - 1);
        }
        if (dx != 0) {
            // Left/Right switches instruments in Sampler mode
            int numInstruments = project_.getInstrumentCount();
            currentInstrument_ = (currentInstrument_ + dx + numInstruments) % numInstruments;
            setCurrentInstrument(currentInstrument_);
        }
        repaint();
        return;
    }

    // Handle VA Synth navigation
    if (instrument && instrument->getType() == model::InstrumentType::VASynth) {
        if (dy != 0) {
            // Up/down navigation within current column
            if (vaSynthCursorCol_ == 0) {
                // Left column: 0 to AmpRelease
                int maxRow = static_cast<int>(VASynthRowType::AmpRelease);
                vaSynthCursorRow_ = std::clamp(vaSynthCursorRow_ + dy, 0, maxRow);
            } else {
                // Right column: FilterAttack to Env2
                int minRow = static_cast<int>(VASynthRowType::FilterAttack);
                int maxRow = static_cast<int>(VASynthRowType::Env2);
                vaSynthCursorRow_ = std::clamp(vaSynthCursorRow_ + dy, minRow, maxRow);
            }
        }
        if (dx != 0) {
            // Left/Right switches columns
            if (dx < 0 && vaSynthCursorCol_ == 1) {
                // Move to left column
                vaSynthCursorCol_ = 0;
                // Clamp row to left column range
                int maxRow = static_cast<int>(VASynthRowType::AmpRelease);
                vaSynthCursorRow_ = std::min(vaSynthCursorRow_, maxRow);
            } else if (dx > 0 && vaSynthCursorCol_ == 0) {
                // Move to right column
                vaSynthCursorCol_ = 1;
                // Ensure we're in right column range
                int minRow = static_cast<int>(VASynthRowType::FilterAttack);
                if (vaSynthCursorRow_ < minRow) {
                    vaSynthCursorRow_ = minRow;
                }
            }
        }
        repaint();
        return;
    }

    // Handle DX Preset navigation
    if (instrument && instrument->getType() == model::InstrumentType::DXPreset) {
        if (dy != 0) {
            dxPresetCursorRow_ = std::clamp(dxPresetCursorRow_ + dy, 0, kNumDXPresetRows - 1);
        }
        if (dx != 0) {
            // Left/Right switches instruments in DXPreset mode
            int numInstruments = project_.getInstrumentCount();
            currentInstrument_ = (currentInstrument_ + dx + numInstruments) % numInstruments;
            setCurrentInstrument(currentInstrument_);
        }
        repaint();
        return;
    }

    // Plaits navigation (original behavior)
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

input::InputContext InstrumentScreen::getInputContext() const
{
    // Text editing mode takes priority
    if (editingName_)
        return input::InputContext::TextEdit;

    // Check instrument type for different UI layouts
    auto* instrument = project_.getInstrument(currentInstrument_);
    if (!instrument)
        return input::InputContext::RowParams;

    // VA Synth has a two-column grid layout
    if (instrument->getType() == model::InstrumentType::VASynth)
    {
        // All VA Synth uses Grid for column navigation
        return input::InputContext::Grid;
    }

    // Sampler/Slicer/Plaits are vertical param lists with potential multi-field mod rows
    if (instrument->getType() == model::InstrumentType::Sampler)
    {
        auto rowType = static_cast<SamplerRowType>(samplerCursorRow_);
        if (rowType == SamplerRowType::Lfo1 || rowType == SamplerRowType::Lfo2 ||
            rowType == SamplerRowType::Env1 || rowType == SamplerRowType::Env2)
        {
            return input::InputContext::Grid;  // Mod rows are grids
        }
    }
    else if (instrument->getType() == model::InstrumentType::Slicer)
    {
        auto rowType = static_cast<SlicerRowType>(slicerCursorRow_);
        if (rowType == SlicerRowType::Lfo1 || rowType == SlicerRowType::Lfo2 ||
            rowType == SlicerRowType::Env1 || rowType == SlicerRowType::Env2)
        {
            return input::InputContext::Grid;  // Mod rows are grids
        }
    }
    else // Plaits (default)
    {
        // Check if on mod row
        if (isModRow(cursorRow_))
            return input::InputContext::Grid;
    }

    // Default: vertical param list with horizontal editing
    return input::InputContext::RowParams;
}

bool InstrumentScreen::handleEdit(const juce::KeyPress& key)
{
    return handleEditKey(key);
}

bool InstrumentScreen::handleEditKey(const juce::KeyPress& key)
{
    // Use centralized key translation for consistent hjkl/arrow handling
    auto action = input::KeyHandler::translateKey(key, getInputContext());
    bool shiftHeld = key.getModifiers().isShiftDown();
    auto textChar = key.getTextCharacter();

    // Skip jump key handling if we're in name editing mode (let text input work)
    // Also check for jump keys DIRECTLY from raw keypress (before translateKey filters them)
    // This allows keys like d,f,m,o,p,s,u,y to work as jump keys on instrument screens
    if (!editingName_ && !key.getModifiers().isAltDown() && !key.getModifiers().isCtrlDown() &&
        !key.getModifiers().isCommandDown() && isJumpKey(static_cast<char>(textChar)))
    {
        auto* inst = project_.getInstrument(currentInstrument_);
        if (inst)
        {
            int targetRow = getRowForJumpKey(inst->getType(), static_cast<char>(textChar));
            if (targetRow >= 0)
            {
                // Delegate to appropriate handler based on instrument type
                if (inst->getType() == model::InstrumentType::Plaits && targetRow < kNumRows)
                {
                    cursorRow_ = targetRow;
                    cursorField_ = 0;
                    repaint();
                    return true;
                }
                else if (inst->getType() == model::InstrumentType::Sampler && targetRow < kNumSamplerRows)
                {
                    samplerCursorRow_ = targetRow;
                    samplerCursorField_ = 0;
                    repaint();
                    return true;
                }
                else if (inst->getType() == model::InstrumentType::Slicer && targetRow < kNumSlicerRows)
                {
                    slicerCursorRow_ = targetRow;
                    slicerCursorField_ = 0;
                    repaint();
                    return true;
                }
                else if (inst->getType() == model::InstrumentType::VASynth && targetRow < kNumVASynthRows)
                {
                    vaSynthCursorRow_ = targetRow;
                    vaSynthCursorField_ = 0;
                    repaint();
                    return true;
                }
            }
        }
    }

    // Handle text editing mode via KeyAction
    if (editingName_)
    {
        auto* instrument = project_.getInstrument(currentInstrument_);
        if (!instrument)
        {
            editingName_ = false;
            repaint();
            return true;
        }

        switch (action.action)
        {
            case input::KeyAction::TextAccept:
                instrument->setName(nameBuffer_);
                editingName_ = false;
                repaint();
                return true;
            case input::KeyAction::TextReject:
                editingName_ = false;
                repaint();
                return true;
            case input::KeyAction::TextBackspace:
                if (!nameBuffer_.empty())
                {
                    nameBuffer_.pop_back();
                    repaint();
                }
                return true;
            case input::KeyAction::TextChar:
                if (nameBuffer_.length() < 16)
                {
                    nameBuffer_ += action.charData;
                    repaint();
                }
                return true;
            default:
                return true;  // Consume all keys in name editing mode
        }
    }

    // Tab cycles instrument types (handle before instrument-type delegation)
    if (action.action == input::KeyAction::TabNext)
    {
        cycleInstrumentType(false);
        return true;
    }
    if (action.action == input::KeyAction::TabPrev)
    {
        cycleInstrumentType(true);
        return true;
    }

    // Check if this is a sampler instrument and handle sampler keys
    auto* instrument = project_.getInstrument(currentInstrument_);
    if (instrument && instrument->getType() == model::InstrumentType::Sampler) {
        return handleSamplerKey(key, true);
    }

    // Check if this is a slicer instrument and handle slicer keys
    if (instrument && instrument->getType() == model::InstrumentType::Slicer) {
        return handleSlicerKey(key, true);
    }

    // Check if this is a VA synth instrument and handle VA synth keys
    if (instrument && instrument->getType() == model::InstrumentType::VASynth) {
        return handleVASynthKey(key, true);
    }

    // Check if this is a DX Preset instrument and handle DX preset keys
    if (instrument && instrument->getType() == model::InstrumentType::DXPreset) {
        return handleDXPresetKey(key, true);
    }

    // Handle standard actions via KeyAction
    switch (action.action)
    {
        case input::KeyAction::NewItem:  // Shift+N
        {
            currentInstrument_ = project_.addInstrument("Inst " + std::to_string(project_.getInstrumentCount() + 1));
            auto* newInst = project_.getInstrument(currentInstrument_);
            if (newInst)
            {
                editingName_ = true;
                nameBuffer_ = newInst->getName();
            }
            repaint();
            return false;  // Let other handlers run too
        }

        case input::KeyAction::Rename:  // 'r'
        {
            auto* inst = project_.getInstrument(currentInstrument_);
            if (inst)
            {
                editingName_ = true;
                nameBuffer_ = inst->getName();
                repaint();
                return true;
            }
            return false;
        }

        case input::KeyAction::PatternPrev:  // '[' - switch instruments
        {
            int numInstruments = project_.getInstrumentCount();
            if (numInstruments > 0)
                currentInstrument_ = (currentInstrument_ - 1 + numInstruments) % numInstruments;
            repaint();
            return true;
        }

        case input::KeyAction::PatternNext:  // ']' - switch instruments
        {
            int numInstruments = project_.getInstrumentCount();
            if (numInstruments > 0)
                currentInstrument_ = (currentInstrument_ + 1) % numInstruments;
            repaint();
            return true;
        }

        case input::KeyAction::Jump:
        {
            // Use engine-aware jump key mapping for Plaits
            int targetRow = getRowForJumpKey(model::InstrumentType::Plaits, action.charData);
            if (targetRow >= 0 && targetRow < kNumRows)
            {
                cursorRow_ = targetRow;
                cursorField_ = 0;
                repaint();
                return true;
            }
            break;
        }

        default:
            break;
    }

    // For mod rows (grid context), handle edit actions
    // Edit2 = horizontal (h/l), Edit1 = vertical (j/k with Alt)
    // ShiftEdit* = coarse adjustment
    if (isModRow(cursorRow_))
    {
        switch (action.action)
        {
            case input::KeyAction::Edit2Inc:  // h/l or Alt+h/l - fine horizontal
            case input::KeyAction::Edit1Inc:  // Alt+j/k - fine vertical
                adjustModField(cursorRow_, cursorField_, 1, false);
                presetModified_ = true;
                if (onNotePreview) onNotePreview(60, currentInstrument_);
                repaint();
                return true;
            case input::KeyAction::Edit2Dec:
            case input::KeyAction::Edit1Dec:
                adjustModField(cursorRow_, cursorField_, -1, false);
                presetModified_ = true;
                if (onNotePreview) onNotePreview(60, currentInstrument_);
                repaint();
                return true;
            case input::KeyAction::ShiftEdit2Inc:  // Shift+Alt+h/l - coarse horizontal
            case input::KeyAction::ShiftEdit1Inc:  // Shift+Alt+j/k - coarse vertical
                adjustModField(cursorRow_, cursorField_, 1, true);
                presetModified_ = true;
                if (onNotePreview) onNotePreview(60, currentInstrument_);
                repaint();
                return true;
            case input::KeyAction::ShiftEdit2Dec:
            case input::KeyAction::ShiftEdit1Dec:
                adjustModField(cursorRow_, cursorField_, -1, true);
                presetModified_ = true;
                if (onNotePreview) onNotePreview(60, currentInstrument_);
                repaint();
                return true;
            case input::KeyAction::ZoomIn:  // +/= without Alt
                adjustModField(cursorRow_, cursorField_, 1, shiftHeld);
                presetModified_ = true;
                if (onNotePreview) onNotePreview(60, currentInstrument_);
                repaint();
                return true;
            case input::KeyAction::ZoomOut:  // - without Alt
                adjustModField(cursorRow_, cursorField_, -1, shiftHeld);
                presetModified_ = true;
                if (onNotePreview) onNotePreview(60, currentInstrument_);
                repaint();
                return true;
            default:
                break;
        }
        // Navigation actions not consumed - let navigate() handle field movement
        return false;
    }

    // Handle Preset row specially - edit actions cycle presets
    if (static_cast<InstrumentRowType>(cursorRow_) == InstrumentRowType::Preset)
    {
        if (presetManager_ && action.isEdit())
        {
            auto* inst = project_.getInstrument(currentInstrument_);
            if (inst)
            {
                int engine = inst->getParams().engine;
                int presetCount = presetManager_->getPresetCount(engine);
                if (presetCount > 0)
                {
                    int newIndex = currentPresetIndex_;
                    if (action.action == input::KeyAction::Edit1Inc || action.action == input::KeyAction::Edit2Inc ||
                        action.action == input::KeyAction::ShiftEdit1Inc || action.action == input::KeyAction::ShiftEdit2Inc)
                        newIndex = (currentPresetIndex_ + 1) % presetCount;
                    else
                        newIndex = (currentPresetIndex_ - 1 + presetCount) % presetCount;
                    loadPreset(newIndex);
                }
            }
            return true;
        }
        return false;
    }

    // Standard (non-grid) value adjustment via edit actions
    // Edit2 = horizontal (primary for RowParams), Edit1 = vertical (Alt+j/k)
    // ShiftEdit* = coarse adjustment
    float fineDelta = 0.05f;
    float coarseDelta = 0.01f;  // Smaller for precision control
    float delta = 0.0f;

    switch (action.action)
    {
        case input::KeyAction::Edit2Inc:  // h/l
        case input::KeyAction::Edit1Inc:  // Alt+j/k
        case input::KeyAction::ZoomIn:
            delta = fineDelta;
            break;
        case input::KeyAction::Edit2Dec:
        case input::KeyAction::Edit1Dec:
        case input::KeyAction::ZoomOut:
            delta = -fineDelta;
            break;
        case input::KeyAction::ShiftEdit2Inc:  // Shift+h/l
        case input::KeyAction::ShiftEdit1Inc:  // Shift+Alt+j/k
            delta = coarseDelta;
            break;
        case input::KeyAction::ShiftEdit2Dec:
        case input::KeyAction::ShiftEdit1Dec:
            delta = -coarseDelta;
            break;
        default:
            break;
    }

    if (delta != 0.0f)
    {
        setRowValue(cursorRow_, delta);
        presetModified_ = true;
        if (onNotePreview) onNotePreview(60, currentInstrument_);
        repaint();
        return true;
    }

    return false;  // Key not consumed - let navigation handle movement
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
        // Voice-per-track architecture: polyphony control removed for Plaits
        // case InstrumentRowType::Polyphony:
        //     clampInt(params.polyphony, delta > 0 ? 1 : -1, 1, 16);
        //     break;
        case InstrumentRowType::Cutoff:
            clamp01(params.filter.cutoff, delta);
            break;
        case InstrumentRowType::Resonance:
            clamp01(params.filter.resonance, delta);
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
    // Use centralized key translation for consistent action handling
    auto action = input::KeyHandler::translateKey(key, getInputContext());

    auto* instrument = project_.getInstrument(currentInstrument_);
    if (!instrument) return false;

    auto& params = instrument->getSamplerParams();

    // Handle name editing mode via actions
    if (editingName_)
    {
        switch (action.action)
        {
            case input::KeyAction::TextAccept:
                instrument->setName(nameBuffer_);
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
                return true;  // Consume all keys in name editing mode
        }
    }

    // Check if current row is a modulation row (has multiple fields)
    auto isModRow = [](int row) {
        auto type = static_cast<SamplerRowType>(row);
        return type == SamplerRowType::Lfo1 || type == SamplerRowType::Lfo2 ||
               type == SamplerRowType::Env1 || type == SamplerRowType::Env2;
    };

    // Handle actions
    switch (action.action)
    {
        case input::KeyAction::NewItem:  // Shift+N
            currentInstrument_ = project_.addInstrument("Inst " + std::to_string(project_.getInstrumentCount() + 1));
            {
                auto* newInst = project_.getInstrument(currentInstrument_);
                if (newInst) {
                    editingName_ = true;
                    nameBuffer_ = newInst->getName();
                }
            }
            repaint();
            return true;

        case input::KeyAction::Rename:  // 'r'
            editingName_ = true;
            nameBuffer_ = instrument->getName();
            repaint();
            return true;

        case input::KeyAction::PatternPrev:  // '['
        {
            int numInstruments = project_.getInstrumentCount();
            if (numInstruments > 0) {
                currentInstrument_ = (currentInstrument_ - 1 + numInstruments) % numInstruments;
                setCurrentInstrument(currentInstrument_);
            }
            return true;
        }

        case input::KeyAction::PatternNext:  // ']'
        {
            int numInstruments = project_.getInstrumentCount();
            if (numInstruments > 0) {
                currentInstrument_ = (currentInstrument_ + 1) % numInstruments;
                setCurrentInstrument(currentInstrument_);
            }
            return true;
        }

        case input::KeyAction::Jump:
        {
            // Use engine-aware jump key mapping for Sampler
            int targetRow = getRowForJumpKey(model::InstrumentType::Sampler, action.charData);
            if (targetRow >= 0 && targetRow < kNumSamplerRows)
            {
                samplerCursorRow_ = targetRow;
                samplerCursorField_ = 0;
                repaint();
                return true;
            }
            break;
        }

        case input::KeyAction::Confirm:  // Enter: audition sample
            if (onNotePreview) onNotePreview(60, currentInstrument_);
            return true;

        case input::KeyAction::NavUp:
            samplerCursorRow_ = std::max(0, samplerCursorRow_ - 1);
            repaint();
            return true;

        case input::KeyAction::NavDown:
            samplerCursorRow_ = std::min(kNumSamplerRows - 1, samplerCursorRow_ + 1);
            repaint();
            return true;

        case input::KeyAction::TabNext:
            if (isModRow(samplerCursorRow_))
            {
                samplerCursorField_ = (samplerCursorField_ + 1) % 4;
                repaint();
                return true;
            }
            break;

        case input::KeyAction::TabPrev:
            if (isModRow(samplerCursorRow_))
            {
                samplerCursorField_ = (samplerCursorField_ + 3) % 4;
                repaint();
                return true;
            }
            break;

        case input::KeyAction::NavLeft:
            if (isModRow(samplerCursorRow_))
            {
                samplerCursorField_ = (samplerCursorField_ + 3) % 4;  // -1 with wrap
                repaint();
                return true;
            }
            break;

        case input::KeyAction::NavRight:
            if (isModRow(samplerCursorRow_))
            {
                samplerCursorField_ = (samplerCursorField_ + 1) % 4;
                repaint();
                return true;
            }
            break;

        case input::KeyAction::Open:  // 'o'
        {
            auto chooser = std::make_shared<juce::FileChooser>(
                "Load Sample",
                juce::File::getSpecialLocation(juce::File::userHomeDirectory),
                "*.wav;*.aiff;*.aif;*.mp3;*.flac;*.ogg"
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
                                updateSamplerDisplay();
                                repaint();
                            }
                        }
                    }
                }
            );
            return true;
        }

        default:
            break;
    }

    // Handle edit actions for value adjustment
    int delta = 0;
    bool isCoarse = false;
    switch (action.action)
    {
        case input::KeyAction::Edit2Inc:
        case input::KeyAction::Edit1Inc:
            delta = 1;
            break;
        case input::KeyAction::Edit2Dec:
        case input::KeyAction::Edit1Dec:
            delta = -1;
            break;
        case input::KeyAction::ShiftEdit2Inc:
        case input::KeyAction::ShiftEdit1Inc:
            delta = 1;
            isCoarse = true;
            break;
        case input::KeyAction::ShiftEdit2Dec:
        case input::KeyAction::ShiftEdit1Dec:
            delta = -1;
            isCoarse = true;
            break;
        case input::KeyAction::ZoomIn:  // +/=
            delta = 1;
            break;
        case input::KeyAction::ZoomOut:  // -
            delta = -1;
            break;
        default:
            break;
    }

    if (delta != 0)
    {
        // Handle modulation rows specially (multi-field)
        if (isModRow(samplerCursorRow_))
        {
            auto& mod = params.modulation;
            auto rowType = static_cast<SamplerRowType>(samplerCursorRow_);

            // Get reference to the appropriate params
            model::SamplerLfoParams* lfo = nullptr;
            model::SamplerModEnvParams* env = nullptr;
            bool isEnv = (rowType == SamplerRowType::Env1 || rowType == SamplerRowType::Env2);

            if (rowType == SamplerRowType::Lfo1) lfo = &mod.lfo1;
            else if (rowType == SamplerRowType::Lfo2) lfo = &mod.lfo2;
            else if (rowType == SamplerRowType::Env1) env = &mod.env1;
            else if (rowType == SamplerRowType::Env2) env = &mod.env2;

            // Adjust based on field
            if (lfo) {
                switch (samplerCursorField_) {
                    case 0: lfo->rateIndex = std::clamp(lfo->rateIndex + delta, 0, 15); break;
                    case 1: lfo->shapeIndex = std::clamp(lfo->shapeIndex + delta, 0, 4); break;
                    case 2: lfo->destIndex = std::clamp(lfo->destIndex + delta, 0, static_cast<int>(model::SamplerModDest::NumDestinations) - 1); break;
                    case 3: lfo->amount = static_cast<int8_t>(std::clamp(static_cast<int>(lfo->amount) + delta, -64, 63)); break;
                }
            } else if (env) {
                float envStep = isCoarse ? 0.01f : 0.05f;
                switch (samplerCursorField_) {
                    case 0: env->attack = std::clamp(env->attack + delta * envStep, 0.001f, 2.0f); break;
                    case 1: env->decay = std::clamp(env->decay + delta * envStep, 0.001f, 10.0f); break;
                    case 2: env->destIndex = std::clamp(env->destIndex + delta, 0, static_cast<int>(model::SamplerModDest::NumDestinations) - 1); break;
                    case 3: env->amount = static_cast<int8_t>(std::clamp(static_cast<int>(env->amount) + delta, -64, 63)); break;
                }
            }
            if (onNotePreview) onNotePreview(60, currentInstrument_);
            repaint();
            return true;
        }

        auto recalculatePitchRatio = [&]() {
            if (params.autoRepitchEnabled && params.rootNote > 0) {
                params.pitchRatio = static_cast<float>(std::pow(2.0, (params.targetRootNote - params.rootNote) / 12.0));
            } else {
                params.pitchRatio = 1.0f;
            }
        };

        float step = isCoarse ? 0.01f : 0.05f;
        int noteStep = isCoarse ? 12 : 1;  // octave with shift, semitone by default

        switch (static_cast<SamplerRowType>(samplerCursorRow_))
        {
            case SamplerRowType::RootNote:
                params.rootNote = std::clamp(params.rootNote + delta * noteStep, 0, 127);
                recalculatePitchRatio();
                break;
            case SamplerRowType::TargetNote:
                params.targetRootNote = std::clamp(params.targetRootNote + delta * noteStep, 0, 127);
                recalculatePitchRatio();
                break;
            case SamplerRowType::AutoRepitch:
                params.autoRepitchEnabled = !params.autoRepitchEnabled;
                recalculatePitchRatio();
                break;
            case SamplerRowType::Attack:
                params.ampEnvelope.attack = std::clamp(params.ampEnvelope.attack + delta * step, 0.001f, 10.0f);
                break;
            case SamplerRowType::Decay:
                params.ampEnvelope.decay = std::clamp(params.ampEnvelope.decay + delta * step, 0.001f, 10.0f);
                break;
            case SamplerRowType::Sustain:
                params.ampEnvelope.sustain = std::clamp(params.ampEnvelope.sustain + delta * step, 0.0f, 1.0f);
                break;
            case SamplerRowType::Release:
                params.ampEnvelope.release = std::clamp(params.ampEnvelope.release + delta * step, 0.001f, 10.0f);
                break;
            case SamplerRowType::Cutoff:
                params.filter.cutoff = std::clamp(params.filter.cutoff + delta * step, 0.0f, 1.0f);
                break;
            case SamplerRowType::Resonance:
                params.filter.resonance = std::clamp(params.filter.resonance + delta * step, 0.0f, 1.0f);
                break;
            default:
                break;
        }
        repaint();
        return true;
    }

    return false;
}

void InstrumentScreen::updateSamplerDisplay() {
    if (!audioEngine_) return;

    auto* inst = project_.getInstrument(currentInstrument_);
    if (!inst || inst->getType() != model::InstrumentType::Sampler) return;

    auto* sampler = audioEngine_->getSamplerProcessor(currentInstrument_);
    if (!sampler) return;

    samplerWaveformDisplay_->setAudioData(&sampler->getSampleBuffer(), sampler->getSampleRate());
}

void InstrumentScreen::paintSamplerUI(juce::Graphics& g) {
    static const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

    auto bounds = getLocalBounds();
    bounds.removeFromTop(30);  // Instrument tabs
    bounds.removeFromTop(26);  // Type selector
    bounds.removeFromTop(30);  // Header

    auto contentArea = bounds.reduced(16, 0);
    contentArea.removeFromTop(8);

    auto* inst = project_.getInstrument(currentInstrument_);
    if (!inst) return;

    const auto& params = inst->getSamplerParams();

    // Waveform takes top portion - handled by child component
    contentArea.removeFromTop(100);
    contentArea.removeFromTop(8);  // Gap

    // Sample info line
    g.setFont(14.0f);
    juce::String sampleName = params.sample.path.empty() ? "(no sample - press 'o' to load)"
        : juce::File(params.sample.path).getFileName();
    g.setColour(cursorColor);
    g.drawText("Sample: " + sampleName, contentArea.removeFromTop(24), juce::Justification::centredLeft);

    if (params.sample.path.empty()) {
        return;  // No more to show
    }

    // Draw parameter rows
    auto drawSamplerRow = [&](int rowIndex, const char* label, const juce::String& value, bool isToggle = false) {
        bool selected = (samplerCursorRow_ == rowIndex);
        auto rowArea = contentArea.removeFromTop(kRowHeight);

        if (selected) {
            g.setColour(highlightColor.withAlpha(0.3f));
            g.fillRect(rowArea);
        }

        // Shortcut key indicator
        char shortcutKey = getJumpKeyForRow(model::InstrumentType::Sampler, rowIndex);
        if (shortcutKey != '\0') {
            g.setColour(selected ? cursorColor : fgColor.darker(0.5f));
            g.setFont(10.0f);
            g.drawText(juce::String::charToString(shortcutKey), rowArea.getX(), rowArea.getY(), kShortcutWidth, kRowHeight, juce::Justification::centred);
        }

        g.setColour(selected ? cursorColor : fgColor);
        g.setFont(14.0f);
        g.drawText(label, rowArea.getX() + kShortcutWidth, rowArea.getY(), kLabelWidth, kRowHeight, juce::Justification::centredLeft);

        if (isToggle) {
            // Draw toggle button style
            auto toggleArea = juce::Rectangle<int>(rowArea.getX() + kShortcutWidth + kLabelWidth, rowArea.getY() + 4, 50, kRowHeight - 8);
            g.setColour(selected ? cursorColor : highlightColor);
            g.fillRoundedRectangle(toggleArea.toFloat(), 4.0f);
            g.setColour(selected ? juce::Colours::black : fgColor);
            g.drawText(value, toggleArea, juce::Justification::centred);
        } else {
            g.drawText(value, rowArea.getX() + kShortcutWidth + kLabelWidth, rowArea.getY(), 200, kRowHeight, juce::Justification::centredLeft);
        }
    };

    // Draw slider row (Plaits style)
    auto drawSamplerSliderRow = [&](int rowIndex, const char* label, float value, const juce::String& valueText) {
        bool selected = (samplerCursorRow_ == rowIndex);
        auto rowArea = contentArea.removeFromTop(kRowHeight);

        if (selected) {
            g.setColour(highlightColor.withAlpha(0.3f));
            g.fillRect(rowArea);
        }

        // Shortcut key indicator
        char shortcutKey = getJumpKeyForRow(model::InstrumentType::Sampler, rowIndex);
        if (shortcutKey != '\0') {
            g.setColour(selected ? cursorColor : fgColor.darker(0.5f));
            g.setFont(10.0f);
            g.drawText(juce::String::charToString(shortcutKey), rowArea.getX(), rowArea.getY(), kShortcutWidth, kRowHeight, juce::Justification::centred);
        }

        g.setColour(selected ? cursorColor : fgColor);
        g.setFont(14.0f);
        g.drawText(label, rowArea.getX() + kShortcutWidth, rowArea.getY(), kLabelWidth, kRowHeight, juce::Justification::centredLeft);

        // Slider bar (Plaits style)
        int barX = rowArea.getX() + kShortcutWidth + kLabelWidth;
        int barY = rowArea.getY() + (kRowHeight - 10) / 2;
        g.setColour(juce::Colour(0xff303030));
        g.fillRect(barX, barY, kBarWidth, 10);
        g.setColour(selected ? cursorColor : juce::Colour(0xff4a9090));
        g.fillRect(barX, barY, static_cast<int>(kBarWidth * value), 10);

        // Value text
        g.setColour(selected ? cursorColor : fgColor);
        g.drawText(valueText, barX + kBarWidth + 10, rowArea.getY(), 100, kRowHeight, juce::Justification::centredLeft);
    };

    // Helper for note name
    auto noteName = [&](int note) -> juce::String {
        if (note < 0) return "---";
        return juce::String(noteNames[note % 12]) + juce::String(note / 12 - 1);
    };

    // Row 0: Root Note (detected/override)
    juce::String rootNoteStr = noteName(params.rootNote);
    if (params.detectedMidiNote >= 0 && params.detectedMidiNote != params.rootNote) {
        rootNoteStr += " (detected: " + noteName(params.detectedMidiNote) + ")";
    }
    drawSamplerRow(0, "ROOT NOTE", rootNoteStr);

    // Row 1: Target Note (for repitch)
    drawSamplerRow(1, "TARGET", noteName(params.targetRootNote));

    // Row 2: Auto-Repitch toggle
    drawSamplerRow(2, "AUTO-REPITCH", params.autoRepitchEnabled ? "ON" : "OFF", true);

    // Row 3: Attack (normalize: 0-2s mapped to 0-1 for bar)
    float attackNorm = std::clamp(params.ampEnvelope.attack / 2.0f, 0.0f, 1.0f);
    juce::String attackStr = juce::String(static_cast<int>(params.ampEnvelope.attack * 1000.0f)) + "ms";
    drawSamplerSliderRow(3, "ATTACK", attackNorm, attackStr);

    // Row 4: Decay (normalize: 0-5s mapped to 0-1 for bar)
    float decayNorm = std::clamp(params.ampEnvelope.decay / 5.0f, 0.0f, 1.0f);
    juce::String decayStr = juce::String(static_cast<int>(params.ampEnvelope.decay * 1000.0f)) + "ms";
    drawSamplerSliderRow(4, "DECAY", decayNorm, decayStr);

    // Row 5: Sustain
    juce::String sustainStr = juce::String(static_cast<int>(params.ampEnvelope.sustain * 100.0f)) + "%";
    drawSamplerSliderRow(5, "SUSTAIN", params.ampEnvelope.sustain, sustainStr);

    // Row 6: Release (normalize: 0-5s mapped to 0-1 for bar)
    float releaseNorm = std::clamp(params.ampEnvelope.release / 5.0f, 0.0f, 1.0f);
    juce::String releaseStr = juce::String(static_cast<int>(params.ampEnvelope.release * 1000.0f)) + "ms";
    drawSamplerSliderRow(6, "RELEASE", releaseNorm, releaseStr);

    // Row 7: Cutoff
    float cutoffHz = 20.0f * std::pow(1000.0f, params.filter.cutoff);
    juce::String cutoffStr = juce::String(static_cast<int>(cutoffHz)) + "Hz";
    drawSamplerSliderRow(7, "CUTOFF", params.filter.cutoff, cutoffStr);

    // Row 8: Resonance
    juce::String resoStr = juce::String(static_cast<int>(params.filter.resonance * 100.0f)) + "%";
    drawSamplerSliderRow(8, "RESONANCE", params.filter.resonance, resoStr);

    // Modulation section header
    contentArea.removeFromTop(8);
    g.setColour(fgColor.darker(0.3f));
    g.drawText("-- MODULATION --", contentArea.removeFromTop(16), juce::Justification::centredLeft);
    contentArea.removeFromTop(4);

    // LFO/ENV rows - multi-field (rate, shape, dest, amount) - Plaits style
    auto drawSamplerModRow = [&](int rowIndex, const char* label, const model::SamplerLfoParams* lfo, const model::SamplerModEnvParams* env) {
        bool selected = (samplerCursorRow_ == rowIndex);
        auto rowArea = contentArea.removeFromTop(kRowHeight);

        if (selected) {
            g.setColour(highlightColor.withAlpha(0.3f));
            g.fillRect(rowArea);
        }

        g.setColour(selected ? cursorColor : fgColor);
        g.setFont(14.0f);
        g.drawText(label, rowArea.getX(), rowArea.getY(), kLabelWidth, kRowHeight, juce::Justification::centredLeft);

        // Draw 4 fields with gray boxes (Plaits style)
        int fieldX = rowArea.getX() + kLabelWidth;
        int fieldWidth = 50;
        int fieldGap = 8;

        for (int f = 0; f < 4; f++) {
            bool fieldSelected = selected && (samplerCursorField_ == f);
            juce::String fieldValue;

            if (lfo) {
                switch (f) {
                    case 0: fieldValue = getLfoRateName(lfo->rateIndex); break;
                    case 1: fieldValue = getLfoShapeName(lfo->shapeIndex); break;
                    case 2: fieldValue = model::getModDestName(static_cast<model::SamplerModDest>(lfo->destIndex)); break;
                    case 3: fieldValue = (lfo->amount >= 0 ? "+" : "") + juce::String(lfo->amount); break;
                }
            } else if (env) {
                switch (f) {
                    case 0: fieldValue = juce::String(static_cast<int>(env->attack * 1000)) + "ms"; break;
                    case 1: fieldValue = juce::String(static_cast<int>(env->decay * 1000)) + "ms"; break;
                    case 2: fieldValue = model::getModDestName(static_cast<model::SamplerModDest>(env->destIndex)); break;
                    case 3: fieldValue = (env->amount >= 0 ? "+" : "") + juce::String(env->amount); break;
                }
            }

            // Gray background box (Plaits style)
            g.setColour(fieldSelected ? cursorColor : juce::Colour(0xff606060));
            g.fillRect(fieldX, rowArea.getY() + 4, fieldWidth, kRowHeight - 8);
            g.setColour(fieldSelected ? juce::Colours::black : fgColor);
            g.setFont(14.0f);
            g.drawText(fieldValue, fieldX, rowArea.getY(), fieldWidth, kRowHeight, juce::Justification::centred);
            fieldX += fieldWidth + fieldGap;
        }
    };

    // Row 9: LFO1
    drawSamplerModRow(static_cast<int>(SamplerRowType::Lfo1), "LFO1", &params.modulation.lfo1, nullptr);
    // Row 10: LFO2
    drawSamplerModRow(static_cast<int>(SamplerRowType::Lfo2), "LFO2", &params.modulation.lfo2, nullptr);
    // Row 11: ENV1
    drawSamplerModRow(static_cast<int>(SamplerRowType::Env1), "ENV1", nullptr, &params.modulation.env1);
    // Row 12: ENV2
    drawSamplerModRow(static_cast<int>(SamplerRowType::Env2), "ENV2", nullptr, &params.modulation.env2);

    // Volume and Pan removed - now in Mixer screen

    // Pitch ratio display
    contentArea.removeFromTop(8);
    g.setColour(fgColor.darker(0.3f));
    g.setFont(12.0f);
    g.drawText("Pitch ratio: " + juce::String(params.pitchRatio, 4), contentArea.removeFromTop(20), juce::Justification::centredLeft);

    // Instructions
    contentArea.removeFromTop(8);
    g.setColour(fgColor.darker(0.5f));
    g.drawText("o: load   Enter: audition   +/-: zoom   Tab: change type", contentArea.removeFromTop(20), juce::Justification::centredLeft);
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
    static const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

    auto bounds = getLocalBounds();
    bounds.removeFromTop(30);  // Instrument tabs
    bounds.removeFromTop(26);  // Type selector
    bounds.removeFromTop(30);  // Header

    auto contentArea = bounds.reduced(16, 0);
    contentArea.removeFromTop(8);

    auto* inst = project_.getInstrument(currentInstrument_);
    if (!inst) return;

    const auto& params = inst->getSlicerParams();

    // Waveform takes top portion - handled by child component
    contentArea.removeFromTop(100);
    contentArea.removeFromTop(8);

    // Sample info line
    g.setFont(14.0f);
    juce::String sampleName = params.sample.path.empty() ? "(no sample - press 'o' to load)"
        : juce::File(params.sample.path).getFileName();
    g.setColour(cursorColor);
    g.drawText("Sample: " + sampleName, contentArea.removeFromTop(24), juce::Justification::centredLeft);

    // Slices info
    g.setColour(fgColor);
    juce::String sliceInfo = "Slices: " + juce::String(static_cast<int>(params.slicePoints.size()));
    sliceInfo += "  Current: " + juce::String(params.currentSlice + 1);
    g.drawText(sliceInfo, contentArea.removeFromTop(24), juce::Justification::centredLeft);

    if (params.sample.path.empty()) {
        return;
    }

    // Draw editable parameter rows
    auto drawSlicerRow = [&](int rowIndex, const char* label, const juce::String& value,
                             const juce::String& extra = "", bool greyed = false) {
        bool selected = (slicerCursorRow_ == rowIndex);
        auto rowArea = contentArea.removeFromTop(kRowHeight);

        if (selected) {
            g.setColour(highlightColor.withAlpha(0.3f));
            g.fillRect(rowArea);
        }

        // Shortcut key indicator
        char shortcutKey = getJumpKeyForRow(model::InstrumentType::Slicer, rowIndex);
        if (shortcutKey != '\0') {
            g.setColour(selected ? cursorColor : fgColor.darker(0.5f));
            g.setFont(10.0f);
            g.drawText(juce::String::charToString(shortcutKey), rowArea.getX(), rowArea.getY(), kShortcutWidth, kRowHeight, juce::Justification::centred);
        }

        g.setColour(greyed ? fgColor.darker(0.6f) : (selected ? cursorColor : fgColor));
        g.setFont(14.0f);
        g.drawText(label, rowArea.getX() + kShortcutWidth, rowArea.getY(), kLabelWidth, kRowHeight, juce::Justification::centredLeft);
        g.drawText(value, rowArea.getX() + kShortcutWidth + kLabelWidth, rowArea.getY(), 120, kRowHeight, juce::Justification::centredLeft);

        if (!extra.isEmpty()) {
            g.setColour(fgColor.darker(0.4f));
            g.drawText(extra, rowArea.getX() + kShortcutWidth + kLabelWidth + 120, rowArea.getY(), 200, kRowHeight, juce::Justification::centredLeft);
        }
    };

    // Draw slider row (Plaits style)
    auto drawSlicerSliderRow = [&](int rowIndex, const char* label, float value, const juce::String& valueText) {
        bool selected = (slicerCursorRow_ == rowIndex);
        auto rowArea = contentArea.removeFromTop(kRowHeight);

        if (selected) {
            g.setColour(highlightColor.withAlpha(0.3f));
            g.fillRect(rowArea);
        }

        // Shortcut key indicator
        char shortcutKey = getJumpKeyForRow(model::InstrumentType::Slicer, rowIndex);
        if (shortcutKey != '\0') {
            g.setColour(selected ? cursorColor : fgColor.darker(0.5f));
            g.setFont(10.0f);
            g.drawText(juce::String::charToString(shortcutKey), rowArea.getX(), rowArea.getY(), kShortcutWidth, kRowHeight, juce::Justification::centred);
        }

        g.setColour(selected ? cursorColor : fgColor);
        g.setFont(14.0f);
        g.drawText(label, rowArea.getX() + kShortcutWidth, rowArea.getY(), kLabelWidth, kRowHeight, juce::Justification::centredLeft);

        // Slider bar (Plaits style)
        int barX = rowArea.getX() + kShortcutWidth + kLabelWidth;
        int barY = rowArea.getY() + (kRowHeight - 10) / 2;
        g.setColour(juce::Colour(0xff303030));
        g.fillRect(barX, barY, kBarWidth, 10);
        g.setColour(selected ? cursorColor : juce::Colour(0xff4a9090));
        g.fillRect(barX, barY, static_cast<int>(kBarWidth * value), 10);

        // Value text
        g.setColour(selected ? cursorColor : fgColor);
        g.drawText(valueText, barX + kBarWidth + 10, rowArea.getY(), 100, kRowHeight, juce::Justification::centredLeft);
    };

    // Chop mode names
    auto getChopModeName = [](model::ChopMode mode) -> const char* {
        switch (mode) {
            case model::ChopMode::Lazy: return "Lazy";
            case model::ChopMode::Divisions: return "Divisions";
            case model::ChopMode::Transients: return "Transients";
        }
        return "Unknown";
    };

    // Row 0: Chop Mode with sub-control
    juce::String chopModeStr = getChopModeName(params.chopMode);
    if (params.chopMode == model::ChopMode::Divisions) {
        chopModeStr += " [" + juce::String(params.numDivisions) + "]";
    } else if (params.chopMode == model::ChopMode::Transients) {
        chopModeStr += " [" + juce::String(static_cast<int>(params.transientSensitivity * 100)) + "%]";
    }
    drawSlicerRow(static_cast<int>(SlicerRowType::ChopMode), "CHOP", chopModeStr);

    // Row 1: Original Bars
    int displayBars = params.getBars();
    juce::String barsStr = displayBars > 0 ? juce::String(displayBars) : "---";
    juce::String barsExtra = (params.bars > 0 && params.detectedBars > 0) ?
                             "(detected: " + juce::String(params.detectedBars) + ")" : "";
    drawSlicerRow(static_cast<int>(SlicerRowType::OriginalBars), "ORIG BARS", barsStr, barsExtra);

    // Row 2: Original BPM
    float displayBPM = params.getOriginalBPM();
    juce::String bpmStr = displayBPM > 0 ? juce::String(displayBPM, 1) : "---";
    juce::String bpmExtra = (params.originalBPM > 0 && params.detectedBPM > 0) ?
                            "(detected: " + juce::String(params.detectedBPM, 1) + ")" : "";
    drawSlicerRow(static_cast<int>(SlicerRowType::OriginalBPM), "ORIG BPM", bpmStr, bpmExtra);

    // Row 3: Target BPM
    drawSlicerRow(static_cast<int>(SlicerRowType::TargetBPM), "TARGET BPM",
                  juce::String(params.targetBPM, 1));

    // Row 4: Speed
    drawSlicerRow(static_cast<int>(SlicerRowType::Speed), "SPEED",
                  "x" + juce::String(params.speed, 3));

    // Row 5: Pitch (greyed when repitch=false)
    juce::String pitchStr = (params.pitchSemitones >= 0 ? "+" : "") +
                            juce::String(params.pitchSemitones) + " st";
    drawSlicerRow(static_cast<int>(SlicerRowType::Pitch), "PITCH", pitchStr,
                  params.repitch ? "" : "(repitch off)", !params.repitch);

    // Row 6: Repitch toggle
    drawSlicerRow(static_cast<int>(SlicerRowType::Repitch), "REPITCH",
                  params.repitch ? "ON" : "OFF");

    // Row 7: Polyphony
    juce::String polyStr = params.polyphony == 1 ? "1 (choke)" : juce::String(params.polyphony);
    drawSlicerRow(static_cast<int>(SlicerRowType::Polyphony), "POLYPHONY", polyStr);

    // Modulation section header
    contentArea.removeFromTop(8);
    g.setColour(fgColor.darker(0.3f));
    g.drawText("-- MODULATION --", contentArea.removeFromTop(16), juce::Justification::centredLeft);
    contentArea.removeFromTop(4);

    // LFO/ENV rows - multi-field (rate, shape, dest, amount) - Plaits style
    auto drawSlicerModRow = [&](int rowIndex, const char* label, const model::SamplerLfoParams* lfo, const model::SamplerModEnvParams* env) {
        bool selected = (slicerCursorRow_ == rowIndex);
        auto rowArea = contentArea.removeFromTop(kRowHeight);

        if (selected) {
            g.setColour(highlightColor.withAlpha(0.3f));
            g.fillRect(rowArea);
        }

        g.setColour(selected ? cursorColor : fgColor);
        g.setFont(14.0f);
        g.drawText(label, rowArea.getX(), rowArea.getY(), kLabelWidth, kRowHeight, juce::Justification::centredLeft);

        // Draw 4 fields with gray boxes (Plaits style)
        int fieldX = rowArea.getX() + kLabelWidth;
        int fieldWidth = 50;
        int fieldGap = 8;

        for (int f = 0; f < 4; f++) {
            bool fieldSelected = selected && (slicerCursorField_ == f);
            juce::String fieldValue;

            if (lfo) {
                switch (f) {
                    case 0: fieldValue = getLfoRateName(lfo->rateIndex); break;
                    case 1: fieldValue = getLfoShapeName(lfo->shapeIndex); break;
                    case 2: fieldValue = model::getModDestName(static_cast<model::SamplerModDest>(lfo->destIndex)); break;
                    case 3: fieldValue = (lfo->amount >= 0 ? "+" : "") + juce::String(lfo->amount); break;
                }
            } else if (env) {
                switch (f) {
                    case 0: fieldValue = juce::String(static_cast<int>(env->attack * 1000)) + "ms"; break;
                    case 1: fieldValue = juce::String(static_cast<int>(env->decay * 1000)) + "ms"; break;
                    case 2: fieldValue = model::getModDestName(static_cast<model::SamplerModDest>(env->destIndex)); break;
                    case 3: fieldValue = (env->amount >= 0 ? "+" : "") + juce::String(env->amount); break;
                }
            }

            // Gray background box (Plaits style)
            g.setColour(fieldSelected ? cursorColor : juce::Colour(0xff606060));
            g.fillRect(fieldX, rowArea.getY() + 4, fieldWidth, kRowHeight - 8);
            g.setColour(fieldSelected ? juce::Colours::black : fgColor);
            g.setFont(14.0f);
            g.drawText(fieldValue, fieldX, rowArea.getY(), fieldWidth, kRowHeight, juce::Justification::centred);
            fieldX += fieldWidth + fieldGap;
        }
    };

    drawSlicerModRow(static_cast<int>(SlicerRowType::Lfo1), "LFO1", &params.modulation.lfo1, nullptr);
    drawSlicerModRow(static_cast<int>(SlicerRowType::Lfo2), "LFO2", &params.modulation.lfo2, nullptr);
    drawSlicerModRow(static_cast<int>(SlicerRowType::Env1), "ENV1", nullptr, &params.modulation.env1);
    drawSlicerModRow(static_cast<int>(SlicerRowType::Env2), "ENV2", nullptr, &params.modulation.env2);

    // Output section header
    // Volume and Pan removed - now in Mixer screen

    // Check lazy chop status
    audio::SlicerInstrument* slicer = nullptr;
    if (audioEngine_) {
        slicer = audioEngine_->getSlicerProcessor(currentInstrument_);
    }

    // Lazy chop status indicator
    if (params.chopMode == model::ChopMode::Lazy && slicer) {
        contentArea.removeFromTop(8);
        if (slicer->isLazyChopPlaying()) {
            g.setColour(juce::Colours::green);
            g.setFont(14.0f);
            g.drawText(">>> PLAYING - press 'c' to add slice, Enter to stop <<<",
                       contentArea.removeFromTop(24), juce::Justification::centred);
        } else {
            g.setColour(fgColor.darker(0.3f));
            g.setFont(12.0f);
            g.drawText("Press Enter to start lazy chop preview",
                       contentArea.removeFromTop(20), juce::Justification::centredLeft);
        }
    }

    // Instructions - vary based on chop mode
    contentArea.removeFromTop(8);
    g.setColour(fgColor.darker(0.5f));
    g.setFont(12.0f);

    if (params.chopMode == model::ChopMode::Lazy) {
        g.drawText("o: load   Enter: play slice   c: chop at playhead   Shift+X: clear",
                   contentArea.removeFromTop(20), juce::Justification::centredLeft);
    } else if (params.chopMode == model::ChopMode::Divisions) {
        g.drawText("o: load   Enter: play slice   Alt+Up/Dn: divisions   h/l: slices",
                   contentArea.removeFromTop(20), juce::Justification::centredLeft);
    } else {
        g.drawText("o: load   Enter: play slice   Alt+Up/Dn: sensitivity   h/l: slices",
                   contentArea.removeFromTop(20), juce::Justification::centredLeft);
    }
    g.drawText("+/-: zoom   Shift+h/l: scroll   Tab: change type",
               contentArea.removeFromTop(20), juce::Justification::centredLeft);
}

bool InstrumentScreen::handleSlicerKey(const juce::KeyPress& key, bool /*isEditMode*/) {
    // Use centralized key translation
    auto action = input::KeyHandler::translateKey(key, getInputContext());

    auto* inst = project_.getInstrument(currentInstrument_);
    if (!inst) return false;

    auto& params = inst->getSlicerParams();

    // Handle actions from translateKey()
    switch (action.action)
    {
        case input::KeyAction::NewItem:
            // Shift+N creates new instrument
            currentInstrument_ = project_.addInstrument("Inst " + std::to_string(project_.getInstrumentCount() + 1));
            if (auto* newInst = project_.getInstrument(currentInstrument_)) {
                editingName_ = true;
                nameBuffer_ = newInst->getName();
            }
            repaint();
            return true;

        case input::KeyAction::PatternPrev:
            // '[' switch to previous instrument
            {
                int numInstruments = project_.getInstrumentCount();
                if (numInstruments > 0) {
                    currentInstrument_ = (currentInstrument_ - 1 + numInstruments) % numInstruments;
                    setCurrentInstrument(currentInstrument_);
                }
            }
            return true;

        case input::KeyAction::PatternNext:
            // ']' switch to next instrument
            {
                int numInstruments = project_.getInstrumentCount();
                if (numInstruments > 0) {
                    currentInstrument_ = (currentInstrument_ + 1) % numInstruments;
                    setCurrentInstrument(currentInstrument_);
                }
            }
            return true;

        case input::KeyAction::Jump:
            // Use engine-aware jump key mapping for Slicer
            // Note: 'c' is handled separately for chop functionality
            {
                int targetRow = getRowForJumpKey(model::InstrumentType::Slicer, action.charData);
                if (targetRow >= 0 && targetRow < kNumSlicerRows)
                {
                    slicerCursorRow_ = targetRow;
                    slicerCursorField_ = 0;
                    repaint();
                    return true;
                }
            }
            break;

        default:
            break;
    }

    // Get slicer processor for lazy chop operations
    audio::SlicerInstrument* slicer = nullptr;
    if (audioEngine_) {
        slicer = audioEngine_->getSlicerProcessor(currentInstrument_);
    }

    // Get textChar for slicer-specific keys not in the action system
    auto textChar = key.getTextCharacter();

    // Enter (Confirm action): always play the current slice
    if (action.action == input::KeyAction::Confirm) {
        // Play the current slice note (mapped to MIDI note)
        // Slices are mapped starting at C-1 (MIDI note 12)
        if (onNotePreview) {
            int sliceNote = params.currentSlice + 12;  // BASE_NOTE = 12
            onNotePreview(sliceNote, currentInstrument_);
        }
        repaint();
        return true;
    }

    // 'c' for chop: add slice at current playhead while a slice is playing
    // (slicer-specific, not an action)
    if (textChar == 'c' || textChar == 'C') {
        if (slicer && params.chopMode == model::ChopMode::Lazy) {
            int64_t playhead = slicer->getPlayheadPosition();
            if (playhead >= 0) {
                int newSliceIndex = slicer->addSliceAtPosition(static_cast<size_t>(playhead));
                if (newSliceIndex >= 0) {
                    // Select the slice that starts at the chop point (ahead of where we chopped)
                    params.currentSlice = newSliceIndex;
                }
                updateSlicerDisplay();
                repaint();
            }
        }
        return true;
    }

    // 'x' to clear all slices (with confirmation via double-press - just clear for now)
    // (slicer-specific, not an action)
    if (textChar == 'x' || textChar == 'X') {
        bool shiftHeld = key.getModifiers().isShiftDown();
        if (slicer && shiftHeld) {
            // Shift+X clears all slices
            slicer->clearSlices();
            updateSlicerDisplay();
            repaint();
        }
        return true;
    }

    // Check if current row is a modulation row (has multiple fields)
    auto isModRow = [](int row) {
        auto type = static_cast<SlicerRowType>(row);
        return type == SlicerRowType::Lfo1 || type == SlicerRowType::Lfo2 ||
               type == SlicerRowType::Env1 || type == SlicerRowType::Env2;
    };

    // Handle edit actions for value adjustment and navigation
    int delta = 0;
    bool isCoarse = false;
    switch (action.action)
    {
        case input::KeyAction::Edit1Inc:  // Alt+k/up - vertical increment
            delta = 1;
            break;
        case input::KeyAction::Edit1Dec:  // Alt+j/down - vertical decrement
            delta = -1;
            break;
        case input::KeyAction::ShiftEdit1Inc:  // Alt+Shift+k/up - coarse increment
            delta = 1;
            isCoarse = true;
            break;
        case input::KeyAction::ShiftEdit1Dec:  // Alt+Shift+j/down - coarse decrement
            delta = -1;
            isCoarse = true;
            break;
        case input::KeyAction::Edit2Inc:  // Alt+l/right - horizontal increment
        case input::KeyAction::ZoomIn:    // +/=
            delta = 1;
            break;
        case input::KeyAction::Edit2Dec:  // Alt+h/left - horizontal decrement
        case input::KeyAction::ZoomOut:   // -
            delta = -1;
            break;
        case input::KeyAction::ShiftEdit2Inc:  // Alt+Shift+l/right
            delta = 1;
            isCoarse = true;
            break;
        case input::KeyAction::ShiftEdit2Dec:  // Alt+Shift+h/left
            delta = -1;
            isCoarse = true;
            break;

        // Navigation actions
        case input::KeyAction::NavUp:
            slicerCursorRow_ = std::max(0, slicerCursorRow_ - 1);
            repaint();
            return true;
        case input::KeyAction::NavDown:
            slicerCursorRow_ = std::min(kNumSlicerRows - 1, slicerCursorRow_ + 1);
            repaint();
            return true;

        // Tab/field navigation on mod rows
        case input::KeyAction::TabNext:
        case input::KeyAction::NavRight:
            if (isModRow(slicerCursorRow_)) {
                slicerCursorField_ = (slicerCursorField_ + 1) % 4;
                repaint();
                return true;
            }
            break;
        case input::KeyAction::TabPrev:
        case input::KeyAction::NavLeft:
            if (isModRow(slicerCursorRow_)) {
                slicerCursorField_ = (slicerCursorField_ + 3) % 4;  // -1 with wrap
                repaint();
                return true;
            }
            break;

        default:
            break;
    }

    // Handle ChopMode sub-parameter adjustment with Edit1 actions
    if (delta != 0 && slicerCursorRow_ == static_cast<int>(SlicerRowType::ChopMode) &&
        (action.action == input::KeyAction::Edit1Inc || action.action == input::KeyAction::Edit1Dec ||
         action.action == input::KeyAction::ShiftEdit1Inc || action.action == input::KeyAction::ShiftEdit1Dec))
    {
        int step = isCoarse ? 1 : 4;  // Fine vs coarse adjustment
        if (params.chopMode == model::ChopMode::Divisions && slicer) {
            params.numDivisions = std::clamp(params.numDivisions + delta * step, 2, 64);
            slicer->chopIntoDivisions(params.numDivisions);
            updateSlicerDisplay();
            repaint();
            return true;
        } else if (params.chopMode == model::ChopMode::Transients && slicer) {
            float sensStep = isCoarse ? 0.02f : 0.1f;
            params.transientSensitivity = std::clamp(params.transientSensitivity + delta * sensStep, 0.0f, 1.0f);
            slicer->chopByTransients(params.transientSensitivity);
            updateSlicerDisplay();
            repaint();
            return true;
        }
    }

    // Handle value adjustment with Edit actions (delta already set by action switch above)
    if (delta != 0) {
        // Handle modulation rows specially (multi-field)
        if (isModRow(slicerCursorRow_))
        {
            auto& mod = params.modulation;
            auto rowType = static_cast<SlicerRowType>(slicerCursorRow_);

            model::SamplerLfoParams* lfo = nullptr;
            model::SamplerModEnvParams* env = nullptr;

            if (rowType == SlicerRowType::Lfo1) lfo = &mod.lfo1;
            else if (rowType == SlicerRowType::Lfo2) lfo = &mod.lfo2;
            else if (rowType == SlicerRowType::Env1) env = &mod.env1;
            else if (rowType == SlicerRowType::Env2) env = &mod.env2;

            if (lfo) {
                switch (slicerCursorField_) {
                    case 0: lfo->rateIndex = std::clamp(lfo->rateIndex + delta, 0, 15); break;
                    case 1: lfo->shapeIndex = std::clamp(lfo->shapeIndex + delta, 0, 4); break;
                    case 2: lfo->destIndex = std::clamp(lfo->destIndex + delta, 0, static_cast<int>(model::SamplerModDest::NumDestinations) - 1); break;
                    case 3: lfo->amount = static_cast<int8_t>(std::clamp(static_cast<int>(lfo->amount) + delta, -64, 63)); break;
                }
            } else if (env) {
                float envStep = isCoarse ? 0.01f : 0.05f;
                switch (slicerCursorField_) {
                    case 0: env->attack = std::clamp(env->attack + delta * envStep, 0.001f, 2.0f); break;
                    case 1: env->decay = std::clamp(env->decay + delta * envStep, 0.001f, 10.0f); break;
                    case 2: env->destIndex = std::clamp(env->destIndex + delta, 0, static_cast<int>(model::SamplerModDest::NumDestinations) - 1); break;
                    case 3: env->amount = static_cast<int8_t>(std::clamp(static_cast<int>(env->amount) + delta, -64, 63)); break;
                }
            }
            if (onNotePreview) onNotePreview(60, currentInstrument_);
            repaint();
            return true;
        }

        float step = isCoarse ? 0.01f : 0.05f;
        float bpmStep = isCoarse ? 0.5f : 5.0f;
        float speedStep = isCoarse ? 0.01f : 0.05f;
        int pitchStep = isCoarse ? 1 : 12;

        switch (static_cast<SlicerRowType>(slicerCursorRow_)) {
            case SlicerRowType::ChopMode: {
                if (isCoarse) {
                    // Coarse (Shift) adjusts sub-parameter (divisions or sensitivity)
                    if (params.chopMode == model::ChopMode::Divisions && slicer) {
                        params.numDivisions = std::clamp(params.numDivisions + delta, 2, 64);
                        slicer->chopIntoDivisions(params.numDivisions);
                        updateSlicerDisplay();
                    } else if (params.chopMode == model::ChopMode::Transients && slicer) {
                        params.transientSensitivity = std::clamp(params.transientSensitivity + delta * 0.05f, 0.0f, 1.0f);
                        slicer->chopByTransients(params.transientSensitivity);
                        updateSlicerDisplay();
                    }
                } else {
                    // Normal edit cycles through chop modes
                    int modeVal = static_cast<int>(params.chopMode);
                    modeVal = (modeVal + delta + 3) % 3;  // 3 modes
                    model::ChopMode newMode = static_cast<model::ChopMode>(modeVal);
                    params.chopMode = newMode;

                    // Trigger appropriate chop action when mode changes
                    if (slicer) {
                        switch (newMode) {
                            case model::ChopMode::Divisions:
                                slicer->chopIntoDivisions(params.numDivisions);
                                break;
                            case model::ChopMode::Transients:
                                slicer->chopByTransients(params.transientSensitivity);
                                break;
                            case model::ChopMode::Lazy:
                                // Don't auto-chop for Lazy mode - user adds slices manually
                                break;
                        }
                        updateSlicerDisplay();
                    }
                }
                break;
            }
            case SlicerRowType::OriginalBars:
                if (slicer) {
                    int newBars = params.getBars() + delta;
                    slicer->editOriginalBars(std::max(1, newBars));
                } else {
                    if (params.bars <= 0) params.bars = params.detectedBars;
                    params.bars = std::max(1, params.bars + delta);
                }
                break;
            case SlicerRowType::OriginalBPM:
                if (slicer) {
                    float newBPM = params.getOriginalBPM() + delta * bpmStep;
                    slicer->editOriginalBPM(std::clamp(newBPM, 40.0f, 250.0f));
                } else {
                    if (params.originalBPM <= 0) params.originalBPM = params.detectedBPM;
                    params.originalBPM = std::clamp(params.originalBPM + delta * bpmStep, 40.0f, 250.0f);
                }
                break;
            case SlicerRowType::TargetBPM:
                if (slicer) {
                    float newBPM = params.targetBPM + delta * bpmStep;
                    slicer->editTargetBPM(std::clamp(newBPM, 40.0f, 250.0f));
                } else {
                    params.targetBPM = std::clamp(params.targetBPM + delta * bpmStep, 40.0f, 250.0f);
                }
                break;
            case SlicerRowType::Speed:
                if (slicer) {
                    float newSpeed = params.speed + delta * speedStep;
                    slicer->editSpeed(std::clamp(newSpeed, 0.1f, 4.0f));
                } else {
                    params.speed = std::clamp(params.speed + delta * speedStep, 0.1f, 4.0f);
                }
                break;
            case SlicerRowType::Pitch:
                if (params.repitch) {
                    if (slicer) {
                        slicer->editPitch(params.pitchSemitones + delta * pitchStep);
                    } else {
                        params.pitchSemitones += delta * pitchStep;
                    }
                }
                break;
            case SlicerRowType::Repitch:
                if (slicer) {
                    slicer->setRepitch(!params.repitch);
                } else {
                    params.repitch = !params.repitch;
                }
                break;
            case SlicerRowType::Polyphony:
                params.polyphony = std::clamp(params.polyphony + delta, 1, 8);
                break;
            default:
                break;
        }
        repaint();
        return true;
    }

    // Load sample (Open action)
    if (action.action == input::KeyAction::Open) {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Load Sample",
            juce::File::getSpecialLocation(juce::File::userHomeDirectory),
            "*.wav;*.aiff;*.aif;*.mp3;*.flac;*.ogg"
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

    // Delete current slice (Delete action)
    if (action.action == input::KeyAction::Delete) {
        if (sliceWaveformDisplay_) {
            sliceWaveformDisplay_->deleteCurrentSlice();
            repaint();
        }
        return true;
    }

    // Navigate slices: SecondaryPrev/Next (','/'.') navigate slices
    // Also handle h/l for slice navigation (slicer-specific, without Alt modifier)
    if (action.action == input::KeyAction::SecondaryPrev) {
        if (sliceWaveformDisplay_) {
            sliceWaveformDisplay_->previousSlice();
            params.currentSlice = sliceWaveformDisplay_->getCurrentSlice();
            repaint();
        }
        return true;
    }
    if (action.action == input::KeyAction::SecondaryNext) {
        if (sliceWaveformDisplay_) {
            sliceWaveformDisplay_->nextSlice();
            params.currentSlice = sliceWaveformDisplay_->getCurrentSlice();
            repaint();
        }
        return true;
    }

    // Slicer-specific slice navigation with h/l (different from vim editing)
    // h/l without modifiers navigate slices; Shift+h/l scrolls the waveform
    bool shiftHeld = key.getModifiers().isShiftDown();
    if ((textChar == 'h' || textChar == 'H') && !shiftHeld && !key.getModifiers().isAltDown()) {
        if (sliceWaveformDisplay_) {
            sliceWaveformDisplay_->previousSlice();
            params.currentSlice = sliceWaveformDisplay_->getCurrentSlice();
            repaint();
        }
        return true;
    }

    if ((textChar == 'l' || textChar == 'L') && !shiftHeld && !key.getModifiers().isAltDown()) {
        if (sliceWaveformDisplay_) {
            sliceWaveformDisplay_->nextSlice();
            params.currentSlice = sliceWaveformDisplay_->getCurrentSlice();
            repaint();
        }
        return true;
    }

    // Zoom with ZoomIn/ZoomOut already handled via delta, but also zoom the waveform display
    if (action.action == input::KeyAction::ZoomIn) {
        if (sliceWaveformDisplay_) sliceWaveformDisplay_->zoomIn();
        return true;
    }
    if (action.action == input::KeyAction::ZoomOut) {
        if (sliceWaveformDisplay_) sliceWaveformDisplay_->zoomOut();
        return true;
    }

    // Scroll waveform: Shift+h/l
    if (shiftHeld && !key.getModifiers().isAltDown()) {
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

void InstrumentScreen::cycleInstrumentType(bool reverse) {
    auto* instrument = project_.getInstrument(currentInstrument_);
    if (!instrument) return;

    auto currentType = instrument->getType();
    model::InstrumentType newType;

    if (!reverse) {
        // Forward: Plaits -> Sampler -> Slicer -> VASynth -> DXPreset -> Plaits
        switch (currentType) {
            case model::InstrumentType::Plaits:
                newType = model::InstrumentType::Sampler;
                break;
            case model::InstrumentType::Sampler:
                newType = model::InstrumentType::Slicer;
                break;
            case model::InstrumentType::Slicer:
                newType = model::InstrumentType::VASynth;
                break;
            case model::InstrumentType::VASynth:
                newType = model::InstrumentType::DXPreset;
                break;
            case model::InstrumentType::DXPreset:
                newType = model::InstrumentType::Plaits;
                break;
            default:
                newType = model::InstrumentType::Plaits;
                break;
        }
    } else {
        // Reverse: Plaits -> DXPreset -> VASynth -> Slicer -> Sampler -> Plaits
        switch (currentType) {
            case model::InstrumentType::Plaits:
                newType = model::InstrumentType::DXPreset;
                break;
            case model::InstrumentType::DXPreset:
                newType = model::InstrumentType::VASynth;
                break;
            case model::InstrumentType::VASynth:
                newType = model::InstrumentType::Slicer;
                break;
            case model::InstrumentType::Slicer:
                newType = model::InstrumentType::Sampler;
                break;
            case model::InstrumentType::Sampler:
                newType = model::InstrumentType::Plaits;
                break;
            default:
                newType = model::InstrumentType::Plaits;
                break;
        }
    }

    instrument->setType(newType);

    // Update visibility of waveform displays
    if (sliceWaveformDisplay_) {
        sliceWaveformDisplay_->setVisible(newType == model::InstrumentType::Slicer);
        if (newType == model::InstrumentType::Slicer) {
            updateSlicerDisplay();
        }
    }
    if (samplerWaveformDisplay_) {
        samplerWaveformDisplay_->setVisible(newType == model::InstrumentType::Sampler);
        if (newType == model::InstrumentType::Sampler) {
            updateSamplerDisplay();
        }
    }

    // Initialize DX7 processor with first preset when switching to DXPreset type
    if (newType == model::InstrumentType::DXPreset && audioEngine_) {
        auto& dxParams = instrument->getDXParams();
        if (dxPresetBank_.getPresetCount() > 0 && dxParams.presetIndex < 0) {
            dxParams.presetIndex = 0;  // Start with first preset
        }
        // Load the preset into the DX7 processor
        auto* dx7 = audioEngine_->getDX7Processor(currentInstrument_);
        if (dx7 && dxParams.presetIndex >= 0) {
            const auto* preset = dxPresetBank_.getPreset(dxParams.presetIndex);
            if (preset) {
                // Copy preset data into DXParams for persistence
                std::copy(preset->packedData.begin(), preset->packedData.end(),
                         dxParams.packedPatch.begin());
                dx7->loadPackedPatch(preset->packedData.data());
                dx7->setPolyphony(dxParams.polyphony);
            }
        }
    }

    repaint();
}

void InstrumentScreen::setCurrentInstrument(int index) {
    currentInstrument_ = index;

    auto* inst = project_.getInstrument(index);
    if (inst) {
        auto type = inst->getType();

        // Update slicer waveform visibility
        if (sliceWaveformDisplay_) {
            sliceWaveformDisplay_->setVisible(type == model::InstrumentType::Slicer);
            if (type == model::InstrumentType::Slicer) {
                updateSlicerDisplay();
            }
        }

        // Update sampler waveform visibility
        if (samplerWaveformDisplay_) {
            samplerWaveformDisplay_->setVisible(type == model::InstrumentType::Sampler);
            if (type == model::InstrumentType::Sampler) {
                updateSamplerDisplay();
            }
        }

        // Sync DX7 processor with preset when switching to a DXPreset instrument
        if (type == model::InstrumentType::DXPreset && audioEngine_) {
            auto& dxParams = inst->getDXParams();
            // Ensure a preset is selected
            if (dxPresetBank_.getPresetCount() > 0 && dxParams.presetIndex < 0) {
                dxParams.presetIndex = 0;
            }
            // Load the preset into the DX7 processor
            auto* dx7 = audioEngine_->getDX7Processor(index);
            if (dx7 && dxParams.presetIndex >= 0) {
                const auto* preset = dxPresetBank_.getPreset(dxParams.presetIndex);
                if (preset) {
                    // Copy preset data into DXParams for persistence
                    std::copy(preset->packedData.begin(), preset->packedData.end(),
                             dxParams.packedPatch.begin());
                    dx7->loadPackedPatch(preset->packedData.data());
                    dx7->setPolyphony(dxParams.polyphony);
                }
            }
        }
    }
    repaint();
}

// ============================================================================
// VA Synth UI
// ============================================================================

void InstrumentScreen::paintVASynthUI(juce::Graphics& g) {
    auto area = getLocalBounds();
    area.removeFromTop(30);  // Instrument tabs
    area.removeFromTop(26);  // Type selector

    auto* inst = project_.getInstrument(currentInstrument_);
    if (!inst || inst->getType() != model::InstrumentType::VASynth) return;

    const auto& params = inst->getVAParams();
    const auto& modParams = params.modulation;

    // Header bar
    auto headerArea = area.removeFromTop(30);
    g.setColour(headerColor);
    g.fillRect(headerArea);

    g.setColour(fgColor);
    g.setFont(18.0f);
    juce::String title = "VA SYNTH: ";
    if (editingName_)
        title += juce::String(nameBuffer_) + "_";
    else
        title += juce::String(inst->getName());
    g.drawText(title, headerArea.reduced(10, 0), juce::Justification::centredLeft, true);

    area = area.reduced(5, 2);

    // Split into two columns
    int colWidth = area.getWidth() / 2;
    auto leftCol = area.removeFromLeft(colWidth);
    auto rightCol = area;

    // Helper: determine which column a row belongs to
    // Left column: rows 0-18 (OSC1/2/3, Noise, Filter, AmpEnv)
    // Right column: rows 19-35 (FilterEnv, Glide, Poly, Mods, FX, Output)
    auto isLeftColumn = [](int row) {
        return row <= static_cast<int>(VASynthRowType::AmpRelease);
    };

    // Waveform names for display
    const char* waveformNames[] = {"SAW", "SQR", "TRI", "PLS", "SIN"};
    constexpr int kRowH = 24;  // Match Plaits row height
    constexpr int kVABarWidth = 100;  // Narrower bars for two-column layout

    // Lambda to draw a simple row (no slider bar)
    auto drawRow = [&](juce::Rectangle<int>& col, int rowIndex, const char* label, const juce::String& value) {
        auto rowArea = col.removeFromTop(kRowH);
        bool isSelected = (rowIndex == vaSynthCursorRow_);
        bool inActiveCol = (vaSynthCursorCol_ == 0 && isLeftColumn(rowIndex)) ||
                           (vaSynthCursorCol_ == 1 && !isLeftColumn(rowIndex));

        if (isSelected && inActiveCol) {
            g.setColour(highlightColor.withAlpha(0.3f));
            g.fillRect(rowArea);
        }

        // Draw shortcut key hint
        char shortcutKey = getJumpKeyForRow(model::InstrumentType::VASynth, rowIndex);
        if (shortcutKey != '\0')
        {
            g.setColour(fgColor.withAlpha(0.4f));
            g.setFont(11.0f);
            g.drawText(juce::String::charToString(shortcutKey), rowArea.getX(), rowArea.getY(), kShortcutWidth, kRowH, juce::Justification::centred);
            rowArea.removeFromLeft(kShortcutWidth);
        }

        g.setColour((isSelected && inActiveCol) ? cursorColor : fgColor);
        g.setFont(14.0f);
        g.drawText(label, rowArea.removeFromLeft(70), juce::Justification::centredLeft);

        g.setColour((isSelected && inActiveCol) ? cursorColor : fgColor);
        g.setFont(14.0f);
        g.drawText(value, rowArea, juce::Justification::centredLeft);
    };

    // Lambda to draw a slider row (with bar like Plaits)
    auto drawSliderRow = [&](juce::Rectangle<int>& col, int rowIndex, const char* label, float value, const juce::String& valueText) {
        auto rowArea = col.removeFromTop(kRowH);
        bool isSelected = (rowIndex == vaSynthCursorRow_);
        bool inActiveCol = (vaSynthCursorCol_ == 0 && isLeftColumn(rowIndex)) ||
                           (vaSynthCursorCol_ == 1 && !isLeftColumn(rowIndex));

        if (isSelected && inActiveCol) {
            g.setColour(highlightColor.withAlpha(0.3f));
            g.fillRect(rowArea);
        }

        // Draw shortcut key hint
        char shortcutKey = getJumpKeyForRow(model::InstrumentType::VASynth, rowIndex);
        if (shortcutKey != '\0')
        {
            g.setColour(fgColor.withAlpha(0.4f));
            g.setFont(11.0f);
            g.drawText(juce::String::charToString(shortcutKey), rowArea.getX(), rowArea.getY(), kShortcutWidth, kRowH, juce::Justification::centred);
            rowArea.removeFromLeft(kShortcutWidth);
        }

        g.setColour((isSelected && inActiveCol) ? cursorColor : fgColor);
        g.setFont(14.0f);
        g.drawText(label, rowArea.removeFromLeft(70), juce::Justification::centredLeft);

        // Slider bar (Plaits style)
        int barX = rowArea.getX();
        int barY = rowArea.getY() + (kRowH - 10) / 2;
        g.setColour(juce::Colour(0xff303030));
        g.fillRect(barX, barY, kVABarWidth, 10);
        g.setColour((isSelected && inActiveCol) ? cursorColor : juce::Colour(0xff4a9090));
        g.fillRect(barX, barY, static_cast<int>(kVABarWidth * value), 10);

        // Value text
        g.setColour((isSelected && inActiveCol) ? cursorColor : fgColor);
        g.setFont(14.0f);
        g.drawText(valueText, barX + kVABarWidth + 8, rowArea.getY(), 60, kRowH, juce::Justification::centredLeft);
    };

    // Lambda for section headers
    auto drawHeader = [&](juce::Rectangle<int>& col, const char* text) {
        col.removeFromTop(4);
        g.setColour(fgColor.darker(0.5f));
        g.setFont(12.0f);
        g.drawText(text, col.removeFromTop(16), juce::Justification::centredLeft);
    };

    // Lambda for mod rows (4 fields) - Plaits style with gray boxes
    auto drawModRow = [&](juce::Rectangle<int>& col, int rowIndex, const char* label, bool isEnv,
                          int rateOrAttack, int shapeOrDecay, int dest, int amount) {
        auto rowArea = col.removeFromTop(kRowH);
        bool isSelected = (rowIndex == vaSynthCursorRow_);
        bool inActiveCol = (vaSynthCursorCol_ == 1);  // Mod rows are in right column

        if (isSelected && inActiveCol) {
            g.setColour(highlightColor.withAlpha(0.3f));
            g.fillRect(rowArea);
        }

        g.setColour((isSelected && inActiveCol) ? cursorColor : fgColor);
        g.setFont(14.0f);
        g.drawText(label, rowArea.removeFromLeft(40), juce::Justification::centredLeft);

        int fw = 50;
        int fieldGap = 4;
        for (int f = 0; f < 4; ++f) {
            bool fieldSel = isSelected && inActiveCol && (vaSynthCursorField_ == f);
            juce::String str;
            if (f == 0) str = isEnv ? (juce::String(rateOrAttack) + "%") : getLfoRateName(rateOrAttack);
            else if (f == 1) str = isEnv ? (juce::String(shapeOrDecay) + "%") : getLfoShapeName(shapeOrDecay);
            else if (f == 2) str = model::getModDestName(static_cast<model::SamplerModDest>(dest));
            else str = (amount >= 0 ? "+" : "") + juce::String(amount);

            // Gray background box (Plaits style)
            g.setColour(fieldSel ? cursorColor : juce::Colour(0xff606060));
            g.fillRect(rowArea.getX(), rowArea.getY() + 4, fw, kRowH - 8);
            g.setColour(fieldSel ? juce::Colours::black : fgColor);
            g.setFont(14.0f);
            g.drawText(str, rowArea.getX(), rowArea.getY(), fw, kRowH, juce::Justification::centred);
            rowArea.removeFromLeft(fw + fieldGap);
        }
    };

    // ===== LEFT COLUMN =====
    drawHeader(leftCol, "-- OSC 1 --");
    drawRow(leftCol, static_cast<int>(VASynthRowType::Osc1Waveform), "WAVE", waveformNames[std::clamp(params.osc1.waveform, 0, 4)]);
    drawRow(leftCol, static_cast<int>(VASynthRowType::Osc1Octave), "OCT", juce::String(params.osc1.octave > 0 ? "+" : "") + juce::String(params.osc1.octave));
    drawSliderRow(leftCol, static_cast<int>(VASynthRowType::Osc1Level), "LEVEL", params.osc1.level, juce::String(static_cast<int>(params.osc1.level * 100)) + "%");

    drawHeader(leftCol, "-- OSC 2 --");
    drawRow(leftCol, static_cast<int>(VASynthRowType::Osc2Waveform), "WAVE", waveformNames[std::clamp(params.osc2.waveform, 0, 4)]);
    drawRow(leftCol, static_cast<int>(VASynthRowType::Osc2Octave), "OCT", juce::String(params.osc2.octave > 0 ? "+" : "") + juce::String(params.osc2.octave));
    drawRow(leftCol, static_cast<int>(VASynthRowType::Osc2Detune), "DETUNE", juce::String(static_cast<int>(params.osc2.cents)) + "c");
    drawSliderRow(leftCol, static_cast<int>(VASynthRowType::Osc2Level), "LEVEL", params.osc2.level, juce::String(static_cast<int>(params.osc2.level * 100)) + "%");

    drawHeader(leftCol, "-- OSC 3 --");
    drawRow(leftCol, static_cast<int>(VASynthRowType::Osc3Waveform), "WAVE", waveformNames[std::clamp(params.osc3.waveform, 0, 4)]);
    drawRow(leftCol, static_cast<int>(VASynthRowType::Osc3Octave), "OCT", juce::String(params.osc3.octave > 0 ? "+" : "") + juce::String(params.osc3.octave));
    drawRow(leftCol, static_cast<int>(VASynthRowType::Osc3Detune), "DETUNE", juce::String(static_cast<int>(params.osc3.cents)) + "c");
    drawSliderRow(leftCol, static_cast<int>(VASynthRowType::Osc3Level), "LEVEL", params.osc3.level, juce::String(static_cast<int>(params.osc3.level * 100)) + "%");

    drawHeader(leftCol, "-- NOISE --");
    drawSliderRow(leftCol, static_cast<int>(VASynthRowType::NoiseLevel), "LEVEL", params.noiseLevel, juce::String(static_cast<int>(params.noiseLevel * 100)) + "%");

    drawHeader(leftCol, "-- FILTER --");
    drawSliderRow(leftCol, static_cast<int>(VASynthRowType::FilterCutoff), "CUTOFF", params.filter.cutoff, juce::String(static_cast<int>(params.filter.cutoff * 100)) + "%");
    drawSliderRow(leftCol, static_cast<int>(VASynthRowType::FilterResonance), "RESON", params.filter.resonance, juce::String(static_cast<int>(params.filter.resonance * 100)) + "%");
    drawSliderRow(leftCol, static_cast<int>(VASynthRowType::FilterEnvAmount), "ENV AMT", params.filter.envAmount, juce::String(static_cast<int>(params.filter.envAmount * 100)) + "%");

    drawHeader(leftCol, "-- AMP ENV --");
    drawSliderRow(leftCol, static_cast<int>(VASynthRowType::AmpAttack), "ATTACK", params.ampEnv.attack, juce::String(static_cast<int>(params.ampEnv.attack * 100)) + "%");
    drawSliderRow(leftCol, static_cast<int>(VASynthRowType::AmpDecay), "DECAY", params.ampEnv.decay, juce::String(static_cast<int>(params.ampEnv.decay * 100)) + "%");
    drawSliderRow(leftCol, static_cast<int>(VASynthRowType::AmpSustain), "SUSTAIN", params.ampEnv.sustain, juce::String(static_cast<int>(params.ampEnv.sustain * 100)) + "%");
    drawSliderRow(leftCol, static_cast<int>(VASynthRowType::AmpRelease), "RELEASE", params.ampEnv.release, juce::String(static_cast<int>(params.ampEnv.release * 100)) + "%");

    // ===== RIGHT COLUMN =====
    drawHeader(rightCol, "-- FILTER ENV --");
    drawSliderRow(rightCol, static_cast<int>(VASynthRowType::FilterAttack), "ATTACK", params.filterEnv.attack, juce::String(static_cast<int>(params.filterEnv.attack * 100)) + "%");
    drawSliderRow(rightCol, static_cast<int>(VASynthRowType::FilterDecay), "DECAY", params.filterEnv.decay, juce::String(static_cast<int>(params.filterEnv.decay * 100)) + "%");
    drawSliderRow(rightCol, static_cast<int>(VASynthRowType::FilterSustain), "SUSTAIN", params.filterEnv.sustain, juce::String(static_cast<int>(params.filterEnv.sustain * 100)) + "%");
    drawSliderRow(rightCol, static_cast<int>(VASynthRowType::FilterRelease), "RELEASE", params.filterEnv.release, juce::String(static_cast<int>(params.filterEnv.release * 100)) + "%");

    drawHeader(rightCol, "-- VOICE --");
    drawSliderRow(rightCol, static_cast<int>(VASynthRowType::Glide), "GLIDE", params.glide, juce::String(static_cast<int>(params.glide * 100)) + "%");
    // Voice-per-track architecture: polyphony removed, one voice per track
    // juce::String polyStr = params.monoMode ? "MONO" : juce::String(params.polyphony);
    // drawRow(rightCol, static_cast<int>(VASynthRowType::Polyphony), "VOICES", polyStr);

    drawHeader(rightCol, "-- MODULATION --");
    drawModRow(rightCol, static_cast<int>(VASynthRowType::Lfo1), "LFO1", false,
               modParams.lfo1.rateIndex, modParams.lfo1.shapeIndex, modParams.lfo1.destIndex, modParams.lfo1.amount);
    drawModRow(rightCol, static_cast<int>(VASynthRowType::Lfo2), "LFO2", false,
               modParams.lfo2.rateIndex, modParams.lfo2.shapeIndex, modParams.lfo2.destIndex, modParams.lfo2.amount);
    drawModRow(rightCol, static_cast<int>(VASynthRowType::Env1), "ENV1", true,
               static_cast<int>(modParams.env1.attack * 100), static_cast<int>(modParams.env1.decay * 100),
               modParams.env1.destIndex, modParams.env1.amount);
    drawModRow(rightCol, static_cast<int>(VASynthRowType::Env2), "ENV2", true,
               static_cast<int>(modParams.env2.attack * 100), static_cast<int>(modParams.env2.decay * 100),
               modParams.env2.destIndex, modParams.env2.amount);

    // Volume and Pan removed - now in Mixer screen
}

bool InstrumentScreen::handleVASynthKey(const juce::KeyPress& key, bool /*isEditMode*/) {
    // Use centralized key translation
    auto action = input::KeyHandler::translateKey(key, getInputContext());

    auto* instrument = project_.getInstrument(currentInstrument_);
    if (!instrument || instrument->getType() != model::InstrumentType::VASynth) return false;

    // Handle actions from translateKey()
    switch (action.action)
    {
        case input::KeyAction::PatternPrev:
            // '[' switch to previous instrument
            {
                int numInstruments = project_.getInstrumentCount();
                if (numInstruments > 0)
                    currentInstrument_ = (currentInstrument_ - 1 + numInstruments) % numInstruments;
            }
            repaint();
            return true;

        case input::KeyAction::PatternNext:
            // ']' switch to next instrument
            {
                int numInstruments = project_.getInstrumentCount();
                if (numInstruments > 0)
                    currentInstrument_ = (currentInstrument_ + 1) % numInstruments;
            }
            repaint();
            return true;

        case input::KeyAction::Jump:
            // Use engine-aware jump key mapping for VA Synth
            {
                int targetRow = getRowForJumpKey(model::InstrumentType::VASynth, action.charData);
                if (targetRow >= 0 && targetRow < kNumVASynthRows)
                {
                    vaSynthCursorRow_ = targetRow;
                    vaSynthCursorField_ = 0;
                    repaint();
                    return true;
                }
            }
            break;

        default:
            break;
    }

    auto& params = instrument->getVAParams();

    auto wrapWaveform = [](int current, int d) {
        int next = current + d;
        if (next < 0) next = 4;
        if (next > 4) next = 0;
        return next;
    };

    auto clampFloat = [](float& value, float d) {
        value = std::clamp(value + d, 0.0f, 1.0f);
    };

    // Check if current row is a modulation row (has multiple fields)
    auto isModRow = [](int row) {
        auto type = static_cast<VASynthRowType>(row);
        return type == VASynthRowType::Lfo1 || type == VASynthRowType::Lfo2 ||
               type == VASynthRowType::Env1 || type == VASynthRowType::Env2;
    };

    // Handle edit actions for value adjustment and navigation
    int valueDelta = 0;
    bool isCoarse = false;
    switch (action.action)
    {
        case input::KeyAction::Edit1Inc:  // Alt+k/up - vertical increment
        case input::KeyAction::Edit2Inc:  // Alt+l/right - horizontal increment
        case input::KeyAction::ZoomIn:    // +/=
            valueDelta = 1;
            break;
        case input::KeyAction::Edit1Dec:  // Alt+j/down - vertical decrement
        case input::KeyAction::Edit2Dec:  // Alt+h/left - horizontal decrement
        case input::KeyAction::ZoomOut:   // -
            valueDelta = -1;
            break;
        case input::KeyAction::ShiftEdit1Inc:  // Alt+Shift+k/up - coarse increment
        case input::KeyAction::ShiftEdit2Inc:  // Alt+Shift+l/right
            valueDelta = 1;
            isCoarse = true;
            break;
        case input::KeyAction::ShiftEdit1Dec:  // Alt+Shift+j/down - coarse decrement
        case input::KeyAction::ShiftEdit2Dec:  // Alt+Shift+h/left
            valueDelta = -1;
            isCoarse = true;
            break;

        // Navigation actions
        case input::KeyAction::NavUp:
            if (vaSynthCursorCol_ == 0) {
                // Left column: 0 to AmpRelease (18)
                vaSynthCursorRow_ = std::max(0, vaSynthCursorRow_ - 1);
            } else {
                // Right column: FilterAttack (19) to Pan
                int minRow = static_cast<int>(VASynthRowType::FilterAttack);
                vaSynthCursorRow_ = std::max(minRow, vaSynthCursorRow_ - 1);
            }
            vaSynthCursorField_ = 0;
            repaint();
            return true;

        case input::KeyAction::NavDown:
            if (vaSynthCursorCol_ == 0) {
                // Left column: max is AmpRelease (18)
                int maxRow = static_cast<int>(VASynthRowType::AmpRelease);
                vaSynthCursorRow_ = std::min(maxRow, vaSynthCursorRow_ + 1);
            } else {
                // Right column: max is Env2
                int maxRow = static_cast<int>(VASynthRowType::Env2);
                vaSynthCursorRow_ = std::min(maxRow, vaSynthCursorRow_ + 1);
            }
            vaSynthCursorField_ = 0;
            repaint();
            return true;

        case input::KeyAction::NavLeft:
            // On mod rows: navigate fields
            if (isModRow(vaSynthCursorRow_)) {
                vaSynthCursorField_ = (vaSynthCursorField_ + 3) % 4;  // -1 with wrap
                repaint();
                return true;
            }
            // Column navigation
            if (vaSynthCursorCol_ == 1) {
                vaSynthCursorCol_ = 0;
                // Keep same relative row position if possible, otherwise clamp
                if (vaSynthCursorRow_ > static_cast<int>(VASynthRowType::AmpRelease)) {
                    vaSynthCursorRow_ = static_cast<int>(VASynthRowType::AmpRelease);
                }
                vaSynthCursorField_ = 0;
            }
            repaint();
            return true;

        case input::KeyAction::NavRight:
            // On mod rows: navigate fields
            if (isModRow(vaSynthCursorRow_)) {
                vaSynthCursorField_ = (vaSynthCursorField_ + 1) % 4;
                repaint();
                return true;
            }
            // Column navigation
            if (vaSynthCursorCol_ == 0) {
                vaSynthCursorCol_ = 1;
                // Map to corresponding row in right column (preserve relative position)
                // Left column has 19 rows (0-18), right has rows from FilterAttack to Env2
                int leftMax = static_cast<int>(VASynthRowType::AmpRelease);
                int rightMin = static_cast<int>(VASynthRowType::FilterAttack);
                int rightMax = static_cast<int>(VASynthRowType::Env2);
                int rightRange = rightMax - rightMin;
                // Map proportionally
                float ratio = static_cast<float>(vaSynthCursorRow_) / static_cast<float>(leftMax);
                vaSynthCursorRow_ = rightMin + static_cast<int>(ratio * rightRange);
                vaSynthCursorRow_ = std::clamp(vaSynthCursorRow_, rightMin, rightMax);
                vaSynthCursorField_ = 0;
            }
            repaint();
            return true;

        case input::KeyAction::TabNext:
            // Tab navigates fields on mod rows
            if (isModRow(vaSynthCursorRow_)) {
                vaSynthCursorField_ = (vaSynthCursorField_ + 1) % 4;
                repaint();
                return true;
            }
            break;

        case input::KeyAction::TabPrev:
            // Shift+Tab navigates fields backward on mod rows
            if (isModRow(vaSynthCursorRow_)) {
                vaSynthCursorField_ = (vaSynthCursorField_ + 3) % 4;
                repaint();
                return true;
            }
            break;

        default:
            break;
    }

    // Determine delta step based on coarse flag
    float delta = isCoarse ? 0.01f : 0.05f;

    if (valueDelta != 0) {
        switch (static_cast<VASynthRowType>(vaSynthCursorRow_)) {
            case VASynthRowType::Osc1Waveform:
                params.osc1.waveform = wrapWaveform(params.osc1.waveform, valueDelta);
                break;
            case VASynthRowType::Osc1Octave:
                params.osc1.octave = std::clamp(params.osc1.octave + valueDelta, -2, 2);
                break;
            case VASynthRowType::Osc1Level:
                clampFloat(params.osc1.level, valueDelta * delta);
                break;
            case VASynthRowType::Osc2Waveform:
                params.osc2.waveform = wrapWaveform(params.osc2.waveform, valueDelta);
                break;
            case VASynthRowType::Osc2Octave:
                params.osc2.octave = std::clamp(params.osc2.octave + valueDelta, -2, 2);
                break;
            case VASynthRowType::Osc2Detune:
                params.osc2.cents = std::clamp(params.osc2.cents + valueDelta * (isCoarse ? 1.0f : 5.0f), -50.0f, 50.0f);
                break;
            case VASynthRowType::Osc2Level:
                clampFloat(params.osc2.level, valueDelta * delta);
                break;
            case VASynthRowType::Osc3Waveform:
                params.osc3.waveform = wrapWaveform(params.osc3.waveform, valueDelta);
                break;
            case VASynthRowType::Osc3Octave:
                params.osc3.octave = std::clamp(params.osc3.octave + valueDelta, -2, 2);
                break;
            case VASynthRowType::Osc3Detune:
                params.osc3.cents = std::clamp(params.osc3.cents + valueDelta * (isCoarse ? 1.0f : 5.0f), -50.0f, 50.0f);
                break;
            case VASynthRowType::Osc3Level:
                clampFloat(params.osc3.level, valueDelta * delta);
                break;
            case VASynthRowType::NoiseLevel:
                clampFloat(params.noiseLevel, valueDelta * delta);
                break;
            case VASynthRowType::FilterCutoff:
                clampFloat(params.filter.cutoff, valueDelta * delta);
                break;
            case VASynthRowType::FilterResonance:
                clampFloat(params.filter.resonance, valueDelta * delta);
                break;
            case VASynthRowType::FilterEnvAmount:
                clampFloat(params.filter.envAmount, valueDelta * delta);
                break;
            case VASynthRowType::AmpAttack:
                clampFloat(params.ampEnv.attack, valueDelta * delta);
                break;
            case VASynthRowType::AmpDecay:
                clampFloat(params.ampEnv.decay, valueDelta * delta);
                break;
            case VASynthRowType::AmpSustain:
                clampFloat(params.ampEnv.sustain, valueDelta * delta);
                break;
            case VASynthRowType::AmpRelease:
                clampFloat(params.ampEnv.release, valueDelta * delta);
                break;
            case VASynthRowType::FilterAttack:
                clampFloat(params.filterEnv.attack, valueDelta * delta);
                break;
            case VASynthRowType::FilterDecay:
                clampFloat(params.filterEnv.decay, valueDelta * delta);
                break;
            case VASynthRowType::FilterSustain:
                clampFloat(params.filterEnv.sustain, valueDelta * delta);
                break;
            case VASynthRowType::FilterRelease:
                clampFloat(params.filterEnv.release, valueDelta * delta);
                break;
            case VASynthRowType::Glide:
                clampFloat(params.glide, valueDelta * delta);
                break;
            // Voice-per-track architecture: polyphony control removed
            // case VASynthRowType::Polyphony:
            //     // Left decreases (towards mono), Right increases
            //     if (params.monoMode && valueDelta > 0) {
            //         // Switch from mono to poly (1 voice)
            //         params.monoMode = false;
            //         params.polyphony = 1;
            //     } else if (!params.monoMode && params.polyphony == 1 && valueDelta < 0) {
            //         // Switch from 1 voice to mono
            //         params.monoMode = true;
            //     } else if (!params.monoMode) {
            //         // Adjust voice count
            //         params.polyphony = std::clamp(params.polyphony + valueDelta, 1, 16);
            //     }
            //     break;

            // Modulation rows - 4 fields each
            case VASynthRowType::Lfo1:
                {
                    auto& lfo = params.modulation.lfo1;
                    switch (vaSynthCursorField_) {
                        case 0: lfo.rateIndex = std::clamp(lfo.rateIndex + valueDelta, 0, 15); break;
                        case 1: lfo.shapeIndex = std::clamp(lfo.shapeIndex + valueDelta, 0, 4); break;
                        case 2: lfo.destIndex = std::clamp(lfo.destIndex + valueDelta, 0, 8); break;
                        case 3: lfo.amount = static_cast<int8_t>(std::clamp(static_cast<int>(lfo.amount) + valueDelta, -64, 63)); break;
                    }
                }
                break;
            case VASynthRowType::Lfo2:
                {
                    auto& lfo = params.modulation.lfo2;
                    switch (vaSynthCursorField_) {
                        case 0: lfo.rateIndex = std::clamp(lfo.rateIndex + valueDelta, 0, 15); break;
                        case 1: lfo.shapeIndex = std::clamp(lfo.shapeIndex + valueDelta, 0, 4); break;
                        case 2: lfo.destIndex = std::clamp(lfo.destIndex + valueDelta, 0, 8); break;
                        case 3: lfo.amount = static_cast<int8_t>(std::clamp(static_cast<int>(lfo.amount) + valueDelta, -64, 63)); break;
                    }
                }
                break;
            case VASynthRowType::Env1:
                {
                    auto& env = params.modulation.env1;
                    switch (vaSynthCursorField_) {
                        case 0: env.attack = std::clamp(env.attack + valueDelta * delta, 0.0f, 1.0f); break;
                        case 1: env.decay = std::clamp(env.decay + valueDelta * delta, 0.0f, 1.0f); break;
                        case 2: env.destIndex = std::clamp(env.destIndex + valueDelta, 0, 8); break;
                        case 3: env.amount = static_cast<int8_t>(std::clamp(static_cast<int>(env.amount) + valueDelta, -64, 63)); break;
                    }
                }
                break;
            case VASynthRowType::Env2:
                {
                    auto& env = params.modulation.env2;
                    switch (vaSynthCursorField_) {
                        case 0: env.attack = std::clamp(env.attack + valueDelta * delta, 0.0f, 1.0f); break;
                        case 1: env.decay = std::clamp(env.decay + valueDelta * delta, 0.0f, 1.0f); break;
                        case 2: env.destIndex = std::clamp(env.destIndex + valueDelta, 0, 8); break;
                        case 3: env.amount = static_cast<int8_t>(std::clamp(static_cast<int>(env.amount) + valueDelta, -64, 63)); break;
                    }
                }
                break;

            default:
                break;
        }
        // Trigger preview note after any value change
        if (onNotePreview) onNotePreview(60, currentInstrument_);
        repaint();
        return true;
    }

    return false;
}

bool InstrumentScreen::isInterestedInFileDrag(const juce::StringArray& files) {
    // Only interested if we're on a Sampler or Slicer instrument
    auto* inst = project_.getInstrument(currentInstrument_);
    if (!inst) return false;

    auto type = inst->getType();
    if (type != model::InstrumentType::Sampler && type != model::InstrumentType::Slicer)
        return false;

    // Check if any file is an audio file
    for (const auto& filePath : files) {
        juce::File file(filePath);
        auto ext = file.getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".aiff" || ext == ".aif" ||
            ext == ".mp3" || ext == ".flac" || ext == ".ogg")
            return true;
    }
    return false;
}

void InstrumentScreen::filesDropped(const juce::StringArray& files, int /*x*/, int /*y*/) {
    if (!audioEngine_) return;

    auto* inst = project_.getInstrument(currentInstrument_);
    if (!inst) return;

    // Find first valid audio file
    juce::File audioFile;
    for (const auto& filePath : files) {
        juce::File file(filePath);
        auto ext = file.getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".aiff" || ext == ".aif" ||
            ext == ".mp3" || ext == ".flac" || ext == ".ogg") {
            audioFile = file;
            break;
        }
    }

    if (!audioFile.existsAsFile()) return;

    auto type = inst->getType();

    if (type == model::InstrumentType::Sampler) {
        if (auto* sampler = audioEngine_->getSamplerProcessor(currentInstrument_)) {
            sampler->setInstrument(inst);
            if (sampler->loadSample(audioFile)) {
                updateSamplerDisplay();
                repaint();
            }
        }
    }
    else if (type == model::InstrumentType::Slicer) {
        if (auto* slicer = audioEngine_->getSlicerProcessor(currentInstrument_)) {
            slicer->setInstrument(inst);
            slicer->loadSample(audioFile);
            updateSlicerDisplay();
            repaint();
        }
    }
}

std::vector<HelpSection> InstrumentScreen::getHelpContent() const
{
    // Get current instrument type for context-specific help
    auto* inst = project_.getInstrument(currentInstrument_);
    auto type = inst ? inst->getType() : model::InstrumentType::Plaits;

    std::vector<HelpSection> sections;

    // Navigation - common to all types
    sections.push_back({"Navigation", {
        {"Up/Down", "Move between parameters"},
        {"[  ]", "Previous/Next instrument"},
        {"Tab / Shift+Tab", "Cycle instrument type"},
        {"Shift+N", "Create new instrument"},
        {"r", "Rename instrument"},
    }});

    // Type-specific help
    if (type == model::InstrumentType::Plaits) {
        sections.push_back({"Plaits Synth", {
            {"Left/Right", "Switch instruments (non-mod rows)"},
            {"Left/Right", "Navigate fields (mod rows)"},
            {"+/- or L/R", "Adjust parameter value"},
            {"Shift", "Fine adjustment (hold)"},
        }});
        sections.push_back({"Presets", {
            {"Left/Right", "Cycle presets (on Preset row)"},
        }});
    }
    else if (type == model::InstrumentType::VASynth) {
        sections.push_back({"VA Synth", {
            {"Left/Right", "Switch instruments"},
            {"+/- or L/R", "Adjust parameter value"},
            {"Shift", "Fine adjustment (hold)"},
        }});
    }
    else if (type == model::InstrumentType::Sampler) {
        sections.push_back({"Sampler", {
            {"Drag & Drop", "Load audio file"},
            {"Left/Right", "Switch instruments"},
            {"+/-", "Adjust parameter value"},
        }});
    }
    else if (type == model::InstrumentType::Slicer) {
        sections.push_back({"Slicer", {
            {"Drag & Drop", "Load audio file"},
            {"Left/Right", "Switch instruments"},
            {"+/-", "Adjust parameter value"},
            {":chop N", "Divide into N equal slices"},
        }});
    }

    // Modulation - common to all
    sections.push_back({"Modulation (LFO/ENV)", {
        {"Left/Right", "Navigate: Rate, Shape, Dest, Amount"},
        {"Alt+Up/Down", "Adjust selected field"},
        {"+/-", "Adjust selected field"},
    }});

    // Commands
    sections.push_back({"Commands", {
        {":sampler", "Create sampler instrument"},
        {":slicer", "Create slicer instrument"},
        {":save-preset name", "Save as user preset"},
        {":delete-preset name", "Delete user preset"},
    }});

    return sections;
}

// ============================================================================
// DX Preset UI Implementation
// ============================================================================

void InstrumentScreen::paintDXPresetUI(juce::Graphics& g) {
    auto area = getLocalBounds();
    area.removeFromTop(30);  // Instrument tabs
    area.removeFromTop(26);  // Type selector

    auto* inst = project_.getInstrument(currentInstrument_);
    if (!inst || inst->getType() != model::InstrumentType::DXPreset) return;

    const auto& dxParams = inst->getDXParams();
    auto& sends = inst->getSends();

    // Header bar
    auto headerArea = area.removeFromTop(30);
    g.setColour(headerColor);
    g.fillRect(headerArea);

    g.setColour(fgColor);
    g.setFont(18.0f);
    juce::String title = "DX7: ";
    if (editingName_)
        title += juce::String(nameBuffer_) + "_";
    else
        title += juce::String(inst->getName());
    g.drawText(title, headerArea.reduced(10, 0), juce::Justification::centredLeft, true);

    area = area.reduced(5, 2);

    constexpr int kRowH = 24;
    constexpr int kDXBarWidth = 180;

    // Lambda to draw a simple row (text value only)
    auto drawTextRow = [&](int rowIndex, const char* label, const juce::String& value) {
        auto rowArea = area.removeFromTop(kRowH);
        bool isSelected = (rowIndex == dxPresetCursorRow_);

        if (isSelected) {
            g.setColour(highlightColor.withAlpha(0.3f));
            g.fillRect(rowArea);
        }

        g.setColour(isSelected ? cursorColor : fgColor);
        g.setFont(14.0f);
        g.drawText(label, rowArea.removeFromLeft(kLabelWidth), juce::Justification::centredLeft);
        g.drawText(value, rowArea, juce::Justification::centredLeft);
    };

    // Lambda to draw a slider row (with bar)
    auto drawSliderRow = [&](int rowIndex, const char* label, float value, const juce::String& valueText) {
        auto rowArea = area.removeFromTop(kRowH);
        bool isSelected = (rowIndex == dxPresetCursorRow_);

        if (isSelected) {
            g.setColour(highlightColor.withAlpha(0.3f));
            g.fillRect(rowArea);
        }

        g.setColour(isSelected ? cursorColor : fgColor);
        g.setFont(14.0f);
        g.drawText(label, rowArea.removeFromLeft(kLabelWidth), juce::Justification::centredLeft);

        // Slider bar
        int barX = rowArea.getX();
        int barY = rowArea.getY() + (kRowH - 10) / 2;
        g.setColour(juce::Colour(0xff303030));
        g.fillRect(barX, barY, kDXBarWidth, 10);
        g.setColour(isSelected ? cursorColor : juce::Colour(0xff4a9090));
        g.fillRect(barX, barY, static_cast<int>(kDXBarWidth * value), 10);

        // Value text
        g.setColour(isSelected ? cursorColor : fgColor);
        g.setFont(14.0f);
        g.drawText(valueText, barX + kDXBarWidth + 8, rowArea.getY(), 60, kRowH, juce::Justification::centredLeft);
    };

    // Lambda for section headers
    auto drawHeader = [&](const char* text) {
        area.removeFromTop(4);
        g.setColour(fgColor.darker(0.5f));
        g.setFont(12.0f);
        g.drawText(text, area.removeFromTop(16), juce::Justification::centredLeft);
    };

    // Get preset info
    const auto& presets = dxPresetBank_.getPresets();
    int presetIndex = dxParams.presetIndex;
    juce::String cartridgeName = "No cartridge";
    juce::String presetName = "INIT";
    int presetInBank = 0;

    if (presetIndex >= 0 && presetIndex < static_cast<int>(presets.size())) {
        const auto& preset = presets[static_cast<size_t>(presetIndex)];
        cartridgeName = juce::String(preset.bankName);
        presetName = juce::String(preset.name);
        presetInBank = preset.patchIndex;
    }

    // Draw rows
    drawHeader("-- PRESET --");
    drawTextRow(static_cast<int>(DXPresetRowType::Cartridge), "CARTRIDGE", cartridgeName);

    // Preset row with index
    juce::String presetStr = juce::String(presetInBank + 1) + ". " + presetName;
    drawTextRow(static_cast<int>(DXPresetRowType::Preset), "PRESET", presetStr);

    drawHeader("-- VOICE --");
    drawTextRow(static_cast<int>(DXPresetRowType::Polyphony), "VOICES",
                juce::String(dxParams.polyphony) + (dxParams.polyphony == 1 ? " (MONO)" : ""));

    // Volume and Pan removed - now in Mixer screen

    // Show total preset count
    area.removeFromTop(10);
    g.setColour(fgColor.darker(0.3f));
    g.setFont(12.0f);
    g.drawText("Total presets: " + juce::String(static_cast<int>(presets.size())),
               area.removeFromTop(16), juce::Justification::centredLeft);
}

bool InstrumentScreen::handleDXPresetKey(const juce::KeyPress& key, bool /*isEditMode*/) {
    // Use centralized key translation
    auto action = input::KeyHandler::translateKey(key, getInputContext());

    auto* instrument = project_.getInstrument(currentInstrument_);
    if (!instrument || instrument->getType() != model::InstrumentType::DXPreset) return false;

    // Handle actions from translateKey()
    switch (action.action)
    {
        case input::KeyAction::PatternPrev:
            // '[' switch to previous instrument
            {
                int numInstruments = project_.getInstrumentCount();
                if (numInstruments > 0)
                    currentInstrument_ = (currentInstrument_ - 1 + numInstruments) % numInstruments;
            }
            repaint();
            return true;

        case input::KeyAction::PatternNext:
            // ']' switch to next instrument
            {
                int numInstruments = project_.getInstrumentCount();
                if (numInstruments > 0)
                    currentInstrument_ = (currentInstrument_ + 1) % numInstruments;
            }
            repaint();
            return true;

        default:
            break;
    }

    auto& dxParams = instrument->getDXParams();
    auto& sends = instrument->getSends();
    const auto& presets = dxPresetBank_.getPresets();
    int numPresets = static_cast<int>(presets.size());

    // Navigation actions
    switch (action.action)
    {
        case input::KeyAction::NavUp:
            dxPresetCursorRow_ = std::max(0, dxPresetCursorRow_ - 1);
            repaint();
            return true;

        case input::KeyAction::NavDown:
            dxPresetCursorRow_ = std::min(kNumDXPresetRows - 1, dxPresetCursorRow_ + 1);
            repaint();
            return true;

        default:
            break;
    }

    // Handle edit actions for value adjustment
    int valueDelta = 0;
    bool isCoarse = false;
    switch (action.action)
    {
        case input::KeyAction::Edit1Inc:
        case input::KeyAction::Edit2Inc:
        case input::KeyAction::ZoomIn:
        case input::KeyAction::NavRight:
            valueDelta = 1;
            break;
        case input::KeyAction::Edit1Dec:
        case input::KeyAction::Edit2Dec:
        case input::KeyAction::ZoomOut:
        case input::KeyAction::NavLeft:
            valueDelta = -1;
            break;
        case input::KeyAction::ShiftEdit1Inc:
        case input::KeyAction::ShiftEdit2Inc:
            valueDelta = 1;
            isCoarse = true;
            break;
        case input::KeyAction::ShiftEdit1Dec:
        case input::KeyAction::ShiftEdit2Dec:
            valueDelta = -1;
            isCoarse = true;
            break;

        default:
            break;
    }

    float delta = isCoarse ? 0.01f : 0.05f;

    if (valueDelta != 0) {
        switch (static_cast<DXPresetRowType>(dxPresetCursorRow_)) {
            case DXPresetRowType::Cartridge:
                // Navigate between cartridges (banks)
                // Find all unique bank names
                if (numPresets > 0) {
                    std::vector<std::string> bankNames;
                    for (const auto& p : presets) {
                        if (std::find(bankNames.begin(), bankNames.end(), p.bankName) == bankNames.end()) {
                            bankNames.push_back(p.bankName);
                        }
                    }

                    if (!bankNames.empty()) {
                        // Find current bank index
                        std::string currentBank;
                        if (dxParams.presetIndex >= 0 && dxParams.presetIndex < numPresets) {
                            currentBank = presets[static_cast<size_t>(dxParams.presetIndex)].bankName;
                        }

                        int bankIdx = 0;
                        for (size_t i = 0; i < bankNames.size(); ++i) {
                            if (bankNames[i] == currentBank) {
                                bankIdx = static_cast<int>(i);
                                break;
                            }
                        }

                        // Navigate to next/prev bank
                        bankIdx = (bankIdx + valueDelta + static_cast<int>(bankNames.size())) % static_cast<int>(bankNames.size());

                        // Find first preset in new bank
                        for (int i = 0; i < numPresets; ++i) {
                            if (presets[static_cast<size_t>(i)].bankName == bankNames[static_cast<size_t>(bankIdx)]) {
                                dxParams.presetIndex = i;
                                break;
                            }
                        }

                        // Update DX7Instrument with new patch and store in DXParams
                        if (audioEngine_) {
                            auto* dx7 = audioEngine_->getDX7Processor(currentInstrument_);
                            if (dx7 && dxParams.presetIndex >= 0) {
                                const auto* preset = dxPresetBank_.getPreset(dxParams.presetIndex);
                                if (preset) {
                                    // Copy preset data into DXParams for persistence
                                    std::copy(preset->packedData.begin(), preset->packedData.end(),
                                             dxParams.packedPatch.begin());
                                    dx7->loadPackedPatch(preset->packedData.data());
                                }
                            }
                        }
                    }
                }
                break;

            case DXPresetRowType::Preset:
                // Navigate within current cartridge
                if (numPresets > 0) {
                    std::string currentBank;
                    if (dxParams.presetIndex >= 0 && dxParams.presetIndex < numPresets) {
                        currentBank = presets[static_cast<size_t>(dxParams.presetIndex)].bankName;
                    }

                    // Find presets in same bank
                    std::vector<int> bankPresets;
                    for (int i = 0; i < numPresets; ++i) {
                        if (presets[static_cast<size_t>(i)].bankName == currentBank) {
                            bankPresets.push_back(i);
                        }
                    }

                    if (!bankPresets.empty()) {
                        // Find current position in bank
                        int posInBank = 0;
                        for (size_t i = 0; i < bankPresets.size(); ++i) {
                            if (bankPresets[i] == dxParams.presetIndex) {
                                posInBank = static_cast<int>(i);
                                break;
                            }
                        }

                        // Navigate
                        int step = isCoarse ? 8 : 1;
                        posInBank = (posInBank + valueDelta * step + static_cast<int>(bankPresets.size())) % static_cast<int>(bankPresets.size());
                        dxParams.presetIndex = bankPresets[static_cast<size_t>(posInBank)];

                        // Update DX7Instrument with new patch and store in DXParams
                        if (audioEngine_) {
                            auto* dx7 = audioEngine_->getDX7Processor(currentInstrument_);
                            if (dx7) {
                                const auto* preset = dxPresetBank_.getPreset(dxParams.presetIndex);
                                if (preset) {
                                    // Copy preset data into DXParams for persistence
                                    std::copy(preset->packedData.begin(), preset->packedData.end(),
                                             dxParams.packedPatch.begin());
                                    dx7->loadPackedPatch(preset->packedData.data());
                                }
                            }
                        }
                    }
                }
                break;

            case DXPresetRowType::Polyphony:
                dxParams.polyphony = std::clamp(dxParams.polyphony + valueDelta, 1, 16);
                if (audioEngine_) {
                    auto* dx7 = audioEngine_->getDX7Processor(currentInstrument_);
                    if (dx7) {
                        dx7->setPolyphony(dxParams.polyphony);
                    }
                }
                break;

            default:
                break;
        }
        repaint();
        return true;
    }

    return false;
}

} // namespace ui
