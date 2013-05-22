-- fill-tile-alias.lua

function params()
    return 'rule'
end

function region()
    return script:paths()[1]:region()
end

function run()
    local path = script:paths()[1]
    painter:setTileRule(script:value('rule'))
    painter:fillPath(path)
end
