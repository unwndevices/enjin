/**
 * @file shape_pack_group.hpp
 * @brief The pack layer: a Look from a Shape pack assembled into a Shape group
 *        (ADR-0016, ADR-0017, Tomodachi #331)
 *
 * Above the generic ShapeGroup (shape_group.hpp), so the group never sees
 * slots. A **ShapePack** reads a pack's index once (the generator's pool) plus
 * each Body's socket counts. A **ShapePackGroup** is one pet on screen:
 *
 * - **Look buffer.** build() carves one buffer from an AssetArena, sized from
 *   the index as the sum over slots of the room the slot's largest piece needs
 *   (its record bytes, then its points widened from Q8.8 to Fixed). setLook()
 *   overwrites it in place and re-reads only the slots whose piece changed,
 *   so rolling Looks never grows the bump arena. A colours-only change reads
 *   nothing: it swaps the group's Remap.
 * - **Assembly.** Each filled Feature slot is drawn once per socket the Body
 *   has for it, in socket order. A socket on the left gets the Feature's `_l`
 *   member, else its single member mirrored; a right socket the `_r` member,
 *   else the single one; a centre socket the single or `_r` member, unmirrored.
 *   Paint order (#317 decision 6): Features drawn behind the Body (limbs,
 *   then top), the Body, then the Features in front (top, limbs, cheeks,
 *   mouth, eyes: eyes always on top); limbs are arms, then legs. Within a
 *   slot, members by socket, a member's Shapes in their record order.
 * - **Poses.** A new Feature starts in its slot's default pose (the first of
 *   the fixed list). A pose swap reads nothing: a record holds all its poses.
 * - **Position** is the Body's feet line: the bottom-centre of its rest
 *   drawing's closed rings, in layer pixels.
 * - **Jelly** (#334). The group's Lattice (default 5×5) is laid over the box
 *   of everything the Look can draw (every pose of every piece at its
 *   sockets) plus a 3 px margin, so a pose swap never moves the grid; a Look
 *   whose box differs lays a new Lattice, at rest. Each piece rides it by its
 *   baked Binding (a Body is field). A `sway` Feature hangs a sway arm on each
 *   socket it sits on (#335): from the socket to the centre of the member's
 *   points over every pose, with the record's hz / ζ (2.4 Hz, ζ 0.27 when the
 *   record has none). A pose swap and a move wake it; tick() is the frame seam
 *   the host calls.
 * - **Breathing** (#335, stepped). breathe() runs the group's stepped drive:
 *   step i of N is a squash-stretch of k = round(amplitude · sin(2πi/N)) px,
 *   about the feet (the Body's top moves k px) or the Body's centre (its top
 *   and feet each move k px), the sides moving in by a whole pixel each where
 *   keeping the Body's area calls for it (ΔW = −2·round(W·ΔH / 2H)). So the
 *   Body's box changes by whole pixels and the pet redraws only when the step
 *   changes, with the jelly asleep. Off after build(); a new Look keeps
 *   breathing, re-fitted to its Body.
 *
 * - **Grab, pull, drag, hit** (#336). build() also carves the group's
 *   ShapeHitStore, so hit() answers slot + member (the socket's place among
 *   the slot's sockets, so a mirrored left member is member 0 and the right
 *   one 1) from the last drawn masks. grab() takes what is drawn at a
 *   point: on a `sway` Feature its arm, on anything else (the Body, a
 *   `field` or `rigid` Feature) the nearest Lattice node; nothing drawn
 *   there, no grab. moveTo() drags the feet line through the position
 *   spring; setPosition() snaps it.
 *
 * A bad Look is refused before anything is read, naming slot + id: a missing
 * body, eyes or mouth; an id not in the pack (unknown or retired); a filled
 * slot the Body has no socket for; a colour outside 0..14 (15 is transparent).
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "../core/fixed.hpp"
#include "../graphics/asset_arena.hpp"
#include "../graphics/canvas.hpp"
#include "../graphics/shape_pack.hpp"
#include "shape_group.hpp"

namespace enjin2 {
namespace shape {

/// A refusal, as text (slot + id where there is one).
struct ShapeError {
    char text[120] = {0};
};

/// Where breathing squashes and stretches about.
enum class BreathPivot : uint8_t { Feet, Centre };

/// Stepped breathing (Tomodachi #335): `steps` whole-pixel poses per period.
struct Breath {
    uint32_t    periodMicros = 3000000;  ///< One breath.
    uint8_t     steps = 4;               ///< 2..16 per breath.
    uint8_t     amplitude = 1;           ///< px the Body's top (and, about the centre, its feet) moves; 0..16.
    BreathPivot pivot = BreathPivot::Feet;
};

/// One variant of the pet: a piece id per filled slot and the role colours.
struct ShapeLook {
    uint16_t id[SHAPE_SLOT_COUNT] = {};
    uint8_t  filled = 0;                ///< Bit per ShapeSlot.
    uint8_t  colour[3] = {0, 0, 0};     ///< Palette index of the body, accent and ink roles.

    void set(ShapeSlot s, uint16_t pieceId)
    {
        id[uint8_t(s)] = pieceId;
        filled = uint8_t(filled | (1u << uint8_t(s)));
    }
    void clear(ShapeSlot s) { filled = uint8_t(filled & ~(1u << uint8_t(s))); }
    bool has(ShapeSlot s) const { return (filled >> uint8_t(s)) & 1u; }
    uint16_t get(ShapeSlot s) const { return id[uint8_t(s)]; }
    /// Same piece (or both empty) in slot s.
    bool samePiece(const ShapeLook& o, ShapeSlot s) const
    {
        return has(s) == o.has(s) && (!has(s) || get(s) == o.get(s));
    }
};

/// A Shape pack opened for Looks: the index plus each Body's sockets.
class ShapePack {
public:
    /// Read the index and every Body's socket table (the pool). Keeps `src`.
    bool open(ShapePackSource& src, ShapeError* err);

    const ShapePackIndex& index() const { return index_; }
    ShapePackSource* source() const { return src_; }

    /// Socket counts per slot of Body `id`, or nullptr when no such Body.
    const uint8_t* bodySockets(uint16_t id) const;

    /// Look buffer room for a record of `recordSize` bytes.
    static size_t regionBytes(uint32_t recordSize);
    /// Look buffer room for slot `s` (its largest piece; 0 if the slot is empty).
    size_t slotRegionBytes(ShapeSlot s) const { return regionBytes(maxRecord_[uint8_t(s)]); }
    /// The whole Look buffer: the sum of the slot regions.
    size_t lookBufferBytes() const;

private:
    struct BodyInfo {
        uint16_t id;
        uint8_t  sockets[SHAPE_SLOT_COUNT];
    };
    ShapePackIndex        index_;
    ShapePackSource*      src_ = nullptr;
    std::vector<BodyInfo> bodies_;
    uint32_t              maxRecord_[SHAPE_SLOT_COUNT] = {};
};

/// One pet: a Look read into a Look buffer and drawn as a Shape group.
class ShapePackGroup : private ShapeItemSource {
public:
    /**
     * @brief Read `look` into the Look buffer, carving the buffer and the hit
     *        masks from `arena` only when this group has none big enough
     *        yet: a clear() → build() loop reuses them and never grows the
     *        arena (#323 §4). All or nothing: on failure the arena is rolled
     *        back and the group is cleared.
     */
    bool build(ShapePack& pack, const ShapeLook& look, AssetArena& arena, ShapeError* err);
    bool built() const { return pack_ != nullptr; }
    /// Forget the pet (not built any more), keeping its Look buffer and hit
    /// masks for the next build(). Resets in place: no temporary group.
    void clear();
    /// clear(), and forget that memory too (its arena is being reclaimed).
    void reset();

    /// Switch to `look` in place. On failure the group keeps its old Look.
    bool setLook(const ShapeLook& look, ShapeError* err);
    const ShapeLook& look() const { return look_; }

    /// Swap slot `s`'s Feature to the pose named `name` (whole-Feature).
    bool setPose(ShapeSlot s, const char* name, ShapeError* err);
    /// Name of slot `s`'s current pose, or nullptr for an empty slot.
    const char* poseName(ShapeSlot s) const;
    /// The poses slot `s`'s Feature has (fixed-list names), up to `max`; 0 if empty.
    uint8_t poseNames(ShapeSlot s, const char** out, uint8_t max) const;

    /// Put the Body's feet line at `feet` (layer pixels): a snap.
    void setPosition(FixedVec2 feet);
    /// Drag the feet line toward `feet` through the position spring: the
    /// jelly lags behind. Call it every frame while dragging.
    void moveTo(FixedVec2 feet);
    /// Where the feet line is now (moving while a drag carries it).
    FixedVec2 position() const { return group_.origin() + feet_; }

    /**
     * @brief Grab what the last redraw drew at layer point `at`: a sway
     *        Feature's arm, else the nearest Lattice node. Wakes the jelly.
     * @return a handle, or -1 when nothing is drawn there (or the grab
     *         slots are full).
     */
    int grab(FixedVec2 at);
    /// Pull grab `handle` toward `target` (layer pixels).
    void pull(int handle, FixedVec2 target) { group_.moveGrab(handle, target); }
    /// Let go, throwing at `velocity` px/s.
    void release(int handle, FixedVec2 velocity) { group_.releaseGrab(handle, velocity); }

    /**
     * @brief What the last redraw drew topmost at layer pixel (x, y).
     * @return false when nothing; else the slot and member (socket place).
     */
    bool hit(int x, int y, ShapeSlot& slot, uint8_t& member) const;

    bool changed() const { return group_.changed(); }

    /**
     * @brief Breathe: step the rest shape through `b.steps` steps per breath,
     *        never waking the jelly (see the file comment). Replaces any
     *        breath already running, from its first step. Off after build().
     * @return false (breathing unchanged) for steps outside 2..16, a zero
     *         period or an amplitude past 16 px.
     */
    bool breathe(const Breath& b, ShapeError* err);
    /// Stop breathing: back to the rest shape.
    void stopBreathing();
    bool breathing() const { return breathing_; }

    /// Advance the jelly by a frame of `dtMicros` (the host calls this before
    /// flush()); true when the field moved and the pet needs a redraw.
    bool tick(uint32_t dtMicros) { return group_.tick(dtMicros); }
    /// Push the jelly at layer point `at` to `velocity` px/s; wakes it.
    void impulse(FixedVec2 at, FixedVec2 velocity) { group_.impulse(at, velocity); }
    /// Settled and snapped to rest: no sim steps, no redraws.
    bool asleep() const { return group_.lattice().asleep(); }
    const Lattice& lattice() const { return group_.lattice(); }

    /// Redraw onto `layer` (see ShapeGroup::redraw).
    template <uint16_t W, uint16_t H>
    bool redraw(Canvas4<W, H>& layer, ShapeDrawScratch& scratch)
    {
        return group_.redraw(layer, scratch, *this);
    }
    /// Redraw only if something changed; true when it drew.
    template <uint16_t W, uint16_t H>
    bool flush(Canvas4<W, H>& layer, ShapeDrawScratch& scratch)
    {
        if (!group_.changed())
            return false;
        redraw(layer, scratch);
        return true;
    }
    /// Clear what the group drew (e.g. when it is freed).
    template <uint16_t W, uint16_t H>
    void erase(Canvas4<W, H>& layer)
    {
        group_.erase(layer);
    }

private:
    struct Slot {
        uint8_t*         region = nullptr;
        size_t           capacity = 0;
        ShapeRecordView  view;
        const FixedVec2* points = nullptr;
        uint8_t          pose = 0;  ///< Record pose number.
        bool             loaded = false;
    };

    bool forEach(ShapeDrawScratch& scratch, ShapeEmit emit, void* ctx) const override;
    bool validate(const ShapeLook& look, ShapeError* err) const;
    bool readSlot(ShapeSlot s, uint16_t id, ShapeError* err);
    void refreshBody();
    void refreshLattice();
    bool startBreath();

    // The arena memory build() carved, kept across clear() for the next build().
    uint8_t*       buffer_ = nullptr;
    size_t         bufferBytes_ = 0;
    ShapeHitStore* hits_ = nullptr;

    ShapePack* pack_ = nullptr;
    ShapeLook  look_;
    Slot       slots_[SHAPE_SLOT_COUNT];
    ShapeGroup group_;
    FixedVec2  carryFeet_{};  ///< Where the last moveTo() heads.
    FixedVec2  feet_{};
    FixedVec2  bodyMin_{}, bodyMax_{};  ///< The Body's rest box (closed rings).
    Breath     breath_;
    bool       breathing_ = false;
};

} // namespace shape
} // namespace enjin2
