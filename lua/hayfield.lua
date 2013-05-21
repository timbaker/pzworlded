-- hay field

function region()
    return script:paths()[1]:region()
end

function run()
    local path = script:paths()[1]
    painter:setLayer(world:tileLayer('0_Floor'))
    painter:setTile(world:tile('floors_exterior_natural_01',16))
    painter:fillPath(path)

    painter:setLayer(world:tileLayer('0_Vegetation'))
    painter:setTile(world:tile('vegetation_farm_01',2))
    painter:fillPath(path)

    local rect = path:region():boundingRect()
    local x = rect:x() + rect:width() / 2
    local y = rect:y()
    painter:eraseRect(Rect:new(x, y, 1, rect:height()))

    x = rect:x()
    y = rect:y() + rect:height() / 2
    painter:eraseRect(Rect:new(x, y, rect:width(), 1))
end

if false then
path2 = path:fuzzEdges(4)
map:tileLayer('0_Floor'):fill(path2, map:tile('soil'))

path2 = path2:fuzzEdges(2)
map:tileLayer('0_Floor'):fill(path2, map:tile('hay'))
end
