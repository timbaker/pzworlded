/*
 * tilesetmanager.h
 * Copyright 2008-2009, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 * Copyright 2009, Edward Hutchins <eah1@yahoo.com>
 *
 * This file is part of Tiled.
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

#ifndef TILESETMANAGER_H
#define TILESETMANAGER_H

#ifdef ZOMBOID
#include "assetmanager.h"
#include "singleton.h"
#include <QFileInfo>
#endif
#include <QObject>
#include <QList>
#include <QMap>
#include <QString>
#include <QSet>
#include <QTimer>

#ifdef ZOMBOID
#include "threads.h"
#include <QVector>
namespace Tiled {
class Tileset;
}
class QImage;
class TilesetImageReaderWorker : public BaseWorker
{
    Q_OBJECT
public:
    TilesetImageReaderWorker(int id, InterruptibleThread *thread);

    ~TilesetImageReaderWorker();

    bool busy();

    typedef Tiled::Tileset Tileset;
signals:
    void imageLoaded(Tiled::Tileset *tileset, Tiled::Tileset *fromThread);

public slots:
    void work();
    void addJob(Tileset *tileset);

private:
    class Job {
    public:
        Job(Tiled::Tileset *tileset) :
            tileset(tileset)
        {
        }

        Tiled::Tileset *tileset;
    };
    QList<Job> mJobs;

    int mID;
    QMutex mJobsMutex;
    bool mHasJobs;
};

class AssetTask_LoadTileset;
#endif // ZOMBOID

namespace Tiled {

#ifdef ZOMBOID
class Tile;
class TilesetImageCache;
#endif
class Tileset;

namespace Internal {

class FileSystemWatcher;

#ifdef ZOMBOID
struct ZTileLayerNames;
#endif

/**
 * A tileset specification that uniquely identifies a certain tileset. Does not
 * take into account tile properties!
 */
struct TilesetSpec
{
    QString imageSource;
    int tileWidth;
    int tileHeight;
    int tileSpacing;
    int margin;
};

/**
 * The tileset manager keeps track of all tilesets used by loaded maps. It also
 * watches the tileset images for changes and will attempt to reload them when
 * they change.
 */
#ifdef ZOMBOID
class TilesetManager : public AssetManager, public Singleton<TilesetManager>
#else
class TilesetManager : public QObject
#endif
{
    Q_OBJECT

public:
#ifdef ZOMBOID
#else
    /**
     * Requests the tileset manager. When the manager doesn't exist yet, it
     * will be created.
     */
    static TilesetManager *instance();

    /**
     * Deletes the tileset manager instance, when it exists.
     */
    static void deleteInstance();
#endif
    /**
     * Searches for a tileset matching the given file name.
     * @return a tileset matching the given file name, or 0 if none exists
     */
    Tileset *findTileset(const QString &fileName) const;

    /**
     * Searches for a tileset matching the given specification.
     * @return a tileset matching the given specification, or 0 if none exists
     */
    Tileset *findTileset(const TilesetSpec &spec) const;

    /**
     * Adds a tileset reference. This will make sure the tileset doesn't get
     * deleted.
     */
    void addReference(Tileset *tileset);

    /**
     * Removes a tileset reference. This needs to be done before a tileset can
     * be deleted.
     */
    void removeReference(Tileset *tileset);

    /**
     * Convenience method to add references to multiple tilesets.
     * @see addReference
     */
    void addReferences(const QList<Tileset*> &tilesets);

    /**
     * Convenience method to remove references from multiple tilesets.
     * @see removeReference
     */
    void removeReferences(const QList<Tileset*> &tilesets);

    /**
     * Returns all currently available tilesets.
     */
    QList<Tileset*> tilesets() const;

    /**
     * Sets whether tilesets are automatically reloaded when their tileset
     * image changes.
     */
    void setReloadTilesetsOnChange(bool enabled);

    bool reloadTilesetsOnChange() const
    { return mReloadTilesetsOnChange; }

#ifdef ZOMBOID
    void changeTilesetSource(Tileset *tileset, const QString &source, bool missing);

    Tile *missingTile() const
    { return mMissingTile; }

    Tileset *missingTileset() const
    { return mMissingTileset; }

    Tile *noBlendTile() const
    { return mNoBlendTile; }

    void setLayerName(Tile *tile, const QString &name);
    QString layerName(Tile *tile);

    TilesetImageCache *imageCache() const { return mTilesetImageCache; }

    void loadTileset(Tileset *tileset, const QString &imageSource);
    void waitForTilesets(const QList<Tileset *> &tilesets = QList<Tileset*>());
#endif

signals:
    /**
     * Emitted when a tileset's images have changed and views need updating.
     */
    void tilesetChanged(Tileset *tileset);

#ifdef ZOMBOID_TILE_LAYER_NAMES
    void tileLayerNameChanged(Tile *tile);
#endif

private slots:
    void fileChanged(const QString &path);
    void fileChangedTimeout();

#ifdef ZOMBOID
    void imageLoaded(QImage *image, Tiled::Tileset *tileset);
    void imageLoaded(Tiled::Tileset *fromThread, Tiled::Tileset *tileset);
#endif

private:
    Q_DISABLE_COPY(TilesetManager)

#ifdef ZOMBOID
public:
    TilesetManager();

    /**
     * Destructor.  Accessed by Singleton::deleteInstance().
     */
    ~TilesetManager() override;

protected:
    Asset* createAsset(AssetPath path, AssetParams* params) override;
    void destroyAsset(Asset* asset) override;
    void startLoading(Asset* asset) override;

    friend class ::AssetTask_LoadTileset;
    void loadTilesetTaskFinished(AssetTask_LoadTileset* task);

private:
    TilesetImageCache *mTilesetImageCache;

    Tileset *mMissingTileset;
    Tile *mMissingTile;

    Tileset *mNoBlendTileset;
    Tile *mNoBlendTile;

    QVector<InterruptibleThread*> mImageReaderThreads;
    QVector<TilesetImageReaderWorker*> mImageReaderWorkers;
    int mNextThreadForJob;
#else
    /**
     * Constructor. Only used by the tileset manager itself.
     */
    TilesetManager();

    /**
     * Destructor.
     */
    ~TilesetManager();

    static TilesetManager *mInstance;
#endif

#ifdef ZOMBOID_TILE_LAYER_NAMES
    QMap<QString,ZTileLayerNames*> mTileLayerNames; // imageSource -> tile layer names

    QFileInfo tileLayerNamesFile(Tileset *ts);
    ZTileLayerNames *layerNamesForTileset(Tileset *ts);

    void readTileLayerNames(Tileset *ts);
    void writeTileLayerNames(ZTileLayerNames *tln);
    void syncTileLayerNames(Tileset *ts);
#endif

    /**
     * Stores the tilesets and maps them to the number of references.
     */
    QMap<Tileset*, int> mTilesets;
    FileSystemWatcher *mWatcher;
    QSet<QString> mChangedFiles;
    QTimer mChangedFilesTimer;
    bool mReloadTilesetsOnChange;
};

} // namespace Internal
} // namespace Tiled

#endif // TILESETMANAGER_H
