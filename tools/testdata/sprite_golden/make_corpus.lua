-- Authors the minimal .aseprite inputs of the sprite golden corpus (Tomodachi #295).
--
--   aseprite -b --script-param out=DIR --script make_corpus.lua
--
-- Run once through make_corpus.py, which also patches the 0 ms frame and writes
-- the .njn goldens. The corpus is frozen: see README.md before running it again.
--
-- Every file is indexed with source transparent index 0; art uses slots 1..14.

local out = app.params["out"]
assert(out, "pass --script-param out=DIR")

-- Distinct, opaque colours, so a wrong slot shows up in a colour dump too.
local PALETTE = {
  {0, 0, 0}, {137, 137, 137}, {255, 255, 255}, {26, 28, 44},
  {93, 39, 93}, {177, 62, 83}, {239, 125, 87}, {255, 205, 117},
  {167, 240, 112}, {56, 183, 100}, {37, 113, 121}, {59, 93, 201},
  {115, 239, 247}, {86, 108, 134}, {51, 60, 87}, {255, 0, 255},
}

local function newSprite(w, h, frames, palette)
  local spr = Sprite(w, h, ColorMode.INDEXED)
  spr.transparentColor = 0
  local colours = palette or PALETTE
  local pal = Palette(#colours)
  for i, c in ipairs(colours) do pal:setColor(i - 1, Color{ r = c[1], g = c[2], b = c[3] }) end
  spr:setPalette(pal)
  for _ = 2, frames do spr:newEmptyFrame() end
  return spr
end

-- Paints the rectangle (x, y, w, h) with `index` into a new cel.
local function rect(spr, layer, frame, x, y, w, h, index)
  local img = Image(w, h, ColorMode.INDEXED)
  for py = 0, h - 1 do
    for px = 0, w - 1 do img:drawPixel(px, py, index) end
  end
  return spr:newCel(layer, frame, img, Point(x, y))
end

-- Links each layer's cels in the given frames to the one in the first frame.
local function link(spr, layers, frameNumbers)
  app.activeSprite = spr
  app.range.layers = layers
  local frames = {}
  for i, f in ipairs(frameNumbers) do frames[i] = spr.frames[f] end
  app.range.frames = frames
  app.command.LinkCels()
end

local function durations(spr, ms)
  for i, d in ipairs(ms) do spr.frames[i].duration = d / 1000 end
end

local function tag(spr, from, to, name, dir, repeats)
  local t = spr:newTag(from, to)
  t.name = name
  t.aniDir = dir
  t.repeats = repeats
  return t
end

local function save(spr, name)
  spr:saveAs(out .. "/" .. name .. ".aseprite")
  spr:close()
end

-- One layer, three frames; frame i paints a 2x2 block of slot i at a moving x.
local function threeFrames()
  local spr = newSprite(4, 4, 3)
  spr.layers[1].name = "art"
  for f = 1, 3 do rect(spr, spr.layers[1], f, f - 1, 1, 2, 2, f) end
  durations(spr, {100, 150, 200})
  return spr
end

local function tagCase(name, dir, repeats)
  local spr = threeFrames()
  tag(spr, 1, 3, "anim", dir, repeats)
  save(spr, name)
end

-- Tag directions (repeat 0 → loop / pingpong).
tagCase("tag_forward", AniDir.FORWARD, 0)
tagCase("tag_reverse", AniDir.REVERSE, 0)
tagCase("tag_pingpong", AniDir.PING_PONG, 0)
tagCase("tag_pingpong_reverse", AniDir.PING_PONG_REVERSE, 0)

-- Repeat counts (1 → once, N → unrolled once).
tagCase("repeat_once", AniDir.FORWARD, 1)
tagCase("repeat_n", AniDir.FORWARD, 3)
tagCase("repeat_n_pingpong", AniDir.PING_PONG, 3)

-- No tags → one looping "default" clip.
do
  local spr = threeFrames()
  save(spr, "untagged")
end

-- 0 ms frame: frame 2 is patched to 0 ms and the header speed to 90 ms by
-- make_corpus.py (Aseprite clamps a frame to at least 1 ms).
do
  local spr = threeFrames()
  durations(spr, {120, 1, 80})
  save(spr, "zero_duration")
end

-- Old palette chunk: Aseprite writes only 0x0004 for an opaque palette of
-- <= 256 colours. A full 256-entry palette exercises the count-0 packet.
do
  local colours = {}
  for i = 0, 255 do colours[i + 1] = {i, 255 - i, (i * 7) % 256} end
  local spr = newSprite(4, 2, 1, colours)
  spr.layers[1].name = "art"
  local img = Image(4, 2, ColorMode.INDEXED)
  for i = 1, 8 do img:drawPixel((i - 1) % 4, (i - 1) // 4, i) end
  spr:newCel(spr.layers[1], 1, img, Point(0, 0))
  save(spr, "old_palette")
end

-- z-index tie (sheet): layer "low" at z +1 and layer "high" at z 0 both sit at
-- order 1. Spec NOTE.5 paints the lower z first, so "low" (slot 5) ends on top
-- where the two overlap; the old tie-break painted "high" (slot 9) on top.
do
  local spr = newSprite(4, 4, 1)
  local low = spr.layers[1]
  low.name = "low"
  local high = spr:newLayer()
  high.name = "high"
  rect(spr, low, 1, 0, 0, 3, 3, 5).zIndex = 1
  rect(spr, high, 1, 1, 1, 3, 3, 9).zIndex = 0
  save(spr, "z_index_tie")
end

-- Sheet: one visible layer plus a hidden one, cels off the origin and partly
-- off-canvas, a linked cel, an empty frame, and two sub-range tags.
do
  local spr = newSprite(6, 5, 5)
  local art = spr.layers[1]
  art.name = "art"
  local guide = spr:newLayer()
  guide.name = "guide"
  guide.isVisible = false
  rect(spr, guide, 1, 0, 0, 6, 5, 14)
  rect(spr, art, 1, 1, 1, 3, 2, 2)
  rect(spr, art, 2, 4, 3, 4, 4, 3)       -- runs off the bottom-right edge
  link(spr, { art }, {2, 3})              -- frame 3 links frame 2
  rect(spr, art, 5, -1, 0, 3, 3, 4)      -- runs off the left edge; frame 4 stays empty
  durations(spr, {100, 60, 60, 70, 140})
  tag(spr, 1, 3, "idle", AniDir.FORWARD, 0)
  tag(spr, 4, 5, "hit", AniDir.REVERSE, 1)
  save(spr, "sheet")
end

-- Layered: three visible parts and a hidden one, a linked cel, a part that
-- moves, a part missing from a frame, and a ping-pong-reverse tag repeated twice.
do
  local spr = newSprite(8, 8, 3)
  local body = spr.layers[1]
  body.name = "body"
  local eye = spr:newLayer()
  eye.name = "eye"
  local hat = spr:newLayer()
  hat.name = "hat"
  local notes = spr:newLayer()
  notes.name = "notes"
  notes.isVisible = false
  rect(spr, body, 1, 1, 2, 6, 6, 3)
  rect(spr, eye, 1, 3, 4, 1, 1, 2)
  rect(spr, hat, 1, 2, 0, 4, 2, 6)
  rect(spr, notes, 1, 0, 0, 8, 8, 1)
  -- Frames 2 and 3 link body and hat to frame 1; the eye moves right in
  -- frame 2 and is back (linked) in frame 3; frame 3 has no hat.
  link(spr, { body, hat }, {1, 2})
  link(spr, { body, eye }, {1, 3})
  rect(spr, eye, 2, 4, 4, 1, 1, 2)
  durations(spr, {100, 80, 120})
  tag(spr, 1, 3, "blink", AniDir.PING_PONG_REVERSE, 2)
  save(spr, "layered")
end
