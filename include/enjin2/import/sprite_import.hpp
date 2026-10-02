/**
 * @file sprite_import.hpp
 * @brief Sprite importer: `.aseprite` → `.njn` v2 (ADR-0015, Tomodachi #291)
 *
 * One function, importSprite(), turns the bytes of an `.aseprite` file into the
 * bytes of a `.njn` v2 asset.  The native CLI (`tools/sprite_import`) and the
 * Studio's asset-tools worker call the same function.
 *
 * ## Kind
 *
 * More than one visible layer gives a **layered sprite** (`LHDR`/`LIMG`/`LPRT`/
 * `LREF`/`LDUR`/`LPIV`/`CLIP`, ADR-0004/0005); otherwise a **flat sheet**
 * (`META`/`PIXL`/`CLIP`), one cell per animation frame.  SpriteImportOptions::kind
 * overrides the detection.
 *
 * ## Clips
 *
 * Each Aseprite tag seeds one clip:
 * - forward → frames in order; reverse → reversed frames;
 * - ping-pong → `pingpong`; ping-pong-reverse → reversed frames + `pingpong`;
 * - repeat 0 → `loop` (`pingpong` for the ping-pong directions), repeat 1 →
 *   `once`, repeat N > 1 → the N passes Aseprite plays, unrolled, as `once`.
 *   A ping-pong pass alternates direction and does not repeat its turn frame.
 *
 * An untagged file gets one `default` loop clip over all frames.  Names are
 * reduced to printable ASCII and capped at 15 characters.  A frame duration of
 * 0 ms falls back to the header speed (then to 100 ms).
 *
 * ## Colours
 *
 * Indexed sources keep their indices as palette slots (0..14); the source's
 * transparent index becomes 15 and the source's palette colours are ignored.
 * RGBA sources need binary alpha and an exact match to the target palette
 * (SpriteImportOptions::palette, the system palette by default).  There is no
 * quantisation: a mismatch fails with SpriteImportStatus::PaletteMismatch and
 * one SpriteColourIssue per offending RGBA value.
 *
 * ## Hardening
 *
 * The input is untrusted.  The importer never asserts or aborts; every problem
 * is an error result.  Sizes are capped (SpriteImportLimits) before anything is
 * allocated, and the output is re-read with NjnV2Reader before it is returned.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "../graphics/njn2.hpp"
#include "../graphics/palette.hpp"

namespace enjin2 {

/// Which `.njn` shape the importer emits.
enum class SpriteKind : uint8_t {
    Auto,     ///< Layered when more than one layer is visible, else a sheet.
    Sheet,    ///< Flat sheet: META + PIXL + CLIP.
    Layered,  ///< Layered sprite: LHDR + LIMG + LPRT + LREF + LDUR + LPIV + CLIP.
};

/// Outcome class of an import.  Everything except Ok carries a message.
enum class SpriteImportStatus : uint8_t {
    Ok,
    Malformed,        ///< Not a readable `.aseprite` (bad magic, truncation, …).
    Unsupported,      ///< Readable, but uses a feature the importer rejects.
    TooLarge,         ///< Exceeds a SpriteImportLimits cap or a format limit.
    PaletteMismatch,  ///< RGBA pixels off the target palette or with partial alpha.
};

/// Size caps, checked before allocating.
struct SpriteImportLimits {
    uint32_t maxFileBytes   = 32u << 20;  ///< Input size.
    uint16_t maxCanvasSide  = 2048;       ///< Canvas width and height.
    uint16_t maxFrames      = 1024;       ///< Animation frames.
    uint16_t maxLayers      = 256;        ///< Layers, hidden ones included.
    uint16_t maxCelSide     = 4096;       ///< One cel's width and height.
    uint32_t maxPixelBytes  = 64u << 20;  ///< All decoded cel bytes together.
};

/// The system palette's 15 opaque colours (Palette's default, the green ramp).
std::array<RGB, PALETTE_MAX_ENTRIES> systemPalette();

struct SpriteImportOptions {
    SpriteKind kind = SpriteKind::Auto;
    /// Target colours for RGBA sources; slot i is palette index i.
    std::array<RGB, PALETTE_MAX_ENTRIES> palette = systemPalette();
    SpriteImportLimits limits;
};

/// One RGBA value an RGBA source can't import: off the palette, or partial alpha.
struct SpriteColourIssue {
    uint8_t  r, g, b, a;
    uint32_t pixels;       ///< In-canvas pixels with this value (each stored cel once).
    uint8_t  nearestSlot;  ///< Closest palette slot by colour (Oklab), 0..14.
    /// True when the value fails on alpha (0 < a < 255) rather than on colour.
    bool partialAlpha() const { return a != 0 && a != 255; }
};

/// One clip as written, for the summary.
struct SpriteClipSummary {
    std::string name;
    NjnLoopMode loopMode;
    uint16_t    frames;
};

struct SpriteImportResult {
    SpriteImportStatus status = SpriteImportStatus::Malformed;
    std::string        error;                 ///< Empty on success.
    std::vector<SpriteColourIssue> colourIssues; ///< PaletteMismatch only; most pixels first.

    std::vector<uint8_t> njn;                 ///< The `.njn` v2 bytes on success.
    SpriteKind kind = SpriteKind::Auto;       ///< Sheet or Layered on success.

    // Summary (filled on success).
    uint16_t canvasW = 0, canvasH = 0;
    uint16_t frames  = 0;
    std::vector<std::string> parts;           ///< Layered: part names, painter order.
    uint16_t images  = 0;                     ///< Layered: part-image pool size.
    std::vector<SpriteClipSummary> clips;
    std::vector<std::string> hiddenLayers;    ///< Hidden layers, ignored.
    /// The source's palette (RGBA by index) after all palette chunks, old
    /// (0x0004) and new (0x2019).  Unset entries are {0,0,0,0}.  Informational:
    /// indexed pixels keep their indices whatever these colours are.
    std::vector<std::array<uint8_t, 4>> sourcePalette;

    bool ok() const { return status == SpriteImportStatus::Ok; }
};

/**
 * @brief Convert an `.aseprite` file to a `.njn` v2 asset.
 * @param data  The file bytes (untrusted).
 * @param size  Byte count.
 * @param opts  Kind override, RGBA target palette, size caps.
 * @return A result whose `njn` holds the asset when ok(), else status + error.
 */
SpriteImportResult importSprite(const uint8_t* data, size_t size,
                                const SpriteImportOptions& opts = {});

/// Lower-case name of a status ("ok", "malformed", …), for logs and the CLI.
const char* spriteImportStatusName(SpriteImportStatus s);

} // namespace enjin2
