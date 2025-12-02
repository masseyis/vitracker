#pragma once

#include <functional>
#include <vector>
#include <memory>
#include <string>

namespace model {

class Action
{
public:
    virtual ~Action() = default;
    virtual void undo() = 0;
    virtual void redo() = 0;
    virtual std::string getDescription() const = 0;
};

template<typename T>
class ValueAction : public Action
{
public:
    ValueAction(T* target, T oldValue, T newValue, const std::string& desc)
        : target_(target), oldValue_(oldValue), newValue_(newValue), description_(desc) {}

    void undo() override { *target_ = oldValue_; }
    void redo() override { *target_ = newValue_; }
    std::string getDescription() const override { return description_; }

private:
    T* target_;
    T oldValue_;
    T newValue_;
    std::string description_;
};

class UndoManager
{
public:
    static UndoManager& instance() {
        static UndoManager inst;
        return inst;
    }

    void push(std::unique_ptr<Action> action) {
        // Clear redo stack
        while (currentIndex_ < static_cast<int>(actions_.size()) - 1)
            actions_.pop_back();

        actions_.push_back(std::move(action));
        currentIndex_ = static_cast<int>(actions_.size()) - 1;

        // Limit history
        while (actions_.size() > MAX_HISTORY)
        {
            actions_.erase(actions_.begin());
            currentIndex_--;
        }
    }

    bool canUndo() const { return currentIndex_ >= 0; }
    bool canRedo() const { return currentIndex_ < static_cast<int>(actions_.size()) - 1; }

    void undo() {
        if (canUndo())
        {
            actions_[static_cast<size_t>(currentIndex_)]->undo();
            currentIndex_--;
            if (onStateChanged) onStateChanged();
        }
    }

    void redo() {
        if (canRedo())
        {
            currentIndex_++;
            actions_[static_cast<size_t>(currentIndex_)]->redo();
            if (onStateChanged) onStateChanged();
        }
    }

    void clear() {
        actions_.clear();
        currentIndex_ = -1;
    }

    std::function<void()> onStateChanged;

private:
    UndoManager() = default;

    std::vector<std::unique_ptr<Action>> actions_;
    int currentIndex_ = -1;
    static constexpr size_t MAX_HISTORY = 100;
};

// Helper to create value actions
template<typename T>
void recordChange(T* target, T newValue, const std::string& desc) {
    T oldValue = *target;
    *target = newValue;
    UndoManager::instance().push(
        std::make_unique<ValueAction<T>>(target, oldValue, newValue, desc)
    );
}

} // namespace model
