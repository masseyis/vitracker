#include "Chain.h"

namespace model {

Chain::Chain() : Chain("Untitled") {}

Chain::Chain(const std::string& name) : name_(name) {}

void Chain::addPattern(int patternIndex)
{
    patternIndices_.push_back(patternIndex);
}

void Chain::removePattern(int position)
{
    if (position >= 0 && position < static_cast<int>(patternIndices_.size()))
    {
        patternIndices_.erase(patternIndices_.begin() + position);
    }
}

void Chain::setPattern(int position, int patternIndex)
{
    if (position >= 0 && position < static_cast<int>(patternIndices_.size()))
    {
        patternIndices_[position] = patternIndex;
    }
}

} // namespace model
