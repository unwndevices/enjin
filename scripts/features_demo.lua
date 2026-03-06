-- scripts/features_demo.lua
-- Demo showcasing new Lua scripting features:
--   1. Sprite flipping (flipH, flipV, rotate90)
--   2. Collision response helpers (aabbOverlap, circleResponse, reflect)
--   3. Seeded RNG (engine.random.seed, integer, float)
--
-- Requires host to push: PIKACHU_DATA (lightuserdata), PIKACHU_W, PIKACHU_H
-- Run with: ./sprite_sdl_test --lua scripts/features_demo.lua

-- Initialization
local sprite = -1
local W, H = 0, 0

-- Sprite flip demo state
local flip_timer = 0
local flip_mode = 0  -- cycles 0..5: normal, flipH, flipV, flipHV, rot90, rot90+flipH
local flip_labels = { "normal", "flipH", "flipV", "flipH+V", "rot90", "rot90+flipH" }

-- Bouncing ball (collision + reflect demo)
local ball = { x = 60, y = 30, vx = 40, vy = 30, r = 4 }

-- RNG sparkle particles
local sparkles = {}
local sparkle_timer = 0

-- Seed the RNG for deterministic sparkles
engine.random.seed(2025)

function init_sparkles()
    for i = 1, 8 do
        sparkles[i] = {
            x = engine.random.float(10, W - 10),
            y = engine.random.float(10, H - 40),
            life = engine.random.float(0.3, 1.5),
            maxlife = 0,
            color = engine.random.integer(1, 14),
        }
        sparkles[i].maxlife = sparkles[i].life
    end
end

-- UPDATE
function update(dt)
    W = gfx.getWidth()
    H = gfx.getHeight()

    -- Animate sprite through flip modes
    flip_timer = flip_timer + dt
    if flip_timer > 1.2 then
        flip_timer = 0
        flip_mode = (flip_mode + 1) % 6
    end

    -- Update bouncing ball with wall collision + reflect
    update_ball(dt)

    -- Update sparkle particles (RNG demo)
    update_sparkles(dt)

    -- Animate sprite
    if sprite >= 0 then
        gfx.updateSprite(sprite, dt)
    end
end

function update_ball(dt)
    ball.x = ball.x + ball.vx * dt
    ball.y = ball.y + ball.vy * dt

    -- Bounce off screen edges using engine.collision.reflect
    local margin = ball.r

    -- Left wall
    if ball.x - margin < 0 then
        ball.x = margin
        local vx, vy = engine.collision.reflect(ball.vx, ball.vy, 1, 0)
        ball.vx, ball.vy = vx, vy
    end
    -- Right wall
    if ball.x + margin > W then
        ball.x = W - margin
        local vx, vy = engine.collision.reflect(ball.vx, ball.vy, -1, 0)
        ball.vx, ball.vy = vx, vy
    end
    -- Top wall
    if ball.y - margin < 0 then
        ball.y = margin
        local vx, vy = engine.collision.reflect(ball.vx, ball.vy, 0, 1)
        ball.vx, ball.vy = vx, vy
    end
    -- Bottom wall (above palette strip)
    if ball.y + margin > H - 10 then
        ball.y = H - 10 - margin
        local vx, vy = engine.collision.reflect(ball.vx, ball.vy, 0, -1)
        ball.vx, ball.vy = vx, vy
    end

    -- Check collision with pikachu sprite using aabbOverlap
    if sprite >= 0 then
        local sx = math.floor(W / 2 - PIKACHU_W / 2) - 2
        local sy = 2
        local hit, ox, oy, ow, oh = engine.collision.aabbOverlap(
            ball.x - ball.r, ball.y - ball.r, ball.r * 2, ball.r * 2,
            sx, sy, PIKACHU_W, PIKACHU_H
        )
        if hit and ow and oh then
            -- Push ball out along the smallest overlap axis
            if ow < oh then
                if ball.vx > 0 then ball.x = sx - ball.r
                else ball.x = sx + PIKACHU_W + ball.r end
                ball.vx = -ball.vx
            else
                if ball.vy > 0 then ball.y = sy - ball.r
                else ball.y = sy + PIKACHU_H + ball.r end
                ball.vy = -ball.vy
            end
        end
    end
end

function update_sparkles(dt)
    sparkle_timer = sparkle_timer + dt

    for i = 1, #sparkles do
        local s = sparkles[i]
        s.life = s.life - dt
        if s.life <= 0 then
            -- Respawn with RNG
            s.x = engine.random.float(10, W - 10)
            s.y = engine.random.float(10, H - 40)
            s.life = engine.random.float(0.3, 1.5)
            s.maxlife = s.life
            s.color = engine.random.integer(1, 14)
        end
    end
end

-- DRAW
function draw()
    gfx.clear(0)

    -- Draw sparkle particles (RNG demo)
    draw_sparkles()

    -- Draw pikachu with current flip mode
    if sprite >= 0 then
        draw_pikachu()
    end

    -- Draw bouncing ball
    draw_ball()

    -- Draw UI labels
    draw_labels()

    -- Palette strip
    draw_palette_strip()
end

function draw_pikachu()
    local cx = math.floor(W / 2 - PIKACHU_W / 2) - 2
    local cy = 2

    local flipH = false
    local flipV = false
    local rot90 = false

    if flip_mode == 1 then flipH = true
    elseif flip_mode == 2 then flipV = true
    elseif flip_mode == 3 then flipH = true; flipV = true
    elseif flip_mode == 4 then rot90 = true
    elseif flip_mode == 5 then rot90 = true; flipH = true
    end

    gfx.drawSprite(sprite, cx, cy, flipH, flipV, rot90)
end

function draw_ball()
    gfx.setColor(9) -- bright red
    gfx.circle(math.floor(ball.x), math.floor(ball.y), ball.r)
end

function draw_sparkles()
    for i = 1, #sparkles do
        local s = sparkles[i]
        if s.life > 0 then
            -- Fade: use brighter color when life is higher
            local frac = s.life / s.maxlife
            gfx.setColor(s.color)
            if frac > 0.5 then
                -- Full sparkle: 3x3 cross
                gfx.setPixel(math.floor(s.x), math.floor(s.y))
                gfx.setPixel(math.floor(s.x) - 1, math.floor(s.y))
                gfx.setPixel(math.floor(s.x) + 1, math.floor(s.y))
                gfx.setPixel(math.floor(s.x), math.floor(s.y) - 1)
                gfx.setPixel(math.floor(s.x), math.floor(s.y) + 1)
            else
                -- Fading: single pixel
                gfx.setPixel(math.floor(s.x), math.floor(s.y))
            end
        end
    end
end

function draw_labels()
    gfx.setColor(7) -- white
    local label = flip_labels[flip_mode + 1] or "?"
    gfx.text(label, 1, H - 18)
end

function draw_palette_strip()
    local strip_h = 8
    local strip_y = H - strip_h
    local cell_w = 8
    for i = 0, 14 do
        gfx.setColor(i)
        gfx.rectangle(i * cell_w, strip_y, cell_w, strip_h)
    end
end

-- Script load: create pikachu sprite
if PIKACHU_DATA then
    sprite = gfx.newSprite(PIKACHU_DATA, PIKACHU_W, PIKACHU_H, 1, 1)
    if sprite >= 0 then
        gfx.setFrame(sprite, 0)
    end
end
