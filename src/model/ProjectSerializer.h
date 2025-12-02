#pragma once

#include "Project.h"
#include <JuceHeader.h>

namespace model {

class ProjectSerializer
{
public:
    static bool save(const Project& project, const juce::File& file);
    static bool load(Project& project, const juce::File& file);

    static juce::String toJson(const Project& project);
    static bool fromJson(Project& project, const juce::String& json);

private:
    static juce::var instrumentToVar(const Instrument& inst);
    static void varToInstrument(Instrument& inst, const juce::var& v);

    static juce::var patternToVar(const Pattern& pattern);
    static void varToPattern(Pattern& pattern, const juce::var& v);

    static juce::var chainToVar(const Chain& chain);
    static void varToChain(Chain& chain, const juce::var& v);
};

} // namespace model
