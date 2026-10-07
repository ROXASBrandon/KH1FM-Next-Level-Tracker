-- Next Level Popup v0.1.2, Steam Global 1.0.0.2 / LuaBackend.
-- Native HUD notifications. EXP, level, rewards and saves are read-only.
LUAGUI_NAME = "Next Level Tracker"
LUAGUI_AUTH = "ROXASBrandon"
LUAGUI_DESC = "Shows EXP to Sora's next level after EXP gains. F6 previews counter."

local tick
function _OnInit()
    tick = nil
    if GAME_ID ~= 0xAF71841E or ENGINE_TYPE ~= "BACKEND" then return end
    if ReadByte(0x4698D2) ~= 106 or ReadInt(0x3EA388) ~= 540680280
        or ReadByte(0x26E20C) ~= 9 then
        ConsolePrint("Next Level Popup: unsupported build; requires Steam Global 1.0.0.2.")
        return
    end
    local loader, reason = package.loadlib(SCRIPT_PATH .. "/io_packages/kh1_next_level_popup.dll",
                                          "kh1_next_level_bootstrap")
    if not loader then
        ConsolePrint("Next Level Popup: helper unavailable: " .. tostring(reason))
        return
    end
    tick = loader
    ConsolePrint("Next Level Popup: helper loaded. Gain EXP or press F6 during gameplay.")
end

function _OnFrame()
    if tick then tick() end
end
