#ifndef ANIMATEDICONEXAMPLES_HPP
#define ANIMATEDICONEXAMPLES_HPP

#include "AnimatedIcon.hpp"
#include <cmath>

namespace enjin
{
  // Example 1: Pulsing Circle Icon
  class PulsingCircleIcon : public AnimatedIcon
  {
  public:
    PulsingCircleIcon()
    {
      // Set the draw function
      SetDrawFunction([](EiseiCanvas& canvas, const Vector2& pos, float param) {
        // Map parameter to radius (param: -1 to 1 -> radius: 5 to 15)
        int radius = 10 + static_cast<int>(param * 5);
        
        // Draw filled circle
        canvas.fillCircle(pos.x, pos.y, radius, 15); // color 15 (white)
        
        // Draw outer ring
        canvas.drawCircle(pos.x, pos.y, radius + 2, 8); // color 8 (gray)
      });
    }
  };
  
  // Example 2: Rotating Square Icon
  class RotatingSquareIcon : public AnimatedIcon
  {
  public:
    RotatingSquareIcon()
    {
      SetDrawFunction([](EiseiCanvas& canvas, const Vector2& pos, float param) {
        // Map parameter to rotation angle (param: -1 to 1 -> angle: -pi to pi)
        float angle = param * M_PI;
        int size = 12;
        
        // Calculate corner points
        float cos_a = std::cos(angle);
        float sin_a = std::sin(angle);
        
        // Define square corners (relative to center)
        float corners[4][2] = {
          {-size/2.0f, -size/2.0f},
          { size/2.0f, -size/2.0f},
          { size/2.0f,  size/2.0f},
          {-size/2.0f,  size/2.0f}
        };
        
        // Rotate and draw lines
        for (int i = 0; i < 4; i++)
        {
          int next = (i + 1) % 4;
          
          // Rotate points
          int x1 = pos.x + cos_a * corners[i][0] - sin_a * corners[i][1];
          int y1 = pos.y + sin_a * corners[i][0] + cos_a * corners[i][1];
          int x2 = pos.x + cos_a * corners[next][0] - sin_a * corners[next][1];
          int y2 = pos.y + sin_a * corners[next][0] + cos_a * corners[next][1];
          
          canvas.drawLine(x1, y1, x2, y2, 15);
        }
      });
    }
  };
  
  // Example 3: Loading Dots Icon
  class LoadingIcon : public AnimatedIcon
  {
  public:
    LoadingIcon()
    {
      SetDrawFunction([](EiseiCanvas& canvas, const Vector2& pos, float param) {
        int radius = 10;
        int numDots = 8;
        
        // Map parameter to active dot position
        float activePos = (param + 1.0f) * 4.0f; // -1 to 1 -> 0 to 8
        
        for (int i = 0; i < numDots; i++)
        {
          float angle = (i / static_cast<float>(numDots)) * 2.0f * M_PI;
          
          // Calculate brightness based on distance from active position
          float dist = std::abs(i - activePos);
          if (dist > numDots/2) dist = numDots - dist; // Wrap around
          float brightness = 1.0f - (dist / (numDots/2.0f));
          
          int x = pos.x + radius * std::cos(angle);
          int y = pos.y + radius * std::sin(angle);
          
          // Use brightness to vary color intensity
          uint8_t color = 8 + static_cast<uint8_t>(brightness * 7); // 8-15
          canvas.fillCircle(x, y, 1, color);
        }
      });
    }
  };
  
  // Example 4: Wave Pattern Icon
  class WaveIcon : public AnimatedIcon
  {
  public:
    WaveIcon()
    {
      SetDrawFunction([](EiseiCanvas& canvas, const Vector2& pos, float param) {
        int width = 20;
        float frequency = 0.2f;
        
        // Map parameter to wave phase and amplitude
        float phase = param * M_PI * 2.0f; // -1 to 1 -> -2pi to 2pi
        float amplitude = 5.0f + param * 3.0f; // -1 to 1 -> 2 to 8
        
        for (int x = -width; x <= width; x++)
        {
          float wave = amplitude * std::sin(x * frequency + phase);
          int y = pos.y + static_cast<int>(wave);
          
          canvas.drawPixel(pos.x + x, y, 15);
          
          // Draw second wave phase-shifted
          float wave2 = amplitude * std::sin(x * frequency + phase + M_PI);
          int y2 = pos.y + static_cast<int>(wave2);
          canvas.drawPixel(pos.x + x, y2, 8);
        }
      });
    }
  };
  
  // Example 5: Morphing Shape Icon
  class MorphingIcon : public AnimatedIcon
  {
  public:
    MorphingIcon()
    {
      SetDrawFunction([](EiseiCanvas& canvas, const Vector2& pos, float param) {
        // Map parameter to morph between triangle and hexagon
        float morph = (param + 1.0f) * 0.5f; // -1 to 1 -> 0 to 1
        int numSides = 3 + static_cast<int>(morph * 3); // 3 to 6 sides
        int radius = 10;
        
        // Draw polygon
        for (int i = 0; i < numSides; i++)
        {
          float angle1 = (i / static_cast<float>(numSides)) * 2.0f * M_PI;
          float angle2 = ((i + 1) / static_cast<float>(numSides)) * 2.0f * M_PI;
          
          int x1 = pos.x + radius * std::cos(angle1);
          int y1 = pos.y + radius * std::sin(angle1);
          int x2 = pos.x + radius * std::cos(angle2);
          int y2 = pos.y + radius * std::sin(angle2);
          
          canvas.drawLine(x1, y1, x2, y2, 15);
        }
      });
    }
  };
}

#endif // ANIMATEDICONEXAMPLES_HPP