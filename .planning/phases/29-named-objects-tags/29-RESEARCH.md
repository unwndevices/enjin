# Phase 29: Named Objects + Tags - Research

**Researched:** 2026-02-27
**Domain:** C++ embedded-safe string identity, fixed-size tag arrays, linear-scan object lookup
**Confidence:** HIGH

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| OBJ-01 | User can assign a name to an Object at construction or via setter | Object class in `object.hpp` has no name field today; add `const char* _name = nullptr` + `setName()` + `getName()` — all inline, zero heap |
| OBJ-02 | User can find an Object by name via `ObjectCollection::findByName()` with O(n) linear scan | `ObjectCollection` in `object_collection.hpp` already has `forEach()` and `findObject<T>()` patterns; add `findByName(const char*)` using `strcmp` from `<cstring>` (already in `types.hpp`) |
| OBJ-03 | User can add up to 8 tags (string literal pointers) to an Object with zero allocation | `std::array<const char*, 8>` in `Object` is the correct pattern; zero heap, matches embedded constraint; project uses `std::array` extensively (see `object.hpp` lines 41, 49) |
| OBJ-04 | User can find all Objects with a given tag via `ObjectCollection::findAllWithTag()` | Pattern already exists: `findObjects(T** results, size_t maxResults)` at `object_collection.hpp:169` — use identical caller-provides-buffer signature |
</phase_requirements>

---

## Summary

Phase 29 adds name and tag identity to the `Object` class and corresponding lookup methods to `ObjectCollection`. The implementation is entirely in-tree C++17 with zero new dependencies. The core value constraint — zero dynamic allocation — drives every design decision: names are stored as raw `const char*` pointers (string literals owned by the caller, not the engine), and tags are an `std::array<const char*, 8>` with a `uint8_t tagCount` counter, both stored directly in `Object`.

The lookup methods are added to `ObjectCollection` as non-template functions. `findByName(const char*)` does a linear O(n) scan using `strcmp` (already available via `<cstring>` which `types.hpp` includes). `findAllWithTag(const char*, Object** results, size_t maxResults)` follows the existing `findObjects()` pattern: the caller provides the output buffer, the function returns the count found. This matches the already-established zero-heap output API for the collection.

No changes are required to `ObjectCollection`'s storage, CMake targets, or any other subsystem. All new code lives in `object.hpp` (new fields + inline methods) and `object_collection.hpp` (two new methods). A dedicated test file `tests/named_objects_test.cpp` is needed and must be registered in `tests/CMakeLists.txt`.

**Primary recommendation:** Add `const char* _name` and `std::array<const char*, MAX_TAGS>` directly to `Object`; add `findByName` and `findAllWithTag` to `ObjectCollection` using `strcmp`; follow the existing `findObjects()` caller-provides-buffer pattern for multi-result lookup.

---

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| C++17 | project standard | `std::array`, `constexpr`, `if constexpr` | `CMAKE_CXX_STANDARD 17` confirmed in `CMakeLists.txt` |
| `<cstring>` | libc | `strcmp` for name/tag comparison | Already included via `types.hpp`; available on ESP32, Emscripten, and desktop |

### Supporting

None. This phase introduces no new dependencies.

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `const char*` name field | `char name[32]` fixed buffer | Fixed buffer allows mutation without external ownership but costs 32 bytes per object (128 objects × 32 bytes = 4 KB); `const char*` costs 4–8 bytes per object and is correct for string literals |
| `const char*` name field | `std::string` | `std::string` causes heap allocation — explicitly ruled out by project core value |
| `strcmp` for tag matching | Pointer identity (`tag == stored_tag`) | Pointer equality only works if callers always use the identical string literal address; `strcmp` is safer and the project already uses it for palette name lookup in `palette.cpp:51` |
| Caller-provides-buffer for `findAllWithTag` | Returning `std::vector` | `std::vector` causes heap allocation — ruled out |

---

## Architecture Patterns

### Files to Modify

```
include/enjin2/core/
├── object.hpp           — add _name field, MAX_TAGS, _tags array, _tagCount; add setName/getName/addTag/hasTag/clearTags
└── object_collection.hpp — add findByName(), findAllWithTag()

tests/
├── named_objects_test.cpp  — new test executable (Wave 0 gap)
└── CMakeLists.txt          — register named_objects_test
```

No `.cpp` changes needed: all new Object methods are inline (matching the existing pattern where `isActive()`, `setActive()`, `getComponentCount()`, etc. are all inline in `object.hpp`). All new `ObjectCollection` methods are non-template and can also be inline in the header (matching the existing inline `update()`, `forEach()`, etc.).

### Pattern 1: Object Name Field

**What:** Add `const char* _name` to `Object` private section, initialized to `nullptr`. Add `setName(const char*)` and `getName()` as inline public methods.

**When to use:** Caller stores a string literal. Engine stores only the pointer. No copy, no allocation.

**Example:**
```cpp
// In object.hpp private section (after existing fields)
const char* _name = nullptr;

// In object.hpp public section
void setName(const char* name) { _name = name; }
const char* getName() const { return _name; }
```

Construction-time naming is achieved by calling `setName()` immediately after `addObject<T>()`, since `Object` currently has no parameterized constructor for identity. Alternatively, add an optional `const char* name = nullptr` parameter to `Object()` constructor — but this is NOT required by OBJ-01 which says "at construction OR via setter." The setter-only approach is simpler and avoids touching the Object constructor chain.

**Note on naming:** The existing codebase uses both leading-underscore (`_name`, `_accumSec`, `_fps` in `sprite.hpp`) and bare names (`position`, `enabled`, `active` in `object.hpp`/`component.hpp`). For new private fields in `Object`, follow the bare-name convention used in the same file (no leading underscore), consistent with `componentCount`, `awoken`, `started`, `active`, `position`, `drawables`, `drawableCount`, `queued_for_removal`.

### Pattern 2: Tag Array

**What:** Add a fixed-size array of `const char*` tag pointers to `Object`. Tags are string literals; the engine stores only the pointer. Capacity is 8 (per OBJ-03 success criteria).

**Example:**
```cpp
// In object.hpp private section
static constexpr size_t MAX_TAGS = 8;
std::array<const char*, MAX_TAGS> tags;
size_t tagCount;

// Constructor initialization
Object() : componentCount(0), awoken(false), started(false), active(false),
           position(nullptr), drawableCount(0), queued_for_removal(false),
           tagCount(0) {
    // existing component/drawable init...
    tags.fill(nullptr);
    // ...
}

// Public methods
bool addTag(const char* tag) {
    if (tagCount >= MAX_TAGS) return false;
    tags[tagCount++] = tag;
    return true;
}

bool hasTag(const char* tag) const {
    for (size_t i = 0; i < tagCount; ++i) {
        if (tags[i] && strcmp(tags[i], tag) == 0) return true;
    }
    return false;
}

void clearTags() {
    tags.fill(nullptr);
    tagCount = 0;
}

size_t getTagCount() const { return tagCount; }
```

**When to use:** Tag is a string literal known at compile time. Pass `"enemy"`, `"player"`, `"collidable"`.

### Pattern 3: ObjectCollection::findByName

**What:** Linear scan through all objects, return first matching by name. Returns `nullptr` when not found.

**Example:**
```cpp
// In object_collection.hpp public section
Object* findByName(const char* name) {
    if (!name) return nullptr;
    for (size_t i = 0; i < objectCount; ++i) {
        if (objects[i] && objects[i]->getName() &&
            strcmp(objects[i]->getName(), name) == 0) {
            return objects[i].get();
        }
    }
    return nullptr;
}
```

**Why not template:** Name-based lookup is not type-parameterized. `findObject<T>()` is templated because it dispatches by type; `findByName` dispatches by string value.

### Pattern 4: ObjectCollection::findAllWithTag

**What:** Caller-provides-buffer multi-result lookup. Follows the exact signature of the existing `findObjects()` method. Returns count of matches written into the caller's array.

**Example:**
```cpp
// In object_collection.hpp public section
size_t findAllWithTag(const char* tag, Object** results, size_t maxResults) {
    if (!tag || !results || maxResults == 0) return 0;
    size_t found = 0;
    for (size_t i = 0; i < objectCount && found < maxResults; ++i) {
        if (objects[i] && objects[i]->hasTag(tag)) {
            results[found++] = objects[i].get();
        }
    }
    return found;
}
```

**Caller usage:**
```cpp
Object* enemies[32];
size_t count = objects.findAllWithTag("enemy", enemies, 32);
for (size_t i = 0; i < count; ++i) {
    enemies[i]->setActive(false);
}
```

This is zero-heap on both the engine side and the caller side (caller uses stack array).

### Pattern 5: Exposing findByName/findAllWithTag via Scene

**What:** `Scene` already proxies `findObject<T>()` and `findObjectWithComponent<T>()` via `ObjectCollection`. Add corresponding wrappers for the new methods.

**Example (scene.hpp):**
```cpp
Object* findByName(const char* name) {
    return objects.findByName(name);
}

size_t findAllWithTag(const char* tag, Object** results, size_t maxResults) {
    return objects.findAllWithTag(tag, results, maxResults);
}
```

This is consistent with existing `Scene::findObject()` and `Scene::findObjectWithComponent()` which are one-line wrappers at `scene.hpp:152-167`.

### Pattern 6: Test Structure

**What:** Follow the existing hand-rolled test pattern used in `input_test.cpp` — no external test framework, ASSERT macro, `int main()` with pass/fail counters, exit code 1 on failure.

**Example:**
```cpp
// tests/named_objects_test.cpp
#include <enjin2/core/object_collection.hpp>
#include <cstdio>
using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; } \
        else { printf("PASS: %s\n", msg); passes++; } \
    } while(0)

// Tests for OBJ-01: setName/getName
// Tests for OBJ-02: findByName returns correct object or nullptr
// Tests for OBJ-03: 8-tag capacity, addTag returns false at capacity
// Tests for OBJ-04: findAllWithTag returns all matching, count correct

int main() { ... return (failures == 0) ? 0 : 1; }
```

### Anti-Patterns to Avoid

- **Copying the name string:** `strdup()`, `strcpy()` into a buffer, or `std::string` — all allocate heap. Store only the `const char*` pointer.
- **Pointer-equality for tag matching:** `tags[i] == tag` works only when the same interned literal is used at every call site. `strcmp` is safer and is the established project pattern (see `palette.cpp:51`).
- **Returning `std::vector<Object*>` from findAllWithTag:** Heap allocation. Use the caller-provides-buffer pattern.
- **Making findByName a template:** Not needed — name lookup is not type-dispatched.
- **Adding name/tags to the `Component` class:** OBJ-01 through OBJ-04 are all scoped to `Object`.
- **Forgetting to initialize `tags.fill(nullptr)` and `tagCount = 0` in Object constructor:** Uninitialized array pointers will cause `strcmp` crashes when scanning tags.
- **Forgetting null-guard on `getName()` before `strcmp`:** An object with `_name == nullptr` passed to `strcmp` is undefined behavior. Always null-check before comparing.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| String comparison | Custom character loop | `strcmp` from `<cstring>` | `<cstring>` already in `types.hpp`; `strcmp` handles null termination, edge cases, and is used elsewhere in the project |
| Multi-result collection | `std::vector` output | Caller-provides-buffer (`Object** results, size_t maxResults`) | Already the project pattern in `findObjects()`; zero allocation |
| Name interning/hashing | Custom hash table | Raw `const char*` + `strcmp` | O(n) with 128 max objects is fast enough; hash adds complexity with no measurable benefit at this scale |

**Key insight:** With `MAX_OBJECTS = 128`, an O(n) linear scan through `findByName` touches at most 128 comparisons per call. At 60 fps this is negligible. No hash map or sorted index is warranted.

---

## Common Pitfalls

### Pitfall 1: Null Name Pointer Not Guarded in `strcmp`

**What goes wrong:** `strcmp(objects[i]->getName(), name)` crashes with SIGSEGV or undefined behavior if `getName()` returns `nullptr` (i.e., the object has no name set).

**Why it happens:** `Object::_name` starts as `nullptr`. Unguarded `strcmp(nullptr, "player")` is undefined behavior in C.

**How to avoid:** Guard both sides: `if (objects[i]->getName() && strcmp(objects[i]->getName(), name) == 0)`. See Pattern 3 above.

**Warning signs:** Crash on first call to `findByName` when any object in the collection has not been given a name.

### Pitfall 2: Tag Overflow Silently Discarded

**What goes wrong:** `addTag()` is called more than 8 times on an object. The 9th call silently returns `false` and the tag is not stored. The caller may not notice.

**Why it happens:** Fixed-capacity array with a counter.

**How to avoid:** `addTag()` returns `bool` — callers should check the return value in debug scenarios. The success criteria says "up to 8 tags" so this is expected behavior, but must be explicitly tested: verify `addTag()` returns `false` on the 9th call and that the 9th tag is not findable.

**Warning signs:** `findAllWithTag()` does not find an object that was tagged — the tag was silently dropped.

### Pitfall 3: Name Pointer Lifetime (String Literal Assumption Violated)

**What goes wrong:** A caller passes a pointer to a stack-allocated `char` buffer (e.g., `char buf[32]; snprintf(buf, 32, "player%d", i); obj->setName(buf)`). The engine stores the dangling pointer. `findByName` reads freed stack memory.

**Why it happens:** The API stores only the pointer, not the string content. This is correct for string literals (static lifetime) but dangerous for temporary buffers.

**How to avoid:** Document clearly that `setName()` and `addTag()` store a raw pointer and require the caller to guarantee the string's lifetime exceeds the object's lifetime. String literals are the intended use case. Do not attempt to sanitize this in the engine — it would require heap allocation.

**Warning signs:** `findByName` returns the wrong object or crashes after an object's name was set from a local buffer.

### Pitfall 4: `Object()` Constructor Not Initializing New Fields

**What goes wrong:** `Object::Object()` in `object.cpp` explicitly initializes `componentCount`, `awoken`, `started`, `active`, `position`, `drawableCount`. If `tagCount` and `tags` array are not explicitly initialized in the constructor, they hold garbage values. `findAllWithTag` reads garbage pointers.

**Why it happens:** `object.cpp` has a manual member initializer list. Adding new fields to `object.hpp` does not automatically initialize them.

**How to avoid:** Add `tagCount(0)` to the member initializer list in `Object::Object()` in `object.cpp`. Call `tags.fill(nullptr)` in the constructor body alongside the existing `drawables.fill(nullptr)` call. Also add `name(nullptr)` to the initializer list.

**Warning signs:** `hasTag()` crashes or returns `true` spuriously on newly constructed objects.

### Pitfall 5: Object Constructor in object.hpp vs. object.cpp

**What goes wrong:** `Object()` is declared in `object.hpp` but defined in `object.cpp`. Adding new members to `object.hpp` means the constructor definition in `object.cpp` must also be updated. Missing this results in uninitialized members.

**Why it happens:** The project splits Object declaration (`.hpp`) from definition (`.cpp`) — unlike many smaller engine classes that are entirely header-defined. A planner looking only at `object.hpp` might write the field additions but miss the constructor body in `object.cpp`.

**How to avoid:** Plan two separate edits: one to `object.hpp` (field declarations + inline methods) and one to `object.cpp` (constructor initializer list + `tags.fill(nullptr)`).

---

## Code Examples

Verified patterns from direct codebase inspection:

### Existing `strcmp` usage in the project (palette.cpp:51)

```cpp
// Source: src/graphics/palette.cpp — confirms strcmp is the project pattern
bool Palette::loadPreset(const char* name)
{
    for (auto& p : PRESETS) {
        if (strcmp(name, p.name) == 0) {
            // ...
        }
    }
}
```

### Existing caller-provides-buffer pattern (object_collection.hpp:169)

```cpp
// Source: include/enjin2/core/object_collection.hpp lines 168-179
template<typename T>
size_t findObjects(T** results, size_t maxResults) {
    static_assert(std::is_base_of<Object, T>::value, "T must derive from Object");
    size_t found = 0;
    for (size_t i = 0; i < objectCount && found < maxResults; ++i) {
        if (auto obj = dynamic_cast<T*>(objects[i].get())) {
            results[found++] = obj;
        }
    }
    return found;
}
```

`findAllWithTag` mirrors this signature exactly (non-template, Object* results instead of T*).

### Existing inline method pattern (object.hpp:242-248)

```cpp
// Source: include/enjin2/core/object.hpp lines 242-248
bool isActive() const { return active; }
void setActive(bool isActive) { active = isActive; }
size_t getComponentCount() const { return componentCount; }
```

New `setName`/`getName`/`addTag`/`hasTag`/`clearTags`/`getTagCount` follow this inline pattern.

### Existing Scene proxy pattern (scene.hpp:152-167)

```cpp
// Source: include/enjin2/core/scene.hpp lines 152-167
template<typename T>
T* findObject() {
    return objects.findObject<T>();
}

template<typename T>
Object* findObjectWithComponent() {
    return objects.findObjectWithComponent<T>();
}
```

New `Scene::findByName` and `Scene::findAllWithTag` are identical one-liner wrappers calling `objects.findByName()` and `objects.findAllWithTag()`.

### Existing test pattern (input_test.cpp)

```cpp
// Source: tests/input_test.cpp
static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; } \
        else { printf("PASS: %s\n", msg); passes++; } \
    } while(0)

int main() {
    // ...
    return (failures == 0) ? 0 : 1;
}
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| No name/tag on Object | Add `const char* name`, `std::array<const char*, 8> tags` | Phase 29 | Objects become identifiable; scripts can locate them by name/tag in later phases |

No deprecated patterns are involved. This is purely additive.

---

## Open Questions

1. **Should Object accept name at constructor time (OBJ-01 says "at construction or via setter")?**
   - What we know: `Object()` in `object.cpp` takes no arguments. `addObject<T>(args...)` forwards args to T's constructor.
   - What's unclear: Whether adding `Object(const char* name = nullptr)` is desired or if setter-only is sufficient.
   - Recommendation: Setter-only is simpler and fully satisfies OBJ-01. If derived-class construction naming is needed later, it can be added without breaking changes. Do NOT add constructor parameter in Phase 29 — keep the scope minimal.

2. **Should `findByName` and `findAllWithTag` search only active objects?**
   - What we know: `findObject<T>()` searches all objects (including inactive); `forEachActive()` exists for active-only iteration.
   - What's unclear: Whether OBJ-02 and OBJ-04 require active-only or all-objects semantics.
   - Recommendation: Search all objects (including inactive), consistent with `findObject<T>()` behavior. Add a note in the docstring. If active-only is needed for a specific use case, the caller can filter on `isActive()`.

3. **Should `addTag` check for duplicate tags?**
   - What we know: The success criteria says "up to 8 tag pointers" with no mention of uniqueness enforcement.
   - What's unclear: Whether adding the same tag twice should be silently allowed (uses 2 slots) or deduplicated.
   - Recommendation: Do NOT deduplicate — deduplication requires an O(n) scan on every `addTag` and adds complexity. The requirement says "up to 8 string literal tag pointers" not "up to 8 unique tags." Document the behavior; callers are responsible for not double-tagging.

---

## Validation Architecture

`workflow.nyquist_validation` is not present in `.planning/config.json` — this section is omitted per instructions.

---

## Sources

### Primary (HIGH confidence)

- Live codebase inspection (2026-02-27):
  - `include/enjin2/core/object.hpp` — full class definition, private fields, existing inline patterns confirmed
  - `include/enjin2/core/object_collection.hpp` — `findObjects()` caller-provides-buffer pattern, `forEach()`, `MAX_OBJECTS = 128` confirmed
  - `include/enjin2/core/scene.hpp` — `findObject<T>()` and `findObjectWithComponent<T>()` proxy patterns confirmed
  - `include/enjin2/core/types.hpp` — `#include <cstring>` confirmed (makes `strcmp` available)
  - `src/core/object.cpp` — constructor body with `drawables.fill(nullptr)` confirmed; initializer list pattern confirmed
  - `src/graphics/palette.cpp:51` — `strcmp(name, p.name)` confirmed as project string comparison pattern
  - `tests/input_test.cpp` — ASSERT macro + pass/fail counter test structure confirmed
  - `tests/CMakeLists.txt` — `add_executable` + `add_test` pattern for new test registration confirmed
  - `CMakeLists.txt` — `enjin2_core` target confirmed; `ENJIN2_BUILD_TESTS` option and `add_subdirectory(tests)` confirmed

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — no new dependencies; C++17 and `<cstring>` confirmed from project files
- Architecture: HIGH — all patterns derived from direct codebase inspection; no speculation
- Pitfalls: HIGH — all pitfalls traced to specific existing code patterns and field initialization behavior

**Research date:** 2026-02-27
**Valid until:** 90 days (stable C++ codebase, no fast-moving dependencies)
