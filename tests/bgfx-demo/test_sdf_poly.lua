--[[
    test_sdf_poly.lua — arbitrary polygon SDF bevel, dynamic lighting, tap to add
    Three quality tiers: H (full bevel, 3 sdPoly calls) / M (edge-darken, 1 call) / L (flat AA, 1 call)
    Auto-detect GPU tier on startup; UI buttons override for testing.
    Rotation: CPU-side vertex rotation per frame (avoids uniform slot limit issue).
--]]
display.setStatusBar(display.HiddenStatusBar)
system.setIdleTimer(false)
math.randomseed(os.time())
display.setDefault("background", 0.05, 0.05, 0.08)
local W, H = display.contentWidth, display.contentHeight

-- Common uniform block (identical for all tiers; only fragment logic differs)
local UNIFORMS = {
    { name="cn",      type="vec4", index=0, default={1,0.5,0.2,5} },
    { name="verts01", type="vec4", index=1, default={0,-0.8,0.76,-0.25} },
    { name="verts23", type="vec4", index=2, default={0.47,0.65,-0.47,0.65} },
    { name="verts45", type="vec4", index=3, default={-0.76,-0.25,0,0} },
    { name="verts67", type="vec4", index=4, default={0,0,0,0} },
    { name="verts89", type="vec4", index=5, default={0,0,0,0} },
}

-- Shared vertex fetch + sdPoly (inlined into each effect's fragment string)
local POLY_FUNCS = [[
    uniform P_COLOR    vec4 u_UserData0;
    uniform P_POSITION vec4 u_UserData1;
    uniform P_POSITION vec4 u_UserData2;
    uniform P_POSITION vec4 u_UserData3;
    uniform P_POSITION vec4 u_UserData4;
    uniform P_POSITION vec4 u_UserData5;

    P_POSITION vec2 getV(int i){
        if(i==0)return u_UserData1.xy; if(i==1)return u_UserData1.zw;
        if(i==2)return u_UserData2.xy; if(i==3)return u_UserData2.zw;
        if(i==4)return u_UserData3.xy; if(i==5)return u_UserData3.zw;
        if(i==6)return u_UserData4.xy; if(i==7)return u_UserData4.zw;
        if(i==8)return u_UserData5.xy; return u_UserData5.zw;
    }
    P_POSITION float sdPoly(P_POSITION vec2 p){
        int n=int(u_UserData0.w);
        P_POSITION float d=dot(p-getV(0),p-getV(0));
        P_POSITION float s=1.0;
        int j=n-1;
        for(int i=0;i<10;i++){
            if(i>=n)break;
            P_POSITION vec2 vi=getV(i),vj=getV(j),e=vj-vi,w=p-vi;
            P_POSITION vec2 b=w-e*clamp(dot(w,e)/dot(e,e),0.0,1.0);
            d=min(d,dot(b,b));
            bvec3 c=bvec3(p.y>=vi.y,p.y<vj.y,e.x*w.y>e.y*w.x);
            if(all(c)||all(not(c)))s=-s;
            j=i;
        }
        return s*sqrt(d);
    }
    P_POSITION float getH(P_POSITION vec2 p,P_POSITION float bw){
        P_POSITION float d=sdPoly(p);
        if(d>0.0)return 0.0;
        P_POSITION float t=clamp(-d/bw,0.0,1.0);
        return t*t*(3.0-2.0*t);
    }
]]

-- HIGH: full bevel + specular, 3 sdPoly calls
graphics.defineEffect({
    language="glsl", category="generator", name="sdfpoly_h",
    isTimeDependent=true, uniformData=UNIFORMS,
    fragment=POLY_FUNCS..[[
    P_COLOR vec4 FragmentKernel(P_UV vec2 texCoord){
        P_COLOR vec3 col=u_UserData0.xyz;
        P_POSITION vec2 p=texCoord*2.0-1.0;
        P_POSITION float d=sdPoly(p);
        P_POSITION float fw=fwidth(d);
        P_POSITION float alpha=smoothstep(fw,-fw,d);
        if(alpha<0.001)discard;
        P_POSITION float bw=0.13,bd=0.087,gs=28.0,si=0.55,lz=1.0;
        P_POSITION vec2 eps=vec2(0.004,0.0);
        P_POSITION float t0=clamp(-d/bw,0.0,1.0);
        P_POSITION float hc=d>0.0?0.0:t0*t0*(3.0-2.0*t0);
        P_POSITION float hx=getH(p+eps.xy,bw)-hc;
        P_POSITION float hy=getH(p+eps.yx,bw)-hc;
        P_POSITION vec3 n=normalize(vec3(-vec2(hx,hy)*(bd/(2.0*eps.x)),1.0));
        P_POSITION vec3 ld=normalize(vec3(0.6,-0.6,lz));
        P_POSITION vec3 hd=normalize(ld+vec3(0,0,1));
        P_POSITION float diff=max(dot(n,ld),0.0);
        P_POSITION float spec=pow(max(dot(n,hd),0.0),gs);
        P_POSITION float flat=smoothstep(0.0,0.8,hc);
        P_POSITION float bright=mix(0.35+diff*0.65, 0.85, flat);
        P_COLOR vec3 lit=col*bright + vec3(spec*si*(1.0-flat*0.8));
        return CoronaColorScale(vec4(lit,alpha));
    }]],
})

-- MID: 1 sdPoly call, edge-to-center gradient shading
graphics.defineEffect({
    language="glsl", category="generator", name="sdfpoly_m",
    isTimeDependent=true, uniformData=UNIFORMS,
    fragment=POLY_FUNCS..[[
    P_COLOR vec4 FragmentKernel(P_UV vec2 texCoord){
        P_COLOR vec3 col=u_UserData0.xyz;
        P_POSITION vec2 p=texCoord*2.0-1.0;
        P_POSITION float d=sdPoly(p);
        P_POSITION float fw=fwidth(d);
        P_POSITION float alpha=smoothstep(fw,-fw,d);
        if(alpha<0.001)discard;
        P_POSITION float rim=smoothstep(0.0,0.18,-d);
        P_COLOR vec3 lit=col*(0.38+rim*0.62);
        return CoronaColorScale(vec4(lit,alpha));
    }]],
})

-- LOW: 1 sdPoly call, flat fill with AA edge only
graphics.defineEffect({
    language="glsl", category="generator", name="sdfpoly_l",
    isTimeDependent=true, uniformData=UNIFORMS,
    fragment=POLY_FUNCS..[[
    P_COLOR vec4 FragmentKernel(P_UV vec2 texCoord){
        P_COLOR vec3 col=u_UserData0.xyz;
        P_POSITION vec2 p=texCoord*2.0-1.0;
        P_POSITION float d=sdPoly(p);
        P_POSITION float fw=fwidth(d);
        P_POSITION float alpha=smoothstep(fw,-fw,d);
        if(alpha<0.001)discard;
        return CoronaColorScale(vec4(col,alpha));
    }]],
})

-- GPU tier auto-detection via renderer string
local function detectTier()
    local r = (system.getInfo("gpuRenderer") or ""):lower()
    if r:find("apple") or r:find("adreno 6") or r:find("adreno 7")
       or r:find("adreno 8") or r:find("mali%-g7") or r:find("mali%-g8")
       or r:find("mali%-g9") then
        return "h"
    elseif r:find("adreno 5") or r:find("mali%-g5") or r:find("mali%-g6")
           or r:find("powervr ge8") then
        return "m"
    else
        return "l"
    end
end

local qualityTier = detectTier()
print("=== GPU tier detected: " .. qualityTier .. " renderer=" .. (system.getInfo("gpuRenderer") or "?"))

local function effectName() return "generator.custom.sdfpoly_" .. qualityTier end

-- update all uniforms every frame: cn (color+n) + rotated vertices
local function updatePoly(obj, angle, verts, color)
    local n = #verts / 2
    obj.fill.effect.cn = {color[1], color[2], color[3], n}
    local ca, sa = math.cos(angle), math.sin(angle)
    local function rx(i) return ca*(verts[i] or 0) - sa*(verts[i+1] or 0) end
    local function ry(i) return sa*(verts[i] or 0) + ca*(verts[i+1] or 0) end
    obj.fill.effect.verts01 = {rx(1),  ry(1),  rx(3),  ry(3)}
    obj.fill.effect.verts23 = {rx(5),  ry(5),  rx(7),  ry(7)}
    obj.fill.effect.verts45 = {rx(9),  ry(9),  rx(11), ry(11)}
    obj.fill.effect.verts67 = {rx(13), ry(13), rx(15), ry(15)}
    obj.fill.effect.verts89 = {rx(17), ry(17), rx(19), ry(19)}
end

local function setPoly(obj, color, verts)
    updatePoly(obj, 0, verts, color)
end

local function ngon(n, r)
    local v = {}
    for i = 0, n-1 do
        local a = math.pi/2 - i*2*math.pi/n
        v[#v+1] = r*math.cos(a); v[#v+1] = -r*math.sin(a)
    end
    return v
end
local function star(n, ro, ri)
    local v = {}
    for i = 0, n-1 do
        local a = math.pi/2 - i*2*math.pi/n
        v[#v+1] = ro*math.cos(a); v[#v+1] = -ro*math.sin(a)
        local b = a - math.pi/n
        v[#v+1] = ri*math.cos(b); v[#v+1] = -ri*math.sin(b)
    end
    return v
end

local SHAPES = {
    {color={0.4,0.65,1.0},  verts=ngon(5,0.80)},
    {color={1.0,0.78,0.15}, verts=star(5,0.85,0.35)},
    {color={0.25,0.85,0.45},verts={-0.80,0.22,0.00,0.22,0.00,0.65,0.80,0.00,0.00,-0.65,0.00,-0.22,-0.80,-0.22}},
    {color={0.8,0.3,0.75},  verts=ngon(6,0.80)},
    {color={0.9,0.45,0.2},  verts=star(4,0.85,0.38)},
    {color={0.4,0.85,0.85}, verts={-0.50,0.80,0.80,0.80,0.80,0.10,0.15,0.10,0.15,-0.80,-0.50,-0.80}},
    {color={1.0,0.4,0.4},   verts=ngon(3,0.85)},
    {color={0.6,0.9,0.3},   verts=star(6,0.82,0.42)},
}
local COLORS = {
    {0.4,0.65,1.0},{1.0,0.78,0.15},{0.25,0.85,0.45},{0.8,0.3,0.75},
    {0.9,0.45,0.2},{0.4,0.85,0.85},{1.0,0.4,0.4},{0.6,0.9,0.3},
    {0.9,0.7,0.2},{0.5,0.3,1.0},{0.2,0.9,0.7},{1.0,0.5,0.7},
}

local sz = 90
local objs = {}

local function addShape(x, y, shapeIdx, colorIdx, initAngle, fixedSpd)
    local si = shapeIdx or math.random(#SHAPES)
    local ci = colorIdx or math.random(#COLORS)
    local s = SHAPES[si]
    local c = COLORS[ci]
    local r = display.newRect(x, y, sz, sz)
    r.fill.effect = effectName()
    setPoly(r, c, s.verts)
    local spd = fixedSpd or (math.random()*0.04 - 0.02)
    if not fixedSpd and math.abs(spd) < 0.008 then spd = 0.012 end
    objs[#objs+1] = {r=r, angle=initAngle or 0, spd=spd, verts=s.verts, color=c}
    return r
end

-- Quality tier UI
local tierBtns = {}
local TIER_COLORS = {
    h = {active={0.2,0.85,0.45}, idle={0.45,0.45,0.45}},
    m = {active={1.0,0.80,0.2},  idle={0.45,0.45,0.45}},
    l = {active={0.9,0.40,0.4},  idle={0.45,0.45,0.45}},
}
local TIER_NAMES = {h="HIGH", m="MID", l="LOW"}

local hud = display.newText("SDF[?]  0 shapes  -- fps", W*0.43, H*0.038, native.systemFont, 11)
hud:setFillColor(1, 0.9, 0.7)

local function applyQualityTier(tier)
    qualityTier = tier
    local eff = effectName()
    for _, o in ipairs(objs) do
        o.r.fill.effect = eff
        updatePoly(o.r, o.angle, o.verts, o.color)
    end
    for t, btn in pairs(tierBtns) do
        local col = (t == tier) and TIER_COLORS[t].active or TIER_COLORS[t].idle
        btn:setFillColor(unpack(col))
    end
    print("=== Quality tier set: " .. tier)
end

local btnY = H * 0.038
local tierOrder = {"h", "m", "l"}
local btnW, btnH = 46, 22
local totalW = #tierOrder * (btnW + 6)
local startX = W - totalW * 0.5 + btnW * 0.5

for i, tier in ipairs(tierOrder) do
    local bx = startX + (i-1) * (btnW + 6)
    local bg = display.newRoundedRect(bx, btnY, btnW, btnH, 5)
    bg:setFillColor(0.10, 0.10, 0.13)
    bg:setStrokeColor(0.28, 0.28, 0.32)
    bg.strokeWidth = 1

    local col = (tier == qualityTier) and TIER_COLORS[tier].active or TIER_COLORS[tier].idle
    local lbl = display.newText(TIER_NAMES[tier], bx, btnY, native.systemFont, 11)
    lbl:setFillColor(unpack(col))
    tierBtns[tier] = lbl

    bg:addEventListener("tap", function() applyQualityTier(tier); return true end)
    lbl:addEventListener("tap", function() applyQualityTier(tier); return true end)
end

-- Initial shapes
local initDefs = {
    {x=W*0.2, y=H*0.30, idx=1, cidx=1, label="Pentagon", spd= 0.020},
    {x=W*0.5, y=H*0.30, idx=2, cidx=2, label="5-Star",   spd=-0.025},
    {x=W*0.8, y=H*0.30, idx=3, cidx=3, label="Arrow",    spd= 0.015},
    {x=W*0.2, y=H*0.68, idx=4, cidx=4, label="Hexagon",  spd=-0.018},
    {x=W*0.5, y=H*0.68, idx=5, cidx=5, label="4-Star",   spd= 0.022},
    {x=W*0.8, y=H*0.68, idx=6, cidx=6, label="L-shape",  spd=-0.012},
}
for _, d in ipairs(initDefs) do
    addShape(d.x, d.y, d.idx, d.cidx, 0, d.spd)
    local lbl = display.newText(d.label, d.x, d.y + sz*0.56, native.systemFont, 10)
    lbl:setFillColor(0.7, 0.7, 0.7)
end

Runtime:addEventListener("tap", function()
    for i = 1, 50 do
        addShape(math.random(sz/2, W-sz/2), math.random(sz/2+50, H-sz/2))
    end
    hud:toFront()
    for _, btn in pairs(tierBtns) do btn:toFront() end
    return true
end)

print("=== SDF_POLY START tier=" .. qualityTier)
local docsDir = system.pathForFile("", system.DocumentsDirectory)
local t = 0
local lastT = system.getTimer()
local fpsSmooth = 60

-- FTL auto-cycle mode: add 200 shapes, cycle H→M→L, screenshot + log fps each
local FTL_MODE = (_G.gameLoopScenario ~= nil)
local FTL_TIER_SEQ = {"h", "m", "l"}
local ftlTierIdx = 0
local ftlPhaseStart = 0   -- ms
local FTL_PHASE_MS = 25000  -- 25 sec per tier (warmup + measure)
local FTL_SHOT_DELAY = 20000 -- screenshot at 20s into each phase
local ftlShotTaken = false
local ftlFpsSamples = {}

if FTL_MODE then
    -- add 200 shapes spread across screen
    for i = 1, 200 do
        addShape(math.random(sz/2, W-sz/2), math.random(sz/2+50, H-sz/2))
    end
    -- start with HIGH
    ftlTierIdx = 1
    applyQualityTier(FTL_TIER_SEQ[ftlTierIdx])
    ftlPhaseStart = system.getTimer()
    ftlShotTaken = false
    print("=== FTL MODE: 200 shapes, cycling H→M→L ===")
end

local nextShot = FTL_MODE and math.huge or 60

Runtime:addEventListener("enterFrame", function()
    for _, o in ipairs(objs) do
        o.angle = o.angle + o.spd
        updatePoly(o.r, o.angle, o.verts, o.color)
    end
    local now = system.getTimer()
    local dt = now - lastT; lastT = now
    local fps = dt > 0 and (1000/dt) or 60
    fpsSmooth = fpsSmooth*0.9 + fps*0.1
    t = t + 1

    if t % 4 == 0 then
        hud.text = string.format("SDF[%s] %d shapes  %.0f fps",
            qualityTier:upper(), #objs, fpsSmooth)
        hud:toFront()
    end

    -- FTL auto-cycle logic
    if FTL_MODE and ftlTierIdx > 0 then
        local elapsed = now - ftlPhaseStart
        local tier = FTL_TIER_SEQ[ftlTierIdx]

        -- collect fps samples after 10s warmup
        if elapsed > 10000 then
            ftlFpsSamples[#ftlFpsSamples+1] = fpsSmooth
        end

        -- screenshot at 20s mark
        if not ftlShotTaken and elapsed >= FTL_SHOT_DELAY then
            local fname = "sdf_ftl_" .. tier .. ".png"
            display.save(display.currentStage, {filename=fname, baseDir=system.DocumentsDirectory})
            print(string.format("=== FTL SHOT [%s]: %s/%s  fps=%.1f ===",
                tier:upper(), docsDir, fname, fpsSmooth))
            ftlShotTaken = true
        end

        -- phase done → next tier or finish
        if elapsed >= FTL_PHASE_MS then
            -- log avg fps for this tier
            local sum = 0
            for _, v in ipairs(ftlFpsSamples) do sum = sum + v end
            local avg = #ftlFpsSamples > 0 and (sum / #ftlFpsSamples) or 0
            print(string.format("=== FTL RESULT [%s]: avg_fps=%.1f samples=%d ===",
                tier:upper(), avg, #ftlFpsSamples))
            ftlFpsSamples = {}

            ftlTierIdx = ftlTierIdx + 1
            if ftlTierIdx > #FTL_TIER_SEQ then
                print("=== FTL COMPLETE ===")
                os.exit(0)
            else
                applyQualityTier(FTL_TIER_SEQ[ftlTierIdx])
                ftlPhaseStart = now
                ftlShotTaken = false
            end
        end
    end

    -- non-FTL timed screenshot
    if t == nextShot then
        local fname = "sdf_shot_" .. t .. ".png"
        display.save(display.currentStage, {filename=fname, baseDir=system.DocumentsDirectory})
        print("=== SHOT: " .. docsDir .. "/" .. fname .. " ===")
        nextShot = nextShot + 300
    end
end)
