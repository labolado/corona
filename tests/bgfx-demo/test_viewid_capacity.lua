print("=== bgfx viewId capacity test ===")

display.setStatusBar(display.HiddenStatusBar)

local group = display.newGroup()
local W, H = display.contentWidth, display.contentHeight

local bg = display.newRect(group, W * 0.5, H * 0.5, W, H)
bg:setFillColor(0.04, 0.06, 0.08)

local count = tonumber(os.getenv("E1A_VIEWID_SNAPSHOT_COUNT") or "220") or 220
local textures = {}

for i = 1, count do
    local tex = graphics.newTexture({ type = "canvas", width = 16, height = 16 })
    textures[#textures + 1] = tex
    local r = display.newRect(0, 0, 12, 12)
    r:setFillColor((i % 5) / 4, ((i + 2) % 7) / 6, ((i + 3) % 9) / 8)
    tex:draw(r)
    tex:invalidate()
    display.remove(r)
end

local marker = display.newRect(group, W * 0.5, 32, W - 40, 34)
marker:setFillColor(0.1, 0.75, 0.25)
marker:toFront()

local label = display.newText({
    parent = group,
    text = "VIEWID CAPACITY 220",
    x = W * 0.5,
    y = 32,
    font = native.systemFontBold,
    fontSize = 16
})
label:setFillColor(1, 1, 1)
label:toFront()

Runtime:addEventListener("enterFrame", function(event)
    marker.rotation = math.sin(event.time * 0.004) * 1.5
end)

timer.performWithDelay(1500, function()
    print("VIEWID_CAPACITY_READY count=" .. tostring(count))
    print("SCREENSHOT_READY")
end)
