#pragma once

#include "WaveformDisplay.h"
#include <vector>

namespace ui {

class SliceWaveformDisplay : public WaveformDisplay {
public:
    SliceWaveformDisplay();

    void paint(juce::Graphics& g) override;

    // Slice management
    void setSlicePoints(const std::vector<size_t>& slices);
    void setCurrentSlice(int index);
    int getCurrentSlice() const { return currentSlice_; }

    // Navigation
    void nextSlice();
    void previousSlice();

    // Editing
    void addSliceAtCursor();
    void deleteCurrentSlice();

    // Get slice position for adding
    size_t getCursorSamplePosition() const;

    std::function<void(size_t)> onAddSlice;
    std::function<void(int)> onDeleteSlice;

private:
    void paintSliceRegions(juce::Graphics& g, juce::Rectangle<int> bounds);
    void paintSliceMarkers(juce::Graphics& g, juce::Rectangle<int> bounds);

    std::vector<size_t> slicePoints_;
    int currentSlice_ = 0;

    // Alternating colors for slice regions
    juce::Colour sliceColourA_ = juce::Colour(0xFF2D5A27);  // Green tint
    juce::Colour sliceColourB_ = juce::Colour(0xFF27455A);  // Blue tint
    juce::Colour currentSliceColour_ = juce::Colour(0xFF5A4827);  // Highlighted
    juce::Colour markerColour_ = juce::Colour(0xFFFFFFFF);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SliceWaveformDisplay)
};

} // namespace ui
