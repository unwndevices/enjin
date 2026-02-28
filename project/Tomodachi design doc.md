(v0.1)

Project: tomodachi
Device: portable MIDI + audio control gadget with an embedded scripting runtime for user-made “apps”
Inputs: A, B, encoder (rotary + click) + sensors (ToF, IMU, mic features, piezo, light, etc.
MCU: Esp32s3 or stm32 + external Bluetooth wifi module.
Outputs: screen (320wX240h), MIDI, native audio engine control
On board mems microphone and 3.5mm jack in
Mono speaker, mono/stereo (undecided) 3.5mm out.


---

1. Goals and non-goals

Goals (MVP)

Run user scripts written in Lua inside a C++ embedded runtime.

Provide a canvas/game-style graphics API (shapes + sprites) built on Enjin.

Provide APIs for:

input (A/B/encoder)

sensor reading (processed signals)

MIDI output (send immediately)

audio engine control (no DSP in Lua)


A system shell owns navigation, launching/killing apps, and always provides an escape/home gesture.

A web-based simulator that runs the same C++ core compiled to WASM, with:

virtual device UI

fake sensors UI

MIDI log (optional WebMIDI later)

integrated editor (CodeMirror/Monaco)

remote-updatable distribution (static hosting)



Non-goals (MVP)

Security / signing / sandbox hardening beyond basic stability limits.

Public scripts hub (favorites/comments) implementation.

Persistent save data API.

Tight MIDI scheduling (timestamps). “Send immediately” only.



---

2. Target hardware assumptions

MCU-class device: ESP32-S3 (or faster STM32 class).

Display: 320×240, color.

Timing: 120 Hz logic/control tick; 30 Hz display refresh.

Sprites: 4-bit packed, individual sprites.

Colors: 15 colors palette + transparent

Budgets per app (initial):

code + manifest: 254 KB

assets (sprites): 254 KB

---

3. High-level architecture

3.1 Shared C++ core (single source of truth)

A portable library used by both firmware and simulator:

Lua VM + script lifecycle

Lua bindings (tomodachi API)

App packaging + manifest parsing

Enjin integration layer

Frame scheduling (120 Hz tick) + render cadence (30 Hz)

Stability limits (instruction/time budget; watchdog hooks)


3.2 Platform adapters

Thin layers providing hardware vs browser functionality:

Device adapter: display driver, input drivers, sensor drivers, audio/MIDI backends, storage partitions, Wi‑Fi/BLE.

Web adapter: canvas blit, keyboard/mouse UI controls, fake sensor sliders/pads, file import/export, optional WebMIDI.


Key principle: the Lua API behavior is implemented once in C++ and is identical in device + WASM builds.


---

4. Scheduling and timing model

4.1 Ticks

Logic tick: 120 Hz (dt ≈ 8.333 ms). Drives script update + input/sensor polling.

Render tick: 30 Hz (dt ≈ 33.333 ms). Drives framebuffer presentation.


4.2 Script lifecycle

A script (“app”) may implement:

function init() end (optional)

function update(dt) end (optional; dt in seconds)

event handlers (optional):

function on_button(name, down) end where name ∈ {"A","B","ENC"}

function on_encoder(delta) end (delta is integer ticks)

function on_sensor(name, value) end (optional generic path)



4.3 Stability limits (MVP)

Per-tick time budget for Lua execution (configurable). If exceeded: yield/abort and notify shell.

Hard “home” gesture owned by shell (e.g., A+B long press) that scripts cannot override.



---

5. Display model and Enjin integration

5.1 Framebuffers / layers

Device uses four 4-bit layers (names are conceptual; final names can change):

BG (background)

FG (foreground)

UI (system/app UI)

OVR (overlay/debug)


Composition policy is owned by the shell/renderer (e.g., OR/XOR rules or priority). For MVP, treat layers as stacked with simple overwrite priority.

5.2 Rendering approach (Model B)

Lua gfx.* calls map directly to Enjin drawing operations targeting the selected layer(s).

At 30 Hz, the platform adapter presents the composed frame (device flush; web canvas blit).



---

6. Lua API (v0.1)

The API is intentionally small and canvas-first.

6.1 app

app.width() -> int

app.height() -> int

app.time() -> number (seconds since app start)


6.2 gfx

Frame/target

gfx.setLayer(name) where name ∈ {"BG","FG","UI","OVR"}

gfx.clear(color) color ∈ {0,1}


Primitives

gfx.pixel(x,y,color)

gfx.line(x1,y1,x2,y2,color)

gfx.rect(x,y,w,h,color)

gfx.fillRect(x,y,w,h,color)

gfx.circle(x,y,r,color)

gfx.fillCircle(x,y,r,color)


Sprites

gfx.sprite(id, x, y, flipX?, flipY?)


(Optional dev convenience)

gfx.text(x,y,str,color) (tiny built-in font; may be simulator-only initially)


6.3 input

input.isDown(name) -> bool where name ∈ {"A","B","ENC"}

input.encoderDelta() -> int (delta since last tick)


(Preferred) apps rely on event callbacks (on_button, on_encoder).

6.4 sensors

Expose processed, script-friendly values (units fixed and documented):

sensors.imu.tiltX() -> number (degrees)

sensors.imu.tiltY() -> number (degrees)

sensors.tof.distance() -> number (mm)

sensors.light.level() -> number (0..1)

sensors.mic.level() -> number (0..1 envelope)


Piezo as event-style:

sensors.piezo.onHit(callback) where callback receives velocity (0..1)


6.5 midi

Immediate send (no timestamps in MVP):

midi.noteOn(ch, note, vel) (vel 0..127)

midi.noteOff(ch, note)

midi.cc(ch, cc, value) (value 0..127)


6.6 audio

Lua controls a native engine via commands:

audio.voiceStart(type, freq, amp) -> voiceId

audio.voiceParam(voiceId, name, value)

audio.voiceStop(voiceId)


Type examples: "sine" | "square" | "noise" (final set TBD).


---

7. App packaging

7.1 Bundle format

A single distributable file: .tomo

Container can be a ZIP-like archive.


Contents:

manifest.json

main.lua

sprites/… (4-bit packed)

optional: manual.md (or manual.txt)


7.2 Manifest (minimum)

id (string, stable)

name

author

version

api_version

entry (e.g., "main.lua")

assets (sprite list: {name,id,w,h,path})

uses (declared usage for manual/system UI; not enforced):

e.g., ["midi","imu","tof","mic","piezo","light","audio"]


manual (optional path)


7.3 Manual page

If manual exists, shell can show it in a standard viewer.

Apps may also implement an in-app help screen, but the shell-level manual is the MVP baseline.



---

8. System shell

Responsibilities:

App list (simple list launcher)

Load/unload apps and manage lifecycle

Global input gesture handling (home/exit is always available)

Surface app metadata + manual

Provide runtime error UI (script crash, timeout) and safe recovery



---

9. Web simulator

9.1 Stack (monorepo)

Simulator: Vite + React + TypeScript + Tailwind + shadcn/ui

Rendering: Canvas2D blit of composed 4bit framebuffer

Core: same C++ library compiled to WASM (Emscripten)


9.2 Integrated editor

Monaco or CodeMirror embedded in the simulator

Runs scripts instantly (hot reload) against the WASM core

Supports:

open/save local .tomo

edit main.lua + manifest.json

upload sprite assets (tooling may pack to 4-bit)



9.3 Simulator controls

Virtual A/B/encoder controls + keyboard mapping

Fake sensors panel:

IMU tilt pad (2D)

ToF slider

light slider

mic level generator (or input from microphone later)

piezo hit button with velocity


MIDI log panel; optional WebMIDI later



---

10. Repository / build layout (monorepo)

Proposed structure:

apps/simulator (Vite)

apps/firmware (device build)

packages/core (C++ core + Lua bindings + Enjin bridge)

packages/core-wasm (Emscripten build outputs)

packages/protocol (TS types + JSON schemas: manifest, catalog)

packages/tools (sprite packer, .tomo builder, catalog generator)

apps/examples (example scripts)



---

11. Post-MVP: public scripts hub (Path 2)

Not implemented in MVP, but the architecture anticipates it.

11.1 Product intent

User-submitted scripts/apps

Favorites and comments

Versioned releases per app


11.2 Auth

Email + GitHub OAuth.


11.3 Compatibility foundations (now)

Stable id + semantic version

api_version compatibility gate

Bundles stored as .tomo with content hashes



---

12. MVP acceptance checklist

Device runs at 120 Hz logic, 30 Hz present.

Shell can launch/exit apps reliably.

Lua API v0.1 implemented on device and in WASM build.

Enjin rendering works identically in device + simulator.

Simulator is shareable via URL and supports remote updates.

Integrated editor can run/edit scripts quickly.

At least two example apps:

tilt → MIDI CC

snake-like minigame → MIDI note