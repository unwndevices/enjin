#pragma once

// The three faces every host has (ADR-0029): monogram (the default), body and
// display. One copy of each face's data: the tables are inline variables.

#include "enjin2/graphics/faces/body.h"
#include "enjin2/graphics/faces/display.h"
#include "enjin2/graphics/faces/monogram.h"

#include <cstring>

namespace enjin2::faces {

/// Every face, the default first.
inline const Face* const all[] = {&monogram, &body, &display};
inline constexpr int count = 3;
/// Their names, in the same order (Lua's Font enum).
inline constexpr const char* kNames[] = {"monogram", "body", "display"};

/// The face named @p name, or nullptr.
inline const Face* find(const char* name) {
    if (!name) return nullptr;
    for (const Face* f : all)
        if (std::strcmp(f->name, name) == 0) return f;
    return nullptr;
}

}  // namespace enjin2::faces
