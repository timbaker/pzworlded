-- place-lot.lua

function params()
    return 'mapfile', 'pos'
end

function region()
    local info = MapInfo:get(script:value('mapfile'))
    if not info then return Region:new() end
    local v = split(script:value('pos'), ' ')
	local x = tonumber(v[1])
	local y = tonumber(v[2])
    local r = info:bounds():translated(x,y):adjusted(-2,-2,0,0)
    --print('r'..tostring(r))
    local rgn = Region:new() + r
    --print('rgn'..tostring(rgn))
    return rgn
    --return info:region():translated(tonumber(v[1]),tonumber(v[2]));
end

split = function (str)
    local arr = {}
    for token in string.gmatch(str, '[^%s]+') do
	table.insert(arr, token)
    end
    return arr
  end

function run()
    local info = MapInfo:get(script:value('mapfile'))
    if info then
        local pos = script:value('pos')
	local v = split(pos)
	local x = tonumber(v[1])
	local y = tonumber(v[2])

	painter:setTile(world:tile('blends_natural_01', 16))
	painter:setLayer(world:tileLayer('0_Floor'))
	painter:fillRect(info:bounds():translated(x, y):adjusted(-2,-2,0,0))

	painter:drawMap(x, y, info)
    end
end
