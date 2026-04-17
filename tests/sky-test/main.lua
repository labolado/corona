-- Sky shader P_UV precision diagnostic
-- 只测一件事：noise/fbm 在 P_UV 精度下是否正常
display.setStatusBar(display.HiddenStatusBar)

local CX, CY = display.contentCenterX, display.contentCenterY
local W, H = display.contentWidth, display.contentHeight

-- Noise shader：用 P_UV（引擎已改为 highp，应该正常）
graphics.defineEffect({
    language = "glsl", category = "filter", name = "noise_puv",
    fragment = [[
P_UV vec2 hash22(P_UV vec2 p) {
    P_UV vec3 p3 = fract(vec3(p.xyx) * vec3(443.897, 441.423, 437.195));
    p3 += dot(p3, p3.yzx+19.19);
    return -1.0+2.0*fract((p3.xx+p3.yz)*p3.zy);
}
P_UV float noise(P_UV vec2 p) {
    P_UV vec2 i=floor(p); P_UV vec2 f=fract(p); P_UV vec2 u=f*f*(3.0-2.0*f);
    return mix(mix(dot(hash22(i),f),dot(hash22(i+vec2(1,0)),f-vec2(1,0)),u.x),
               mix(dot(hash22(i+vec2(0,1)),f-vec2(0,1)),dot(hash22(i+vec2(1,1)),f-vec2(1,1)),u.x),u.y);
}
P_UV float fbm4(P_UV vec2 p) {
    P_UV mat2 m2=mat2(1.6,1.2,-1.2,1.6); P_UV float amp=0.5,h=0.0;
    for(int i=0;i<4;i++){h+=amp*noise(p);amp*=0.5;p=m2*p;}
    return 0.5+0.5*h;
}
P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
    P_UV float n = fbm4(uv * 4.0 - vec2(20.0, 50.0));
    P_UV float c = smoothstep(0.5, 0.8, n);
    return vec4(c, n, n, 1.0);
}
]]
})

-- 上半：noise 输出（应该有颜色纹理，黑色=P_UV精度不够）
local top = display.newRect(CX, H * 0.25, W - 20, H * 0.4)
top:setFillColor(1)
top.fill.effect = "filter.custom.noise_puv"

-- 下半左：纯色参考 (0.6 gray)
local botL = display.newRect(W * 0.25, H * 0.75, W * 0.4, H * 0.3)
botL:setFillColor(0.6, 0.6, 0.6)

-- 下半右：uniform passthrough 参考
graphics.defineEffect({
    language = "glsl", category = "filter", name = "gray_out",
    fragment = [[
P_COLOR vec4 FragmentKernel(P_UV vec2 uv) {
    return vec4(0.6, 0.6, 0.6, 1.0);
}
]]
})
local botR = display.newRect(W * 0.75, H * 0.75, W * 0.4, H * 0.3)
botR:setFillColor(1)
botR.fill.effect = "filter.custom.gray_out"

-- 标签
local t1 = display.newText("TOP: noise (black=BROKEN, colored=OK)", CX, 15, native.systemFont, 12)
t1:setFillColor(1, 0, 0)
local t2 = display.newText("BOT: L=Lua gray  R=shader gray (should match)", CX, H * 0.58, native.systemFont, 12)
t2:setFillColor(1, 0, 0)

print("P_UV noise test loaded")
