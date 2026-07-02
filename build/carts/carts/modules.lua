--[[cx8 cart]]
-- title:  Module Demo
-- author: Chromaplex Team
-- desc:   Run with --mod 0 --mod 1 to see expanded hardware!

function _init()
    -- Try loading some modules
    local loaded = {}
    for id = 0, 4 do
        if mod_check(id) then
            local info = mod_info(id)
            table.insert(loaded, info)
        end
    end
    _loaded = loaded
end

function _update()
    -- nothing dynamic in this demo
end

function _draw()
    cls(1)

    print("EXPANSION MODULE BAY", 60, 6, 7)
    line(8, 14, SCREEN_W - 8, 14, 3)

    -- Show all modules
    for id = 0, 4 do
        local info = mod_info(id)
        if info then
            local y = 22 + id * 22
            local status_col = info.loaded and 27 or 9
            local status_txt = info.loaded and "[LOADED]" or "[EMPTY]"

            rectfill(8, y, SCREEN_W - 8, y + 18, 2)
            rect(8, y, SCREEN_W - 8, y + 18, 3)

            print(info.name, 12, y + 2, 7)
            print(info.manufacturer, 120, y + 2, 5)
            print(info.flavor, 12, y + 9, 4)
            print(status_txt, SCREEN_W - 56, y + 9, status_col)
        end
    end

    print("RUN WITH:  --mod 0 --mod 1  ETC.", 16, SCREEN_H - 8, 61)
end
