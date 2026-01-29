#ifndef ANIMATEDICON_HPP
#define ANIMATEDICON_HPP

#include "Components/C_AnimatedIcon.hpp"
#include "Components/C_PositionAnimator.hpp"
#include "Object.hpp"

namespace enjin
{
  class AnimatedIcon : public Object
  {
  public:
    AnimatedIcon()
    {
      // Default position to center
      position->SetPosition(Vector2(63, 63));
      
      // Create the animated icon component
      animatedIcon = AddComponent<C_AnimatedIcon>();
      animatedIcon->SetDrawLayer(DrawLayer::UI);
      animatedIcon->SetBlendMode(BlendMode::Normal);
      
      // Initialize position animator for transitions
      InitAnimation();
    }
    
    // Set custom draw function for the icon
    void SetDrawFunction(C_AnimatedIcon::DrawFunction func)
    {
      animatedIcon->SetDrawFunction(func);
    }
    
    // Set custom update function for animation logic
    void SetUpdateFunction(C_AnimatedIcon::UpdateFunction func)
    {
      animatedIcon->SetUpdateFunction(func);
    }
    
    // Animation parameter control (-1 to 1)
    void SetParameter(float param)
    {
      animatedIcon->SetParameter(param);
    }
    
    float GetParameter() const
    {
      return animatedIcon->GetParameter();
    }
    
    // Visibility control
    void SetVisibility(bool visibility)
    {
      animatedIcon->SetVisibility(visibility);
    }
    
    // Blend mode control
    void SetBlendMode(BlendMode mode)
    {
      animatedIcon->SetBlendMode(mode);
    }
    
    // Draw layer control
    void SetDrawLayer(DrawLayer layer)
    {
      animatedIcon->SetDrawLayer(layer);
    }
    
    // Transition animations
    void Transition(bool in)
    {
      if (in)
      {
        SetVisibility(true);
        posTransition->SetAnimation(posAnimationIn);
        posTransition->StartAnimation(true);
      }
      else
      {
        posTransition->SetAnimation(posAnimationOut);
        posTransition->StartAnimation();
      }
    }
    
    // Position control wrappers
    void SetPosition(int16_t x, int16_t y) { position->SetPosition(x, y); }
    void SetPosition(Vector2 pos) { position->SetPosition(pos); }
    
    // Offset control
    void AddOffset(Vector2 offset) { animatedIcon->AddOffset(offset); }
    void AddOffset(int16_t x, int16_t y) { animatedIcon->AddOffset(Vector2(x, y)); }
    
    // Anchor point control
    void SetAnchorPoint(Anchor anchor) { animatedIcon->SetAnchorPoint(anchor); }
    
    // Get the component for direct access if needed
    std::shared_ptr<C_AnimatedIcon> GetAnimatedIcon() { return animatedIcon; }
    
  private:
    void InitAnimation()
    {
      posTransition = AddComponent<C_PositionAnimator>();
      
      // Default transition in: slide from bottom
      posAnimationIn.AddKeyframe({0, Vector2(63, 128), Easing::Step});
      posAnimationIn.AddKeyframe({90, Vector2(63, 63), Easing::EaseOutQuart});
      
      // Default transition out: slide to bottom
      posAnimationOut.AddKeyframe({0, Vector2(63, 63), Easing::Step});
      posAnimationOut.AddKeyframe({200, Vector2(63, 128), Easing::EaseInQuad});
    }
    
    std::shared_ptr<C_AnimatedIcon> animatedIcon;
    std::shared_ptr<C_PositionAnimator> posTransition;
    PositionAnimation posAnimationIn, posAnimationOut;
  };
}

#endif // ANIMATEDICON_HPP