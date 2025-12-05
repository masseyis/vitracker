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

    // Playhead position (-1 means no playhead visible)
    void setPlayheadPosition(int64_t samplePosition);
    int64_t getPlayheadPosition() const { return playheadPosition_; }

protected:
    const juce::AudioBuffer<float>* audioBuffer_ = nullptr;
    int sampleRate_ = 44100;
    float zoom_ = 1.0f;           // 1.0 = fit whole sample
    float scrollPosition_ = 0.0f; // 0.0-1.0 position in sample

    // Colors matched to project color scheme (from Screen.h)
    juce::Colour waveformColour_ = juce::Colour(0xFF7c7cff);    // cursorColor - bright purple
    juce::Colour backgroundColour_ = juce::Colour(0xff1a1a2e);  // bgColor - dark blue
    juce::Colour centreLineColour_ = juce::Colour(0xff2a2a4e);  // headerColor - darker blue
    juce::Colour playheadColour_ = juce::Colour(0xFFff6b6b);    // Red playhead line

    // Playhead position in samples (-1 = hidden)
    int64_t playheadPosition_ = -1;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};

} // namespace ui
