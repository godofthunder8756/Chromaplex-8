-- title: NETLINK-1 Demo
-- author: CyberConnect
-- desc: LAN multiplayer networking demo

-- Load the NETLINK-1 module
mod_load(MOD_NETLINK)

local state = "menu"   -- menu / hosting / joining / game
local my_x, my_y = 128, 72
local players = {}     -- remote player positions
local msg_log = {}     -- message log
local t = 0

function log_msg(text)
    table.insert(msg_log, 1, text)
    if #msg_log > 6 then table.remove(msg_log) end
end

function _init()
    log_msg("NETLINK-1 DEMO")
    log_msg("Z=Host  X=Join  A=Direct IP")
end

function _update()
    t = t + 1

    if state == "menu" then
        if btnp(BTN_A) then
            -- Host a session
            if net_host(7888, "Player1") then
                state = "hosting"
                log_msg("Hosting on port 7888...")
            end
        elseif btnp(BTN_B) then
            -- Auto-discover and join
            if net_join("Player2") then
                state = "joining"
                log_msg("Scanning LAN...")
            end
        end
    end

    -- Check network state transitions
    local ns = net_state()
    if state == "joining" and ns == NET_CONNECTED then
        state = "game"
        log_msg("Connected! Player #"..net_id())
    elseif state == "hosting" then
        state = "game"
        log_msg("Hosting! Waiting for players...")
    end

    -- Game state: move and sync
    if state == "game" then
        local speed = 2
        if btn(BTN_LEFT)  then my_x = my_x - speed end
        if btn(BTN_RIGHT) then my_x = my_x + speed end
        if btn(BTN_UP)    then my_y = my_y - speed end
        if btn(BTN_DOWN)  then my_y = my_y + speed end

        -- Clamp
        my_x = mid(4, my_x, SCREEN_W - 4)
        my_y = mid(4, my_y, SCREEN_H - 4)

        -- Send position every 2 frames
        if t % 2 == 0 then
            local data = string.format("%d,%d", my_x, my_y)
            net_send(0, data)
        end

        -- Receive other players' positions
        while true do
            local msg = net_recv()
            if not msg then break end
            local x, y = msg.data:match("(%d+),(%d+)")
            if x and y then
                players[msg.from] = { x = tonumber(x), y = tonumber(y) }
            end
        end
    end
end

function _draw()
    cls(0)

    -- Title
    print("NETLINK-1", 2, 2, 61)
    print("\"The world is your lobby.\"", 2, 10, 5)

    if state == "menu" then
        -- Menu
        print("Z = HOST GAME", 80, 50, 42)
        print("X = JOIN (LAN SCAN)", 80, 60, 44)
        print("A = JOIN BY IP", 80, 70, 48)

        -- Decorative antenna
        local ax, ay = 40, 65
        line(ax, ay, ax, ay - 20, 61)
        circ(ax, ay - 22, 3, 61)
        if (t / 10) % 2 < 1 then
            pset(ax, ay - 22, 14)  -- blinking LED
        end
    else
        -- Show connection status
        local status_col = 5
        local status_text = "???"
        local ns = net_state()
        if ns == NET_HOSTING then
            status_text = "HOSTING"
            status_col = 35
        elseif ns == NET_JOINING then
            status_text = "SCANNING..."
            status_col = 26
        elseif ns == NET_CONNECTED then
            status_text = "CONNECTED"
            status_col = 42
        end

        print(status_text, SCREEN_W - #status_text * 5 - 2, 2, status_col)
        print("Peers: "..net_peers(), 2, 20, 5)
        print("ID: "..net_id(), 80, 20, 5)

        -- Draw my player
        circfill(my_x, my_y, 4, 42)
        print("P"..net_id(), my_x - 4, my_y - 10, 7)

        -- Draw remote players
        local colours = {14, 44, 26}
        for id, pos in pairs(players) do
            local col = colours[(id % #colours) + 1]
            circfill(pos.x, pos.y, 4, col)
            print("P"..id, pos.x - 4, pos.y - 10, 7)
        end
    end

    -- Message log
    for i, msg in ipairs(msg_log) do
        print(msg, 2, SCREEN_H - 8 * i, 5)
    end
end
