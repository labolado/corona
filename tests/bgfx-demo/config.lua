--[[
    config.lua - Solar2D bgfx Test Demo Configuration

    High resolution content area for accurate benchmarks on all devices.
    2048 width ensures objects fill the screen on modern devices.
--]]

local aspectRatio = display.pixelHeight / display.pixelWidth
application = {
    content = {
        width = 2048,
        height = math.ceil(2048 * aspectRatio),
        scale = "letterbox",
        fps = 60,
        imageSuffix = {}
    }
}
