#include "Pattern.h"
#include <algorithm>

namespace model {

Pattern::Pattern() : Pattern("Untitled") {}

Pattern::Pattern(const std::string& name) : name_(name)
{
    for (auto& track : tracks_)
    {
        track.resize(MAX_LENGTH);
    }
}

void Pattern::setLength(int length)
{
    length_ = std::clamp(length, 1, MAX_LENGTH);
}

Step& Pattern::getStep(int track, int row)
{
    return tracks_[track][row];
}

const Step& Pattern::getStep(int track, int row) const
{
    return tracks_[track][row];
}

void Pattern::clear()
{
    for (auto& track : tracks_)
    {
        for (auto& step : track)
        {
            step.clear();
        }
    }
}

} // namespace model
