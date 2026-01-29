#ifndef ORBITUI_HPP
#define ORBITUI_HPP

#include <cstdint>

#include "../Components/C_Canvas.hpp"
#include "../Components/C_Draw.hpp"
#include "../Components/C_Label.hpp"
#include "../Components/C_PositionAnimator.hpp"
#include "../Components/C_Sprite.hpp"
#include "../Object.hpp"

#include "../utils/Utils.hpp"
#include "Fonts/numbers.h"
#include "assets/orbit_pos.h"
#include "assets/orbit_neg.h"
#include "assets/orbit_hz.h"
#include "assets/orbit_mult.h"
#include "assets/orbit_name.h"
#include "assets/orbit_target.h"

namespace enjin
{
  class OrbitUI : public Object
  {
  public:
    OrbitUI()
    {
      position->SetPosition(63, 63);
      draw = AddComponent<C_Draw>([this](EiseiCanvas &canvas)
                                  { Draw(canvas); });
      draw->SetDrawLayer(DrawLayer::UI);
      draw->SetBlendMode(BlendMode::Normal);
      draw->SetAnchorPoint(Anchor::CENTER_);
      draw->SetVisibility(false);

      base_label = AddComponent<C_Label>(80, 40, &numbers28pt7b, 1);
      base_label->SetDrawLayer(DrawLayer::UI);
      base_label->SetAnchorPoint(Anchor::CENTER_LEFT);
      base_label->AddOffset(Vector2(-43, 2));
      base_label->SetVisibility(false);

      hz_unit = AddComponent<C_Sprite>(static_cast<uint8_t>(orbit_hz_width),
                                       static_cast<uint8_t>(orbit_hz_height));
      hz_unit->SetDrawLayer(DrawLayer::UI);
      hz_unit->SetBlendMode(BlendMode::Normal);
      hz_unit->SetAnchorPoint(Anchor::CENTER_);
      hz_unit->AddOffset(Vector2(29, -4));
      hz_unit->SetVisibility(false);
      hz_unit->setMatte(0x0);
      hz_unit->setMonochromePalette(0xFF, 0x00);
      hz_unit->loadMonochrome(orbit_hz_data_1bit, orbit_hz_width, orbit_hz_height);

      mult_unit = AddComponent<C_Sprite>(static_cast<uint8_t>(orbit_mult_width),
                                         static_cast<uint8_t>(orbit_mult_height));
      mult_unit->SetDrawLayer(DrawLayer::UI);
      mult_unit->SetBlendMode(BlendMode::Normal);
      mult_unit->SetAnchorPoint(Anchor::CENTER_);
      mult_unit->AddOffset(Vector2(29, 6));
      mult_unit->SetVisibility(false);
      mult_unit->setMatte(0x0);
      mult_unit->setMonochromePalette(0xFF, 0x00);
      mult_unit->loadMonochrome(orbit_mult_data_1bit, orbit_mult_width, orbit_mult_height);

      pos_icon = AddComponent<C_Sprite>(static_cast<uint8_t>(orbit_pos_width),
                                        static_cast<uint8_t>(orbit_pos_height));
      pos_icon->SetDrawLayer(DrawLayer::UI);
      pos_icon->SetBlendMode(BlendMode::Normal);
      pos_icon->SetAnchorPoint(Anchor::CENTER_);
      pos_icon->AddOffset(Vector2(18, -23));
      pos_icon->SetVisibility(false);
      pos_icon->setMatte(0x0);
      pos_icon->setMonochromePalette(0xFF, 0x00);
      pos_icon->loadMonochrome(orbit_pos_data_1bit, orbit_pos_width, orbit_pos_height);

      neg_icon = AddComponent<C_Sprite>(static_cast<uint8_t>(orbit_neg_width),
                                        static_cast<uint8_t>(orbit_neg_height));
      neg_icon->SetDrawLayer(DrawLayer::UI);
      neg_icon->SetBlendMode(BlendMode::Normal);
      neg_icon->SetAnchorPoint(Anchor::CENTER_);
      neg_icon->AddOffset(Vector2(-18, -23));
      neg_icon->SetVisibility(false);
      neg_icon->setMatte(0x0);
      neg_icon->setMonochromePalette(0xFF, 0x00);
      neg_icon->loadMonochrome(orbit_neg_data_1bit, orbit_neg_width, orbit_neg_height);

      target_icon = AddComponent<C_Sprite>(static_cast<uint8_t>(orbit_target_frame_width),
                                           static_cast<uint8_t>(orbit_target_frame_height));
      target_icon->SetDrawLayer(DrawLayer::UI);
      target_icon->SetBlendMode(BlendMode::Normal);
      target_icon->SetAnchorPoint(Anchor::CENTER_);
      target_icon->AddOffset(Vector2(0, -25));
      target_icon->SetVisibility(false);
      target_icon->setMatte(0x0);
      target_icon->setMonochromePalette(0xFF, 0x00);
      target_icon->loadMonochrome(orbit_target_frames_1bit[0], orbit_target_frame_width, orbit_target_frame_height);

      view_name = AddComponent<C_Sprite>(static_cast<uint8_t>(orbit_name_frame_width),
                                         static_cast<uint8_t>(orbit_name_frame_height));
      view_name->SetDrawLayer(DrawLayer::UI);
      view_name->SetBlendMode(BlendMode::Normal);
      view_name->SetAnchorPoint(Anchor::CENTER_);
      view_name->AddOffset(Vector2(0, 24));
      view_name->SetVisibility(false);
      view_name->setMatte(0x0);
      view_name->setMonochromePalette(0xFF, 0x00);
      view_name->loadMonochrome(orbit_name_frames_1bit[0], orbit_name_frame_width, orbit_name_frame_height);

      SetBaseHz(1.0);

      InitAnimation();

      RefreshViewState();
    };

    void EnterTransition() { SetVisibility(true); }

    void ExitTransition() { SetVisibility(false); };

    void SetVisibility(bool visibility)
    {
      draw->SetVisibility(visibility);
      base_label->SetVisibility(visibility);
      pos_icon->SetVisibility(visibility);
      neg_icon->SetVisibility(visibility);
      mult_unit->SetVisibility(visibility);
      hz_unit->SetVisibility(visibility);
      view_name->SetVisibility(visibility);
      target_icon->SetVisibility(visibility);
      RefreshViewState();
    }

    void Draw(EiseiCanvas &canvas)
    {
      // Draw the background
      canvas.fillCircle(position->x, position->y, 40, 0);
      canvas.drawCircle(position->x, position->y, 40, 14);
      canvas.drawLine(position->x - 31, position->y - 13, position->x + 31, position->y - 13, 4);
      canvas.drawLine(position->x - 31, position->y + 15, position->x + 31, position->y + 15, 4);
    }

    void Update(uint16_t deltaTime) override { Object::Update(deltaTime); };

    void SetBaseHz(float hz)
    {
      this->speed = hz;
      if (current_satellite == -1)
      {
        DisplaySpeedValue();
      }
    };

    void SetMultiplier(uint8_t index, float multiplier)
    {
      if (index >= 4)
      {
        return;
      }

      this->multiplier[index] = multiplier;
      if (current_satellite == static_cast<int8_t>(index))
      {
        DisplayMultiplierValue(index);
      }
    };

    void ViewHz()
    {
      current_satellite = -1;
      RefreshViewState();
    }

    void ViewMultiplier(uint8_t index)
    {
      if (index > 3U)
      {
        index = 3U;
      }

      current_satellite = static_cast<int8_t>(index);
      RefreshViewState();
    }

    PositionAnimation pos_animation_in, pos_animation_out;
    std::shared_ptr<C_PositionAnimator> pos_transition;

  private:
    std::shared_ptr<C_Draw> draw;
    std::shared_ptr<C_Label> base_label;
    std::shared_ptr<C_Sprite> pos_icon;
    std::shared_ptr<C_Sprite> neg_icon;
    std::shared_ptr<C_Sprite> mult_unit;
    std::shared_ptr<C_Sprite> hz_unit;
    std::shared_ptr<C_Sprite> view_name;
    std::shared_ptr<C_Sprite> target_icon;
    float speed = 1.0f;
    float multiplier[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    int8_t current_satellite = -1;

    void RefreshViewState()
    {
      if (!mult_unit || !hz_unit || !target_icon || !view_name)
      {
        return;
      }

      if (current_satellite < 0)
      {
        mult_unit->setMonochromePalette(0x02U, 0x00);
        hz_unit->setMonochromePalette(0xFFU, 0x00);
        target_icon->LoadFrame(0U);
        view_name->LoadFrame(0U);
        DisplaySpeedValue();
      }
      else
      {
        const uint8_t index = ClampMultiplierIndex(current_satellite);
        mult_unit->setMonochromePalette(0xFFU, 0x00);
        hz_unit->setMonochromePalette(0x02U, 0x00);
        target_icon->LoadFrame(static_cast<uint8_t>(index + 1U));
        view_name->LoadFrame(1U);
        DisplayMultiplierValue(index);
      }
    }

    void DisplaySpeedValue()
    {
      base_label->SetString(formatHz(speed));
      UpdatePolarityIcons(speed);
    }

    void DisplayMultiplierValue(uint8_t index)
    {
      if (index >= 4)
      {
        return;
      }

      base_label->SetString(formatMult(multiplier[index]));
      UpdatePolarityIcons(multiplier[index]);
    }

    void UpdatePolarityIcons(float value)
    {
      if (!pos_icon || !neg_icon)
      {
        return;
      }

      if (value > 0.0f)
      {
        pos_icon->setMonochromePalette(0xFFU, 0x00);
        neg_icon->setMonochromePalette(0x02U, 0x00);
      }
      else if (value < 0.0f)
      {
        pos_icon->setMonochromePalette(0x02U, 0x00);
        neg_icon->setMonochromePalette(0xFFU, 0x00);
      }
      else
      {
        pos_icon->setMonochromePalette(0x02U, 0x00);
        neg_icon->setMonochromePalette(0x02U, 0x00);
      }
    }

    uint8_t ClampMultiplierIndex(int8_t index) const
    {
      if (index < 0)
      {
        return 0U;
      }
      if (index > 3)
      {
        return 3U;
      }
      return static_cast<uint8_t>(index);
    }
    void InitAnimation() {}
  };
} // namespace enjin

#endif // OrbitUI_HPP
