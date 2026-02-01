---
id: Label
title: Label
sidebar_label: Label
---

# Label

 component for text display with word wrapping and styling. Labelclassenjin2_1_1Labelcompound


A versatile text display component that supports:
Custom fonts (GFX-style fonts)Text wrapping and alignmentBackground colors and bordersTooltip-style pointersOpacity/blend modes 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/components/label.hpp`

## Public Methods

### ` Label(Object *owner, uint16_t w, uint16_t h, const GFXfont *textFont=nullptr, uint8_t fontSize=1, uint8_t textColor=14, uint8_t backgroundColor=0, uint8_t pointerHeight=0)`

Construct a new  component. Labelclassenjin2_1_1Labelcompound

paramownerclassenjin2_1_1Component_1a3349b9210509e148f9f7625f6a39b022memberThe object that owns this component wWidth of the label in pixels hHeight of the label in pixels textFontFont to use (nullptr for default) fontSizeFont size multiplier textColorText color (0-15 for 4-bit grayscale) backgroundColorBackground color (0 for transparent) pointerHeightHeight of tooltip pointer (0 for no pointer) 

---

### `void draw(ICanvas< uint8_t > &canvas)`

Draw the label to the canvas. 

paramcanvasThe canvas to draw to 

---

### `void setText(const std::string &newText)`

Set the label text. 

paramnewTextText to display 

---

### `const std::string & getText() const const`

Get the current text. 

returnCurrent text string 

---

### `void setTextColor(uint8_t color)`

Set text color. 

paramcolorNew text color (0-15) 

---

### `void setBackgroundColor(uint8_t color)`

Set background color. 

paramcolorBackground color (0 for transparent) 

---

### `void setAlignment(LabelAlign align)`

Set text alignment. 

paramalignText alignment mode 

---

### `void setMargins(int16_t left, int16_t right=-1)`

Set margins. 

paramleftLeft margin in pixels rightRight margin in pixels (defaults to left margin) 

---

### `void setWordWrap(bool wrap)`

Enable or disable word wrapping. 

paramwrapTrue to enable word wrapping 

---

### `void setFontSize(uint8_t size)`

Set font size. 

paramsizeFont size multiplier (1 = normal, 2 = double, etc.) 

---

### `void setPointerHeight(uint8_t height)`

Set pointer height for tooltip-style labels. 

paramheightHeight of pointer in pixels (0 for no pointer) 

---

## Private Methods

### `std::vector< std::string > split(const std::string &s, char delimiter) const const`


        


        

---

### `void layoutText()`

Layout and render text to internal canvas. 


        

---

### `void renderText(int16_t padding, int16_t line_spacing, int16_t box_height)`

Render text with word wrapping and alignment. 


        

---

### `int16_t calculateLineX(const std::string &line, int16_t padding)`

Calculate X position for a line based on alignment. 


        

---

