local composer = require("composer")
local scene = composer.newScene()

-- Inline vec3
local vec3_mt = {}
vec3_mt.__index = vec3_mt
local function vec3(x, y, z)
    return setmetatable({x = x or 0, y = y or 0, z = z or 0}, vec3_mt)
end
function vec3_mt:rot_around(axis, angle)
    local alen = axis.x*self.x + axis.y*self.y + axis.z*self.z
    local ax, ay, az = axis.x*alen, axis.y*alen, axis.z*alen
    local px, py, pz = self.x-ax, self.y-ay, self.z-az
    local rx = axis.y*self.z - axis.z*self.y
    local ry = axis.z*self.x - axis.x*self.z
    local rz = axis.x*self.y - axis.y*self.x
    local c, s = math.cos(angle), math.sin(angle)
    return vec3(ax+px*c+rx*s, ay+py*c+ry*s, az+pz*c+rz*s)
end

local CX, CY = display.contentCenterX, display.contentCenterY
local PROJ = 1250
local function pos3d(v) return PROJ*v.x/v.z+CX, PROJ*v.y/v.z+CY end

local function createSkybox(group, faceImages, nSlices, texSize)
    local n = nSlices
    local IDX = 1
    local faces = {}
    for i, img in ipairs(faceImages) do
        local sw = math.floor(texSize/n)
        local sheet = graphics.newImageSheet(img, {width=sw, height=sw, numFrames=n*n, sheetContentWidth=texSize, sheetContentHeight=texSize})
        if sheet then
            local face = {slices={}, map={}, pos=vec3(0,0,0), numSlices=n}
            for y=1,n do for x=1,n do
                local ind = x+(y-1)*n
                local s = display.newImage(group, sheet, ind)
                if s then s.anchorX=0; s.anchorY=0; s.mX=x-1; s.mY=y-1; s.pivot=face; s.faceSize=texSize/n; face.slices[ind]=s end
            end end
            faces[#faces+1] = face
        end
    end
    -- Init maps
    local d = 1/n
    if #faces >= 6 then
        for x=0,n do for y=0,n do faces[1].map[x+y*(n+1)+IDX]=vec3((x*d-0.5)*2,(y*d-0.5)*2,1) end end
        for x=0,n do for y=0,n do faces[2].map[x+y*(n+1)+IDX]=vec3(((n-x)*d-0.5)*2,(y*d-0.5)*2,-1) end end
        for z=0,n do for y=0,n do faces[3].map[z+y*(n+1)+IDX]=vec3(-1,(y*d-0.5)*2,(z*d-0.5)*2) end end
        for z=0,n do for y=0,n do faces[4].map[z+y*(n+1)+IDX]=vec3(1,(y*d-0.5)*2,((n-z)*d-0.5)*2) end end
        for x=0,n do for z=0,n do faces[5].map[x+z*(n+1)+IDX]=vec3((x*d-0.5)*2,1,((n-z)*d-0.5)*2) end end
        for x=0,n do for z=0,n do faces[6].map[x+z*(n+1)+IDX]=vec3((x*d-0.5)*2,-1,(z*d-0.5)*2) end end
        for _,s in pairs(faces[2].slices) do s.isVisible=false end -- hide front face
    end
    return faces, IDX
end

local rotW = vec3(-1,0,0)
local rotA = vec3(0,1,0)

local function renderSkybox(faces, IDX, speed)
    for _,f in ipairs(faces) do
        for j=1,#f.map do f.map[j]=f.map[j]:rot_around(rotW,speed); f.map[j]=f.map[j]:rot_around(rotA,speed) end
        f.pos=f.pos:rot_around(rotW,speed); f.pos=f.pos:rot_around(rotA,speed)
    end
    table.sort(faces, function(a,b) return a.pos.z<b.pos.z end)
    for i=#faces,1,-1 do for _,s in pairs(faces[i].slices) do s:toFront() end end
    for _,face in ipairs(faces) do
        for _,obj in pairs(face.slices) do
            local map=obj.pivot.map; local mX,mY=obj.mX,obj.mY; local nn=obj.pivot.numSlices+1
            local tl=map[mX+mY*nn+IDX]; local bl=map[mX+(mY+1)*nn+IDX]; local br=map[mX+1+(mY+1)*nn+IDX]; local tr=map[mX+1+mY*nn+IDX]
            if tl.z<0 or bl.z<0 or br.z<0 or tr.z<0 then obj.isVisible=false
            else
                obj.isVisible=true; local fs=obj.faceSize; local p=obj.path; local x,y
                x,y=pos3d(tl); p.x1,p.y1=x,y
                x,y=pos3d(bl); p.x2,p.y2=x,y-fs
                x,y=pos3d(br); p.x3,p.y3=x-fs,y-fs
                x,y=pos3d(tr); p.x4,p.y4=x-fs,y
            end
        end
    end
end

-- Register tint shader
pcall(function()
    graphics.defineEffect({language="glsl", category="filter", name="tint",
        fragment=[[
            P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
                P_COLOR vec4 col = texture2D(CoronaSampler0, uv);
                P_COLOR vec4 res = vec4(1.0);
                res.rgb *= col.a; res.a = col.a;
                return CoronaColorScale(res);
            }
        ]]})
end)

local faceImgs = {"t1.jpg","soil2.jpg","grass1.png","test_gradient.png","castle-ground1.jpg","desert-ground-1.jpg"}

function scene:create(event)
    local g = self.view
    print("[SceneA] create — skybox + snapshot + tint")

    -- 1) Skybox background (like mech SkyboxBg)
    local skyboxGroup = display.newGroup()
    g:insert(skyboxGroup)
    local faces, IDX = createSkybox(skyboxGroup, faceImgs, 2, 512)
    self._faces = faces
    self._IDX = IDX

    -- 2) Snapshot with game objects + tint shader (like mech PreviewPage)
    local workGroup = display.newGroup()
    math.randomseed(42)
    for i=1,15 do
        local r = display.newRect(workGroup, math.random(20,280), math.random(60,400), 30+math.random(30), 30+math.random(30))
        r:setFillColor(math.random(), math.random(), math.random())
        r.rotation = math.random(360)
    end
    local W,H = display.contentWidth, display.contentHeight
    local snap = display.newSnapshot(g, W, H)
    snap.x, snap.y = CX, CY
    workGroup:translate(-CX, -CY)
    snap.group:insert(workGroup)
    snap:invalidate()
    snap.fill.effect = "filter.custom.tint"
    snap:setFillColor(1, 1, 1, 0.3)
    self._snapshot = snap

    -- 3) UI elements on top
    display.newText(g, "Scene A: Skybox+Snap+Tint", CX, 20, native.systemFontBold, 14):setFillColor(1)

    local btn = display.newRect(g, CX, display.contentHeight-40, 140, 36)
    btn:setFillColor(0.2, 0.5, 0.9)
    display.newText(g, "Go to Scene B", btn.x, btn.y, native.systemFont, 14):setFillColor(1)
    btn:addEventListener("tap", function()
        composer.gotoScene("scene_switch_b", {effect="slideLeft", time=300})
    end)

    -- enterFrame: rotate skybox + invalidate snapshot
    self._enterFrame = function()
        if self._faces then renderSkybox(self._faces, self._IDX, 0.0002) end
        if self._snapshot and self._snapshot.invalidate then self._snapshot:invalidate() end
    end
    Runtime:addEventListener("enterFrame", self._enterFrame)
end

function scene:show(event)
    if event.phase == "did" then print("[SceneA] show:did") end
end
function scene:hide(event)
    if event.phase == "will" then
        if self._enterFrame then Runtime:removeEventListener("enterFrame", self._enterFrame); self._enterFrame=nil end
    end
end
function scene:destroy(event)
    print("[SceneA] destroy (recycled)")
    if self._enterFrame then Runtime:removeEventListener("enterFrame", self._enterFrame); self._enterFrame=nil end
    self._faces=nil; self._snapshot=nil
end

scene:addEventListener("create", scene)
scene:addEventListener("show", scene)
scene:addEventListener("hide", scene)
scene:addEventListener("destroy", scene)
return scene
