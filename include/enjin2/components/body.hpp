/**
 * @file body.hpp
 * @brief C_Body — the single dynamic circle body (component, ADR-0003 §4)
 *
 * The only dynamic entity in either slice (pinball ball / maze marble): a circle
 * with position, velocity, radius, restitution and drag. It steps against a
 * scene-level @ref ColliderSet (via `engine.scene.colliders()`), referenced here
 * with setColliders() — never owned. Every `step(dt)` runs N fixed-dt substeps
 * (swept conservative-advancement TOI + depenetration+impulse, see
 * colliders.hpp) and **buffers** the resolved contacts, which the applet polls
 * (numContacts()/contact(i)) — there is no callback registry.
 *
 * Attach from Lua with `obj:add("C_Body", {radius=4, restitution=.8})`; the
 * sole attach verb (ADR-0003 §2).
 */
#pragma once

#include "../core/component.hpp"
#include "../core/object.hpp"
#include "../core/colliders.hpp"

namespace enjin2 {

class C_Body : public Component {
public:
    explicit C_Body(Object* owner)
        : Component(owner)
        , m_body()
    {}

    // ── Body state ────────────────────────────────────────────────────────────

    void setPosition(float x, float y) { m_body.x = x; m_body.y = y; }
    void setVelocity(float vx, float vy) { m_body.vx = vx; m_body.vy = vy; }
    void setRadius(float r) { m_body.radius = r; }
    void setRestitution(float e) { m_body.restitution = e; }
    void setDrag(float d) { m_body.drag = d; }

    float getX() const { return m_body.x; }
    float getY() const { return m_body.y; }
    float getVX() const { return m_body.vx; }
    float getVY() const { return m_body.vy; }
    float getRadius() const { return m_body.radius; }
    float getRestitution() const { return m_body.restitution; }
    float getDrag() const { return m_body.drag; }

    const BodyState& state() const { return m_body; }

    // ── Step configuration ────────────────────────────────────────────────────

    /// Gravity acceleration (px/s²). Downward on screen = +y (default 320, pinball).
    void setGravity(float gx, float gy) { m_gx = gx; m_gy = gy; }
    float gravityX() const { return m_gx; }
    float gravityY() const { return m_gy; }

    /// Number of fixed-dt substeps per frame (default 4, ADR-0003 §4).
    void setSubsteps(int n) { m_substeps = n > 0 ? n : 1; }
    int getSubsteps() const { return m_substeps; }

    /// true = swept + depenetration; false = naive discrete (tunnels).
    void setSwept(bool s) { m_swept = s; }
    bool isSwept() const { return m_swept; }

    /// Which collider set this body steps against (scene-level resource, non-owning).
    void setColliders(ColliderSet* set) { m_colliders = set; }
    ColliderSet* getColliders() const { return m_colliders; }

    // ── Stepping + contact buffer (polled, no callbacks) ─────────────────────

    /// Advance by one frame (dt seconds) against the active collider set. Clears
    /// and refills the contact buffer; the applet then polls numContacts()/contact(i).
    void step(float dt) {
        m_contacts.clear();
        if (m_colliders == nullptr) return;
        stepFrame(m_body, *m_colliders, m_gx, m_gy, dt, m_substeps, m_swept,
                  m_contacts, m_stats);
    }

    /// Number of contacts buffered by the most recent step().
    size_t numContacts() const { return m_contacts.size(); }

    /// The i-th buffered contact (0-based), or nullptr when out of range.
    const Contact* contact(size_t i) const {
        return i < m_contacts.size() ? &m_contacts[i] : nullptr;
    }

    /// Health counters from the most recent step().
    const StepStats& stats() const { return m_stats; }

private:
    BodyState m_body;
    float m_gx{0.0f};
    float m_gy{320.0f};
    int m_substeps{4};
    bool m_swept{true};
    ColliderSet* m_colliders{nullptr};
    std::vector<Contact> m_contacts;
    StepStats m_stats;
};

} // namespace enjin2
