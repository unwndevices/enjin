#include "enjin2/ui/dialog.hpp"
#include "enjin2/graphics/canvas.hpp"

#include <cstdlib>
#include <iostream>

using namespace enjin2;

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << "FAIL: " << msg << " (line " << __LINE__ << ")\n"; \
        std::exit(1); \
    } \
} while (0)

namespace {

DialogOptions narrowOptions() {
    DialogOptions options;
    options.box = Rect(2, 2, 40, 18);
    options.style = {Pixel4(1), Pixel4(2), Pixel4(3), 1, 0, 1, 0, 0, 0, 0};
    options.cps = 10.0f;
    return options;
}

void testPaginationAndAdvanceFsm() {
    Dialog dialog;
    dialog.show("one two three", narrowOptions());

    ASSERT(dialog.active(), "show activates the singleton dialog");
    ASSERT(dialog.revealing(), "the first page starts revealing");
    ASSERT(dialog.page() == 1, "pages are polled one-based");
    ASSERT(dialog.pageCount() == 3, "word wrap and box height paginate immediately");
    ASSERT(!dialog.done(), "a newly shown dialog is not done");

    dialog.update(0.05f);
    ASSERT(dialog.revealedCharacters() == 0, "fractional character time accumulates");
    dialog.update(0.05f);
    ASSERT(dialog.revealedCharacters() == 1, "reveal timing is measured in seconds");

    dialog.advance();
    ASSERT(!dialog.revealing(), "advance while revealing completes the page");
    dialog.advance();
    ASSERT(dialog.page() == 2 && dialog.revealing(), "advance moves to the next page");
    dialog.advance();
    dialog.advance();
    dialog.advance();
    dialog.advance();
    ASSERT(!dialog.active() && dialog.done(), "advance on the revealed last page closes naturally");
}

void testTimingIsFrameRateIndependent() {
    Dialog oneStep;
    Dialog splitSteps;
    DialogOptions options = narrowOptions();
    options.box = Rect(0, 0, 100, 20);
    options.cps = 30.0f;
    oneStep.show("abcdefghij", options);
    splitSteps.show("abcdefghij", options);

    oneStep.update(0.2f);
    for (int i = 0; i < 12; ++i) splitSteps.update(1.0f / 60.0f);
    ASSERT(oneStep.revealedCharacters() == 6, "one update reveals cps * elapsed characters");
    ASSERT(splitSteps.revealedCharacters() == oneStep.revealedCharacters(),
           "frame partitions produce the same reveal count");
}

void testExplicitBreakHideAndDraw() {
    Dialog dialog;
    DialogOptions options = narrowOptions();
    static const uint8_t portraitPixels[] = {4, 4, 4, 4};
    const SpriteSheet portrait(portraitPixels, 2, 2, 1, 1);
    options.portrait = &portrait;
    options.portraitFrame = 0;
    dialog.show("a\fb", options);
    ASSERT(dialog.pageCount() == 2, "form feed forces an explicit page break");

    Canvas4<48, 24> canvas;
    canvas.clear(Pixel4(0));
    dialog.advance();
    dialog.draw(canvas);
    ASSERT(canvas.getPixel(3, 3).value == 1, "draw fills the configured box");
    ASSERT(canvas.getPixel(2, 2).value == 3, "draw strokes the configured border");
    ASSERT(canvas.getPixel(4, 4).value == 4, "draw renders the optional portrait frame");

    dialog.hide();
    ASSERT(!dialog.active() && !dialog.done(), "hide cancels without reporting natural completion");
}

} // namespace

int main() {
    testPaginationAndAdvanceFsm();
    testTimingIsFrameRateIndependent();
    testExplicitBreakHideAndDraw();
    std::cout << "dialog tests passed\n";
    return 0;
}
