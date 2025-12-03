#pragma once

#include <JuceHeader.h>

namespace ui {

class WaveformDisplay : public juce::Component {
public:
    WaveformDisplay();

    void setAudioData(const juce::AudioBuffer<float>* buffer, int sampleRate);
    void paint(juce::Graphics& g) override;

    void setZoom(float zoom);
    void setScrollPosition(float position);
    float getZoom() const { return zoom_; }
    float getScrollPosition() const { return scrollPosition_; }

    void zoomIn();
    void zoomOut();
    void scrollLeft();
    void scrollRight();

private:
    const juce::AudioBuffer<float>* audioBuffer_ = nullptr;
    int sampleRate_ = 44100;
    float zoom_ = 1.0f;           // 1.0 = fit whole sample
    float scrollPosition_ = 0.0f; // 0.0-1.0 position in sample

    juce::Colour waveformColour_ = juce::Colour(0xFF4EC9B0);
    juce::Colour backgroundColour_ = juce::Colour(0xFF1E1E1E);
    juce::Colour centreLineColour_ = juce::Colour(0xFF3C3C3C);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};

} // namespace ui
