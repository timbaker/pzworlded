-- path-from-nodes.lua

function run()
    local pathLayer = script:currentPathLayer()
    local path = pathLayer:newPath()
    for _,node in pairs(script:nodes()) do
        -- verify nodes are in pathLayer
        path:addNode(node)
    end
end
