#include "../../include/enjin2/ui/system.hpp"

namespace enjin2 {

// Initialize static system ID counter
SystemID SystemBase::nextSystemID = 1;

// EntityManager is now a header-only template (see ui/system.hpp): its member
// functions are defined inline so the entity-id space can be sized per World.

} // namespace enjin2