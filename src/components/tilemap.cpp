#include "../../include/enjin2/components/tilemap.hpp"
#include "../../include/enjin2/core/object.hpp"
#include <cstring>

namespace enjin2 {

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
// Map data
// ---------------------------------------------------------------------------

void C_Tilemap::setTiles(const uint8_t* data, uint8_t w, uint8_t h) {
    if (!data) return;

    // Clamp to maximum dimensions
    if (w > MAX_MAP_W) w = MAX_MAP_W;
    if (h > MAX_MAP_H) h = MAX_MAP_H;

    // Zero entire array first (keeps areas outside w*h transparent)
    memset(m_tiles, 0, sizeof(m_tiles));

    // Copy rows. Stride inside m_tiles matches m_mapW (set below to w).
    // Each row in 'data' has 'w' bytes; each row in m_tiles also has 'w'
    // bytes (using m_mapW = w as stride), so we can copy row-by-row.
    for (uint8_t row = 0; row < h; ++row) {
        memcpy(&m_tiles[row * w], &data[row * w], w);
    }

    m_mapW = w;
    m_mapH = h;
}

void C_Tilemap::setTile(uint8_t tx, uint8_t ty, uint8_t tileId) {
    if (tx >= m_mapW || ty >= m_mapH) return;
    m_tiles[ty * m_mapW + tx] = tileId;
}

uint8_t C_Tilemap::getTile(uint8_t tx, uint8_t ty) const {
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

uint8_t C_Tilemap::tileAtPixel(int16_t px, int16_t py) const {
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
            const uint8_t tileId = m_tiles[ty * m_mapW + tx];
            if (tileId == 0) continue;  // transparent sentinel — skip draw

            // Screen-space position of this tile's top-left corner
            const int16_t px = static_cast<int16_t>(tx * tileW - m_scrollX);
            const int16_t py = static_cast<int16_t>(ty * tileH - m_scrollY);

            // Tile ID used directly as frameIndex:
            //   tile 0 = skip (transparent); tile 1 = frame 1; tile 2 = frame 2; etc.
            // Frame 0 in the tileset is intentionally "wasted" — this removes
            // an off-by-one subtract from the hot rendering path.
            m_sheet.draw(canvas, tileId, px, py);
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
