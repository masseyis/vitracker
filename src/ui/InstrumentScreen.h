#pragma once

#include "Screen.h"
#include "../audio/PlaitsInstrument.h"

namespace ui {

// Row types for InstrumentScreen
enum class InstrumentRowType {
    Engine,
    Harmonics,
    Timbre,
    Morph,
    Attack,
    Decay,
    Polyphony,
    Cutoff,
    Resonance,
    Lfo1,       // Multi-field: rate, shape, dest, amount
    Lfo2,
    Env1,
    Env2,
    Volume,     // Per-instrument mixer controls
    Pan,
    Reverb,
    Delay,
    Chorus,
    Drive,
    Sidechain,
    NumRows
};

class InstrumentScreen : public Screen
{
public:
    InstrumentScreen(model::Project& project, input::ModeManager& modeManager);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void navigate(int dx, int dy) override;
    bool handleEdit(const juce::KeyPress& key) override;
    bool handleEditKey(const juce::KeyPress& key);

    std::string getTitle() const override { return "INSTRUMENT"; }

    std::function<void(int note, int instrument)> onNotePreview;

    int getCurrentInstrument() const { return currentInstrument_; }
    void setCurrentInstrument(int index) { currentInstrument_ = index; }

    void setAudioEngine(audio::AudioEngine* engine) override;

private:
    void drawInstrumentTabs(juce::Graphics& g, juce::Rectangle<int> area);
    void drawRow(juce::Graphics& g, juce::Rectangle<int> area, int row, bool selected);
    void drawModRow(juce::Graphics& g, juce::Rectangle<int> area, int row, bool selected);
    void drawSliderRow(juce::Graphics& g, juce::Rectangle<int> area, const char* label,
                       float value, float modValue, bool selected);

    bool editingName_ = false;
    std::string nameBuffer_;

    bool isModRow(int row) const;
    int getNumFieldsForRow(int row) const;
    int getModFieldCount() const { return 4; }  // rate, shape, dest, amount

    // Get/set values through PlaitsInstrument processor
    float getRowValue(int row) const;
    void setRowValue(int row, float delta);
    void adjustModField(int row, int field, int delta, bool fineAdjust = false);

    // LFO/ENV names
    static const char* getLfoRateName(int rate);
    static const char* getLfoShapeName(int shape);
    static const char* getModDestName(int dest);

    int currentInstrument_ = 0;
    int cursorRow_ = 0;
    int cursorField_ = 0;  // For multi-field rows (LFO/ENV)

    audio::AudioEngine* audioEngine_ = nullptr;

    static constexpr int kNumRows = static_cast<int>(InstrumentRowType::NumRows);
    static constexpr int kRowHeight = 24;
    static constexpr int kLabelWidth = 80;
    static constexpr int kBarWidth = 180;

    static const char* engineNames_[16];
    static const char* engineParamLabels_[16][3];  // Per-engine labels for harmonics/timbre/morph
};

} // namespace ui
