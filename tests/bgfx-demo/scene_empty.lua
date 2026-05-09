local composer = require("composer")
local scene = composer.newScene()

function scene:create(event)
    -- empty scene for cleanup between tests
end

scene:addEventListener("create", scene)
return scene
