#include "WaveformDisplay.h"

namespace ui {

WaveformDisplay::WaveformDisplay() {
    setOpaque(true);
}

void WaveformDisplay::setAudioData(const juce::AudioBuffer<float>* buffer, int sampleRate) {
    audioBuffer_ = buffer;
    sampleRate_ = sampleRate;
    repaint();
}

void WaveformDisplay::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    g.fillAll(backgroundColour_);

    // Centre line
    g.setColour(centreLineColour_);
    g.drawHorizontalLine(bounds.getCentreY(), 0.0f, static_cast<float>(bounds.getWidth()));

    if (!audioBuffer_ || audioBuffer_->getNumSamples() == 0) {
        g.setColour(juce::Colours::grey);
        g.drawText("No sample loaded", bounds, juce::Justification::centred);
        return;
    }

    const int numSamples = audioBuffer_->getNumSamples();
    const float* data = audioBuffer_->getReadPointer(0);

    // Calculate visible range based on zoom and scroll
    const float visibleRatio = 1.0f / zoom_;
    const int visibleSamples = static_cast<int>(numSamples * visibleRatio);
    const int startSample = static_cast<int>(scrollPosition_ * (numSamples - visibleSamples));

    const float centreY = bounds.getCentreY();
    const float halfHeight = bounds.getHeight() * 0.45f;

    g.setColour(waveformColour_);

    juce::Path waveformPath;
    bool pathStarted = false;

    const int width = bounds.getWidth();
    const float samplesPerPixel = static_cast<float>(visibleSamples) / width;

    for (int x = 0; x < width; ++x) {
        const int sampleIndex = startSample + static_cast<int>(x * samplesPerPixel);

        if (sampleIndex >= 0 && sampleIndex < numSamples) {
            // Find min/max in this pixel's sample range
            float minVal = 0.0f, maxVal = 0.0f;
            const int rangeStart = sampleIndex;
            const int rangeEnd = std::min(static_cast<int>(rangeStart + samplesPerPixel + 1), numSamples);

            for (int i = rangeStart; i < rangeEnd; ++i) {
                minVal = std::min(minVal, data[i]);
                maxVal = std::max(maxVal, data[i]);
            }

            const float y1 = centreY - maxVal * halfHeight;
            const float y2 = centreY - minVal * halfHeight;

            if (!pathStarted) {
                waveformPath.startNewSubPath(static_cast<float>(x), y1);
                pathStarted = true;
            }

            waveformPath.lineTo(static_cast<float>(x), y1);
            waveformPath.lineTo(static_cast<float>(x), y2);
        }
    }

    g.strokePath(waveformPath, juce::PathStrokeType(1.0f));

    // Draw playhead if active
    if (playheadPosition_ >= 0 && playheadPosition_ < numSamples) {
        // Convert sample position to x coordinate
        if (playheadPosition_ >= startSample && playheadPosition_ < startSample + visibleSamples) {
            const float playheadX = static_cast<float>(playheadPosition_ - startSample) / visibleSamples * width;
            g.setColour(playheadColour_);
            g.drawVerticalLine(static_cast<int>(playheadX), 0.0f, static_cast<float>(bounds.getHeight()));
        }
    }

    // Draw time info
    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.setFont(12.0f);

    const float startTime = static_cast<float>(startSample) / sampleRate_;
    const float endTime = static_cast<float>(startSample + visibleSamples) / sampleRate_;
    const float totalTime = static_cast<float>(numSamples) / sampleRate_;

    juce::String timeText = juce::String::formatted("%.2fs - %.2fs / %.2fs  Zoom: %.0f%%",
        startTime, endTime, totalTime, zoom_ * 100.0f);
    g.drawText(timeText, bounds.reduced(4), juce::Justification::bottomLeft);
}

void WaveformDisplay::setZoom(float zoom) {
    zoom_ = juce::jlimit(1.0f, 100.0f, zoom);
    repaint();
}

void WaveformDisplay::setScrollPosition(float position) {
    scrollPosition_ = juce::jlimit(0.0f, 1.0f, position);
    repaint();
}

void WaveformDisplay::zoomIn() {
    setZoom(zoom_ * 1.5f);
}

void WaveformDisplay::zoomOut() {
    setZoom(zoom_ / 1.5f);
}

void WaveformDisplay::scrollLeft() {
    setScrollPosition(scrollPosition_ - 0.1f / zoom_);
}

void WaveformDisplay::scrollRight() {
    setScrollPosition(scrollPosition_ + 0.1f / zoom_);
}

void WaveformDisplay::setPlayheadPosition(int64_t samplePosition) {
    if (playheadPosition_ != samplePosition) {
        playheadPosition_ = samplePosition;
        repaint();
    }
}

} // namespace ui
