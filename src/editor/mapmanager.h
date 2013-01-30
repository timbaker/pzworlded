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

#include "map.h"
#include "filesystemwatcher.h"

#include <QDateTime>
#include <QMap>
#include <QTimer>

#include <QMutexLocker>
#include <QThread>
#include <QWaitCondition>
class MapInfo;
class MapReaderThread : public QThread
{
    Q_OBJECT
public:
    MapReaderThread();
    ~MapReaderThread();

    void run();

    void addJob(MapInfo *mapInfo);

    typedef Tiled::Map Map;

signals:
    void mapLoaded(Map *map, MapInfo *mapInfo);
    void mapFailedToLoad(const QString error, MapInfo *mapInfo);

private:
    Map *loadMap(MapInfo *mapInfo);

    class Job {
    public:
        Job(MapInfo *mapInfo) :
            mapInfo(mapInfo)
        {
        }

        MapInfo *mapInfo;
    };
    QList<Job> mJobs;

    QMutex mMutex;
    QWaitCondition mWaitForJobs;
    bool mQuit;
    QString mError;
};

class MapInfo
{
public:
    MapInfo(Tiled::Map::Orientation orientation,
            int width, int height,
            int tileWidth, int tileHeight)
        : mOrientation(orientation)
        , mWidth(width)
        , mHeight(height)
        , mTileWidth(tileWidth)
        , mTileHeight(tileHeight)
        , mMap(0)
        , mPlaceholder(false)
        , mBeingEdited(false)
        , mMapRefCount(0)
        , mReferenceEpoch(0)
        , mLoading(false)
    {

    }

    bool isValid() const { return mWidth > 0 && mHeight > 0; }

    Tiled::Map::Orientation orientation() const { return mOrientation; }
    int width() const { return mWidth; }
    int height() const { return mHeight; }
    QSize size() const { return QSize(mWidth, mHeight); }
    int tileWidth() const { return mTileWidth; }
    int tileHeight() const { return mTileHeight; }

    void setFilePath(const QString& path) { mFilePath = path; }
    const QString &path() { return mFilePath; }

    Tiled::Map *map() const { return mMap; }

    void setBeingEdited(bool edited) { mBeingEdited = edited; }
    bool isBeingEdited() const { return mBeingEdited; }

    bool isLoading() const { return mLoading; }

private:
    Tiled::Map::Orientation mOrientation;
    int mWidth;
    int mHeight;
    int mTileWidth;
    int mTileHeight;
    QString mFilePath;
    Tiled::Map *mMap;
    bool mPlaceholder;
    bool mBeingEdited;
    int mMapRefCount;
    int mReferenceEpoch;
    bool mLoading;

    friend class MapManager;
};

class MapManager : public QObject
{
    Q_OBJECT
public:
    static MapManager *instance();
    static void deleteInstance();

    /**
     * Returns the canonical (absolute) path for a map file.
     * \a mapName may or may not include .tmx extension.
     * \a mapName may be relative or absolute.
     */
    QString pathForMap(const QString &mapName, const QString &relativeTo);

    MapInfo *loadMap(const QString &mapName,
                     const QString &relativeTo = QString(),
                     bool asynch = false);

    MapInfo *newFromMap(Tiled::Map *map, const QString &mapFilePath = QString());

    MapInfo *mapInfo(const QString &mapFilePath);

    /**
     * The "empty map" is used when a WorldCell has no map.
     * The user still needs to view the cell to place Lots etc.
     */
    MapInfo *getEmptyMap();

    /**
      * A "placeholder" map is used when a sub-map could not be loaded.
      * The user still needs to move/delete the sub-map.
      * A placeholder map may be updated to a real map when the
      * user changes the maptools directory.
      */
    MapInfo *getPlaceholderMap(const QString &mapName, int width, int height);

    /**
     * Converts a map to Isometric or LevelIsometric.
     * A new map is returned if conversion occurred.
     */
    Tiled::Map *convertOrientation(Tiled::Map *map, Tiled::Map::Orientation orient);

    /**
      * Call this when the map's size or tile size changes.
      */
    void mapChanged(MapInfo *mapInfo);

    void addReferenceToMap(MapInfo *mapInfo);
    void removeReferenceToMap(MapInfo *mapInfo);
    void purgeUnreferencedMaps();

    void newMapFileCreated(const QString &path);

    QString errorString() const
    { return mError; }

    typedef Tiled::Map Map;
signals:
    void mapAboutToChange(MapInfo *mapInfo);
    void mapFileChanged(MapInfo *mapInfo);
    void mapFileCreated(const QString &path);
    void mapLoaded(MapInfo *mapInfo);

private slots:
    void fileChanged(const QString &path);
    void fileChangedTimeout();

    void metaTilesetAdded(Tiled::Tileset *tileset);
    void metaTilesetRemoved(Tiled::Tileset *tileset);

    void mapLoadedByThread(Map *map, MapInfo *mapInfo);
    void mapFailedToLoadByThread(const QString error, MapInfo *mapInfo);

private:
    Q_DISABLE_COPY(MapManager)
    static MapManager *mInstance;
    MapManager();
    ~MapManager();

    QMap<QString,MapInfo*> mMapInfo;

    Tiled::Internal::FileSystemWatcher *mFileSystemWatcher;
    QSet<QString> mChangedFiles;
    QTimer mChangedFilesTimer;

    int mReferenceEpoch;

    MapReaderThread mMapReaderThread;

    QString mError;
};

#endif // MAPMANAGER_H
