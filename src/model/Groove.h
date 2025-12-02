#pragma once

#include <array>
#include <string>
#include <vector>

namespace model {

struct GrooveTemplate
{
    std::string name;
    std::array<float, 16> timings;  // Timing offset for each of 16 steps (-0.5 to 0.5)

    static GrooveTemplate straight() {
        GrooveTemplate g;
        g.name = "Straight";
        g.timings.fill(0.0f);
        return g;
    }

    static GrooveTemplate swing50() {
        GrooveTemplate g;
        g.name = "Swing 50%";
        for (int i = 0; i < 16; ++i)
            g.timings[static_cast<size_t>(i)] = (i % 2 == 1) ? 0.167f : 0.0f;  // Delay odd steps
        return g;
    }

    static GrooveTemplate swing66() {
        GrooveTemplate g;
        g.name = "Swing 66%";
        for (int i = 0; i < 16; ++i)
            g.timings[static_cast<size_t>(i)] = (i % 2 == 1) ? 0.33f : 0.0f;
        return g;
    }

    static GrooveTemplate mpc60() {
        GrooveTemplate g;
        g.name = "MPC 60%";
        for (int i = 0; i < 16; ++i)
            g.timings[static_cast<size_t>(i)] = (i % 2 == 1) ? 0.2f : 0.0f;
        return g;
    }

    static GrooveTemplate humanize() {
        GrooveTemplate g;
        g.name = "Humanize";
        // Random-ish but consistent pattern
        float offsets[] = {0, 0.02f, -0.01f, 0.03f, 0, -0.02f, 0.01f, 0.02f,
                          -0.01f, 0.02f, 0, 0.01f, -0.02f, 0.03f, 0.01f, -0.01f};
        for (int i = 0; i < 16; ++i)
            g.timings[static_cast<size_t>(i)] = offsets[i];
        return g;
    }
};

class GrooveManager
{
public:
    GrooveManager() {
        templates_.push_back(GrooveTemplate::straight());
        templates_.push_back(GrooveTemplate::swing50());
        templates_.push_back(GrooveTemplate::swing66());
        templates_.push_back(GrooveTemplate::mpc60());
        templates_.push_back(GrooveTemplate::humanize());
    }

    const std::vector<GrooveTemplate>& getTemplates() const { return templates_; }

    const GrooveTemplate& getTemplate(int index) const {
        if (index >= 0 && index < static_cast<int>(templates_.size()))
            return templates_[static_cast<size_t>(index)];
        return templates_[0];
    }

    int getTemplateCount() const { return static_cast<int>(templates_.size()); }

private:
    std::vector<GrooveTemplate> templates_;
};

} // namespace model
