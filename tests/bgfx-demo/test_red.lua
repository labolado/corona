display.setDefault("background", 0, 0, 0)
local colors = {
    {1,0,0, "RED"},
    {0,1,0, "GREEN"},
    {0,0,1, "BLUE"},
    {1,1,1, "WHITE"},
    {0.5,0.5,0.5, "GRAY50"},
}
for i, c in ipairs(colors) do
    local y = 40 + (i-1) * 80
    local rect = display.newRect(160, y, 200, 50)
    rect:setFillColor(c[1], c[2], c[3])
    display.newText(c[4], 350, y, native.systemFont, 16)
end
