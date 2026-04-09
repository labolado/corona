-- gpu_compat.lua
-- GPU compatibility detection and workarounds
-- Usage: local gpu = require("gpu_compat"); if gpu.needsWorkaround then ... end

local M = {}

function M.detect()
    local renderer = system.getInfo("GL_RENDERER") or ""
    local vendor = system.getInfo("GL_VENDOR") or ""

    M.renderer = renderer
    M.vendor = vendor

    -- MediaTek + Mali combo: driver bug in glDrawArrays
    -- Affected: Mali-G57 MC2, Mali-G52 on Dimensity SoCs
    M.isMediaTekMali = (renderer:find("Mali%-G57") or renderer:find("Mali%-G52"))
        and (vendor:find("ARM") or vendor:find("Arm"))

    -- Known problematic GPUs
    M.needsWorkaround = M.isMediaTekMali or false

    if M.needsWorkaround then
        print("GPU compat: workaround needed for " .. renderer .. " (" .. vendor .. ")")
    end

    return M
end

-- Call at startup
M.detect()

return M
