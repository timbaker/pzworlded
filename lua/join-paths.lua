-- join-paths.lua

function reverse(t)
    local ret = {}
    for _,v in pairs(t) do table.insert(ret,1,v) end
    return ret
end

function run()
    local path1 = script:paths()[1]
    local path2 = script:paths()[2]
    local nodes1 = path1:nodes()
    local nodes2 = path2:nodes()
    if path1:first() == path1:last() or path2:first() == path2:last() then
	print('can\'t join closed paths')
	return
    end

    if nodes1[#nodes1] == nodes2[1] then
	print 'path1 -> path2'
    elseif nodes2[#nodes2] == nodes1[1] then
	print 'path2 -> path1'
    elseif nodes1[#nodes1] == nodes2[#nodes2] then
	print 'path1 -> path2-reversed'
	nodes2 = reverse(nodes2)
    elseif nodes1[1] == nodes2[1] then
	print 'path1-reversed -> path2'
	nodes1 = reverse(nodes1)
    end

    local joined = script:currentPathLayer():newPath()
    if nodes1[#nodes1] == nodes2[1] then
	for _,node in pairs(nodes1) do
	    joined:addNode(node)
	end
	table.remove(nodes2,1)
	if nodes2[#nodes2] == nodes1[1] then
	    --table.remove(nodes2)
	end
	for i=1,#nodes2 do
	    joined:addNode(nodes2[i])
	end
    elseif nodes2[#nodes2] == nodes1[1] then
	for _,node in pairs(nodes2) do
	    joined:addNode(node)
	end
	table.remove(nodes1,1)
	if nodes1[#nodes1] == nodes2[1] then
	    --table.remove(nodes1)
	end
	for i=1,#nodes1 do
	    joined:addNode(nodes1[i])
	end
    else
	print "can't join paths, start-/end-points don't meet"
	return
    end
    print('joined path has '..joined:nodeCount()..' nodes')
    local t = {}
    for _,n in pairs(joined:nodes()) do
	table.insert(t,tostring(n))
    end
    print(table.concat(t,','))
    path1:layer():removePath(path1)
    path2:layer():removePath(path2)
end
