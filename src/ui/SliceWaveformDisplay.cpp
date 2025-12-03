#include "SliceWaveformDisplay.h"

namespace ui {

SliceWaveformDisplay::SliceWaveformDisplay() {
}

void SliceWaveformDisplay::setSlicePoints(const std::vector<size_t>& slices) {
    slicePoints_ = slices;
    if (currentSlice_ >= static_cast<int>(slicePoints_.size())) {
        currentSlice_ = slicePoints_.empty() ? 0 : static_cast<int>(slicePoints_.size()) - 1;
    }
    repaint();
}

void SliceWaveformDisplay::setCurrentSlice(int index) {
    if (slicePoints_.empty()) {
        currentSlice_ = 0;
    } else {
        currentSlice_ = juce::jlimit(0, static_cast<int>(slicePoints_.size()) - 1, index);
    }
    repaint();
}

void SliceWaveformDisplay::nextSlice() {
    if (!slicePoints_.empty()) {
        setCurrentSlice(currentSlice_ + 1);
    }
}

void SliceWaveformDisplay::previousSlice() {
    if (!slicePoints_.empty()) {
        setCurrentSlice(currentSlice_ - 1);
    }
}

void SliceWaveformDisplay::addSliceAtCursor() {
    if (onAddSlice) {
        onAddSlice(getCursorSamplePosition());
    }
}

void SliceWaveformDisplay::deleteCurrentSlice() {
    if (onDeleteSlice && !slicePoints_.empty()) {
        onDeleteSlice(currentSlice_);
    }
}

size_t SliceWaveformDisplay::getCursorSamplePosition() const {
    // Calculate sample position at center of current view
    if (!audioBuffer_ || audioBuffer_->getNumSamples() == 0) return 0;

    const int numSamples = audioBuffer_->getNumSamples();
    const float visibleRatio = 1.0f / getZoom();
    const int visibleSamples = static_cast<int>(numSamples * visibleRatio);
    const int startSample = static_cast<int>(getScrollPosition() * (numSamples - visibleSamples));

    return static_cast<size_t>(startSample + visibleSamples / 2);
}

void SliceWaveformDisplay::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();

    // Draw background
    g.fillAll(backgroundColour_);

    // Draw slice regions first (behind waveform)
    if (!slicePoints_.empty() && audioBuffer_ && audioBuffer_->getNumSamples() > 0) {
        paintSliceRegions(g, bounds);
    }

    // Draw centre line
    g.setColour(centreLineColour_);
    g.drawHorizontalLine(bounds.getCentreY(), 0.0f, static_cast<float>(bounds.getWidth()));

    // Draw waveform
    if (audioBuffer_ && audioBuffer_->getNumSamples() > 0) {
        const int numSamples = audioBuffer_->getNumSamples();
        const float* data = audioBuffer_->getReadPointer(0);

        const float visibleRatio = 1.0f / getZoom();
        const int visibleSamples = static_cast<int>(numSamples * visibleRatio);
        const int startSample = static_cast<int>(getScrollPosition() * (numSamples - visibleSamples));

        const float centreY = static_cast<float>(bounds.getCentreY());
        const float halfHeight = bounds.getHeight() * 0.45f;

        g.setColour(waveformColour_);

        juce::Path waveformPath;
        bool pathStarted = false;

        const int width = bounds.getWidth();
        const float samplesPerPixel = static_cast<float>(visibleSamples) / width;

        for (int x = 0; x < width; ++x) {
            const int sampleIndex = startSample + static_cast<int>(x * samplesPerPixel);

            if (sampleIndex >= 0 && sampleIndex < numSamples) {
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

        // Draw slice markers on top
        paintSliceMarkers(g, bounds);
    } else {
        g.setColour(juce::Colours::grey);
        g.drawText("No sample loaded", bounds, juce::Justification::centred);
    }

    // Draw info text
    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.setFont(12.0f);

    juce::String infoText;
    if (!slicePoints_.empty()) {
        infoText = juce::String::formatted("Slice: %d/%d  Zoom: %.0f%%",
            currentSlice_ + 1, static_cast<int>(slicePoints_.size()), getZoom() * 100.0f);
    } else {
        infoText = juce::String::formatted("No slices  Zoom: %.0f%%", getZoom() * 100.0f);
    }
    g.drawText(infoText, bounds.reduced(4), juce::Justification::bottomLeft);
}

void SliceWaveformDisplay::paintSliceRegions(juce::Graphics& g, juce::Rectangle<int> bounds) {
    if (!audioBuffer_ || audioBuffer_->getNumSamples() == 0) return;

    const int numSamples = audioBuffer_->getNumSamples();
    const float visibleRatio = 1.0f / getZoom();
    const int visibleSamples = static_cast<int>(numSamples * visibleRatio);
    const int startSample = static_cast<int>(getScrollPosition() * (numSamples - visibleSamples));
    const int endSample = startSample + visibleSamples;
    const float samplesPerPixel = static_cast<float>(visibleSamples) / bounds.getWidth();

    for (int i = 0; i < static_cast<int>(slicePoints_.size()); ++i) {
        size_t sliceStart = slicePoints_[i];
        size_t sliceEnd = (i + 1 < static_cast<int>(slicePoints_.size()))
            ? slicePoints_[i + 1]
            : static_cast<size_t>(numSamples);

        // Skip if slice is outside visible range
        if (sliceEnd < static_cast<size_t>(startSample) || sliceStart > static_cast<size_t>(endSample)) continue;

        // Calculate pixel positions
        int x1 = static_cast<int>((static_cast<float>(sliceStart) - startSample) / samplesPerPixel);
        int x2 = static_cast<int>((static_cast<float>(sliceEnd) - startSample) / samplesPerPixel);

        x1 = juce::jlimit(0, bounds.getWidth(), x1);
        x2 = juce::jlimit(0, bounds.getWidth(), x2);

        // Choose color based on slice index and selection
        juce::Colour regionColour;
        if (i == currentSlice_) {
            regionColour = currentSliceColour_;
        } else {
            regionColour = (i % 2 == 0) ? sliceColourA_ : sliceColourB_;
        }

        g.setColour(regionColour);
        g.fillRect(x1, 0, x2 - x1, bounds.getHeight());
    }
}

void SliceWaveformDisplay::paintSliceMarkers(juce::Graphics& g, juce::Rectangle<int> bounds) {
    if (!audioBuffer_ || audioBuffer_->getNumSamples() == 0) return;

    const int numSamples = audioBuffer_->getNumSamples();
    const float visibleRatio = 1.0f / getZoom();
    const int visibleSamples = static_cast<int>(numSamples * visibleRatio);
    const int startSample = static_cast<int>(getScrollPosition() * (numSamples - visibleSamples));
    const float samplesPerPixel = static_cast<float>(visibleSamples) / bounds.getWidth();

    g.setFont(10.0f);

    for (int i = 0; i < static_cast<int>(slicePoints_.size()); ++i) {
        int x = static_cast<int>((static_cast<float>(slicePoints_[i]) - startSample) / samplesPerPixel);

        if (x >= 0 && x < bounds.getWidth()) {
            // Draw marker line
            g.setColour(markerColour_);
            g.drawVerticalLine(x, 0.0f, static_cast<float>(bounds.getHeight()));

            // Draw slice number
            g.setColour(juce::Colours::white);
            g.drawText(juce::String(i), x + 2, 2, 20, 14, juce::Justification::centredLeft);
        }
    }
}

} // namespace ui
