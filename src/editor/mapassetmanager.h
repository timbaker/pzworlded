#ifndef MAPINFOMANAGER_H
#define MAPINFOMANAGER_H

#include "assetmanager.h"
#include "singleton.h"

class MapAsset;
class MapInfo;

class AssetTask_LoadBuilding;
class AssetTask_LoadMap;

namespace BuildingEditor {
class Building;
}

namespace Tiled {
class Map;
}

class MapAsset : public Asset
{
public:
    MapAsset(AssetPath path, AssetManager* manager)
        : Asset(path, manager)
    {}

    ~MapAsset() override;

    MapAsset(Tiled::Map* map, AssetPath path, AssetManager* manager);

    Tiled::Map* map() const { return mMap; }

    BuildingEditor::Building* mBuilding = nullptr;
    Tiled::Map* mMap = nullptr;
};

class MapAssetManager : public AssetManager, public Singleton<MapAssetManager>
{
public:
    MapAssetManager();

protected:
    friend class AssetTask_LoadBuilding;
    friend class AssetTask_LoadMap;
    void loadBuildingTaskFinished(AssetTask_LoadBuilding* task);
    void loadMapTaskFinished(AssetTask_LoadMap* task);

protected:
protected:
    Asset* createAsset(AssetPath path, AssetParams* params) override;
    void destroyAsset(Asset* asset) override;
    void startLoading(Asset* asset) override;
};

#endif // MAPINFOMANAGER_H
