#pragma once

#include <string>
#include <cstdint>
#include "../model/Step.h"

namespace audio {

// Base class for all instrument types (Plaits, Sampler, etc.)
class InstrumentProcessor
{
public:
    virtual ~InstrumentProcessor() = default;

    // Lifecycle
    virtual void init(double sampleRate) = 0;
    virtual void setSampleRate(double sampleRate) = 0;

    // Note handling
    virtual void noteOn(int note, float velocity) = 0;
    virtual void noteOff(int note) = 0;
    virtual void allNotesOff() = 0;

    // Note handling with FX commands (default implementation ignores FX)
    virtual void noteOnWithFX(int note, float velocity, const model::Step& step) {
        // Default: ignore FX and just trigger note
        (void)step;
        noteOn(note, velocity);
    }

    // Audio processing
    virtual void process(float* outL, float* outR, int numSamples) = 0;

    // Instrument type identification
    virtual const char* getTypeName() const = 0;

    // Parameter access (generic interface)
    virtual int getNumParameters() const = 0;
    virtual const char* getParameterName(int index) const = 0;
    virtual float getParameter(int index) const = 0;
    virtual void setParameter(int index, float value) = 0;

    // Get/set all parameters for serialization
    virtual void getState(void* data, int maxSize) const = 0;
    virtual void setState(const void* data, int size) = 0;
};

} // namespace audio
