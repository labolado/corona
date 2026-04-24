-- test_particles.lua: 粒子系统回归测试
-- 用 display.newEmitter 内联参数测试爆炸+烟雾两种粒子
-- 运行: SOLAR2D_TEST=particles SOLAR2D_BACKEND=bgfx

display.setStatusBar(display.HiddenStatusBar)

local W, H = display.contentWidth, display.contentHeight
local backend = os.getenv("SOLAR2D_BACKEND") or "gl"
print("=== Particle System Test (" .. backend .. ") ===")

local pass, fail = 0, 0
local function check(name, cond)
    if cond then
        pass = pass + 1; print("[PASS] " .. name)
    else
        fail = fail + 1; print("[FAIL] " .. name)
    end
end

-- Background
display.newRect(W/2, H/2, W, H):setFillColor(0.05, 0.05, 0.1)

-- 爆炸粒子配置（burst，短寿命，快速扩散）
local explosionParams = {
    textureFileName          = "shape_white.png",
    maxParticles             = 80,
    duration                 = -1,
    startParticleSize        = 18,
    startParticleSizeVariance = 8,
    finishParticleSize       = 2,
    finishParticleSizeVariance = 1,
    particleLifespan         = 0.6,
    particleLifespanVariance = 0.2,
    emitterType              = 0,           -- 0=gravity, 1=radial
    sourcePositionVariancex  = 5,
    sourcePositionVariancey  = 5,
    speed                    = 120,
    speedVariance            = 40,
    gravityx                 = 0,
    gravityy                 = 80,
    angle                    = 90,
    angleVariance            = 180,
    startColorRed            = 1.0,
    startColorGreen          = 0.6,
    startColorBlue           = 0.1,
    startColorAlpha          = 1.0,
    startColorVarianceRed    = 0,
    startColorVarianceGreen  = 0.2,
    startColorVarianceBlue   = 0,
    startColorVarianceAlpha  = 0,
    finishColorRed           = 0.8,
    finishColorGreen         = 0.1,
    finishColorBlue          = 0,
    finishColorAlpha         = 0,
    finishColorVarianceRed   = 0,
    finishColorVarianceGreen = 0,
    finishColorVarianceBlue  = 0,
    finishColorVarianceAlpha = 0,
    radialAcceleration       = 0,
    radialAccelVariance      = 0,
    tangentialAcceleration   = 0,
    tangentialAccelVariance  = 0,
    rotationStart            = 0,
    rotationStartVariance    = 60,
    rotationEnd              = 90,
    rotationEndVariance      = 30,
    blendFuncSource          = 770,  -- GL_SRC_ALPHA
    blendFuncDestination     = 771,  -- GL_ONE_MINUS_SRC_ALPHA
}

-- 烟雾粒子配置（持续，上升，透明衰减）
local smokeParams = {
    textureFileName          = "shape_white.png",
    maxParticles             = 120,
    duration                 = -1,
    startParticleSize        = 12,
    startParticleSizeVariance = 4,
    finishParticleSize       = 30,
    finishParticleSizeVariance = 8,
    particleLifespan         = 2.0,
    particleLifespanVariance = 0.5,
    emitterType              = 0,
    sourcePositionVariancex  = 8,
    sourcePositionVariancey  = 4,
    speed                    = 40,
    speedVariance            = 15,
    gravityx                 = 0,
    gravityy                 = -30,
    angle                    = 90,
    angleVariance            = 15,
    startColorRed            = 0.7,
    startColorGreen          = 0.7,
    startColorBlue           = 0.7,
    startColorAlpha          = 0.8,
    startColorVarianceRed    = 0.1,
    startColorVarianceGreen  = 0.1,
    startColorVarianceBlue   = 0.1,
    startColorVarianceAlpha  = 0.1,
    finishColorRed           = 0.3,
    finishColorGreen         = 0.3,
    finishColorBlue          = 0.3,
    finishColorAlpha         = 0,
    finishColorVarianceRed   = 0,
    finishColorVarianceGreen = 0,
    finishColorVarianceBlue  = 0,
    finishColorVarianceAlpha = 0,
    radialAcceleration       = 0,
    radialAccelVariance      = 0,
    tangentialAcceleration   = 10,
    tangentialAccelVariance  = 5,
    rotationStart            = 0,
    rotationStartVariance    = 180,
    rotationEnd              = 360,
    rotationEndVariance      = 90,
    blendFuncSource          = 770,
    blendFuncDestination     = 771,
}

-- 创建爆炸粒子
local emitter1, emitter2
local ok1, err1 = pcall(function()
    emitter1 = display.newEmitter(explosionParams)
    emitter1.x = W * 0.3
    emitter1.y = H * 0.5
end)
check("explosion emitter created", ok1 and emitter1 ~= nil)
if err1 then print("  err: " .. tostring(err1)) end

-- 创建烟雾粒子
local ok2, err2 = pcall(function()
    emitter2 = display.newEmitter(smokeParams)
    emitter2.x = W * 0.7
    emitter2.y = H * 0.6
end)
check("smoke emitter created", ok2 and emitter2 ~= nil)
if err2 then print("  err: " .. tostring(err2)) end

-- 验证 emitter 属性
if emitter1 then
    check("emitter1 has x", type(emitter1.x) == "number")
    check("emitter1 has y", type(emitter1.y) == "number")
    check("emitter1 is visible", emitter1.isVisible ~= false)
end

-- 验证 start/stop 不崩溃
if emitter1 then
    local okStop = pcall(function()
        emitter1:stop()
        emitter1:start()
    end)
    check("emitter stop/start no crash", okStop)
end

-- 标签
local function label(txt, x, y)
    local t = display.newText(txt, x, y, native.systemFont, 13)
    t:setFillColor(1, 1, 0.7)
    return t
end

label("Explosion", W * 0.3, H * 0.5 + 80)
label("Smoke",     W * 0.7, H * 0.6 + 80)

local title = display.newText("Particle System Test - " .. backend, W/2, 28, native.systemFontBold, 15)
title:setFillColor(1, 1, 1)

-- 等粒子运行 60 帧进入稳态后发截图信号
local frameCount = 0
Runtime:addEventListener("enterFrame", function()
    frameCount = frameCount + 1
    if frameCount == 60 then
        check("emitter1 still alive at frame 60", emitter1 ~= nil)
        check("emitter2 still alive at frame 60", emitter2 ~= nil)
        print(string.format("\n=== PARTICLE TEST RESULTS (%s): Pass %d | Fail %d ===", backend, pass, fail))
        if fail == 0 then print("TEST PASS: particles") else print("TEST FAIL: particles") end
        print("SCREENSHOT_READY")  -- 通知 test_compare.sh 截图
    end
end)
