#ifndef C_ANIMATEDICON_HPP
#define C_ANIMATEDICON_HPP

#include "C_Drawable.hpp"
#include <functional>
#include <algorithm>

namespace enjin
{
  class C_AnimatedIcon : public C_Drawable
  {
  public:
    // Function types for custom draw and update logic
    using DrawFunction = std::function<void(EiseiCanvas&, const Vector2&, float)>;
    using UpdateFunction = std::function<void(float, float&)>;
    
    C_AnimatedIcon() : animationParameter(0.0f) {}
    
    // Set custom draw function (receives parameter in -1 to 1 range)
    void SetDrawFunction(DrawFunction func)
    {
      drawFunc = func;
    }
    
    // Set custom update function for additional logic
    void SetUpdateFunction(UpdateFunction func)
    {
      updateFunc = func;
    }
    
    // Set animation parameter (-1 to 1)
    void SetParameter(float param)
    {
      animationParameter = std::clamp(param, -1.0f, 1.0f);
    }
    
    // Get current animation parameter
    float GetParameter() const
    {
      return animationParameter;
    }
    
    // Component lifecycle
    void Update(float deltaTime) override
    {
      // Call custom update function if set
      if (updateFunc)
      {
        updateFunc(deltaTime, animationParameter);
      }
    }
    
    // Drawing implementation
    void Draw(EiseiCanvas& canvas) override
    {
      if (!visibility || !drawFunc)
        return;
        
      // Get position from owner
      Vector2 pos = GetOwnerPosition();
      
      // Apply offset
      pos.x += offset.x;
      pos.y += offset.y;
      
      // Call custom draw function with canvas, position, and animation parameter
      drawFunc(canvas, pos, animationParameter);
    }
    
    bool ContinueToDraw() const override
    {
      return visibility;
    }
    
  private:
    DrawFunction drawFunc;
    UpdateFunction updateFunc;
    float animationParameter; // -1 to 1 range
  };
}

#endif // C_ANIMATEDICON_HPP