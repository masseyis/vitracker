#include "Instrument.h"

namespace model {

Instrument::Instrument() : Instrument("Untitled") {}

Instrument::Instrument(const std::string& name) : name_(name) {}

} // namespace model
