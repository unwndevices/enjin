#ifndef ENJIN2_TESTS_VISUAL_PARITY_FONTS_HPP
#define ENJIN2_TESTS_VISUAL_PARITY_FONTS_HPP

// The three GFX fonts Eisei ships text through, vendored verbatim from the
// unwn repo (Libs/Adafruit-GFX-Library/Fonts/) as bench fixtures for the M3
// text sweep (unwn #158 locked the font set: absolute8pt7b, MN80P1,
// Awkward8pt7b, plus the built-in glcd font, which needs no data here).
// Only the preludes were adapted: the Arduino `#include <Adafruit_GFX.h>`
// is dropped and PROGMEM is a no-op on host. Glyph and bitmap bytes are
// untouched — the bench compares pixels, so the data must be the shipped
// data.

#include <enjin2/graphics/gfxfont.h>

#ifndef PROGMEM
#define PROGMEM
#endif

#include "1980P1.h"   // GFXfont MN80P1
#include "absolute.h" // GFXfont absolute8pt7b
#include "awkward.h"  // GFXfont Awkward8pt7b

#endif // ENJIN2_TESTS_VISUAL_PARITY_FONTS_HPP
