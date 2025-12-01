#pragma once

#include "Step.h"
#include <string>
#include <vector>
#include <array>

namespace model {

class Pattern
{
public:
    static constexpr int NUM_TRACKS = 16;
    static constexpr int MAX_LENGTH = 128;
    static constexpr int DEFAULT_LENGTH = 16;

    Pattern();
    explicit Pattern(const std::string& name);

    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

    int getLength() const { return length_; }
    void setLength(int length);

    Step& getStep(int track, int row);
    const Step& getStep(int track, int row) const;

    void clear();

private:
    std::string name_;
    int length_ = DEFAULT_LENGTH;
    std::array<std::vector<Step>, NUM_TRACKS> tracks_;
};

} // namespace model
