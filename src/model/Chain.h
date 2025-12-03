#pragma once

#include <string>
#include <vector>

namespace model {

// Each entry in a chain: pattern index + transpose (in scale degrees)
struct ChainEntry
{
    int patternIndex = -1;
    int transpose = 0;  // Scale degrees to transpose (e.g., +2 = up 2 scale degrees)
};

class Chain
{
public:
    Chain();
    explicit Chain(const std::string& name);

    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

    const std::string& getScaleLock() const { return scaleLock_; }
    void setScaleLock(const std::string& scale) { scaleLock_ = scale; }

    // Legacy accessors for pattern indices only
    std::vector<int> getPatternIndices() const;
    void addPattern(int patternIndex);
    void removePattern(int position);
    void setPattern(int position, int patternIndex);
    int getPatternCount() const { return static_cast<int>(entries_.size()); }

    // New accessors for full entry data
    const std::vector<ChainEntry>& getEntries() const { return entries_; }
    const ChainEntry& getEntry(int position) const;
    void setTranspose(int position, int transpose);
    int getTranspose(int position) const;

private:
    std::string name_;
    std::string scaleLock_;  // e.g., "C minor", empty = no lock
    std::vector<ChainEntry> entries_;

    static const ChainEntry emptyEntry_;
};

} // namespace model
