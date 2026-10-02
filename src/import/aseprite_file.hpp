/**
 * @file aseprite_file.hpp
 * @brief Strict `.aseprite` reader for the sprite importer (internal, #291)
 *
 * Parses the header, frames and the chunks the importer needs (layer, cel,
 * tags, old and new palette) into descriptors that point into the caller's
 * buffer.  Cel pixels are not decoded here: decodeCel() inflates one cel on
 * demand, so hidden layers never cost memory and never fail an import.
 *
 * Spec: https://github.com/aseprite/aseprite/blob/main/docs/ase-file-specs.md
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <enjin2/import/sprite_import.hpp>

namespace enjin2 {
namespace ase {

constexpr uint16_t DEPTH_INDEXED = 8;
constexpr uint16_t DEPTH_RGBA    = 32;

constexpr uint16_t LAYER_IMAGE   = 0;
constexpr uint16_t LAYER_GROUP   = 1;
constexpr uint16_t LAYER_TILEMAP = 2;

constexpr uint16_t CEL_RAW        = 0;
constexpr uint16_t CEL_LINKED     = 1;
constexpr uint16_t CEL_COMPRESSED = 2;

constexpr uint8_t TAG_FORWARD           = 0;
constexpr uint8_t TAG_REVERSE           = 1;
constexpr uint8_t TAG_PINGPONG          = 2;
constexpr uint8_t TAG_PINGPONG_REVERSE  = 3;

/// A status + message pair; status Ok means no error.
struct Error {
    SpriteImportStatus status = SpriteImportStatus::Ok;
    std::string message;
    explicit operator bool() const { return status != SpriteImportStatus::Ok; }
};

inline Error malformed(std::string m) { return {SpriteImportStatus::Malformed, std::move(m)}; }
inline Error unsupported(std::string m) { return {SpriteImportStatus::Unsupported, std::move(m)}; }
inline Error tooLarge(std::string m) { return {SpriteImportStatus::TooLarge, std::move(m)}; }

struct Layer {
    std::string name;
    bool     visible = false;
    uint16_t type = LAYER_IMAGE;
    uint16_t childLevel = 0;
    uint16_t blendMode = 0;
    uint8_t  opacity = 255;   ///< 255 unless the header marks layer opacity valid.
};

/// One cel chunk, not yet decoded.  Pixels live at payload[0..payloadSize).
struct Cel {
    bool     present = false;
    uint16_t type = CEL_RAW;
    int16_t  x = 0, y = 0;
    uint8_t  opacity = 255;
    int16_t  zIndex = 0;
    uint16_t w = 0, h = 0;          ///< Raw and compressed cels.
    uint16_t linkedFrame = 0;       ///< Linked cels.
    const uint8_t* payload = nullptr;
    size_t   payloadSize = 0;
};

struct Tag {
    uint16_t from = 0, to = 0;
    uint8_t  direction = TAG_FORWARD;  ///< TAG_FORWARD .. TAG_PINGPONG_REVERSE.
    uint16_t repeat = 0;      ///< 0 = forever.
    std::string name;         ///< Raw bytes (usually UTF-8).
};

struct File {
    uint16_t width = 0, height = 0;
    uint16_t depth = 0;
    uint16_t speed = 0;               ///< Deprecated header speed (ms).
    uint8_t  transparentIndex = 0;    ///< Indexed sources only.
    std::vector<Layer> layers;
    std::vector<uint16_t> durations;  ///< Per frame, as stored (0 allowed).
    std::vector<std::vector<Cel>> cels; ///< [frame][layer]; present=false = no cel.
    std::vector<Tag> tags;
    std::vector<std::array<uint8_t, 4>> palette; ///< Cumulative over all palette chunks.

    size_t bytesPerPixel() const { return depth == DEPTH_RGBA ? 4 : 1; }
};

/// Parse @p data.  On success @p out points into @p data, which must outlive it.
Error parse(const uint8_t* data, size_t size, const SpriteImportLimits& limits, File& out);

/// Follow links from (frame, layer) to the cel that owns pixels.  Returns null
/// with @p err set on a bad link; null with no error when there is no cel.
/// @p frameOut receives the owning frame.
const Cel* resolve(const File& f, size_t frame, size_t layer, size_t& frameOut, Error& err);

/// Inflate/copy a raw or compressed cel's pixels (w*h*bytesPerPixel bytes).
/// @p budget is decremented by the decoded size and must cover it.
Error decodeCel(const File& f, const Cel& cel, size_t frame, size_t layer,
                uint64_t& budget, std::vector<uint8_t>& out);

/// Inflate a zlib stream into exactly @p outLen bytes (stb_zlib.cpp).  False on
/// a corrupt stream or any other output size.
bool inflateZlib(const uint8_t* in, size_t inLen, uint8_t* out, size_t outLen);

} // namespace ase
} // namespace enjin2
