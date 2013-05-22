-- row-of-houses.lua

function params()
    return {}
end

function region()
    return script:paths()[1]:region()
end

function run()
    local path = script:paths()[1]

    painter:setTile(world:tile('blends_natural_01', 16))
    painter:setLayer(world:tileLayer('0_Floor'))
    painter:fillPath(path)

    painter:setTileAlias('darkmedgrass')
    painter:setLayer(world:tileLayer('0_Vegetation'))
    painter:fillPath(path)

    local info = MapInfo:get('C:\\Users\\Tim\\Desktop\\ProjectZomboid\\Tools\\Buildings\\tiny-house.tbx')
    local rect = path:region():boundingRect()
    if rect:width() > rect:height() then
	local x = rect:x() + 10
	local y = rect:y() + (rect:height() - info:bounds():height()) / 2
	while x + info:bounds():width() < rect:right() do
	    painter:drawMap(x, y, info)
	    x = x + info:bounds():width() + 12
	end
    else
    end
end
