/*
 * Copyright 2019, Tim Baker <treectrl@users.sf.net>
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

#ifndef MAPINFOMANAGER_H
#define MAPINFOMANAGER_H

#include "assetmanager.h"
#include "map.h"
#include "mapinfo.h"
#include "filesystemwatcher.h"
#include "singleton.h"
#include "threads.h"

#include <QDateTime>
#include <QMap>
#include <QTimer>

class MapInfo;
class AssetTask_LoadMapInfo;

namespace BuildingEditor {
class Building;
}

#if 0
class MapReaderWorker : public BaseWorker
{
    Q_OBJECT
public:
    MapReaderWorker(InterruptibleThread *thread, int id);
    ~MapReaderWorker();

    typedef Tiled::Map Map;
    typedef BuildingEditor::Building Building;

signals:
    void loaded(Map *map, MapInfo *mapInfo);
    void loaded(Building *building, MapInfo *mapInfo);
    void failedToLoad(const QString error, MapInfo *mapInfo);

public slots:
    void work();
    void addJob(MapInfo *mapInfo, int priority);
    void possiblyRaisePriority(MapInfo *mapInfo, int priority);

private:
    Map *loadMap(MapInfo *mapInfo);
    Building *loadBuilding(MapInfo *mapInfo);

    class Job {
    public:
        Job(MapInfo *mapInfo, int priority) :
            mapInfo(mapInfo),
            priority(priority)
        {
        }

        MapInfo *mapInfo;
        int priority;
    };
    QList<Job> mJobs;

    int mID;
    void debugJobs(const char *msg);

    QString mError;
};
#endif

class MapInfoManager : public AssetManager, public Singleton<MapInfoManager>
{
    Q_OBJECT
public:
    MapInfoManager();
    ~MapInfoManager() override;

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

    MapInfo *mapInfo(const QString &mapFilePath);

    /**
      * Call this when the map's size or tile size changes.
      */
    void mapParametersChanged(MapInfo *mapInfo);
#ifdef WORLDED
    void addReferenceToMap(MapInfo *mapInfo);
    void removeReferenceToMap(MapInfo *mapInfo);
    void purgeUnreferencedMaps();

    void newMapFileCreated(const QString &path);
#endif
    QString errorString() const
    { return mError; }

    typedef BuildingEditor::Building Building;

signals:
    void mapAboutToChange(MapInfo *mapInfo);
    void mapChanged(MapInfo *mapInfo);
    void mapFileChanged(MapInfo *mapInfo);
#ifdef WORLDED
    void mapFileCreated(const QString &path);
#endif
    void mapLoaded(MapInfo *mapInfo);
    void mapFailedToLoad(MapInfo *mapInfo);

private slots:
    void fileChanged(const QString &path);
    void fileChangedTimeout();

    void mapLoadedByThread(MapInfo *mapInfo);
    void failedToLoadByThread(const QString error, MapInfo *mapInfo);

    void processDeferrals();

protected:
    Asset* createAsset(AssetPath path, AssetParams* params) override;
    void destroyAsset(Asset* asset) override;
    void startLoading(Asset* asset) override;

    friend class AssetTask_LoadMapInfo;
    void loadMapInfoTaskFinished(AssetTask_LoadMapInfo* task);

private:
    Tiled::Internal::FileSystemWatcher *mFileSystemWatcher;
    QSet<QString> mChangedFiles;
    QTimer mChangedFilesTimer;

    friend class MapInfoManagerDeferral;
    void deferThreadResults(bool defer);
    int mDeferralDepth;
    struct MapDeferral
    {
        MapDeferral(MapInfo *mapInfo) :
            mapInfo(mapInfo)
        {}

        MapInfo *mapInfo;
    };
    QList<MapDeferral> mDeferredMaps;
    MapInfo *mWaitingForMapInfo;

#ifdef WORLDED
    int mReferenceEpoch;
#endif
    QString mError;
};

class MapInfoManagerDeferral
{
public:
    MapInfoManagerDeferral() :
        mReleased(false)
    {
        MapInfoManager::instance().deferThreadResults(true);
    }

    ~MapInfoManagerDeferral()
    {
        if (!mReleased)
            MapInfoManager::instance().deferThreadResults(false);
    }

    void release()
    {
        MapInfoManager::instance().deferThreadResults(false);
        mReleased = true;
    }

private:
    bool mReleased;
};

#endif // MAPMANAGER_H
