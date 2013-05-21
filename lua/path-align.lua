function run()
    local xs = {}
    local ys = {}
    
    for _,node in pairs(script:nodes()) do
	table.insert(xs,node:x())
	table.insert(ys,node:y())
    end
    local minx = math.min(table.unpack(xs))
    local maxx = math.max(table.unpack(xs))
    local midx = math.floor(minx + (maxx - minx) / 2)

    local miny = math.min(table.unpack(ys))
    local maxy = math.max(table.unpack(ys))
    local midy = math.floor(miny + (maxy - miny) / 2)

    print(minx..'-'..midx..'-'..maxx..' '..miny..'-'..midy..'-'..maxy)
    if true then

    for _,node in pairs(script:nodes()) do
	if maxx - minx < maxy - miny then
	    node:setX(midx)
	else
	    node:setY(midy)
	end
    end
    end
end
