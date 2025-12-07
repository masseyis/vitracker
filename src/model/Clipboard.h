#pragma once

#include "Step.h"
#include <vector>
#include <algorithm>

namespace model {

struct Selection
{
    int startTrack = 0;
    int startRow = 0;
    int startColumn = 0;
    int endTrack = 0;
    int endRow = 0;
    int endColumn = 0;

    bool isValid() const { return startTrack >= 0 && startRow >= 0; }

    int minTrack() const { return std::min(startTrack, endTrack); }
    int maxTrack() const { return std::max(startTrack, endTrack); }
    int minRow() const { return std::min(startRow, endRow); }
    int maxRow() const { return std::max(startRow, endRow); }
    int minColumn() const { return std::min(startColumn, endColumn); }
    int maxColumn() const { return std::max(startColumn, endColumn); }

    int width() const { return maxTrack() - minTrack() + 1; }
    int height() const { return maxRow() - minRow() + 1; }
};

class Clipboard
{
public:
    static Clipboard& instance() {
        static Clipboard inst;
        return inst;
    }

    void copy(const std::vector<std::vector<Step>>& data) {
        data_ = data;
    }

    const std::vector<std::vector<Step>>& getData() const { return data_; }
    bool isEmpty() const { return data_.empty(); }

    int getWidth() const { return data_.empty() ? 0 : static_cast<int>(data_.size()); }
    int getHeight() const { return data_.empty() || data_[0].empty() ? 0 : static_cast<int>(data_[0].size()); }

private:
    Clipboard() = default;
    std::vector<std::vector<Step>> data_;  // [track][row]
};

} // namespace model
