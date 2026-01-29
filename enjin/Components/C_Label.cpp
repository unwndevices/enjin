#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../Object.hpp"
#include "C_Label.hpp"

namespace enjin {
// Helper function to split string by delimiter
std::vector<std::string> split(const std::string &s, char delimiter) {
  std::vector<std::string> tokens;
  std::string token;
  std::istringstream tokenStream(s);
  while (std::getline(tokenStream, token, delimiter)) {
    tokens.push_back(token);
  }
  return tokens;
}

C_Label::C_Label(Object *owner, uint8_t width, uint8_t height, const GFXfont *font,
                 uint8_t font_size, uint8_t labelColor, uint8_t bgColor,
                 uint8_t pointer)
    : Component(owner), C_Drawable(width, height + pointer), font(font),
      font_size(font_size), string(std::string("hey")), text_width(0),
      pointer(pointer), labelColor(labelColor), bgColor(bgColor) {
  position = owner->GetComponent<C_Position>();

  if (!position) {
    std::cerr << "C_Tooltip requires C_Position component.\n";
  }
}

void C_Label::Draw(EiseiCanvas &canvas) {
  uint8_t color = labelColor;
  // --- Configuration ---
  const int16_t padding = 0;      // Horizontal padding inside the box
  const int16_t line_spacing = 4; // Vertical space between lines
  const int16_t default_font_height =
      15; // Approx height for default font size 1

  switch (GetBlendMode()) {
  case BlendMode::Normal:
    color = labelColor;
    break;
  case BlendMode::Opacity50:
    color = labelColor / 2;
    break;
  case BlendMode::Opacity25:
    color = labelColor / 4;
    break;
  default:
    break;
  }

  canvas.setTextColor(color);
  canvas.setTextSize(font_size);
  canvas.setFont(font);

  int16_t text_start_x = GetOffsetPosition().x;
  int16_t text_start_y = GetOffsetPosition().y;
  int16_t box_height = height - pointer;

  // --- Pre-calculation Phase ---
  std::vector<std::string> lines;
  int16_t max_line_width = 0;
  int16_t total_text_height = 0;

  if (!string.empty()) {
    std::vector<std::string> words = split(string, ' ');
    std::string current_line;
    int16_t x1, y1;
    uint16_t word_w, word_h, space_w, space_h;
    canvas.getTextBounds(" ", 0, 0, &x1, &y1, &space_w, &space_h);

    for (const auto &word : words) {
      if (word.empty())
        continue;

      uint16_t current_line_w, current_line_h;
      std::string test_line = current_line;
      if (!test_line.empty()) {
        test_line += " ";
      }
      test_line += word;
      canvas.getTextBounds(test_line.c_str(), 0, 0, &x1, &y1, &current_line_w,
                           &current_line_h);

      if (current_line_w > width - 2 * padding) {
        if (!current_line.empty()) {
          lines.push_back(current_line);
        }
        current_line = word;
      } else {
        current_line = test_line;
      }
    }
    if (!current_line.empty()) {
      lines.push_back(current_line);
    }

    bool first_line = true;
    for (const auto &line : lines) {
      uint16_t line_w, line_h;
      canvas.getTextBounds(line.c_str(), 0, 0, &x1, &y1, &line_w, &line_h);
      if (line_w > max_line_width) {
        max_line_width = line_w;
      }
      if (first_line) {
        first_line = false;
      } else {
        total_text_height += line_spacing;
      }
      total_text_height += line_h;
    }
  }

  // --- Drawing Phase ---
  if (bgColor) {
    canvas.fillRoundRect(text_start_x, text_start_y, width, box_height, 8,
                         bgColor);
    canvas.drawRoundRect(text_start_x, text_start_y, width, box_height, 8,
                         color);
  }

  if (pointer) {
    int16_t pointer_base_y = text_start_y + box_height - 1;
    int16_t pointer_tip_y = text_start_y + box_height + pointer - 1;
    int16_t pointer_center_x = text_start_x + width / 2;
    canvas.fillTriangle(pointer_center_x - 3, pointer_base_y,
                        pointer_center_x + 3, pointer_base_y, pointer_center_x,
                        pointer_tip_y, bgColor);
    canvas.drawLine(pointer_center_x - 3, pointer_base_y, pointer_center_x,
                    pointer_tip_y, color);
    canvas.drawLine(pointer_center_x + 3, pointer_base_y, pointer_center_x,
                    pointer_tip_y, color);
  }

  int16_t text_block_start_y =
      text_start_y + (box_height - total_text_height) / 2;
  if (text_block_start_y < text_start_y) {
    text_block_start_y = text_start_y;
  }

  int16_t current_cursor_y = text_block_start_y;

  for (const auto &line : lines) {
    int16_t x1, y1;
    uint16_t line_w, line_h;
    canvas.getTextBounds(line.c_str(), 0, 0, &x1, &y1, &line_w, &line_h);
    int16_t line_start_x = text_start_x + (width - line_w) / 2;
    if (line_start_x < text_start_x + padding) {
      line_start_x = text_start_x + padding;
    }
    canvas.setCursor(line_start_x - x1, current_cursor_y - y1);
    canvas.print(line.c_str());
    current_cursor_y += line_h + line_spacing;
  }
}

bool C_Label::ContinueToDraw() const { return !owner->IsQueuedForRemoval(); }

void C_Label::Awake() {}

void C_Label::SetString(std::string string) { this->string = string; }

void C_Label::Update(uint16_t deltaTime) {}
} // namespace enjin