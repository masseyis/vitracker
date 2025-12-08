#pragma once

#include "Screen.h"
#include "WaveformDisplay.h"
#include "../model/Project.h"
#include "../audio/AudioEngine.h"

namespace ui {

class SamplerScreen : public Screen {
public:
    enum class RowType {
        Sample,
        RootNote,
        FilterCutoff,
        FilterResonance,
        AmpAttack,
        AmpDecay,
        AmpSustain,
        AmpRelease,
        FilterEnvAmount,
        SendReverb,
        SendDelay,
        SendChorus,
        COUNT
    };

    SamplerScreen(model::Project& project, input::ModeManager& modeManager);

    void paint(juce::Graphics& g) override;
    void resized() override;

    std::string getTitle() const override { return "SAMPLER"; }

    void setCurrentInstrument(int index);
    int getCurrentInstrument() const { return currentInstrument_; }

    bool handleEdit(const juce::KeyPress& key) override;
    void navigate(int dx, int dy) override;

private:
    void updateWaveformDisplay();
    void loadSample();
    void adjustValue(int delta);
    float getRowValue(RowType row) const;
    void setRowValue(RowType row, float value);
    juce::String getRowName(RowType row) const;
    juce::String getRowValueString(RowType row) const;

    int currentInstrument_ = 0;
    int currentRow_ = 0;

    WaveformDisplay waveformDisplay_;

    static constexpr int WAVEFORM_HEIGHT = 100;
    static constexpr int ROW_HEIGHT = 24;
    static constexpr int LABEL_WIDTH = 140;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SamplerScreen)
};

} // namespace ui
