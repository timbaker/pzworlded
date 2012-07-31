require 'TheWorld'

local noise = print
-- local noise = function(s) end

local world = {}
function initWorldData(file)
	local data = TheWorld()
	---
	world.propertydef = {}
	for i = 1, #data.propertydef do
		local pd = data.propertydef[i]
		world.propertydef[pd.name] = pd.default
	end
	---
	world.cells = {}
	for x = 0, 100-1 do
		world.cells[x] = {}
		for y = 0, 100-1 do
			world.cells[x][y] = {
				lots = {},
				objects = {},
				properties = {}
			}
		end
	end
	for i = 1, #data.cells do
		local cd = data.cells[i]
		local cell = world.cells[cd.x][cd.y]
		cell.map = cd.map
		local n = 0
		if cd.properties then
			for j = 1, #cd.properties do
				local prop = cd.properties[j]
				cell.properties[prop.name] = prop.value
			end
		end
		cell.lots = cd.lots or {}
		cell.objects = cd.objects or {}
	end
end

function getCellProperty(x, y, name)
	noise('getCellProperty ' ..x.. ','..y.. ' '..name)
	local v = world.cells[x][y].properties[name]
	if v ~= nil then return v end
	return world.propertydef[name]
end

function AddTrees(cell)
	noise("Adding trees.")
	for x=0, cell:getWidth()-1 do
		for y=0, cell:getHeight()-1 do
			local square = cell:getGridSquare(x, y, 0)
			if square:isCommonGrass() and not square:isZone("NoTreeSpawn") then
				local rand = ZombRandBetween(2, 7)
				if(rand>3) then
					local vegitation =  IsoObject.new(square, "TileTrees_"..rand, false)
					square:AddTileObject(vegitation)
				end
				if(ZombRand(5)==0) then
					local rand = ZombRand(6)
					if(rand==0 or rand==1 or rand==2 or rand==3) then
						rand = -1
					end
					if(rand==4) then
						rand = ZombRand(4)
					end
					if(rand==5) then
						rand = 16
					end
					if(rand >= 0) then
						local vegitation =  IsoTree.new(square, "TileTrees_"..rand)
						square:AddTileObject(vegitation)
						square:RecalcAllWithNeighbours(true)
					end
				end
			end
		end
	end
end

function getMainCellLot(x, y)
	noise('getMainCellLot ' ..x.. ',' .. y)
	return world.cells[x][y].map or "Lot_Rural_00"
end

function getStartZombiesByGrid(x, y)
	return getCellProperty(x, y, "start-zombies")
end

function cellSurvivorRemoteness(x, y)
	return getCellProperty(x, y, "survivor-remoteness")
end

function cellSurvivorSpawns(x, y)
	return getCellProperty(x, y, "survivor-spawns")
end

function getStartIndoorZombiesByGrid(x, y)
	return getCellProperty(x, y, "start-indoor-zombies")
end

function getZombieAttractionFactorByGrid(x, y)
	return getCellProperty(x, y, "zombie-attraction-factor")
end

function mapLoaded(cell, name, x, y)
	noise('Map Loaded "' ..name.. '" x,y=' ..x.. ',' ..y)

	local grabnewseed = ZombRand(12345)
	if getCellProperty(x, y, 'random-trees') then
		noise 'Doing random vegitation'
		AddTrees(cell)
	end

	for i = 1, #world.cells[x][y].lots do
		local lot = world.cells[x][y].lots[i]
		local name = "media/lots/" .. lot.map .. ".lot"
		noise('PlaceLot '..name..' '..lot.x..','..lot.y..' z='..lot.level)
		cell:PlaceLot(name, lot.x, lot.y, lot.level, true)
	end

	for i = 1, #world.cells[x][y].objects do
		local obj = world.cells[x][y].objects[i]
		if obj.type == 'zone' then
			noise('AddZone '..obj.name)
			cell:AddZone(obj.name, obj.x, obj.y, obj.width, obj.height, obj.level)
		end
		if obj.type == 'roomDef' then
			-- TODO: handle 'roomDef' objects
		end
	end

	if getCellProperty(x, y, 'random-farmhouse') then
		if ZombRand(5) == 0 then
			cell:PlaceLot("media/lots/Lot_Rural_Farmhouse_00.lot", 50 + ZombRand(200), 50 + ZombRand(200), 0, true)
		end
		for i=0,5 do
			if ZombRand(5)==0 then
				cell:PlaceLot("media/lots/Lot_Rural_Copse_00.lot", ZombRand(300), ZombRand(300), 0, true)
			end
			if ZombRand(5)==0 then
				cell:PlaceLot("media/lots/Lot_Rural_TreeCluster_00.lot", ZombRand(300), ZombRand(300), 0, true)
			end
		end
	end
end

noise('PZWorldEd mapScripts.lua loaded')

initWorldData()

Events.OnPostMapLoad.Add(mapLoaded)

