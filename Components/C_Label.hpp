#ifndef C_LABEL_HPP
#define C_LABEL_HPP

#include "C_Drawable.hpp"
#include "C_Position.hpp"
#include "Component.hpp"
#include <memory>
#include <string>
#include <Fonts/absolute.h>

namespace enjin {

class C_Label : public C_Drawable {
public:
  C_Label(Object *owner, uint8_t width, uint8_t height, const GFXfont *font = &absolute8pt7b, uint8_t font_size = 1,
          uint8_t labelColor = 14U, uint8_t bgColor = 0,
          uint8_t pointer = 0); // Added parameters here
  void Awake() override;
  void Update(uint16_t deltaTime) override;
  void Draw(EiseiCanvas &canvas) override;
  bool ContinueToDraw() const override;
  void SetString(std::string string);
  void SetMargins(int left_margin, int right_margin = 0) {
    this->left_margin = left_margin;
    this->right_margin = right_margin;
  };

private:
  std::string string;
  uint8_t text_width, pointer;
  uint8_t labelColor, bgColor;
  const GFXfont *font;
  uint8_t font_size;
  int left_margin, right_margin = 0;
};
} // namespace enjin
#endif // C_LABEL_HPP
