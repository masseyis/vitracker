#pragma once

#include <string>
#include <vector>

namespace model {

class Chain
{
public:
    Chain();
    explicit Chain(const std::string& name);

    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

    const std::string& getScaleLock() const { return scaleLock_; }
    void setScaleLock(const std::string& scale) { scaleLock_ = scale; }

    const std::vector<int>& getPatternIndices() const { return patternIndices_; }
    void addPattern(int patternIndex);
    void removePattern(int position);
    void setPattern(int position, int patternIndex);
    int getPatternCount() const { return static_cast<int>(patternIndices_.size()); }

private:
    std::string name_;
    std::string scaleLock_;  // e.g., "C minor", empty = no lock
    std::vector<int> patternIndices_;
};

} // namespace model
