#include "../../include/enjin2/components/tilemap.hpp"
#include "../../include/enjin2/core/object.hpp"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace enjin2 {

// Floor division for negative values (C++ truncates toward zero). Defined in
// the coordinate-helpers section below; declared here for the attribute query
// code that precedes it.
static int16_t floorDiv(int16_t a, int16_t b);

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

C_Tilemap::C_Tilemap(Object* owner)
    : C_Drawable(owner, 0, 0)
{
    buffer_index = 0;  // Layer 0 = background (v1.4 convention)
    memset(m_tiles, 0, sizeof(m_tiles));
}

// ---------------------------------------------------------------------------
// Tileset
// ---------------------------------------------------------------------------

void C_Tilemap::setSheet(const SpriteSheet& sheet) {
    m_sheet = sheet;
}

// ---------------------------------------------------------------------------
// Tile attributes
// ---------------------------------------------------------------------------

void C_Tilemap::setAttrs(const TileAttr* attrs, uint16_t count) {
    m_attrCount = 0;
    if (count > TM_MAX_TILES) count = TM_MAX_TILES;
    if (attrs != nullptr && count > 0) {
        memcpy(m_attrs, attrs, static_cast<size_t>(count) * sizeof(TileAttr));
        m_attrCount = count;
    }
}

void C_Tilemap::setPalbank(uint8_t index, const Remap& remap) {
    m_palbanks[index & 0x0F] = remap;
}

void C_Tilemap::setPalbanks(const Remap* banks, uint8_t count) {
    if (!banks || count == 0) return;
    for (uint8_t i = 0; i < count && i < TM_PALBANK_COUNT; ++i) {
        m_palbanks[i] = banks[i];
    }
}

TileAttr C_Tilemap::attrAt(uint8_t tx, uint8_t ty) const {
    if (tx >= m_mapW || ty >= m_mapH) {
        return TileAttr{};
    }
    const uint16_t cell = getTile(tx, ty);
    const uint16_t tid  = cellTileId(cell);
    if (tid >= m_attrCount) {
        return TileAttr{};
    }
    TileAttr a = m_attrs[tid];
    a.flags = static_cast<uint8_t>(
        (a.flags & static_cast<uint8_t>(~TileAttr::DIR_MASK)) |
        (tmResolveDir(a.dir(), cellHFlip(cell), cellVFlip(cell)) << TileAttr::DIR_SHIFT));
    return a;
}

TileAttr C_Tilemap::attrAtPixel(int16_t px, int16_t py) const {
    const int16_t tileW = static_cast<int16_t>(m_sheet.cellW);
    const int16_t tileH = static_cast<int16_t>(m_sheet.cellH);
    if (tileW == 0 || tileH == 0) return TileAttr{};
    const int16_t tx = floorDiv(px, tileW);
    const int16_t ty = floorDiv(py, tileH);
    if (tx < 0 || ty < 0 ||
        tx >= static_cast<int16_t>(m_mapW) || ty >= static_cast<int16_t>(m_mapH)) {
        return TileAttr{};
    }
    return attrAt(static_cast<uint8_t>(tx), static_cast<uint8_t>(ty));
}

// ---------------------------------------------------------------------------
// Collision query — axis-separated swept slide (ADR-0003 §3)
// ---------------------------------------------------------------------------

namespace {

constexpr float SWEEP_EPS = 1e-5f;

// Does the tile at (tx,ty) block movement in the given direction along X?
// (movingPositive = moving toward +X). One-way tiles honour the Playdate rule:
// they block only motion into their facing face.
inline bool blocksAlongX(const C_Tilemap& tm, int tx, int ty, bool movingPositive) {
    const TileAttr a = tm.attrAt(static_cast<uint8_t>(tx), static_cast<uint8_t>(ty));
    if (a.solid()) return true;
    if (a.oneway()) {
        const uint8_t d = a.dir();
        if (movingPositive && d == static_cast<uint8_t>(TileDir::Left))  return true;
        if (!movingPositive && d == static_cast<uint8_t>(TileDir::Right)) return true;
    }
    return false;
}

// Same as above, but for the Y axis (+Y = downward).
inline bool blocksAlongY(const C_Tilemap& tm, int tx, int ty, bool movingPositive) {
    const TileAttr a = tm.attrAt(static_cast<uint8_t>(tx), static_cast<uint8_t>(ty));
    if (a.solid()) return true;
    if (a.oneway()) {
        const uint8_t d = a.dir();
        if (movingPositive && d == static_cast<uint8_t>(TileDir::Up))   return true;
        if (!movingPositive && d == static_cast<uint8_t>(TileDir::Down)) return true;
    }
    return false;
}

// Slide the AABB along the X axis by `d` px (map space). On a block, `x` is
// clamped to contact, `hit`/`nx` are set and `tf` records the consumed fraction.
inline void slideAxisX(const C_Tilemap& tm, float& x, float y, float w, float h,
                       float d, bool& hit, float& nx, float& tf) {
    hit = false; nx = 0.0f; tf = 1.0f;
    if (std::fabs(d) < SWEEP_EPS) return;
    const float tw = static_cast<float>(tm.getSheet().cellW);
    const float th = static_cast<float>(tm.getSheet().cellH);
    if (tw <= 0.0f || th <= 0.0f) return;

    const bool pos = d > 0.0f;
    const float remaining = pos ? d : -d;
    const float lead = pos ? (x + w) : x;

    int ry0 = static_cast<int>(std::floor(y / th));
    int ry1 = static_cast<int>(std::floor((y + h - SWEEP_EPS) / th));
    if (ry1 < ry0) ry1 = ry0;

    const float lo = std::min(lead, lead + d);
    const float hi = std::max(lead, lead + d);
    const int cLo = static_cast<int>(std::ceil((lo + SWEEP_EPS) / tw));
    const int cHi = static_cast<int>(std::floor((hi - SWEEP_EPS) / tw));

    float maxDist = remaining;
    bool blocked  = false;
    for (int ty = ry0; ty <= ry1; ++ty) {
        if (ty < 0 || ty >= static_cast<int>(tm.getMapHeight())) continue;
        for (int c = cLo; c <= cHi; ++c) {
            const int col = pos ? c : (c - 1);   // column entered across line c*tw
            if (col < 0 || col >= static_cast<int>(tm.getMapWidth())) continue;
            if (!blocksAlongX(tm, col, ty, pos)) continue;
            const float dist = std::fabs(static_cast<float>(c) * tw - lead);
            if (dist < maxDist) { maxDist = dist; blocked = true; }
        }
    }

    const float moved = blocked ? maxDist : remaining;
    x += pos ? moved : -moved;
    if (blocked) {
        hit = true;
        nx  = pos ? -1.0f : 1.0f;
        tf  = moved / remaining;
    }
}

// Mirror of slideAxisX for the Y axis.
inline void slideAxisY(const C_Tilemap& tm, float x, float& y, float w, float h,
                       float d, bool& hit, float& ny, float& tf) {
    hit = false; ny = 0.0f; tf = 1.0f;
    if (std::fabs(d) < SWEEP_EPS) return;
    const float tw = static_cast<float>(tm.getSheet().cellW);
    const float th = static_cast<float>(tm.getSheet().cellH);
    if (tw <= 0.0f || th <= 0.0f) return;

    const bool pos = d > 0.0f;
    const float remaining = pos ? d : -d;
    const float lead = pos ? (y + h) : y;

    int rx0 = static_cast<int>(std::floor(x / tw));
    int rx1 = static_cast<int>(std::floor((x + w - SWEEP_EPS) / tw));
    if (rx1 < rx0) rx1 = rx0;

    const float lo = std::min(lead, lead + d);
    const float hi = std::max(lead, lead + d);
    const int cLo = static_cast<int>(std::ceil((lo + SWEEP_EPS) / th));
    const int cHi = static_cast<int>(std::floor((hi - SWEEP_EPS) / th));

    float maxDist = remaining;
    bool blocked  = false;
    for (int c = cLo; c <= cHi; ++c) {
        const int row = pos ? c : (c - 1);   // row entered across line c*th
        if (row < 0 || row >= static_cast<int>(tm.getMapHeight())) continue;
        for (int tx = rx0; tx <= rx1; ++tx) {
            if (tx < 0 || tx >= static_cast<int>(tm.getMapWidth())) continue;
            if (!blocksAlongY(tm, tx, row, pos)) continue;
            const float dist = std::fabs(static_cast<float>(c) * th - lead);
            if (dist < maxDist) { maxDist = dist; blocked = true; }
        }
    }

    const float moved = blocked ? maxDist : remaining;
    y += pos ? moved : -moved;
    if (blocked) {
        hit = true;
        ny  = pos ? -1.0f : 1.0f;
        tf  = moved / remaining;
    }
}

} // namespace

SweepResult C_Tilemap::sweepAabb(float x, float y, float w, float h,
                                 float vx, float vy, float dt) const {
    SweepResult r;
    r.x = x;
    r.y = y;

    bool hx = false, hy = false;
    float nx = 0.0f, ny = 0.0f, tfx = 1.0f, tfy = 1.0f;

    slideAxisX(*this, r.x, r.y, w, h, vx * dt, hx, nx, tfx);
    slideAxisY(*this, r.x, r.y, w, h, vy * dt, hy, ny, tfy);

    r.normalX = nx;
    r.normalY = ny;
    r.hit = hx || hy;
    r.t = std::min(tfx, tfy);
    return r;
}

SweepResult C_Tilemap::sweepCircle(float cx, float cy, float r,
                                   float vx, float vy, float dt) const {
    SweepResult res = sweepAabb(cx - r, cy - r, 2.0f * r, 2.0f * r, vx, vy, dt);
    res.x += r;
    res.y += r;
    return res;
}

std::vector<Rect> C_Tilemap::buildSolidRects() const {
    std::vector<Rect> out;
    const int w = m_mapW;
    const int h = m_mapH;
    if (w == 0 || h == 0) return out;

    const int16_t tw = static_cast<int16_t>(m_sheet.cellW);
    const int16_t th = static_cast<int16_t>(m_sheet.cellH);
    if (tw <= 0 || th <= 0) return out;

    std::vector<bool> covered(static_cast<size_t>(w) * h, false);

    for (int ty = 0; ty < h; ++ty) {
        for (int tx = 0; tx < w; ++tx) {
            if (covered[static_cast<size_t>(ty) * w + tx]) continue;
            if (!isSolid(static_cast<uint8_t>(tx), static_cast<uint8_t>(ty))) continue;

            int runEnd = tx;
            while (runEnd + 1 < w &&
                   isSolid(static_cast<uint8_t>(runEnd + 1), static_cast<uint8_t>(ty)) &&
                   !covered[static_cast<size_t>(ty) * w + (runEnd + 1)]) {
                ++runEnd;
            }

            int bottom = ty;
            while (bottom + 1 < h) {
                bool full = true;
                for (int c = tx; c <= runEnd; ++c) {
                    if (!isSolid(static_cast<uint8_t>(c), static_cast<uint8_t>(bottom + 1)) ||
                        covered[static_cast<size_t>(bottom + 1) * w + c]) {
                        full = false;
                        break;
                    }
                }
                if (!full) break;
                ++bottom;
            }

            for (int r = ty; r <= bottom; ++r) {
                for (int c = tx; c <= runEnd; ++c) {
                    covered[static_cast<size_t>(r) * w + c] = true;
                }
            }

            out.push_back(Rect(
                static_cast<int16_t>(tx * tw),
                static_cast<int16_t>(ty * th),
                static_cast<uint16_t>((runEnd - tx + 1) * tw),
                static_cast<uint16_t>((bottom - ty + 1) * th)));
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Map data
// ---------------------------------------------------------------------------

void C_Tilemap::setTiles(const uint8_t* data, uint8_t w, uint8_t h) {
    if (!data) return;

    // Clamp to maximum dimensions
    if (w > MAX_MAP_W) w = MAX_MAP_W;
    if (h > MAX_MAP_H) h = MAX_MAP_H;

    // Zero entire array first (keeps areas outside w*h transparent)
    memset(m_tiles, 0, sizeof(m_tiles));

    // Each input byte is a plain tile id → widen into a packed cell (band 0,
    // no flip, palbank 0). Stride inside m_tiles matches m_mapW (= w).
    for (uint8_t row = 0; row < h; ++row) {
        for (uint8_t col = 0; col < w; ++col) {
            m_tiles[row * w + col] = static_cast<uint16_t>(data[row * w + col]);
        }
    }

    m_mapW = w;
    m_mapH = h;
}

void C_Tilemap::setTiles(const uint16_t* data, uint8_t w, uint8_t h) {
    if (!data) return;

    if (w > MAX_MAP_W) w = MAX_MAP_W;
    if (h > MAX_MAP_H) h = MAX_MAP_H;

    // Zero entire array first (keeps areas outside w*h transparent)
    memset(m_tiles, 0, sizeof(m_tiles));

    // Copy packed cells verbatim, row by row (stride m_mapW = w).
    for (uint8_t row = 0; row < h; ++row) {
        memcpy(&m_tiles[row * w], &data[row * w], w * sizeof(uint16_t));
    }

    m_mapW = w;
    m_mapH = h;
}

void C_Tilemap::setTile(uint8_t tx, uint8_t ty, uint16_t cell) {
    if (tx >= m_mapW || ty >= m_mapH) return;
    m_tiles[ty * m_mapW + tx] = cell;
}

uint16_t C_Tilemap::getTile(uint8_t tx, uint8_t ty) const {
    if (tx >= m_mapW || ty >= m_mapH) return 0;
    return m_tiles[ty * m_mapW + tx];
}

// ---------------------------------------------------------------------------
// Camera offset
// ---------------------------------------------------------------------------

void C_Tilemap::setScroll(int16_t sx, int16_t sy) {
    m_scrollX = sx;
    m_scrollY = sy;
}

// ---------------------------------------------------------------------------
// Coordinate helpers
// ---------------------------------------------------------------------------

// Floor division for negative values (C++ truncates toward zero).
static inline int16_t floorDiv(int16_t a, int16_t b) {
    // b is always tile size (positive). Use int32_t arithmetic to avoid
    // sign-extension issues with int16_t.
    int32_t q = static_cast<int32_t>(a) / static_cast<int32_t>(b);
    // If remainder != 0 and signs differ, floor is one less than truncation.
    int32_t r = static_cast<int32_t>(a) % static_cast<int32_t>(b);
    if (r != 0 && ((a < 0) != (b < 0))) {
        q -= 1;
    }
    return static_cast<int16_t>(q);
}

void C_Tilemap::pixelToTile(int16_t px, int16_t py, int16_t& tx, int16_t& ty) const {
    const int16_t tileW = static_cast<int16_t>(m_sheet.cellW);
    const int16_t tileH = static_cast<int16_t>(m_sheet.cellH);
    if (tileW == 0 || tileH == 0) { tx = 0; ty = 0; return; }

    // Screen pixel + scroll = world pixel; world pixel / tileSize = tile coord.
    int16_t worldX = static_cast<int16_t>(px + m_scrollX);
    int16_t worldY = static_cast<int16_t>(py + m_scrollY);
    tx = floorDiv(worldX, tileW);
    ty = floorDiv(worldY, tileH);
}

void C_Tilemap::tileToPixel(int16_t tx, int16_t ty, int16_t& px, int16_t& py) const {
    const int16_t tileW = static_cast<int16_t>(m_sheet.cellW);
    const int16_t tileH = static_cast<int16_t>(m_sheet.cellH);
    // World pixel = tile coord * tileSize; screen pixel = world pixel - scroll.
    px = static_cast<int16_t>(tx * tileW - m_scrollX);
    py = static_cast<int16_t>(ty * tileH - m_scrollY);
}

uint16_t C_Tilemap::tileAtPixel(int16_t px, int16_t py) const {
    int16_t tx = 0, ty = 0;
    pixelToTile(px, py, tx, ty);
    if (tx < 0 || ty < 0 || tx >= static_cast<int16_t>(m_mapW) || ty >= static_cast<int16_t>(m_mapH)) {
        return 0;
    }
    return getTile(static_cast<uint8_t>(tx), static_cast<uint8_t>(ty));
}

// ---------------------------------------------------------------------------
// C_Drawable overrides
// ---------------------------------------------------------------------------

void C_Tilemap::draw(ICanvas<Pixel4>& canvas) {
    if (!is_visible || !m_sheet.data || m_mapW == 0 || m_mapH == 0) return;

    const int16_t tileW = static_cast<int16_t>(m_sheet.cellW);
    const int16_t tileH = static_cast<int16_t>(m_sheet.cellH);
    if (tileW == 0 || tileH == 0) return;

    const int16_t canvasW = static_cast<int16_t>(canvas.getWidth());
    const int16_t canvasH = static_cast<int16_t>(canvas.getHeight());

    // First visible tile index (clamp negatives to 0)
    int16_t startTX = m_scrollX / tileW;
    int16_t startTY = m_scrollY / tileH;
    if (startTX < 0) startTX = 0;
    if (startTY < 0) startTY = 0;

    // Last visible tile (exclusive) — +1 for partial tiles at right/bottom edge
    int16_t endTX = (m_scrollX + canvasW) / tileW + 1;
    int16_t endTY = (m_scrollY + canvasH) / tileH + 1;
    if (endTX > static_cast<int16_t>(m_mapW)) endTX = static_cast<int16_t>(m_mapW);
    if (endTY > static_cast<int16_t>(m_mapH)) endTY = static_cast<int16_t>(m_mapH);

    for (int16_t ty = startTY; ty < endTY; ++ty) {
        for (int16_t tx = startTX; tx < endTX; ++tx) {
            const uint16_t cell = m_tiles[ty * m_mapW + tx];
            const uint16_t tileId = cellTileId(cell);
            if (tileId == 0) continue;  // transparent sentinel — skip draw

            // Screen-space position of this tile's top-left corner
            const int16_t px = static_cast<int16_t>(tx * tileW - m_scrollX);
            const int16_t py = static_cast<int16_t>(ty * tileH - m_scrollY);

            // Tile id (low 9 bits) used as frameIndex:
            //   tile 0 = skip (transparent); tile 1 = frame 1; etc.
            // Frame 0 in the tileset is intentionally "wasted" — this removes
            // an off-by-one subtract from the hot rendering path. Flip + palbank
            // bits render here (ADR-0003 §3); band is honored by the compositor
            // restore filter (drawCellBand), not this bulk draw.
            m_sheet.draw(canvas, tileId, px, py,
                         cellHFlip(cell), cellVFlip(cell),
                         m_palbanks[cellPalbank(cell) & 0x0F]);
        }
    }
}

bool C_Tilemap::continueToDraw() const {
    return !owner->isQueuedForRemoval();
}

void C_Tilemap::drawWithOffset(ICanvas<Pixel4>& canvas, Point offset) {
    // Screen-space tilemap (e.g. HUD minimap): ignore camera offset
    if (m_screenSpace) {
        draw(canvas);
        return;
    }
    // Camera offset is additive with tilemap's own scroll offset.
    // offset = getScreenOffset() = -(camera_pos + shake) [negative].
    // Subtracting the (negative) offset yields: scroll + camera_pos — correct
    // because the tilemap should start rendering from world position camera_pos
    // further into the map.
    const int16_t savedScrollX = m_scrollX;
    const int16_t savedScrollY = m_scrollY;
    m_scrollX = static_cast<int16_t>(m_scrollX - offset.x);
    m_scrollY = static_cast<int16_t>(m_scrollY - offset.y);
    draw(canvas);
    m_scrollX = savedScrollX;
    m_scrollY = savedScrollY;
}

} // namespace enjin2
