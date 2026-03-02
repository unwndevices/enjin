The Good: Simple and Intuitive Parts
Graphics API Structure: The Love2D-inspired structure works exceptionally well. Having functions like 
setColor(c)
, 
rectangle()
, 
circle()
, and 
line()
 made it trivial to draw all the assets procedurally without needing any sprites.
Built-in Collision Mechanics: Having native C++ math backing operations like engine.collision.aabbOverlap(x1, y1, w1, h1, x2, y2, w2, h2) is a huge productivity booster. Returning the overlap area allows for fast, robust resolution (deciding whether to flip vx or vy based on whether ow < oh).
Structured API Tables: Putting isolated functionalities into engine.* tables like engine.time.now(), engine.input.held(), and engine.random.float() yields auto-complete-friendly, easily discoverable code.
Immediate-Mode Input: Functions like engine.input.just_pressed(BTN_Z) made implementing the title/serve/gameover screens effortless.
The Clunky: Hacky and Non-Optimal Areas
Rigid Frame Management: The boilerplate around the sprite_sdl_test execution is still not quite a fully unified game engine. We had to modify the C++ host to change the canvas size from 128x128 to 320x240. Lua shouldn't need a recompilation of the host to dictate canvas resolution.
Object Creation vs Manual State: We didn't use the Entity Component System (ECS) — i.e., engine.scene.spawn() — for the Arkanoid game because ECS in Lua right now is tightly bound to 
ObjectProxy
 logic where managing many objects (60+ bricks) seemed riskier and heavier than a simple 2D loop over a table (bricks[r][c] = true). In simple arcade games, pure Lua state management is easier.
Missing "State" Abstractions: While C_StateMachine is exposed for individual objects, there isn't a global "gamestate" manager exposed to Lua. State transitions like Title -> Serve -> Play had to be handwritten globally as local state = "title" wrapping the update/draw logic.
Drawing Coordinate Types: Some API calls silently expect integers and drop floats, or vice versa. Wrapping positions in math.floor() before drawing is technically safe but slightly annoying. It would be nice if the bindings naturally cast/floored float coordinates internally before dispatching to the 4-bit canvas.

---------------------

Game Loop: The update(dt) and draw() lifecycle is standard, predictable, and immediately familiar to anyone who's used PICO-8, love2d, or TIC-80.
State Machine Binding: Mapping logic to the delta time frame (
dt
) allows predictable stat decay and animation frame advancement seamlessly.
Global Drawing Context: The stateful drawing functions (clear, setColor, rectangle, circle, text) are straightforward and simple to chain without having to pass context handles around.
Input Polling: The engine.input.just_pressed() and engine.input.held() functions are very easy to query inside states.
Clunky, Hacky & Non-Optimal Parts
Magic Constants Verbosity:
There are no built-in Lua tables exposing engine constants (e.g., no engine.colors.RED or engine.buttons.LEFT).
Every script requires a bloated preamble that manually copies integer constants for buttons (BTN_LEFT = 2) and palettes (C_RED = 8). This makes scripts needlessly verbose and prone to breakage if the host mapping changes.
Lack of Text Centering/Formatting Helpers:
Calculating UI layouts (e.g., centering the interaction text) forces the developer to manually calculate pixel widths (either using an untested global getTextWidth or hacky string length estimations string.len(text) * 6).
Passing w / 2 forces manual math.floor() usage because Lua numbers are floats, and non-integer pixel coordinates could cause weird rendering artifacting.
Stateful Text Scale Management:
Drawing text with different sizes requires toggling global state using setTextSize() before drawing, then resetting it back if needed elsewhere. Passing scale directly in text(str, x, y, scale) would be far less error-prone.
Global Namespace Pollution:
High-level engine subsystems live beneath engine.* (e.g., engine.input, engine.collision), but drawing primitives (circle, text, setColor) are dumped directly into the global namespace. Grouping them inside engine.graphics.* or engine.draw.* would be cleaner.
UI & Progress Bars:
Creating simple UI items like stat bars requires raw rectangle math (
(val / max) * w
). The lack of built-in high-level UI drawing components means every script implements standard progress bars entirely from scratch.
Geometry as Sprites:
Since managing sprites is slightly heavy if you want basic shapes, creating dynamic animated characters out of geometry requires writing a lot of hardcoded offset math (petX - 10, petY - 5). This code isn't easily reusable across games.
