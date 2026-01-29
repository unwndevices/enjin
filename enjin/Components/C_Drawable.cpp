#include "C_Drawable.hpp"

namespace enjin
{

    Vector2 C_Drawable::abs_center(63, 63);

    C_Drawable::C_Drawable(uint8_t width, uint8_t height) : anchorOffset(Vector2(0, 0)), sortOrder(0), layer(DrawLayer::Default), blendMode(BlendMode::Normal), is_visible(true), width(width), height(height)
    {
    }

    C_Drawable::~C_Drawable() {}

    void C_Drawable::SetSortOrder(int order)
    {
        sortOrder = order;
    }

    int C_Drawable::GetSortOrder() const
    {
        return sortOrder;
    }

    void C_Drawable::SetDrawLayer(DrawLayer drawLayer)
    {
        layer = drawLayer;
    }

    DrawLayer C_Drawable::GetDrawLayer() const
    {
        return layer;
    }

    void C_Drawable::SetAnchorPoint(Anchor anchor)
    {
        this->anchor = anchor;
        switch (anchor)
        {
        case Anchor::TOP_LEFT:
            anchorOffset.x = 0;
            anchorOffset.y = 0;
            break;
        case Anchor::TOP_RIGHT:
            anchorOffset.x = width;
            anchorOffset.y = 0;
            break;
        case Anchor::BOTTOM_LEFT:
            anchorOffset.x = 0;
            anchorOffset.y = height;
            break;
        case Anchor::BOTTOM_RIGHT:
            anchorOffset.x = width;
            anchorOffset.y = height;

            break;
        case Anchor::CENTER_:
            anchorOffset.x = width / 2;
            anchorOffset.y = height / 2;
            break;
        case Anchor::CENTER_LEFT:
            anchorOffset.x = 0;
            anchorOffset.y = height / 2;
            break;
        case Anchor::CENTER_RIGHT:
            anchorOffset.x = width;
            anchorOffset.y = height / 2;
            break;
        case Anchor::CENTER_TOP:
            anchorOffset.x = width / 2;
            anchorOffset.y = 0;
            break;
        case Anchor::CENTER_BOTTOM:
            anchorOffset.x = width / 2;
            anchorOffset.y = height;
            break;
        }
    }
}