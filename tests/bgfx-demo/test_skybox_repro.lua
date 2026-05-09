--[[
    test_skybox_repro.lua - Reproduce mech_test_copy skybox rendering bug
    Usage: SOLAR2D_TEST=skybox_repro SOLAR2D_BACKEND=bgfx ./Corona\ Simulator ...

    Core pattern from mech_test_copy:
    - graphics.newImageSheet → sliced display.newImage
    - Per-frame path.x1..x4 manipulation (3D perspective projection)
    - vec3 rotation math
    - z-sorting with toFront()

    Env: REPRO_AUTO=1 for auto-capture
--]]

display.setStatusBar(display.HiddenStatusBar)
print("=== skybox_repro test ===")

local W = display.contentWidth
local H = display.contentHeight
local CX = display.contentCenterX
local CY = display.contentCenterY

------------------------------------------------------------
-- Inline vec3 (minimal)
------------------------------------------------------------
local vec3_mt = {}
vec3_mt.__index = vec3_mt

local function vec3(x, y, z)
    return setmetatable({x = x or 0, y = y or 0, z = z or 0}, vec3_mt)
end

function vec3_mt:rot_around(axis, angle)
    local alen = axis.x * self.x + axis.y * self.y + axis.z * self.z
    local ax, ay, az = axis.x * alen, axis.y * alen, axis.z * alen
    local px, py, pz = self.x - ax, self.y - ay, self.z - az
    local rx = axis.y * self.z - axis.z * self.y
    local ry = axis.z * self.x - axis.x * self.z
    local rz = axis.x * self.y - axis.y * self.x
    local c, s = math.cos(angle), math.sin(angle)
    return vec3(ax + px*c + rx*s, ay + py*c + ry*s, az + pz*c + rz*s)
end

------------------------------------------------------------
-- Simplified SlicedImage (matches mech skybox.lua pattern)
------------------------------------------------------------
local PROJ_SCALE = 1250  -- s = 250 * 5 from original

local function position3d(v)
    return PROJ_SCALE * v.x / v.z + CX, PROJ_SCALE * v.y / v.z + CY
end

local function createSlicedImage(group, filename, numSlices, imgW, imgH)
    local n = numSlices
    local sw = math.floor(imgW / n)
    local sh = math.floor(imgH / n)
    local sheet = graphics.newImageSheet(filename, {
        width = sw, height = sh,
        numFrames = n * n,
        sheetContentWidth = imgW,
        sheetContentHeight = imgH
    })
    if not sheet then
        print("WARNING: Failed to create ImageSheet for " .. filename)
        return nil
    end

    local face = {
        slices = {},
        map = {},
        pos = vec3(0, 0, 0),
        numSlices = n,
    }

    for y = 1, n do
        for x = 1, n do
            local ind = x + (y - 1) * n
            local s = display.newImage(group, sheet, ind)
            if s then
                s.anchorX = 0
                s.anchorY = 0
                s.mX = x - 1
                s.mY = y - 1
                s.pivot = face
                s.faceSize = imgW / n
                face.slices[ind] = s
            end
        end
    end
    return face
end

local function setFaceVisibility(face, vis)
    for _, s in pairs(face.slices) do s.isVisible = vis end
end

local function faceFront(face)
    for _, s in pairs(face.slices) do s:toFront() end
end

------------------------------------------------------------
-- Skybox: 6 faces with 3D projection (matching mech pattern)
------------------------------------------------------------
local group = display.newGroup()

-- Background
local bg = display.newRect(group, CX, CY, W, H)
bg:setFillColor(0.12, 0.12, 0.18)

-- Use actual mech skybox images (1024x1024, same as mech_test_copy)
local faceImages = {
    "skybox01/front.jpg",  -- back (mech naming: front.jpg = cube back face)
    "skybox01/back.jpg",   -- front
    "skybox01/left.jpg",   -- left
    "skybox01/right.jpg",  -- right
    "skybox01/top.jpg",    -- up
    "skybox01/bottom.jpg", -- down
}

local n = 4  -- 4x4 slices per face (exact mech_test_copy setting)
local s = 1024  -- face texture size (exact mech_test_copy setting)

local faces = {}
for i, img in ipairs(faceImages) do
    local face = createSlicedImage(group, img, n, s, s)
    if face then
        faces[#faces + 1] = face
        print("[skybox] Created face " .. i .. ": " .. img)
    end
end

-- Initialize cube vertex maps (same math as mech skybox)
local d = 1 / n
local IDX_ADD = 1

if #faces >= 6 then
    local back, front, left, right, up, down = faces[1], faces[2], faces[3], faces[4], faces[5], faces[6]

    for x = 0, n do
        for y = 0, n do
            back.map[x + y*(n+1) + IDX_ADD] = vec3((x*d - 0.5)*2, (y*d - 0.5)*2, 1)
        end
    end
    for x = 0, n do
        for y = 0, n do
            front.map[x + y*(n+1) + IDX_ADD] = vec3(((n-x)*d - 0.5)*2, (y*d - 0.5)*2, -1)
        end
    end
    for z = 0, n do
        for y = 0, n do
            left.map[z + y*(n+1) + IDX_ADD] = vec3(-1, (y*d - 0.5)*2, (z*d - 0.5)*2)
        end
    end
    for z = 0, n do
        for y = 0, n do
            right.map[z + y*(n+1) + IDX_ADD] = vec3(1, (y*d - 0.5)*2, ((n-z)*d - 0.5)*2)
        end
    end
    for x = 0, n do
        for z = 0, n do
            down.map[x + z*(n+1) + IDX_ADD] = vec3((x*d - 0.5)*2, 1, ((n-z)*d - 0.5)*2)
        end
    end
    for x = 0, n do
        for z = 0, n do
            up.map[x + z*(n+1) + IDX_ADD] = vec3((x*d - 0.5)*2, -1, (z*d - 0.5)*2)
        end
    end

    setFaceVisibility(front, false)  -- back face hidden
end

-- Rotation axes (same as mech SkyboxBg)
local rotW = vec3(-1, 0, 0)
local rotA = vec3(0, 1, 0)
local rotSpeed = 0.001 * 0.2  -- slow rotation

-- Per-frame render (THE KEY FUNCTION - path manipulation)
local function renderSkybox()
    -- Rotate all vertices
    for _, face in ipairs(faces) do
        for j = 1, #face.map do
            face.map[j] = face.map[j]:rot_around(rotW, rotSpeed)
            face.map[j] = face.map[j]:rot_around(rotA, rotSpeed)
        end
        face.pos = face.pos:rot_around(rotW, rotSpeed)
        face.pos = face.pos:rot_around(rotA, rotSpeed)
    end

    -- Z-sort faces
    table.sort(faces, function(a, b) return a.pos.z < b.pos.z end)
    for i = #faces, 1, -1 do faceFront(faces[i]) end

    -- Update path for each slice (THE CRITICAL CODE PATH)
    for _, face in ipairs(faces) do
        for _, obj in pairs(face.slices) do
            if obj and obj.pivot then
                local map = obj.pivot.map
                local mX, mY = obj.mX, obj.mY
                local nn = obj.pivot.numSlices + 1

                local topLeft = map[mX + mY*nn + IDX_ADD]
                local bottomLeft = map[mX + (mY+1)*nn + IDX_ADD]
                local bottomRight = map[mX+1 + (mY+1)*nn + IDX_ADD]
                local topRight = map[mX+1 + mY*nn + IDX_ADD]

                if topLeft.z < 0 or bottomLeft.z < 0 or bottomRight.z < 0 or topRight.z < 0 then
                    obj.isVisible = false
                else
                    obj.isVisible = true
                    local fs = obj.faceSize
                    local path = obj.path
                    local x, y
                    x, y = position3d(topLeft)
                    path.x1, path.y1 = x, y
                    x, y = position3d(bottomLeft)
                    path.x2, path.y2 = x, y - fs
                    x, y = position3d(bottomRight)
                    path.x3, path.y3 = x - fs, y - fs
                    x, y = position3d(topRight)
                    path.x4, path.y4 = x - fs, y
                end
            end
        end
    end
end

-- Run render every frame
local frameCount = 0
local function onEnterFrame()
    frameCount = frameCount + 1
    renderSkybox()
end
Runtime:addEventListener("enterFrame", onEnterFrame)

-- Label
local label = display.newText(group, "Skybox Repro (frame: 0)", CX, 20, native.systemFontBold, 14)
label:setFillColor(1)
label:toFront()

timer.performWithDelay(500, function()
    label.text = "Skybox Repro (frame: " .. frameCount .. ")"
    label:toFront()
end, 0)

-- Auto-capture at fixed frame count (eliminates timing differences)
local TARGET_FRAME = 120
local captured = false
if os.getenv("REPRO_AUTO") then
    local origEnterFrame = onEnterFrame
    Runtime:removeEventListener("enterFrame", onEnterFrame)
    onEnterFrame = function()
        frameCount = frameCount + 1
        renderSkybox()
        if frameCount == TARGET_FRAME and not captured then
            captured = true
            print("[AUTO] Capturing at frame " .. frameCount)
            local stage = display.currentStage
            local cap = display.capture(stage, {saveToPhotoLibrary = false})
            if cap then
                local backend = os.getenv("SOLAR2D_BACKEND") or "unknown"
                display.save(cap, {filename = "skybox_" .. backend .. ".png", baseDir = system.TemporaryDirectory})
                print("[AUTO] Saved skybox_" .. backend .. ".png")
                display.remove(cap)
            end
        end
    end
    Runtime:addEventListener("enterFrame", onEnterFrame)
end
