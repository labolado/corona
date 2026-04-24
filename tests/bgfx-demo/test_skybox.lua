-- Minimal skybox path distortion test
-- Tests q-coordinate perspective-correct texture mapping
-- GL should look correct, bgfx shows vertical line artifacts

display.setDefault("background", 0.2, 0.2, 0.2)

local label = display.newText("Path Distortion Test (q-coord)", display.contentCenterX, 30, native.systemFont, 14)

-- Simple checkerboard image for clear distortion visibility
local img = display.newImageRect("test_checker.png", 200, 200)
img.x, img.y = display.contentCenterX - 120, display.contentCenterY - 80

-- Apply path distortion (simulating perspective like skybox does)
local path = img.path
path.x1, path.y1 = -30, -20   -- top-left
path.x2, path.y2 = -10, 20    -- bottom-left
path.x3, path.y3 = 50, 30     -- bottom-right
path.x4, path.y4 = 40, -10    -- top-right

-- Second image with extreme distortion
local img2 = display.newImageRect("test_checker.png", 200, 200)
img2.x, img2.y = display.contentCenterX + 120, display.contentCenterY - 80

local path2 = img2.path
path2.x1, path2.y1 = -60, -40
path2.x2, path2.y2 = -20, 40
path2.x3, path2.y3 = 80, 50
path2.x4, path2.y4 = 70, -30

-- Third: animate path distortion like skybox rotation
local img3 = display.newImageRect("test_checker.png", 300, 300)
img3.x, img3.y = display.contentCenterX, display.contentCenterY + 150

local t = 0
Runtime:addEventListener("enterFrame", function()
    t = t + 0.02
    local path3 = img3.path
    local dx = math.sin(t) * 60
    local dy = math.cos(t * 0.7) * 40
    path3.x1 = -dx
    path3.y1 = -dy
    path3.x2 = -dx * 0.5
    path3.y2 = dy
    path3.x3 = dx
    path3.y3 = dy * 0.8
    path3.x4 = dx * 0.7
    path3.y4 = -dy * 0.5
end)

local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
local info = display.newText("Backend: " .. backend, display.contentCenterX, display.contentHeight - 20, native.systemFont, 12)

local fc2 = 0
Runtime:addEventListener("enterFrame", function()
    fc2 = fc2 + 1
    if fc2 == 30 then
        print("SCREENSHOT_READY")
    end
end)
