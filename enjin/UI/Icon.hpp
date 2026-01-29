#ifndef ICON_HPP
#define ICON_HPP

#include "Components/C_Animation.hpp"
#include "Components/C_Sprite.hpp"
#include "Object.hpp"

namespace enjin
{
  class Icon : public Object
  {
  public:
    Icon(uint8_t width, uint8_t height, const uint8_t *data = nullptr)
    {
      position->SetPosition(Vector2(63, 63));

      sprite = AddComponent<C_Sprite>(width, height);
      sprite->SetDrawLayer(DrawLayer::UI);
      sprite->SetBlendMode(BlendMode::Normal);
      if (data)
      {
        Load(data, width, height);
      }
      InitAnimation();
    }

    void Load(const uint8_t *data, uint8_t width, uint8_t height)
    {
      sprite->Load(data, width, height);
    }

    void SetVisibility(bool visibility) { sprite->SetVisibility(visibility); }

    void Transition(bool in)
    {
      if (in)
      {
        SetVisibility(true);
        pos_transition->SetAnimation(pos_animation_in);
        pos_transition->StartAnimation(true);
      }
      else
      {
        pos_transition->SetAnimation(pos_animation_out);
        pos_transition->StartAnimation();
      }
    }

    // Convenience wrappers so external code can manipulate position and offset
    void SetPosition(int16_t x, int16_t y) { position->SetPosition(x, y); }
    void SetPosition(Vector2 pos) { position->SetPosition(pos); }

    void AddOffset(Vector2 offset) { sprite->AddOffset(offset); }
    void AddOffset(int16_t x, int16_t y) { sprite->AddOffset(Vector2(x, y)); }

    void SetAnchorPoint(Anchor anchor) { sprite->SetAnchorPoint(anchor); }

    PositionAnimation pos_animation_in, pos_animation_out;
    std::shared_ptr<C_PositionAnimator> pos_transition;

    void InitAnimation()
    {
      pos_transition = AddComponent<C_PositionAnimator>();
      pos_animation_in.AddKeyframe({0, Vector2(63, 128), Easing::Step});
      pos_animation_in.AddKeyframe({90, Vector2(63, 63), Easing::EaseOutQuart});

      pos_animation_out.AddKeyframe({0, Vector2(63, 63), Easing::Step});
      pos_animation_out.AddKeyframe({200, Vector2(63, 128), Easing::EaseInQuad});
    }

  private:
    std::shared_ptr<C_Sprite> sprite;
  };
} // namespace enjin

#endif // ICON_HPP