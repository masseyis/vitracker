#include "SamplerScreen.h"
#include "../input/KeyHandler.h"

namespace ui {

SamplerScreen::SamplerScreen(model::Project& project, input::ModeManager& modeManager)
    : Screen(project, modeManager)
{
    addAndMakeVisible(waveformDisplay_);
}

void SamplerScreen::setCurrentInstrument(int index) {
    currentInstrument_ = index;
    updateWaveformDisplay();
    repaint();
}

void SamplerScreen::updateWaveformDisplay() {
    if (audioEngine_) {
        if (auto* sampler = audioEngine_->getSamplerProcessor(currentInstrument_)) {
            waveformDisplay_.setAudioData(&sampler->getSampleBuffer(), sampler->getSampleRate());
            return;
        }
    }
    waveformDisplay_.setAudioData(nullptr, 44100);
}

void SamplerScreen::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();

    // Background
    g.fillAll(juce::Colour(0xFF1E1E1E));

    // Title
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);

    juce::String title = "SAMPLER";
    if (auto* inst = project_.getInstrument(currentInstrument_)) {
        title += " - " + juce::String(inst->getName());
    }
    g.drawText(title, bounds.removeFromTop(28), juce::Justification::centredLeft);

    // Skip waveform area
    bounds.removeFromTop(WAVEFORM_HEIGHT + 8);

    // Draw parameter rows
    g.setFont(14.0f);

    for (int i = 0; i < static_cast<int>(RowType::COUNT); ++i) {
        auto rowBounds = bounds.removeFromTop(ROW_HEIGHT);
        auto row = static_cast<RowType>(i);

        // Highlight current row
        if (i == currentRow_) {
            g.setColour(juce::Colour(0xFF2D2D2D));
            g.fillRect(rowBounds);
        }

        // Label
        g.setColour(juce::Colours::grey);
        g.drawText(getRowName(row), rowBounds.removeFromLeft(LABEL_WIDTH),
                   juce::Justification::centredLeft);

        // Value
        g.setColour(i == currentRow_ ? juce::Colour(0xFF4EC9B0) : juce::Colours::white);
        g.drawText(getRowValueString(row), rowBounds.removeFromLeft(200),
                   juce::Justification::centredLeft);

        // Value bar for numeric params
        if (row != RowType::Sample) {
            float value = getRowValue(row);
            auto barBounds = rowBounds.removeFromLeft(100).reduced(2);
            g.setColour(juce::Colour(0xFF3C3C3C));
            g.fillRect(barBounds);
            g.setColour(juce::Colour(0xFF4EC9B0));
            g.fillRect(barBounds.removeFromLeft(static_cast<int>(barBounds.getWidth() * value)));
        }
    }
}

void SamplerScreen::resized() {
    auto bounds = getLocalBounds();
    bounds.removeFromTop(28); // Title
    waveformDisplay_.setBounds(bounds.removeFromTop(WAVEFORM_HEIGHT));
}

bool SamplerScreen::handleEdit(const juce::KeyPress& key) {
    // Use centralized key translation (RowParams context for sampler)
    auto action = input::KeyHandler::translateKey(key, input::InputContext::RowParams, false);

    // Waveform controls (always active)
    if (action.action == input::KeyAction::ZoomIn) {
        waveformDisplay_.zoomIn();
        return true;
    }
    if (action.action == input::KeyAction::ZoomOut) {
        waveformDisplay_.zoomOut();
        return true;
    }

    // Secondary scroll (Shift+H/L mapped to SecondaryPrev/SecondaryNext)
    if (action.action == input::KeyAction::SecondaryPrev) {
        waveformDisplay_.scrollLeft();
        return true;
    }
    if (action.action == input::KeyAction::SecondaryNext) {
        waveformDisplay_.scrollRight();
        return true;
    }

    // Load sample command ('o' mapped to Open action)
    if (action.action == input::KeyAction::Open) {
        loadSample();
        return true;
    }

    // Value adjustment (Edit1Inc/Dec for RowParams context)
    if (action.action == input::KeyAction::Edit1Inc ||
        action.action == input::KeyAction::ShiftEdit1Inc) {
        adjustValue(1);
        return true;
    }
    if (action.action == input::KeyAction::Edit1Dec ||
        action.action == input::KeyAction::ShiftEdit1Dec) {
        adjustValue(-1);
        return true;
    }

    return false;
}

void SamplerScreen::navigate(int /*dx*/, int dy) {
    int numRows = static_cast<int>(RowType::COUNT);
    currentRow_ = (currentRow_ + dy + numRows) % numRows;
    repaint();
}

void SamplerScreen::loadSample() {
    auto* chooserPtr = new juce::FileChooser(
        "Load Sample",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "*.wav;*.aiff;*.aif;*.mp3;*.flac"
    );

    chooserPtr->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooserPtr](const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file.existsAsFile() && audioEngine_) {
                if (auto* sampler = audioEngine_->getSamplerProcessor(currentInstrument_)) {
                    if (auto* inst = project_.getInstrument(currentInstrument_)) {
                        sampler->setInstrument(inst);
                        if (sampler->loadSample(file)) {
                            updateWaveformDisplay();
                            repaint();
                        }
                    }
                }
            }
            delete chooserPtr;
        }
    );
}

void SamplerScreen::adjustValue(int delta) {
    auto row = static_cast<RowType>(currentRow_);
    float current = getRowValue(row);
    float step = 0.01f;

    if (row == RowType::RootNote) {
        step = 1.0f / 127.0f;
    }

    setRowValue(row, current + delta * step);
    repaint();
}

float SamplerScreen::getRowValue(RowType row) const {
    auto* inst = project_.getInstrument(currentInstrument_);
    if (!inst) return 0.0f;

    const auto& params = inst->getSamplerParams();
    const auto& sends = inst->getSends();

    switch (row) {
        case RowType::Sample: return 0.0f;
        case RowType::RootNote: return params.rootNote / 127.0f;
        case RowType::FilterCutoff: return params.filter.cutoff;
        case RowType::FilterResonance: return params.filter.resonance;
        case RowType::AmpAttack: return params.ampEnvelope.attack;
        case RowType::AmpDecay: return params.ampEnvelope.decay;
        case RowType::AmpSustain: return params.ampEnvelope.sustain;
        case RowType::AmpRelease: return params.ampEnvelope.release;
        case RowType::FilterEnvAmount: return (params.filterEnvAmount + 1.0f) / 2.0f;
        case RowType::SendReverb: return sends.reverb;
        case RowType::SendDelay: return sends.delay;
        case RowType::SendChorus: return sends.chorus;
        default: return 0.0f;
    }
}

void SamplerScreen::setRowValue(RowType row, float value) {
    auto* inst = project_.getInstrument(currentInstrument_);
    if (!inst) return;

    value = juce::jlimit(0.0f, 1.0f, value);
    auto& params = inst->getSamplerParams();
    auto& sends = inst->getSends();

    switch (row) {
        case RowType::Sample: break;
        case RowType::RootNote: params.rootNote = static_cast<int>(value * 127); break;
        case RowType::FilterCutoff: params.filter.cutoff = value; break;
        case RowType::FilterResonance: params.filter.resonance = value; break;
        case RowType::AmpAttack: params.ampEnvelope.attack = value; break;
        case RowType::AmpDecay: params.ampEnvelope.decay = value; break;
        case RowType::AmpSustain: params.ampEnvelope.sustain = value; break;
        case RowType::AmpRelease: params.ampEnvelope.release = value; break;
        case RowType::FilterEnvAmount: params.filterEnvAmount = value * 2.0f - 1.0f; break;
        case RowType::SendReverb: sends.reverb = value; break;
        case RowType::SendDelay: sends.delay = value; break;
        case RowType::SendChorus: sends.chorus = value; break;
        default: break;
    }
}

juce::String SamplerScreen::getRowName(RowType row) const {
    switch (row) {
        case RowType::Sample: return "Sample";
        case RowType::RootNote: return "Root Note";
        case RowType::FilterCutoff: return "Filter Cutoff";
        case RowType::FilterResonance: return "Filter Resonance";
        case RowType::AmpAttack: return "Amp Attack";
        case RowType::AmpDecay: return "Amp Decay";
        case RowType::AmpSustain: return "Amp Sustain";
        case RowType::AmpRelease: return "Amp Release";
        case RowType::FilterEnvAmount: return "Filter Env Amt";
        case RowType::SendReverb: return "Send Reverb";
        case RowType::SendDelay: return "Send Delay";
        case RowType::SendChorus: return "Send Chorus";
        default: return "";
    }
}

juce::String SamplerScreen::getRowValueString(RowType row) const {
    auto* inst = project_.getInstrument(currentInstrument_);
    if (!inst) return "";

    const auto& params = inst->getSamplerParams();

    switch (row) {
        case RowType::Sample:
            return params.sample.path.empty() ? "(no sample)" : juce::File(params.sample.path).getFileName();
        case RowType::RootNote: {
            static const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
            int note = params.rootNote;
            return juce::String(noteNames[note % 12]) + juce::String(note / 12 - 1);
        }
        default:
            return juce::String(getRowValue(row), 2);
    }
}

} // namespace ui
