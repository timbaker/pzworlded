-- fill-tile-alias.lua

function params()
    return 'alias', 'layer'
end

function region()
    return script:paths()[1]:region()
end

function run()
    local path = script:paths()[1]

    painter:setTileAlias(script:value('alias'))
    painter:setLayer(world:tileLayer(script:value('layer')))
    painter:fillPath(path)
end
