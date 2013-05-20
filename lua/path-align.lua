function run()
    for _,path in pairs(script:paths()) do
	for i=0,path:nodeCount()-2 do
	    local dx = path:nodeAt(i+1):x()-path:nodeAt(i):x()
	    local dy = path:nodeAt(i+1):y()-path:nodeAt(i):y()
	    if dy / dx <= 0.05 then
		path:nodeAt(i+1):setY(path:nodeAt(i):y())
	    else
		path:nodeAt(i+1):setX(path:nodeAt(i):x())
	    end
	end
    end
end
