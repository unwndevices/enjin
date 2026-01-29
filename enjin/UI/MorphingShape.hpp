#ifndef MORPHING_SHAPE_HPP
#define MORPHING_SHAPE_HPP

#include "../Object.hpp"
#include "../Components/C_Position.hpp"
#include "../Components/C_MorphingShape.hpp"
#include "../utils/Types.hpp"

namespace enjin
{
    class MorphingShape : public Object
    {
    public:
        MorphingShape(uint16_t max_radius, uint16_t inner_radius_4pt, uint8_t color = 15, int num_vertices = 64)
        {
            position->SetPosition(Vector2(63, 63));
            // Add Morphing Shape Component
            morph_component = AddComponent<C_MorphingShape>(max_radius, inner_radius_4pt, num_vertices, color);
            morph_component->SetDrawLayer(DrawLayer::UI);
            morph_component->SetBlendMode(BlendMode::Normal);
            morph_component->SetAnchorPoint(Anchor::CENTER_); // Center drawing within the component canvas

            SetMorph(0.0f); // Start as a circle
        }

        // Set the morph state (0.0 to 1.0)
        void SetMorph(float position)
        {
            if (morph_component)
            {
                morph_component->SetMorph(position);
            }
        }

        void SetShape(float shape)
        {
            if (morph_component)
            {
                morph_component->SetShape(shape);
            }
        }

        void SetColor(uint8_t color)
        {
            if (morph_component)
            {
                morph_component->SetColor(color);
            }
        }

        void SetRadii(uint16_t max_r, uint16_t inner_r4)
        {
            if (morph_component)
            {
                morph_component->SetRadii(max_r, inner_r4);
            }
        }

    private:
        std::shared_ptr<C_MorphingShape> morph_component;
    };

} // namespace enjin

#endif // MORPHING_SHAPE_HPP