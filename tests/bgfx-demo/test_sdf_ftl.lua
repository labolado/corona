--[[
    test_sdf_ftl.lua — SDF quality tier stress test for Firebase Test Lab
    Fully automatic: no user interaction required.
    Flow: add 200 shapes → cycle HIGH(25s) → MID(25s) → LOW(25s) → exit
    Each phase: warmup 10s, measure fps 15s, screenshot at 20s, log result.
--]]
display.setStatusBar(display.HiddenStatusBar)
system.setIdleTimer(false)
math.randomseed(12345)  -- fixed seed for reproducibility
display.setDefault("background", 0.05, 0.05, 0.08)
local W, H = display.contentWidth, display.contentHeight

local UNIFORMS = {
    { name="cn",      type="vec4", index=0, default={1,0.5,0.2,5} },
    { name="verts01", type="vec4", index=1, default={0,-0.8,0.76,-0.25} },
    { name="verts23", type="vec4", index=2, default={0.47,0.65,-0.47,0.65} },
    { name="verts45", type="vec4", index=3, default={-0.76,-0.25,0,0} },
    { name="verts67", type="vec4", index=4, default={0,0,0,0} },
    { name="verts89", type="vec4", index=5, default={0,0,0,0} },
}

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
        P_POSITION float s=1.0; int j=n-1;
        for(int i=0;i<10;i++){
            if(i>=n)break;
            P_POSITION vec2 vi=getV(i),vj=getV(j),e=vj-vi,w=p-vi;
            P_POSITION vec2 b=w-e*clamp(dot(w,e)/dot(e,e),0.0,1.0);
            d=min(d,dot(b,b));
            bvec3 c=bvec3(p.y>=vi.y,p.y<vj.y,e.x*w.y>e.y*w.x);
            if(all(c)||all(not(c)))s=-s; j=i;
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

graphics.defineEffect({
    language="glsl", category="generator", name="sdfftl_h",
    isTimeDependent=true, uniformData=UNIFORMS,
    fragment=POLY_FUNCS..[[
    P_COLOR vec4 FragmentKernel(P_UV vec2 texCoord){
        P_COLOR vec3 col=u_UserData0.xyz;
        P_POSITION vec2 p=texCoord*2.0-1.0;
        P_POSITION float d=sdPoly(p);
        P_POSITION float fw=fwidth(d); P_POSITION float alpha=smoothstep(fw,-fw,d);
        if(alpha<0.001)discard;
        P_POSITION float bw=0.13,bd=0.087,gs=28.0,si=0.55,lz=1.0;
        P_POSITION vec2 eps=vec2(0.004,0.0);
        P_POSITION float t0=clamp(-d/bw,0.0,1.0);
        P_POSITION float hc=d>0.0?0.0:t0*t0*(3.0-2.0*t0);
        P_POSITION float hx=getH(p+eps.xy,bw)-hc; P_POSITION float hy=getH(p+eps.yx,bw)-hc;
        P_POSITION vec3 n=normalize(vec3(-vec2(hx,hy)*(bd/(2.0*eps.x)),1.0));
        P_POSITION vec3 ld=normalize(vec3(0.6,-0.6,lz)); P_POSITION vec3 hd=normalize(ld+vec3(0,0,1));
        P_POSITION float diff=max(dot(n,ld),0.0); P_POSITION float spec=pow(max(dot(n,hd),0.0),gs);
        P_POSITION float flat=smoothstep(0.0,0.8,hc);
        P_COLOR vec3 lit=col*(mix(0.35+diff*0.65,0.85,flat))+vec3(spec*si*(1.0-flat*0.8));
        return CoronaColorScale(vec4(lit,alpha));
    }]],
})
graphics.defineEffect({
    language="glsl", category="generator", name="sdfftl_m",
    isTimeDependent=true, uniformData=UNIFORMS,
    fragment=POLY_FUNCS..[[
    P_COLOR vec4 FragmentKernel(P_UV vec2 texCoord){
        P_COLOR vec3 col=u_UserData0.xyz;
        P_POSITION vec2 p=texCoord*2.0-1.0;
        P_POSITION float d=sdPoly(p); P_POSITION float fw=fwidth(d);
        P_POSITION float alpha=smoothstep(fw,-fw,d); if(alpha<0.001)discard;
        return CoronaColorScale(vec4(col*(0.38+smoothstep(0.0,0.18,-d)*0.62),alpha));
    }]],
})
graphics.defineEffect({
    language="glsl", category="generator", name="sdfftl_l",
    isTimeDependent=true, uniformData=UNIFORMS,
    fragment=POLY_FUNCS..[[
    P_COLOR vec4 FragmentKernel(P_UV vec2 texCoord){
        P_COLOR vec3 col=u_UserData0.xyz;
        P_POSITION vec2 p=texCoord*2.0-1.0;
        P_POSITION float d=sdPoly(p); P_POSITION float fw=fwidth(d);
        P_POSITION float alpha=smoothstep(fw,-fw,d); if(alpha<0.001)discard;
        return CoronaColorScale(vec4(col,alpha));
    }]],
})

local function ngon(n, r)
    local v={}; for i=0,n-1 do local a=math.pi/2-i*2*math.pi/n; v[#v+1]=r*math.cos(a); v[#v+1]=-r*math.sin(a) end; return v
end
local function star(n, ro, ri)
    local v={}
    for i=0,n-1 do
        local a=math.pi/2-i*2*math.pi/n; v[#v+1]=ro*math.cos(a); v[#v+1]=-ro*math.sin(a)
        local b=a-math.pi/n; v[#v+1]=ri*math.cos(b); v[#v+1]=-ri*math.sin(b)
    end; return v
end

local SHAPES={
    {color={0.4,0.65,1.0},  verts=ngon(5,0.80)},
    {color={1.0,0.78,0.15}, verts=star(5,0.85,0.35)},
    {color={0.25,0.85,0.45},verts={-0.80,0.22,0.00,0.22,0.00,0.65,0.80,0.00,0.00,-0.65,0.00,-0.22,-0.80,-0.22}},
    {color={0.8,0.3,0.75},  verts=ngon(6,0.80)},
    {color={0.9,0.45,0.2},  verts=star(4,0.85,0.38)},
    {color={0.4,0.85,0.85}, verts={-0.50,0.80,0.80,0.80,0.80,0.10,0.15,0.10,0.15,-0.80,-0.50,-0.80}},
    {color={1.0,0.4,0.4},   verts=ngon(3,0.85)},
    {color={0.6,0.9,0.3},   verts=star(6,0.82,0.42)},
}
local COLORS={
    {0.4,0.65,1.0},{1.0,0.78,0.15},{0.25,0.85,0.45},{0.8,0.3,0.75},
    {0.9,0.45,0.2},{0.4,0.85,0.85},{1.0,0.4,0.4},{0.6,0.9,0.3},
    {0.9,0.7,0.2},{0.5,0.3,1.0},{0.2,0.9,0.7},{1.0,0.5,0.7},
}

local TIER_SEQ = {"h","m","l"}
local tierIdx = 1
local currentTier = TIER_SEQ[tierIdx]
local sz = 70
local objs = {}
local docsDir = system.pathForFile("", system.DocumentsDirectory)

local function effName(t) return "generator.custom.sdfftl_" .. t end

local function updatePoly(obj, angle, verts, color)
    local n=#verts/2
    obj.fill.effect.cn={color[1],color[2],color[3],n}
    local ca,sa=math.cos(angle),math.sin(angle)
    local function rx(i) return ca*(verts[i] or 0)-sa*(verts[i+1] or 0) end
    local function ry(i) return sa*(verts[i] or 0)+ca*(verts[i+1] or 0) end
    obj.fill.effect.verts01={rx(1),ry(1),rx(3),ry(3)}
    obj.fill.effect.verts23={rx(5),ry(5),rx(7),ry(7)}
    obj.fill.effect.verts45={rx(9),ry(9),rx(11),ry(11)}
    obj.fill.effect.verts67={rx(13),ry(13),rx(15),ry(15)}
    obj.fill.effect.verts89={rx(17),ry(17),rx(19),ry(19)}
end

local function switchTier(t)
    currentTier = t
    local eff = effName(t)
    for _, o in ipairs(objs) do
        o.r.fill.effect = eff
        updatePoly(o.r, o.angle, o.verts, o.color)
    end
    print(string.format("=== FTL TIER SWITCH → %s | shapes=%d ===", t:upper(), #objs))
end

-- HUD
local hud = display.newText("SDF-FTL init...", W*0.5, H*0.035, native.systemFont, 12)
hud:setFillColor(1,0.9,0.5)

-- Add 200 shapes spread across screen
print("=== SDF_FTL START: adding 200 shapes ===")
for i = 1, 200 do
    local si = ((i-1) % #SHAPES) + 1
    local ci = ((i-1) % #COLORS) + 1
    local s = SHAPES[si]
    local c = COLORS[ci]
    local margin = sz * 0.6
    local x = margin + math.random() * (W - margin*2)
    local y = H*0.07 + math.random() * (H*0.88)
    local r = display.newRect(x, y, sz, sz)
    r.fill.effect = effName(currentTier)
    r.fill.effect.cn = {c[1],c[2],c[3],#s.verts/2}
    r.fill.effect.verts01={s.verts[1] or 0,s.verts[2] or 0,s.verts[3] or 0,s.verts[4] or 0}
    r.fill.effect.verts23={s.verts[5] or 0,s.verts[6] or 0,s.verts[7] or 0,s.verts[8] or 0}
    r.fill.effect.verts45={s.verts[9] or 0,s.verts[10] or 0,s.verts[11] or 0,s.verts[12] or 0}
    r.fill.effect.verts67={s.verts[13] or 0,s.verts[14] or 0,s.verts[15] or 0,s.verts[16] or 0}
    r.fill.effect.verts89={s.verts[17] or 0,s.verts[18] or 0,s.verts[19] or 0,s.verts[20] or 0}
    local spd = (math.random()*0.04 - 0.02)
    if math.abs(spd) < 0.008 then spd = 0.012 end
    objs[#objs+1] = {r=r, angle=0, spd=spd, verts=s.verts, color=c}
end
print("=== 200 shapes added, starting HIGH tier ===")

-- Start at HIGH
switchTier("h")

-- Phase timing (all in ms)
local PHASE_MS    = 25000   -- 25s per tier
local SHOT_AT_MS  = 20000   -- screenshot at 20s mark
local WARMUP_MS   = 8000    -- ignore first 8s fps samples

local phaseStart = system.getTimer()
local shotTaken  = false
local fpsSamples = {}
local lastT      = system.getTimer()
local fpsSmooth  = 60

Runtime:addEventListener("enterFrame", function()
    -- rotate shapes
    for _, o in ipairs(objs) do
        o.angle = o.angle + o.spd
        updatePoly(o.r, o.angle, o.verts, o.color)
    end

    local now = system.getTimer()
    local dt = now - lastT; lastT = now
    local fps = dt > 0 and (1000/dt) or 60
    fpsSmooth = fpsSmooth*0.9 + fps*0.1

    local elapsed = now - phaseStart

    -- collect fps after warmup
    if elapsed > WARMUP_MS then
        fpsSamples[#fpsSamples+1] = fpsSmooth
    end

    -- HUD
    hud.text = string.format("SDF-FTL [%s] %d shapes  %.0f fps  phase %.0fs/25s",
        currentTier:upper(), #objs, fpsSmooth, elapsed/1000)
    hud:toFront()

    -- screenshot at 20s
    if not shotTaken and elapsed >= SHOT_AT_MS then
        local fname = "sdf_ftl_" .. currentTier .. ".png"
        display.save(display.currentStage, {filename=fname, baseDir=system.DocumentsDirectory})
        print(string.format("=== FTL_SHOT [%s]: %s/%s fps=%.1f ===",
            currentTier:upper(), docsDir, fname, fpsSmooth))
        shotTaken = true
    end

    -- phase end → next tier or exit
    if elapsed >= PHASE_MS then
        -- compute avg fps
        local sum = 0
        for _, v in ipairs(fpsSamples) do sum = sum + v end
        local avg = #fpsSamples > 0 and (sum/#fpsSamples) or 0
        print(string.format("=== FTL_RESULT [%s]: avg_fps=%.1f min_fps=%.1f samples=%d ===",
            currentTier:upper(), avg,
            (function() local m=999; for _,v in ipairs(fpsSamples) do if v<m then m=v end end; return m end)(),
            #fpsSamples))

        tierIdx = tierIdx + 1
        if tierIdx > #TIER_SEQ then
            print("=== FTL_COMPLETE ===")
            os.exit(0)
        else
            fpsSamples = {}
            shotTaken  = false
            phaseStart = now
            switchTier(TIER_SEQ[tierIdx])
        end
    end
end)
