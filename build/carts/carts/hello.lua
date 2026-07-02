--[[cx8 cart]]
-- title:  Hello Chromaplex
-- author: Chromaplex Team
-- desc:   Demo cartridge showcasing CX8 features

-- ═══════════════════════════════════════════════════════
--  HELLO CHROMAPLEX — Feature Demo
--  Controls:  Arrow keys = move,  Z = action,  X = cycle
-- ═══════════════════════════════════════════════════════

local player = { x = 128, y = 72, speed = 2, color = 11 }
local stars = {}
local particles = {}
local frame = 0
local demo_mode = 1      -- 1=shapes, 2=sprites, 3=audio
local demo_names = { "SHAPES", "SPRITES", "AUDIO" }

-- ─── INIT ──────────────────────────────────────────────

function _init()
    -- Generate starfield
    for i = 1, 80 do
        stars[i] = {
            x = rnd(SCREEN_W),
            y = rnd(SCREEN_H),
            speed = 0.2 + rnd(1.5),
            color = 3 + flr(rnd(5))
        }
    end

    -- Define some sprites in the spritesheet
    -- Sprite 1: A small ship shape
    local ship = {
        {0,0,7,7,7,7,0,0},
        {0,0,0,7,7,0,0,0},
        {0,0,7,11,11,7,0,0},
        {0,7,11,7,7,11,7,0},
        {7,11,11,11,11,11,11,7},
        {7,0,11,11,11,11,0,7},
        {0,0,7,0,0,7,0,0},
        {0,0,11,0,0,11,0,0},
    }
    for row = 0, 7 do
        for col = 0, 7 do
            sset(col, row, ship[row + 1][col + 1])
        end
    end

    -- Sprite 2: A gem/collectible
    local gem = {
        {0,0,0,21,21,0,0,0},
        {0,0,21,29,29,21,0,0},
        {0,21,29,7,7,29,21,0},
        {21,29,7,7,7,7,29,21},
        {21,29,29,7,7,29,29,21},
        {0,21,29,29,29,29,21,0},
        {0,0,21,29,29,21,0,0},
        {0,0,0,21,21,0,0,0},
    }
    for row = 0, 7 do
        for col = 0, 7 do
            sset(8 + col, row, gem[row + 1][col + 1])
        end
    end
end

-- ─── UPDATE ────────────────────────────────────────────

function _update()
    frame = frame + 1

    -- Movement
    if btn(BTN_LEFT)  then player.x = player.x - player.speed end
    if btn(BTN_RIGHT) then player.x = player.x + player.speed end
    if btn(BTN_UP)    then player.y = player.y - player.speed end
    if btn(BTN_DOWN)  then player.y = player.y + player.speed end

    -- Wrap around
    if player.x < -8 then player.x = SCREEN_W end
    if player.x > SCREEN_W then player.x = -8 end
    if player.y < -8 then player.y = SCREEN_H end
    if player.y > SCREEN_H then player.y = -8 end

    -- Cycle demo mode
    if btnp(BTN_B) then
        demo_mode = demo_mode + 1
        if demo_mode > #demo_names then demo_mode = 1 end
    end

    -- Spawn particles
    if btnp(BTN_A) then
        for i = 1, 8 do
            particles[#particles + 1] = {
                x = player.x + 4,
                y = player.y + 4,
                dx = rnd(4) - 2,
                dy = rnd(4) - 2,
                life = 20 + flr(rnd(20)),
                color = 8 + flr(rnd(8))
            }
        end
        -- Play a sound
        sfx(0, 440 + rnd(220), 0.3, WAVE_TRIANGLE, 0.5)
        envelope(0, 0.01, 0.1, 0.0, 0.2)
    end

    -- Update stars
    for i, s in ipairs(stars) do
        s.x = s.x - s.speed
        if s.x < 0 then
            s.x = SCREEN_W
            s.y = rnd(SCREEN_H)
        end
    end

    -- Update particles
    for i = #particles, 1, -1 do
        local p = particles[i]
        p.x = p.x + p.dx
        p.y = p.y + p.dy
        p.dy = p.dy + 0.1  -- gravity
        p.life = p.life - 1
        if p.life <= 0 then
            table.remove(particles, i)
        end
    end
end

-- ─── DRAW ──────────────────────────────────────────────

function _draw()
    cls(0)

    -- Stars
    for _, s in ipairs(stars) do
        pset(flr(s.x), flr(s.y), s.color)
    end

    if demo_mode == 1 then
        draw_shapes()
    elseif demo_mode == 2 then
        draw_sprites()
    elseif demo_mode == 3 then
        draw_audio_viz()
    end

    -- Particles
    for _, p in ipairs(particles) do
        local c = p.color
        if p.life < 5 then c = 3 end
        pset(flr(p.x), flr(p.y), c)
    end

    -- Player ship (sprite 0)
    spr(0, flr(player.x), flr(player.y))

    -- HUD
    rectfill(0, 0, SCREEN_W - 1, 8, 1)
    print("CHROMAPLEX 8 DEMO", 2, 2, 7)
    print("MODE: " .. demo_names[demo_mode], 130, 2, 61)

    -- Controls help
    print("ARROWS:MOVE  Z:PARTICLES  X:MODE", 2, SCREEN_H - 7, 4)
end

-- ─── Demo: Shapes ──────────────────────────────────────

function draw_shapes()
    local t = frame * 0.02

    -- Animated rectangles
    for i = 0, 5 do
        local x = 30 + i * 35
        local y = 40 + sin(t + i * 0.1) * 20
        local c = 8 + i * 2
        rectfill(x, y, x + 10, y + 10, c)
        rect(x - 1, y - 1, x + 11, y + 11, c + 1)
    end

    -- Animated circles
    for i = 0, 7 do
        local a = t + i * 0.125
        local cx = 128 + cos(a) * 40
        local cy = 100 + sin(a) * 20
        circfill(cx, cy, 4 + sin(t * 2 + i) * 2, 32 + i)
    end

    -- Lines radiating from center
    for i = 0, 15 do
        local a = t * 0.5 + i / 16
        local ex = 128 + cos(a) * 50
        local ey = 72 + sin(a) * 30
        line(128, 72, ex, ey, 40 + (i % 8))
    end

    print("PRIMITIVES: LINE RECT CIRC TRI", 30, 20, 5)
end

-- ─── Demo: Sprites ─────────────────────────────────────

function draw_sprites()
    local t = frame * 0.03

    print("SPRITE SCALING & ROTATION", 45, 20, 5)

    -- Draw gem sprites at different scales
    for i = 0, 4 do
        local scale = 1 + i * 0.5
        local x = 20 + i * 45
        local y = 50
        spr(1, x, y, 1, 1, false, false, scale, 0)
        print(scale .. "x", x, y + scale * 10, 7)
    end

    -- Draw rotating sprites
    for i = 0, 5 do
        local angle = (frame * 2 + i * 60) % 360
        local x = 30 + i * 40
        spr(1, x, 95, 1, 1, false, false, 2.0, angle)
    end

    -- Flipping demo
    spr(0, 40,  120, 1, 1, false, false, 2, 0)
    print("NORMAL", 34, 138, 4)
    spr(0, 100, 120, 1, 1, true,  false, 2, 0)
    print("FLIP-X", 94, 138, 4)
    spr(0, 160, 120, 1, 1, false, true,  2, 0)
    print("FLIP-Y", 154, 138, 4)
    spr(0, 220, 120, 1, 1, true,  true,  2, 0)
    print("BOTH", 220, 138, 4)
end

-- ─── Demo: Audio visualisation ─────────────────────────

function draw_audio_viz()
    print("AUDIO ENGINE — PRESS Z TO PLAY", 35, 20, 5)

    -- Waveform names
    local waves = {"SQUARE", "TRIANGLE", "SAW", "NOISE", "PULSE", "SINE"}

    for i, name in ipairs(waves) do
        local y = 30 + (i - 1) * 16
        local col = 40 + (i % 8)

        print(name, 16, y + 2, col)

        -- Draw waveform preview
        for x = 0, 80 do
            local t = x / 80
            local sample = 0
            if i == 1 then sample = t < 0.5 and 1 or -1
            elseif i == 2 then sample = t < 0.5 and (4*t - 1) or (3 - 4*t)
            elseif i == 3 then sample = 2*t - 1
            elseif i == 4 then sample = rnd(2) - 1
            elseif i == 5 then sample = t < 0.3 and 1 or -1
            elseif i == 6 then sample = sin(t)
            end
            pset(80 + x, y + 6 - flr(sample * 5), col)
        end
    end

    -- Channel activity indicator
    print("CHANNELS:", 180, 30, 5)
    for i = 0, 3 do
        local active = (frame % 60 < 30) and (i == 0) -- simplified
        rectfill(180 + i * 16, 40, 190 + i * 16, 50,
                 active and 27 or 2)
        print(tostring(i), 183 + i * 16, 42, 7)
    end
end
