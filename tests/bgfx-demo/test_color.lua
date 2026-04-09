-- test_color.lua
-- 纯色测试：大色块无边框无透明，方便像素级对比 GL vs bgfx
-- 用法: SOLAR2D_TEST=color SOLAR2D_BACKEND=bgfx ./Corona\ Simulator ...

local W = display.contentWidth
local H = display.contentHeight
local backend = os.getenv("SOLAR2D_BACKEND") or "gl"

-- 纯黑背景，消除背景色干扰
display.setDefault("background", 0, 0, 0)

local label = display.newText(backend:upper(), W/2, 20, native.systemFont, 14)
label:setFillColor(1)

-- 测试色块：纯色矩形，无 stroke，无 alpha
-- 每个色块 120x80，排列成网格
local colors = {
    { name="Red",     r=1,   g=0,   b=0   },
    { name="Green",   r=0,   g=1,   b=0   },
    { name="Blue",    r=0,   g=0,   b=1   },
    { name="Cyan",    r=0,   g=1,   b=1   },
    { name="Magenta", r=1,   g=0,   b=1   },
    { name="Yellow",  r=1,   g=1,   b=0   },
    { name="White",   r=1,   g=1,   b=1   },
    { name="Gray50",  r=0.5, g=0.5, b=0.5 },
    { name="Orange",  r=1,   g=0.5, b=0   },
    { name="Teal",    r=0,   g=0.5, b=0.5 },
    { name="Purple",  r=0.5, g=0,   b=0.5 },
    { name="Lime",    r=0.5, g=1,   b=0   },
}

local cols = 3
local bw, bh = 120, 80
local startX = (W - cols * bw) / 2 + bw/2
local startY = 60

for i, c in ipairs(colors) do
    local col = ((i-1) % cols)
    local row = math.floor((i-1) / cols)
    local x = startX + col * bw
    local y = startY + row * (bh + 25)

    local rect = display.newRect(x, y, bw - 4, bh - 4)
    rect:setFillColor(c.r, c.g, c.b)
    rect.strokeWidth = 0

    -- 标注颜色名和 RGB 值
    local rgb255 = string.format("%d,%d,%d", c.r*255, c.g*255, c.b*255)
    local lbl = display.newText(c.name .. "\n" .. rgb255, x, y + bh/2 + 8, native.systemFont, 9)
    lbl:setFillColor(1)
end

-- 底部：半透明测试（检查 alpha blend）
local alphaY = startY + 4 * (bh + 25) + 20
local alphaLabel = display.newText("Alpha Blend Test (over black bg)", W/2, alphaY, native.systemFont, 11)
alphaLabel:setFillColor(1)

local alphas = { 0.25, 0.5, 0.75, 1.0 }
for i, a in ipairs(alphas) do
    local x = (W / 5) * i
    local rect = display.newRect(x, alphaY + 50, 60, 40)
    rect:setFillColor(0, 1, 1)  -- cyan
    rect.alpha = a
    rect.strokeWidth = 0

    local lbl = display.newText("a=" .. a, x, alphaY + 80, native.systemFont, 9)
    lbl:setFillColor(1)
end

print("COLOR_TEST: backend=" .. backend .. " — pure color blocks for pixel comparison")
