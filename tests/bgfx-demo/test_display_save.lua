-- test_display_save.lua: Verify display.save() produces non-blank image
local rect = display.newRect(display.contentCenterX, display.contentCenterY, 100, 100)
rect:setFillColor(1, 0, 0)

timer.performWithDelay(500, function()
    local savePath = system.pathForFile("test_save_output.png", system.DocumentsDirectory)
    display.save(display.currentStage, {
        filename = "test_save_output.png",
        baseDir = system.DocumentsDirectory,
    })
    print("DISPLAY_SAVE_TEST: saved to " .. tostring(savePath))

    -- Check file exists and has non-zero size
    local f = io.open(savePath, "rb")
    if f then
        local size = f:seek("end")
        f:close()
        if size and size > 100 then
            print("DISPLAY_SAVE_TEST: PASS (file size: " .. size .. " bytes)")
        else
            print("DISPLAY_SAVE_TEST: FAIL (file too small: " .. tostring(size) .. " bytes)")
        end
    else
        print("DISPLAY_SAVE_TEST: FAIL (file not found)")
    end

    timer.performWithDelay(500, function()
        native.requestExit()
    end)
end)
