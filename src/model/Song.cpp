#include "Song.h"
#include <algorithm>

namespace model {

Song::Song()
{
    clear();
}

const std::vector<int>& Song::getTrack(int trackIndex) const
{
    return tracks_[trackIndex];
}

void Song::setChain(int trackIndex, int position, int chainIndex)
{
    if (trackIndex < 0 || trackIndex >= NUM_TRACKS) return;

    auto& track = tracks_[trackIndex];
    while (static_cast<int>(track.size()) <= position)
    {
        track.push_back(-1);
    }
    track[position] = chainIndex;
}

void Song::addChain(int trackIndex, int chainIndex)
{
    if (trackIndex >= 0 && trackIndex < NUM_TRACKS)
    {
        tracks_[trackIndex].push_back(chainIndex);
    }
}

void Song::removeChain(int trackIndex, int position)
{
    if (trackIndex < 0 || trackIndex >= NUM_TRACKS) return;

    auto& track = tracks_[trackIndex];
    if (position >= 0 && position < static_cast<int>(track.size()))
    {
        track.erase(track.begin() + position);
    }
}

int Song::getLength() const
{
    int maxLen = 0;
    for (const auto& track : tracks_)
    {
        maxLen = std::max(maxLen, static_cast<int>(track.size()));
    }
    return maxLen;
}

void Song::setLength(int length)
{
    if (length < 1) length = 1;

    for (auto& track : tracks_)
    {
        if (static_cast<int>(track.size()) < length)
        {
            // Extend with empty cells
            track.resize(static_cast<size_t>(length), -1);
        }
        else if (static_cast<int>(track.size()) > length)
        {
            // Truncate
            track.resize(static_cast<size_t>(length));
        }
    }
}

void Song::clear()
{
    for (auto& track : tracks_)
    {
        track.clear();
    }
}

} // namespace model
