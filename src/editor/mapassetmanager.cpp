#include "mapassetmanager.h"

#include "mapinfo.h"

#include "assettask.h"
#include "assettaskmanager.h"

#include "BuildingEditor/building.h"
#include "BuildingEditor/buildingreader.h"

#include "mapreader.h"

#include <QFileInfo>

using namespace BuildingEditor;
using namespace Tiled;

SINGLETON_IMPL(MapAssetManager)

MapAsset::~MapAsset()
{
    int dbg = 1;
}

MapAsset::MapAsset(Map *map, AssetPath path, AssetManager *manager)
    : Asset(path, manager)
    , mMap(map)
{
    onCreated(AssetState::READY);
}

// // // // //

class MapAssetManager_MapReader : public MapReader
{
protected:
    /**
     * Overridden to make sure the resolved reference is canonical.
     */
    QString resolveReference(const QString &reference, const QString &mapPath) override
    {
        QString resolved = MapReader::resolveReference(reference, mapPath);
        QString canonical = QFileInfo(resolved).canonicalFilePath();

        // Make sure that we're not returning an empty string when the file is
        // not found.
        return canonical.isEmpty() ? resolved : canonical;
    }
};

MapAssetManager::MapAssetManager()
{

}
class AssetTask_LoadBuilding : public BaseAsyncAssetTask
{
public:
    AssetTask_LoadBuilding(MapAsset* asset)
        : BaseAsyncAssetTask(asset)
    {

    }

    void run() override
    {
        QString path = m_asset->getPath().getString();

        BuildingReader reader;
        mBuilding = reader.read(path);
        if (mBuilding != nullptr)
        {
            mError = reader.errorString();
        }
        else
        {
            bLoaded = true;
        }
    }

    void handleResult() override
    {
        MapAssetManager::instance().loadBuildingTaskFinished(this);
    }

    void release() override
    {
        delete mBuilding;
        delete this;
    }

    Building* mBuilding = nullptr;
    bool bLoaded = false;
    QString mError;
};

class AssetTask_LoadMap : public BaseAsyncAssetTask
{
public:
    AssetTask_LoadMap(MapAsset* asset)
        : BaseAsyncAssetTask(asset)
    {

    }

    void run() override
    {
        QString path = m_asset->getPath().getString();

        MapAssetManager_MapReader reader;
    //    reader.setTilesetImageCache(TilesetManager::instance().imageCache()); // not thread-safe class
        mMap = reader.readMap(path);
        if (mMap == nullptr)
        {
            mError = reader.errorString();
        }
        else
        {
            bLoaded = true;
        }
    }

    void handleResult() override
    {
        MapAssetManager::instance().loadMapTaskFinished(this);
    }

    void release() override
    {
        delete mMap;
        delete this;
    }

    Tiled::Map* mMap = nullptr;
    bool bLoaded = false;
    QString mError;
};

void MapAssetManager::loadBuildingTaskFinished(AssetTask_LoadBuilding *task)
{
    MapAsset* mapAsset = static_cast<MapAsset*>(task->m_asset);
    if (task->bLoaded)
    {
        mapAsset->mBuilding = task->mBuilding;
        task->mBuilding = nullptr;
        onLoadingSucceeded(task->m_asset);
    }
    else
    {
        onLoadingFailed(task->m_asset);
    }
}

void MapAssetManager::loadMapTaskFinished(AssetTask_LoadMap *task)
{
    MapAsset* mapAsset = static_cast<MapAsset*>(task->m_asset);
    if (task->bLoaded)
    {
        mapAsset->mMap = task->mMap;
        task->mMap = nullptr;
        onLoadingSucceeded(task->m_asset);
    }
    else
    {
        // TODO: handle task->mError
        onLoadingFailed(task->m_asset);
    }
}

Asset *MapAssetManager::createAsset(AssetPath path, AssetParams *params)
{
    return new MapAsset(path, this/*, params*/);
}

void MapAssetManager::destroyAsset(Asset *asset)
{
    Q_UNUSED(asset);
}

void MapAssetManager::startLoading(Asset *asset)
{
    MapAsset* mapAsset = static_cast<MapAsset*>(asset);

    QString path = asset->getPath().getString();
    if (path.endsWith(QLatin1String(".tbx")))
    {
        AssetTask_LoadBuilding* assetTask = new AssetTask_LoadBuilding(mapAsset);
        setTask(asset, assetTask);
        AssetTaskManager::instance().submit(assetTask);
    }
    else
    {
        AssetTask_LoadMap* assetTask = new AssetTask_LoadMap(mapAsset);
        setTask(asset, assetTask);
        AssetTaskManager::instance().submit(assetTask);
    }
}
