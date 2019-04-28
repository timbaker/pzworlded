/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MAPMANAGER_H
#define MAPMANAGER_H

#include "assetmanager.h"
#include "filesystemwatcher.h"
#include "singleton.h"
#include "threads.h"

#include "map.h"

#include <QDateTime>
#include <QMap>
#include <QTimer>

class MapAsset;

namespace BuildingEditor {
class Building;
}

class AssetTask_LoadBuilding;
class AssetTask_LoadMap;

class MapManager : public AssetManager, public Singleton<MapManager>
{
    Q_OBJECT
public:
    MapManager();
    ~MapManager() override;

    /**
     * Returns the canonical (absolute) path for a map file.
     * \a mapName may or may not include .tmx extension.
     * \a mapName may be relative or absolute.
     */
    QString pathForMap(const QString &mapName, const QString &relativeTo);

    enum LoadPriority {
        PriorityLow,
        PriorityMedium,
        PriorityHigh
    };

    MapAsset *loadMap(const QString &mapName,
                     const QString &relativeTo = QString(),
                     bool asynch = false, LoadPriority priority = PriorityHigh);

    MapAsset *newFromMap(Tiled::Map *map, const QString &mapFilePath = QString());

    /**
     * The "empty map" is used when a WorldCell has no map.
     * The user still needs to view the cell to place Lots etc.
     */
    MapAsset *getEmptyMap();

    /**
      * A "placeholder" map is used when a sub-map could not be loaded.
      * The user still needs to move/delete the sub-map.
      * A placeholder map may be updated to a real map when the
      * user changes the maptools directory.
      */
    MapAsset *getPlaceholderMap(const QString &mapName, int width, int height);

    /**
     * Converts a map to Isometric or LevelIsometric.
     * A new map is returned if conversion occurred.
     */
    Tiled::Map *convertOrientation(Tiled::Map *map, Tiled::Map::Orientation orient);

    /**
      * Call this when the map's size or tile size changes.
      */
    void mapParametersChanged(MapAsset *mapAsset);
#ifdef WORLDED
    void addReferenceToMap(MapAsset *mapAsset);
    void removeReferenceToMap(MapAsset *mapAsset);
    void purgeUnreferencedMaps();

    void newMapFileCreated(const QString &path);
#endif
    QString errorString() const
    { return mError; }

    typedef BuildingEditor::Building Building;

signals:
    void mapAboutToChange(MapAsset *mapAsset);
    void mapChanged(MapAsset *mapAsset);
    void mapFileChanged(MapAsset *mapAsset);
#ifdef WORLDED
    void mapFileCreated(const QString &path);
#endif
    void mapLoaded(MapAsset *mapAsset);
    void mapFailedToLoad(MapAsset *mapAsset);

private slots:
    void fileChanged(const QString &path);
    void fileChangedTimeout();

    void metaTilesetAdded(Tiled::Tileset *tileset);
    void metaTilesetRemoved(Tiled::Tileset *tileset);

    void mapLoadedByThread(MapAsset *mapAsset);
    void buildingLoadedByThread(MapAsset* mapAsset);
    void failedToLoadByThread(const QString error, MapAsset *mapAsset);

    void processDeferrals();

protected:
    friend class AssetTask_LoadBuilding;
    friend class AssetTask_LoadMap;
    void loadBuildingTaskFinished(AssetTask_LoadBuilding* task);
    void loadMapTaskFinished(AssetTask_LoadMap* task);

protected:
    Asset* createAsset(AssetPath path, AssetParams* params) override;
    void destroyAsset(Asset* asset) override;
    void startLoading(Asset* asset) override;
    void unloadData(Asset* asset) override;

private:
    Tiled::Internal::FileSystemWatcher *mFileSystemWatcher;
    QSet<QString> mChangedFiles;
    QTimer mChangedFilesTimer;

    friend class MapManagerDeferral;
    void deferThreadResults(bool defer);
    int mDeferralDepth;
    struct MapDeferral
    {
        MapDeferral(MapAsset* mapAsset) :
            mapAsset(mapAsset)
        {}

        MapAsset *mapAsset;
    };
    QList<MapDeferral> mDeferredMaps;
    MapAsset *mWaitingForMapInfo;

#ifdef WORLDED
    int mReferenceEpoch;
#endif
    QString mError;
};

class MapManagerDeferral
{
public:
    MapManagerDeferral() :
        mReleased(false)
    {
        MapManager::instance().deferThreadResults(true);
    }

    ~MapManagerDeferral()
    {
        if (!mReleased)
            MapManager::instance().deferThreadResults(false);
    }

    void release()
    {
        MapManager::instance().deferThreadResults(false);
        mReleased = true;
    }

private:
    bool mReleased;
};

#endif // MAPMANAGER_H
