#pragma once

#include <string>
#include <array>

namespace model {

struct PlaitsParams
{
    int engine = 0;           // 0-15 engine type
    float harmonics = 0.5f;   // 0.0-1.0
    float timbre = 0.5f;      // 0.0-1.0
    float morph = 0.5f;       // 0.0-1.0
    float attack = 0.0f;      // 0.0-1.0
    float decay = 0.5f;       // 0.0-1.0
    float lpgColour = 0.5f;   // 0.0-1.0
};

struct SendLevels
{
    float reverb = 0.0f;
    float delay = 0.0f;
    float chorus = 0.0f;
    float drive = 0.0f;
    float sidechainDuck = 0.0f;
};

class Instrument
{
public:
    Instrument();
    explicit Instrument(const std::string& name);

    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

    PlaitsParams& getParams() { return params_; }
    const PlaitsParams& getParams() const { return params_; }

    SendLevels& getSends() { return sends_; }
    const SendLevels& getSends() const { return sends_; }

private:
    std::string name_;
    PlaitsParams params_;
    SendLevels sends_;
};

} // namespace model
