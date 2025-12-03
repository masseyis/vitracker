#include "Chain.h"

namespace model {

const ChainEntry Chain::emptyEntry_;

Chain::Chain() : Chain("Untitled") {}

Chain::Chain(const std::string& name) : name_(name) {}

std::vector<int> Chain::getPatternIndices() const
{
    std::vector<int> indices;
    indices.reserve(entries_.size());
    for (const auto& entry : entries_)
    {
        indices.push_back(entry.patternIndex);
    }
    return indices;
}

void Chain::addPattern(int patternIndex)
{
    ChainEntry entry;
    entry.patternIndex = patternIndex;
    entry.transpose = 0;
    entries_.push_back(entry);
}

void Chain::removePattern(int position)
{
    if (position >= 0 && position < static_cast<int>(entries_.size()))
    {
        entries_.erase(entries_.begin() + position);
    }
}

void Chain::setPattern(int position, int patternIndex)
{
    if (position >= 0 && position < static_cast<int>(entries_.size()))
    {
        entries_[static_cast<size_t>(position)].patternIndex = patternIndex;
    }
}

const ChainEntry& Chain::getEntry(int position) const
{
    if (position >= 0 && position < static_cast<int>(entries_.size()))
    {
        return entries_[static_cast<size_t>(position)];
    }
    return emptyEntry_;
}

void Chain::setTranspose(int position, int transpose)
{
    if (position >= 0 && position < static_cast<int>(entries_.size()))
    {
        entries_[static_cast<size_t>(position)].transpose = transpose;
    }
}

int Chain::getTranspose(int position) const
{
    if (position >= 0 && position < static_cast<int>(entries_.size()))
    {
        return entries_[static_cast<size_t>(position)].transpose;
    }
    return 0;
}

} // namespace model
