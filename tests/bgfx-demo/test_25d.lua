-- Test 2.5D perspective-correct texturing (q-coordinate)
display.setDefault("background", 0.2, 0.2, 0.3)
local CX, CY = display.contentCenterX, display.contentCenterY

-- Create a textured rect with 2.5D path offsets
local img = display.newImageRect("castle-ground1.jpg", 300, 300)
if img then
    img.x, img.y = CX, CY
    -- Apply 2.5D perspective via path offsets
    img.path.x1 = -50
    img.path.y1 = -30
    img.path.x2 = -80
    img.path.y2 = 30
    img.path.x3 = 80
    img.path.y3 = 30
    img.path.x4 = 50
    img.path.y4 = -30
end

-- A second rect with stronger perspective
local img2 = display.newImageRect("grass_track1.png", 250, 250)
if img2 then
    img2.x, img2.y = CX, CY + 250
    img2.path.x1 = -100
    img2.path.y1 = -60
    img2.path.x2 = -120
    img2.path.y2 = 60
    img2.path.x3 = 120
    img2.path.y3 = 60
    img2.path.x4 = 100
    img2.path.y4 = -60
end

-- Normal rect for comparison (no offsets, q=1.0)
local img3 = display.newImageRect("castle-ground1.jpg", 150, 150)
if img3 then
    img3.x, img3.y = CX + 200, 100
end

display.newText("2.5D Perspective Test (q-divide)", CX, 15, native.systemFont, 14):setFillColor(1,1,0)
display.newText("Left: 2.5D offset | Right: normal", CX, 35, native.systemFont, 12):setFillColor(0.8,0.8,0.8)
