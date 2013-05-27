function region()
    return script:paths()[1]:stroke(2):region()
end

function run()
    painter:setLayer(world:tileLayer('0_Vegetation'))
    painter:erasePath(script:paths()[1]:stroke(2))

    painter:setTile(world:tile('floors_exterior_natural_01', 12))
    painter:setLayer(world:tileLayer('0_Floor'))
    painter:strokePath(script:paths()[1], 2)
end
