/**
 * @file sprite_reimport.hpp
 * @brief Re-import merge: a new sprite source into an existing `.njn` (Tomodachi #297)
 *
 * The sprite source (Aseprite) owns the pixels and the animation frames; the
 * Studio owns the clips.  reimportSprite() imports the new source with
 * importSprite() and merges its result into the existing `.njn`:
 *
 * - **Pixels and animation frames** come from the new source (sheet: `META`,
 *   `PIXL`; layered: `LHDR`, `LIMG`, `LPRT`, `LREF`, `LDUR`).
 * - **The pivot (`LPIV`) and every other chunk** (unknown ids included) are kept
 *   byte-for-byte, in their place.  So is the kind: a source that no longer fits
 *   it (a sheet's source gaining visible layers, or the reverse) fails with
 *   SpriteImportStatus::KindMismatch.
 * - **Clips are kept by name, index-only** (no fingerprints).  A clip frame keeps
 *   its animation-frame number, so a frame inserted in the source shifts what it
 *   shows; a clip frame whose number no longer exists is removed.
 *
 * ## The follow set
 *
 * Which clips still follow their source tag (clip name → tag name, as in the
 * file).  The Studio passes it from the sprite sidecar; the CLI has none, so it
 * passes an empty set and every clip counts as edited.
 *
 * - **A followed clip whose tag is still there** gets the tag's new frames,
 *   seeded as on first import (same tag mapping, ms from the source, no events)
 *   and keeps its name and position.
 * - **A followed clip whose tag is gone** stays, detached (left out of the new
 *   follow set) and flagged `tagRemoved`; its frames are treated as edited.
 * - **An edited clip** keeps its clip frames, ms, events and loop mode; only the
 *   clip frames past the new last animation frame go.
 * - **A clip left empty** is kept and flagged `emptied` (Code may name it).
 * - **A new tag** (no clip follows it) adds a clip after the existing ones,
 *   seeded and following, unless its clip name is taken: then nothing is added
 *   and the clip with that name is flagged with `clashTag`.  This is how a tag
 *   whose clip was edited shows up, so it doesn't count as touching a clip.
 * - The untagged source's `default` clip is never added on a re-import.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "sprite_import.hpp"

namespace enjin2 {

/// Clip name → the source tag it follows (tag named as in the file, UTF-8).
using SpriteFollowSet = std::map<std::string, std::string>;

struct SpriteReimportOptions {
    /// PNG slicing, reused from the source sidecar.
    uint16_t cellW = 0, cellH = 0;
    /// Target colours for RGBA sources; slot i is palette index i.
    std::array<RGB, PALETTE_MAX_ENTRIES> palette = systemPalette();
    SpriteImportLimits limits;
};

/// What the merge did to one clip of the result (in the result's clip order).
struct SpriteReimportClip {
    std::string name;
    /// The animation-frame number of each clip frame, before and after.  An
    /// added clip has an empty `before`.
    std::vector<uint16_t> before;
    std::vector<uint16_t> after;
    /// Positions in `before` removed because their animation frame no longer exists.
    std::vector<uint16_t> removed;
    NjnLoopMode loopBefore = NjnLoopMode::Loop;
    NjnLoopMode loopAfter  = NjnLoopMode::Loop;
    bool added      = false;  ///< Seeded from a new tag.
    bool following  = false;  ///< Follows its tag after the merge (in `follows`).
    bool changed    = false;  ///< Its `CLIP` entry differs: frames, ms, events or loop mode.
    bool tagRemoved = false;  ///< Followed a tag the new source lacks: kept, detached.
    bool emptied    = false;  ///< Had clip frames and has none now: kept.
    /// A new tag whose clip name is this clip's, so it wasn't added (empty: none).
    std::string clashTag;
};

enum class SpriteTagChangeKind : uint8_t {
    Added,    ///< A tag no clip follows: a clip was added, or `clash`.
    Removed,  ///< A followed tag the new source lacks.
    Changed,  ///< A followed tag whose seeded clip differs from the clip it follows.
};

struct SpriteTagChange {
    SpriteTagChangeKind kind = SpriteTagChangeKind::Added;
    std::string tag;     ///< As named in the file.
    std::string clip;    ///< The clip it added, follows, or clashed with.
    bool clash = false;  ///< Added only: `clip` already existed, so nothing was added.
};

struct SpriteReimportResult {
    SpriteImportStatus status = SpriteImportStatus::Malformed;
    std::string        error;                     ///< Empty on success.
    std::vector<SpriteColourIssue> colourIssues;  ///< PaletteMismatch only.

    std::vector<uint8_t> njn;                     ///< The merged `.njn` on success.
    SpriteKind kind = SpriteKind::Auto;           ///< The existing (and kept) kind.
    uint32_t framesBefore = 0;                    ///< Animation frames (a sheet's cells) before.
    uint16_t framesAfter  = 0;                    ///< The new source's animation frames.

    std::vector<SpriteReimportClip> clips;        ///< The result's clips, in `CLIP` order.
    std::vector<SpriteTagChange>    tags;         ///< Removed / Changed in clip order, then Added.
    SpriteFollowSet follows;                      ///< The new follow set.
    /// No clip changed, was added, lost its tag or was emptied: the Studio can
    /// apply without a dialog.  Name clashes don't count.
    bool touchesNoClip = false;

    /// The new source's own import summary (canvas, parts, seeded clips, …);
    /// its `njn` is cleared.
    SpriteImportResult source;

    bool ok() const { return status == SpriteImportStatus::Ok; }
};

/**
 * @brief Merge a new sprite source into an existing `.njn`, keeping its clips by name.
 * @param njn         The existing `.njn` v2 (sheet or layered; untrusted).
 * @param njnSize     Its byte count.
 * @param source      The new `.aseprite` bytes (untrusted).
 * @param sourceSize  Its byte count.
 * @param follows     Which clips follow which tag (empty: every clip is edited).
 * @param opts        RGBA target palette and size caps.
 * @return The merged `.njn` and its report when ok(), else status + error.
 */
SpriteReimportResult reimportSprite(const uint8_t* njn, size_t njnSize,
                                    const uint8_t* source, size_t sourceSize,
                                    const SpriteFollowSet& follows,
                                    const SpriteReimportOptions& opts = {});

} // namespace enjin2
