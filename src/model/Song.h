#pragma once

#include <array>
#include <vector>

namespace model {

class Song
{
public:
    static constexpr int NUM_TRACKS = 16;

    Song();

    // Each track is a sequence of chain indices (-1 = empty)
    const std::vector<int>& getTrack(int trackIndex) const;
    void setChain(int trackIndex, int position, int chainIndex);
    void addChain(int trackIndex, int chainIndex);
    void removeChain(int trackIndex, int position);

    int getLength() const;  // Length of longest track

    void clear();

private:
    std::array<std::vector<int>, NUM_TRACKS> tracks_;
};

} // namespace model
