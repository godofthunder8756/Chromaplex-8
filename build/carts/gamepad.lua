-- title: Gamepad Test
-- author: Chromaplex Team
-- desc: Controller/gamepad testing utility

local t = 0
local players = {}

function _init()
    for i = 0, 3 do
        players[i] = {
            x = 64 + i * 48,
            y = 72,
            col = ({42, 14, 44, 26})[i + 1]
        }
    end
end

function _update()
    t = t + 1

    -- Move each player independently
    for i = 0, 3 do
        local speed = 2
        if btn_player(i, BTN_LEFT)  then players[i].x = players[i].x - speed end
        if btn_player(i, BTN_RIGHT) then players[i].x = players[i].x + speed end
        if btn_player(i, BTN_UP)    then players[i].y = players[i].y - speed end
        if btn_player(i, BTN_DOWN)  then players[i].y = players[i].y + speed end

        -- Clamp
        players[i].x = mid(6, players[i].x, SCREEN_W - 6)
        players[i].y = mid(20, players[i].y, SCREEN_H - 6)

        -- Rumble on A press
        if btnp_player(i, BTN_A) then
            rumble(i, 0.5, 200)
        end
    end
end

function _draw()
    cls(0)

    -- Title bar
    rectfill(0, 0, SCREEN_W - 1, 14, 1)
    print("GAMEPAD TEST", 2, 2, 7)

    local count = gamepad_count()
    print(count.." CONTROLLER(S)", SCREEN_W - 75, 2, count > 0 and 35 or 14)

    -- Controller info
    for i = 0, 3 do
        local bx = 2 + i * 64
        local by = SCREEN_H - 28

        -- Status box
        rect(bx, by, bx + 60, by + 26, 3)

        local connected = gamepad_connected(i)
        local status_col = connected and 35 or 5
        print("P"..i, bx + 2, by + 2, players[i].col)

        if connected then
            local name = gamepad_name(i) or "?"
            -- Truncate long names
            if #name > 9 then name = name:sub(1, 9).."." end
            print(name, bx + 14, by + 2, 7)
            print("READY", bx + 2, by + 10, 35)
        else
            if i == 0 then
                print("KEYBOARD", bx + 14, by + 2, 5)
                print("ACTIVE", bx + 2, by + 10, 42)
            else
                print("---", bx + 14, by + 2, 3)
                print("EMPTY", bx + 2, by + 10, 5)
            end
        end

        -- Show button states
        local btns = ""
        if btn_player(i, BTN_A) then btns = btns.."A" end
        if btn_player(i, BTN_B) then btns = btns.."B" end
        if btn_player(i, BTN_X) then btns = btns.."X" end
        if btn_player(i, BTN_Y) then btns = btns.."Y" end
        if #btns > 0 then
            print(btns, bx + 2, by + 18, 7)
        end
    end

    -- Draw players in game area
    for i = 0, 3 do
        local p = players[i]
        -- Only draw if controller connected (or player 0)
        if i == 0 or gamepad_connected(i) then
            circfill(p.x, p.y, 5, p.col)
            print("P"..i, p.x - 4, p.y - 10, 7)

            -- Direction indicator
            if btn_player(i, BTN_A) then
                circ(p.x, p.y, 7, 7)  -- highlight on A
            end
        end
    end

    -- Instructions
    print("ARROWS/STICK=MOVE  Z/A=RUMBLE", 2, 16, 5)
end
