#pragma once

#include "Screen.h"
#include "../audio/PlaitsInstrument.h"
#include "../audio/SamplerInstrument.h"
#include "../model/PresetManager.h"
#include "WaveformDisplay.h"
#include "SliceWaveformDisplay.h"
#include "../audio/SlicerInstrument.h"
#include "../audio/VASynthInstrument.h"

namespace ui {

// VA Synth-specific row types
enum class VASynthRowType {
    Osc1Waveform,
    Osc1Octave,
    Osc1Level,
    Osc2Waveform,
    Osc2Octave,
    Osc2Detune,
    Osc2Level,
    Osc3Waveform,
    Osc3Octave,
    Osc3Detune,
    Osc3Level,
    NoiseLevel,
    FilterCutoff,
    FilterResonance,
    FilterEnvAmount,
    AmpAttack,
    AmpDecay,
    AmpSustain,
    AmpRelease,
    FilterAttack,
    FilterDecay,
    FilterSustain,
    FilterRelease,
    Glide,
    Polyphony,  // Voice count (1-16, 1 = mono with legato)
    Lfo1,           // Multi-field: rate, shape, dest, amount
    Lfo2,
    Env1,           // Multi-field: attack, decay, dest, amount
    Env2,
    Reverb,         // FX sends
    Delay,
    Chorus,
    Drive,
    Sidechain,
    Volume,
    Pan,
    NumVASynthRows
};

// Sampler-specific row types
enum class SamplerRowType {
    RootNote,       // Manual override for detected pitch
    TargetNote,     // Target note for auto-repitch
    AutoRepitch,    // Toggle auto-repitch on/off
    Attack,
    Decay,
    Sustain,
    Release,
    Cutoff,
    Resonance,
    Lfo1,           // Multi-field: rate, shape, dest, amount
    Lfo2,
    Env1,           // Multi-field: attack, decay, dest, amount
    Env2,
    Reverb,         // FX sends
    Delay,
    Chorus,
    Drive,
    Sidechain,
    Volume,
    Pan,
    NumSamplerRows
};

// Slicer-specific row types
enum class SlicerRowType {
    ChopMode,       // Lazy / Divisions / Transients (with sub-controls)
    OriginalBars,   // Number of bars (editable, recalculates BPM)
    OriginalBPM,    // Override detected BPM (editable, recalculates bars)
    TargetBPM,      // Desired output BPM (editable)
    Speed,          // Playback speed multiplier (x0.5, x1, x2, etc.)
    Pitch,          // Pitch shift in semitones (editable when repitch=true)
    Repitch,        // Toggle: ON = pitch changes with speed, OFF = RubberBand
    Polyphony,      // Voice count (1 = choke/mono)
    Lfo1,           // Multi-field: rate, shape, dest, amount
    Lfo2,
    Env1,           // Multi-field: attack, decay, dest, amount
    Env2,
    Reverb,         // FX sends
    Delay,
    Chorus,
    Drive,
    Sidechain,
    Volume,
    Pan,
    NumSlicerRows
};

// Row types for InstrumentScreen
enum class InstrumentRowType {
    Preset,     // Preset selection row (new!)
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

class InstrumentScreen : public Screen, public juce::Timer, public juce::FileDragAndDropTarget
{
public:
    InstrumentScreen(model::Project& project, input::ModeManager& modeManager);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void navigate(int dx, int dy) override;
    bool handleEdit(const juce::KeyPress& key) override;
    bool handleEditKey(const juce::KeyPress& key);

    std::string getTitle() const override { return "INSTRUMENT"; }
    std::vector<HelpSection> getHelpContent() const override;

    // Key context for centralized key handling
    input::InputContext getInputContext() const override;

    // FileDragAndDropTarget interface
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    std::function<void(int note, int instrument)> onNotePreview;

    int getCurrentInstrument() const { return currentInstrument_; }
    void setCurrentInstrument(int index);

    void setAudioEngine(audio::AudioEngine* engine) override;
    void setPresetManager(model::PresetManager* manager) { presetManager_ = manager; }

    // Preset management
    void loadPreset(int presetIndex);
    int getCurrentPresetIndex() const { return currentPresetIndex_; }
    bool isPresetModified() const { return presetModified_; }

    // Slicer UI update (called after :chop command)
    void updateSlicerDisplay();

    // Timer callback for playhead updates
    void timerCallback() override;

private:
    void drawInstrumentTabs(juce::Graphics& g, juce::Rectangle<int> area);
    void drawTypeSelector(juce::Graphics& g, juce::Rectangle<int> area);
    void cycleInstrumentType(bool reverse = false);
    void drawRow(juce::Graphics& g, juce::Rectangle<int> area, int row, bool selected);
    void drawModRow(juce::Graphics& g, juce::Rectangle<int> area, int row, bool selected);
    void drawSliderRow(juce::Graphics& g, juce::Rectangle<int> area, const char* label,
                       float value, float modValue, bool selected);

    // Sampler-specific key handling
    bool handleSamplerKey(const juce::KeyPress& key, bool isEditMode);

    // Slicer-specific methods
    bool handleSlicerKey(const juce::KeyPress& key, bool isEditMode);
    void paintSlicerUI(juce::Graphics& g);

    // VA Synth-specific methods
    bool handleVASynthKey(const juce::KeyPress& key, bool isEditMode);
    void paintVASynthUI(juce::Graphics& g);

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
    model::PresetManager* presetManager_ = nullptr;

    // Per-engine preset tracking
    int currentPresetIndex_ = 0;
    bool presetModified_ = false;

    // Slicer UI component
    std::unique_ptr<SliceWaveformDisplay> sliceWaveformDisplay_;

    // Sampler UI component
    std::unique_ptr<WaveformDisplay> samplerWaveformDisplay_;
    void updateSamplerDisplay();
    void paintSamplerUI(juce::Graphics& g);

    // Sampler/Slicer/VASynth cursor tracking
    int samplerCursorRow_ = 0;
    int samplerCursorField_ = 0;  // For multi-field rows (LFO/ENV)
    int slicerCursorRow_ = 0;
    int slicerCursorField_ = 0;   // For multi-field rows (LFO/ENV)
    int vaSynthCursorRow_ = 0;
    int vaSynthCursorField_ = 0;  // For multi-field rows (LFO/ENV)
    int vaSynthCursorCol_ = 0;    // 0=left column, 1=right column

    static constexpr int kNumRows = static_cast<int>(InstrumentRowType::NumRows);
    static constexpr int kNumSamplerRows = static_cast<int>(SamplerRowType::NumSamplerRows);
    static constexpr int kNumSlicerRows = static_cast<int>(SlicerRowType::NumSlicerRows);
    static constexpr int kNumVASynthRows = static_cast<int>(VASynthRowType::NumVASynthRows);
    static constexpr int kRowHeight = 24;
    static constexpr int kShortcutWidth = 16;  // Width for jump key indicator
    static constexpr int kLabelWidth = 80;
    static constexpr int kBarWidth = 180;

    // Get the jump key for a row (returns '\0' if no jump key)
    static char getJumpKeyForRow(model::InstrumentType type, int row);
    // Get row number for a jump key (returns -1 if not found)
    static int getRowForJumpKey(model::InstrumentType type, char key);

    static const char* engineNames_[16];
    static const char* engineParamLabels_[16][3];  // Per-engine labels for harmonics/timbre/morph
};

} // namespace ui
