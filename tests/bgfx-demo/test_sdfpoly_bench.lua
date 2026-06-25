--[[
    test_sdfpoly_bench.lua — SDF polygon bevel performance benchmark
    Tests N objects with arbitrary polygon SDF bevel effect (Method A: N draw calls).
    Tiers: 100 / 300 / 600 / 1000 / 2000 objects.
    Usage: SOLAR2D_TEST=sdfpoly_bench ./Corona\ Simulator ... tests/bgfx-demo
           or set via GameLoopActivity on Android
--]]
display.setStatusBar(display.HiddenStatusBar)
system.setIdleTimer(false)
display.setDefault("background", 0.05, 0.05, 0.08)

local W, H = display.contentWidth, display.contentHeight
local WARMUP, MEASURE = 60, 180
local TIERS = {100, 300, 600, 1000, 2000}

print("=== SDFPOLY BENCH START ===")
print("Display: "..W.."x"..H)

-- Define the polygon SDF bevel effect (same as test_sdf_poly.lua)
graphics.defineEffect({
    language="glsl", category="filter", name="sdfpoly",
    uniformData={
        { name="cn",      type="vec4", index=0, default={1,0.5,0.2,5} },
        { name="verts01", type="vec4", index=1, default={0,0,0,0} },
        { name="verts23", type="vec4", index=2, default={0,0,0,0} },
        { name="verts45", type="vec4", index=3, default={0,0,0,0} },
        { name="verts67", type="vec4", index=4, default={0,0,0,0} },
        { name="verts89", type="vec4", index=5, default={0,0,0,0} },
        { name="rot",     type="vec4", index=6, default={0,0,0,0} },
    },
    fragment=[[
        uniform P_COLOR    vec4 u_UserData0;
        uniform P_POSITION vec4 u_UserData1;
        uniform P_POSITION vec4 u_UserData2;
        uniform P_POSITION vec4 u_UserData3;
        uniform P_POSITION vec4 u_UserData4;
        uniform P_POSITION vec4 u_UserData5;
        uniform P_POSITION vec4 u_UserData6;
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
            P_POSITION float d=sdPoly(p); if(d>0.0)return 0.0;
            P_POSITION float t=clamp(-d/bw,0.0,1.0); return t*t*(3.0-2.0*t);
        }
        P_COLOR vec4 FragmentKernel(P_UV vec2 texCoord){
            P_COLOR vec3 col=u_UserData0.xyz;
            P_POSITION vec2 p=texCoord*2.0-1.0;
            P_POSITION float a=u_UserData6.x;
            P_POSITION float ca=cos(a),sa=sin(a);
            p=vec2(ca*p.x-sa*p.y,sa*p.x+ca*p.y);
            P_POSITION float d=sdPoly(p);
            P_POSITION float fw=fwidth(d);
            P_POSITION float alpha=smoothstep(fw,-fw,d);
            if(alpha<0.001)discard;
            P_POSITION float bw=0.12,bd=1.8,gs=28.0,si=0.75,lz=0.8;
            P_POSITION vec2 eps=vec2(0.005,0.0);
            P_POSITION float hc=getH(p,bw);
            P_POSITION float hx=getH(p+eps.xy,bw)-getH(p-eps.xy,bw);
            P_POSITION float hy=getH(p+eps.yx,bw)-getH(p-eps.yx,bw);
            P_POSITION vec3 n=normalize(vec3(-vec2(hx,hy)*(bd/(2.0*eps.x)),1.0));
            P_POSITION vec3 ld=normalize(vec3(0.707,-0.707,lz));
            P_POSITION vec3 hd=normalize(ld+vec3(0,0,1));
            P_POSITION float diff=max(dot(n,ld),0.0);
            P_POSITION float spec=pow(max(dot(n,hd),0.0),gs);
            P_POSITION float sh=mix(0.55,1.0,smoothstep(0.0,0.6,hc));
            P_COLOR vec3 lit=col*vec3(0.25,0.18,0.0)*sh
                            +vec3(1.0,0.95,0.85)*diff*0.65+vec3(spec*si);
            return CoronaColorScale(vec4(lit,alpha));
        }
    ]],
})

local function setPoly(obj, color, n, verts)
    local function v(i) return verts[i] or 0 end
    obj.fill.effect.cn     ={color[1],color[2],color[3],n}
    obj.fill.effect.verts01={v(1),v(2),v(3),v(4)}
    obj.fill.effect.verts23={v(5),v(6),v(7),v(8)}
    obj.fill.effect.verts45={v(9),v(10),v(11),v(12)}
    obj.fill.effect.verts67={v(13),v(14),v(15),v(16)}
    obj.fill.effect.verts89={v(17),v(18),v(19),v(20)}
    obj.fill.effect.rot={0,0,0,0}
end

-- Preset shapes: pentagon, hexagon, 5-star, arrow (mix = realistic game scenario)
local SHAPES={
    {n=5, verts={0,-0.8, 0.76,-0.25, 0.47,0.65, -0.47,0.65, -0.76,-0.25}},
    {n=6, verts={0,-0.8, 0.69,-0.4, 0.69,0.4, 0,-0.8, -0.69,0.4, -0.69,-0.4, 0,0.8}},  -- fixed below
    {n=10, verts={0,-0.85, 0.197,-0.271, 0.809,-0.271, 0.318,0.104,
                  0.5,0.688, 0,0.337, -0.5,0.688, -0.318,0.104, -0.809,-0.271, -0.197,-0.271}},
    {n=7, verts={-0.80,0.22, 0.00,0.22, 0.00,0.65, 0.80,0.00,
                 0.00,-0.65, 0.00,-0.22, -0.80,-0.22}},
}
-- fix hexagon
SHAPES[2].verts={0,-0.80, 0.69,-0.40, 0.69,0.40, 0,0.80, -0.69,0.40, -0.69,-0.40}

local COLORS={
    {0.4,0.65,1.0}, {1.0,0.78,0.15}, {0.25,0.85,0.45}, {0.8,0.3,0.75},
    {0.9,0.45,0.2}, {0.4,0.85,0.85}, {1.0,0.4,0.4},    {0.6,0.9,0.3},
}

-- HUD
local hud={}
local function makeHUD()
    hud.tier  =display.newText("Tier: -",   W*0.5,20,native.systemFont,14)
    hud.fps   =display.newText("FPS: -",    W*0.5,40,native.systemFont,14)
    hud.draws =display.newText("Draws: -",  W*0.5,60,native.systemFont,12)
    for _,t in pairs(hud) do t:setFillColor(1,1,0) end
end
makeHUD()

local tierIdx=0
local objs={}
local frame=0
local fpsBuf={}
local drawsBuf={}

local function clearObjs()
    for _,o in ipairs(objs) do display.remove(o) end
    objs={}
end

local function setupTier(count)
    clearObjs()
    frame=0; fpsBuf={}; drawsBuf={}
    local sz=math.max(18, math.min(40, math.floor(math.sqrt(W*H/count)*0.55)))
    local cols=math.ceil(math.sqrt(count * W/H))
    local rows=math.ceil(count/cols)
    local dx=W/(cols+1); local dy=H/(rows+1)
    for i=1,count do
        local col=((i-1)%cols)+1
        local row=math.floor((i-1)/cols)+1
        local x=dx*col + math.random(-4,4)
        local y=dy*row + math.random(-4,4)
        local r=display.newRect(x,y,sz,sz)
        r.fill.effect="filter.custom.sdfpoly"
        local si=((i-1)%#SHAPES)+1
        local ci=((i-1)%#COLORS)+1
        local s=SHAPES[si]
        setPoly(r, COLORS[ci], s.n, s.verts)
        r._angle=math.random()*math.pi*2
        r._spd=(math.random()-0.5)*0.04
        objs[#objs+1]=r
    end
    hud.tier.text="Tier "..tierIdx.."/"..#TIERS..": "..count.." objects"
    hud.tier:toFront(); hud.fps:toFront(); hud.draws:toFront()
end

local function advance()
    tierIdx=tierIdx+1
    if tierIdx>#TIERS then
        print("=== SDFPOLY BENCH END ===")
        clearObjs()
        os.exit(0)
        return
    end
    print("--- Tier "..tierIdx..": "..TIERS[tierIdx].." objects ---")
    setupTier(TIERS[tierIdx])
end

advance()

local lastTime=system.getTimer()
Runtime:addEventListener("enterFrame",function()
    frame=frame+1
    local now=system.getTimer()
    local dt=now-lastTime; lastTime=now
    local fps=dt>0 and (1000/dt) or 60

    -- animate all objects
    for _,o in ipairs(objs) do
        o._angle=o._angle+o._spd
        o.fill.effect.rot={o._angle,0,0,0}
    end

    if frame>WARMUP then
        fpsBuf[#fpsBuf+1]=fps
        local stats=system.getInfo("graphicsFPS") and {draws=0,submits=0} or nil
        if graphics.getDirtyStats then
            local s=graphics.getDirtyStats()
            drawsBuf[#drawsBuf+1]=(s and s.draws) or 0
        end
    end

    hud.fps.text="FPS: "..math.floor(fps+0.5)
    hud.fps:toFront()

    if frame==WARMUP+MEASURE then
        -- compute stats
        local sumFPS,minFPS,maxFPS=0,9999,0
        for _,f in ipairs(fpsBuf) do
            sumFPS=sumFPS+f
            if f<minFPS then minFPS=f end
            if f>maxFPS then maxFPS=f end
        end
        local n=#fpsBuf
        local avgFPS=n>0 and (sumFPS/n) or 0
        local avgDraws=0
        if #drawsBuf>0 then
            local sd=0; for _,d in ipairs(drawsBuf) do sd=sd+d end
            avgDraws=sd/#drawsBuf
        end
        print(string.format(
            "=== SDFPOLY RESULT count=%d AvgFPS=%.1f MinFPS=%.1f MaxFPS=%.1f AvgDraws=%.0f ===",
            TIERS[tierIdx], avgFPS, minFPS, maxFPS, avgDraws))
        hud.draws.text=string.format("Avg %.0f FPS | %d draws",avgFPS,avgDraws)
        hud.draws:toFront()
        advance()
    end
end)
