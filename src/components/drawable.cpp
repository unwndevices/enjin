#include "../../include/enjin2/components/drawable.hpp"
#include "../../include/enjin2/core/object.hpp"

namespace enjin2 {

// Static member definition (matches original Enjin abs_center)
Point C_Drawable::abs_center(63, 63);

C_Drawable::C_Drawable(Object* owner, uint8_t width, uint8_t height)
    : Component(owner)
    , position(nullptr)
    , anchor_offset(0, 0)
    , buffer_index(0)
    , blend_mode(BlendMode::Normal)
    , anchor(Anchor::TOP_LEFT)
    , is_visible(true)
    , width(width)
    , height(height)
{
    // Get position component from owner (matches original Enjin pattern)
    position = owner->getComponent<C_Position>();
}

bool C_Drawable::continueToDraw() const {
    // Check if object is not queued for removal (matches original Enjin)
    return !owner->isQueuedForRemoval();
}

void C_Drawable::SetAnchorPoint(Anchor anchor) {
    this->anchor = anchor;
    
    // Calculate anchor offset based on anchor point (matches original Enjin exactly)
    switch (anchor) {
        case Anchor::TOP_LEFT:
            anchor_offset.x = 0;
            anchor_offset.y = 0;
            break;
        case Anchor::TOP_CENTER:
            anchor_offset.x = width / 2;
            anchor_offset.y = 0;
            break;
        case Anchor::TOP_RIGHT:
            anchor_offset.x = width;
            anchor_offset.y = 0;
            break;
        case Anchor::CENTER_LEFT:
            anchor_offset.x = 0;
            anchor_offset.y = height / 2;
            break;
        case Anchor::CENTER:
            anchor_offset.x = width / 2;
            anchor_offset.y = height / 2;
            break;
        case Anchor::CENTER_RIGHT:
            anchor_offset.x = width;
            anchor_offset.y = height / 2;
            break;
        case Anchor::BOTTOM_LEFT:
            anchor_offset.x = 0;
            anchor_offset.y = height;
            break;
        case Anchor::BOTTOM_CENTER:
            anchor_offset.x = width / 2;
            anchor_offset.y = height;
            break;
        case Anchor::BOTTOM_RIGHT:
            anchor_offset.x = width;
            anchor_offset.y = height;
            break;
    }
}

Point C_Drawable::GetOffsetPosition() const {
    if (!position) {
        return Point(0, 0);
    }
    
    // Return position with anchor offset applied (matches original Enjin)
    Point pos = position->getPosition() - anchor_offset;
    return pos;
}

} // namespace enjin2