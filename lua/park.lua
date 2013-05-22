function params()
    return
end

function region()
    -- print('park.lua:region')
    return script:paths()[1]:region()
end

function run()
    -- print('park.lua:run')
    --map:tileLayer('0_Floor'):fill(path, map:tile('blends_natural_01', 16))
    painter:setTile(world:tile('blends_natural_01', 16))
    painter:setLayer(world:tileLayer('0_Floor'))
    painter:fillPath(script:paths()[1])
end
