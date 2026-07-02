--[[cx8 cart]]
-- title:  Bouncing Ball
-- author: Chromaplex Team
-- desc:   Minimal example — a bouncing ball with trail

local ball = {
    x = 128, y = 72,
    dx = 2.5, dy = 1.8,
    r = 6,
    color = 11,
    trail = {}
}

function _init()
    -- nothing special
end

function _update()
    -- Store trail position
    table.insert(ball.trail, 1, {x = ball.x, y = ball.y})
    if #ball.trail > 30 then
        table.remove(ball.trail)
    end

    -- Move
    ball.x = ball.x + ball.dx
    ball.y = ball.y + ball.dy

    -- Bounce
    if ball.x - ball.r < 0 or ball.x + ball.r >= SCREEN_W then
        ball.dx = -ball.dx
        sfx(0, 660, 0.2, WAVE_SINE)
        envelope(0, 0.0, 0.05, 0.0, 0.1)
    end
    if ball.y - ball.r < 0 or ball.y + ball.r >= SCREEN_H then
        ball.dy = -ball.dy
        sfx(1, 440, 0.2, WAVE_SINE)
        envelope(1, 0.0, 0.05, 0.0, 0.1)
    end

    -- Clamp
    ball.x = mid(ball.r, ball.x, SCREEN_W - ball.r - 1)
    ball.y = mid(ball.r, ball.y, SCREEN_H - ball.r - 1)

    -- Change colour on button press
    if btnp(BTN_A) then
        ball.color = 8 + flr(rnd(56))
    end
end

function _draw()
    cls(0)

    -- Draw trail
    for i, t in ipairs(ball.trail) do
        local alpha = 1 - (i / #ball.trail)
        local c = (alpha > 0.5) and ball.color or 1
        local r = flr(ball.r * alpha)
        if r > 0 then
            circfill(t.x, t.y, r, c)
        end
    end

    -- Draw ball
    circfill(ball.x, ball.y, ball.r, ball.color)
    circ(ball.x, ball.y, ball.r, 7)

    -- Info
    print("BOUNCING BALL", 2, 2, 5)
    print("Z: CHANGE COLOR", 2, SCREEN_H - 7, 4)
end
