#include "Project.h"

namespace model {

Project::MixerState::MixerState()
{
    trackVolumes.fill(1.0f);
    trackPans.fill(0.0f);
    trackMutes.fill(false);
    trackSolos.fill(false);
    busLevels.fill(0.5f);
}

Project::Project() : Project("Untitled") {}

Project::Project(const std::string& name) : name_(name)
{
    // Create one default instrument, pattern, and chain
    addInstrument("Init");
    addPattern("Pattern 1");
    addChain("Chain 1");
}

int Project::addInstrument(const std::string& name)
{
    if (instruments_.size() >= MAX_INSTRUMENTS) return -1;
    instruments_.push_back(std::make_unique<Instrument>(name));
    return static_cast<int>(instruments_.size()) - 1;
}

Instrument* Project::getInstrument(int index)
{
    if (index < 0 || index >= static_cast<int>(instruments_.size())) return nullptr;
    return instruments_[index].get();
}

const Instrument* Project::getInstrument(int index) const
{
    if (index < 0 || index >= static_cast<int>(instruments_.size())) return nullptr;
    return instruments_[index].get();
}

int Project::addPattern(const std::string& name)
{
    if (patterns_.size() >= MAX_PATTERNS) return -1;
    patterns_.push_back(std::make_unique<Pattern>(name));
    return static_cast<int>(patterns_.size()) - 1;
}

Pattern* Project::getPattern(int index)
{
    if (index < 0 || index >= static_cast<int>(patterns_.size())) return nullptr;
    return patterns_[index].get();
}

const Pattern* Project::getPattern(int index) const
{
    if (index < 0 || index >= static_cast<int>(patterns_.size())) return nullptr;
    return patterns_[index].get();
}

int Project::addChain(const std::string& name)
{
    if (chains_.size() >= MAX_CHAINS) return -1;
    chains_.push_back(std::make_unique<Chain>(name));
    return static_cast<int>(chains_.size()) - 1;
}

Chain* Project::getChain(int index)
{
    if (index < 0 || index >= static_cast<int>(chains_.size())) return nullptr;
    return chains_[index].get();
}

const Chain* Project::getChain(int index) const
{
    if (index < 0 || index >= static_cast<int>(chains_.size())) return nullptr;
    return chains_[index].get();
}

} // namespace model
