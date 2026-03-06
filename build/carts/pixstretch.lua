-- title: PIXEL-STRETCH PRO Demo
-- author: VisualFX Co.
-- desc: Visual effects showcase

-- Load the PIXEL-STRETCH PRO module
mod_load(MOD_PIXSTRETCH)

local t = 0
local demo = 1
local demos = {
    "PALETTE CYCLE",
    "SCREEN FADE",
    "SCREEN SHAKE",
    "COLOUR TINT",
    "FLASH",
    "SCANLINE WAVE",
    "DITHERING",
    "ALL TOGETHER"
}

function _init()
    -- Start with a nice gradient background
end

function draw_scene()
    -- Draw a colourful test scene
    -- Gradient bars
    for i = 0, 63 do
        local x = (i % 16) * 16
        local y = 20 + flr(i / 16) * 24
        rectfill(x, y, x + 15, y + 23, i)
    end

    -- Some shapes
    circfill(128, 72, 20, 42)
    circfill(128, 72, 14, 35)
    circfill(128, 72, 8, 28)

    -- Floating text
    local tx = 128 + sin(t * 0.01) * 40
    local ty = 120 + cos(t * 0.01) * 10
    print("PIXEL-STRETCH PRO", tx - 42, ty, 61)
end

function _update()
    t = t + 1

    -- Switch demos with left/right
    if btnp(BTN_RIGHT) then
        demo = demo + 1
        if demo > #demos then demo = 1 end
        fx_reset()
    end
    if btnp(BTN_LEFT) then
        demo = demo - 1
        if demo < 1 then demo = #demos end
        fx_reset()
    end

    -- Apply current demo effect
    if demo == 1 then
        -- Palette cycling: cycle the blue range
        fx_cycle(40, 47, 2.0)
        -- Also cycle greens
        fx_cycle(32, 39, 1.5)

    elseif demo == 2 then
        -- Fade in and out
        local fade_val = 0.5 + 0.5 * cos(t * 0.005)
        fx_fade(fade_val, 5.0)

    elseif demo == 3 then
        -- Shake on button press
        if btnp(BTN_A) then
            fx_shake(8, 0.3)
        end

    elseif demo == 4 then
        -- Colour tint that shifts over time
        local r = 0.5 + 0.5 * sin(t * 0.01)
        local g = 0.5 + 0.5 * sin(t * 0.013)
        local b = 0.5 + 0.5 * sin(t * 0.017)
        fx_tint(r, g, b, 0.3)

    elseif demo == 5 then
        -- Flash on button press
        if btnp(BTN_A) then
            fx_flash(7, 8)   -- white flash
        end
        if btnp(BTN_B) then
            fx_flash(14, 8)  -- red flash
        end

    elseif demo == 6 then
        -- Scanline wave
        local amp = 2 + sin(t * 0.01) * 3
        fx_wave(amp, 6)

    elseif demo == 7 then
        -- Cycle through dither modes
        local mode = flr(t / 60) % 7
        fx_dither(mode)

    elseif demo == 8 then
        -- Everything at once!
        fx_cycle(40, 47, 1.0)
        fx_tint(0.2, 0.1, 0.4, 0.15)
        fx_wave(1, 3)
        if t % 120 == 0 then
            fx_shake(4, 0.2)
            fx_flash(61, 4)
        end
    end
end

function _draw()
    cls(0)
    draw_scene()

    -- UI bar at top
    rectfill(0, 0, SCREEN_W - 1, 16, 1)
    print("PIXEL-STRETCH PRO", 2, 2, 61)
    print(demos[demo], 2, 9, 7)
    print("</>  SWITCH", SCREEN_W - 55, 2, 5)

    -- Instructions
    if demo == 3 or demo == 5 then
        print("PRESS Z TO TRIGGER", 80, SCREEN_H - 8, 7)
    end
    if demo == 5 then
        print("PRESS X FOR RED", 80, SCREEN_H - 16, 14)
    end
    if demo == 7 then
        local modes = {"NONE","BAYER2","BAYER4","CHECKER","HLINE","VLINE","DIAG"}
        local mode = flr(t / 60) % 7
        print("MODE: "..modes[mode + 1], 80, SCREEN_H - 8, 7)
    end
end
