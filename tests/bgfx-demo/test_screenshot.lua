local function captureScreenshot(filename)
    display.save(display.currentStage, {
        filename = filename,
        baseDir = system.DocumentsDirectory,
        captureOffscreenArea = true,
    })
    print("Screenshot saved: " .. filename)
end

timer.performWithDelay(3000, function()
    local backend = system.getInfo("gpu") or "unknown"
    captureScreenshot("screenshot_" .. backend .. ".png")
end)
