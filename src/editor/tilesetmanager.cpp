/*
 * tilesetmanager.cpp
 * Copyright 2008-2010, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
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

#include "tilesetmanager.h"

#include "filesystemwatcher.h"
#include "tileset.h"

#include <QImage>
#ifdef ZOMBOID
#include "preferences.h"
#include "tile.h"
#include <QDebug>
#include <QDir>
#include <QImageReader>
#include <QMetaType>
#endif

using namespace Tiled;
using namespace Tiled::Internal;

#ifdef ZOMBOID
SINGLETON_IMPL(TilesetManager)
#else
TilesetManager *TilesetManager::mInstance = 0;
#endif

TilesetManager::TilesetManager():
#ifdef ZOMBOID
    mTilesetImageCache(new TilesetImageCache),
#endif
    mWatcher(new FileSystemWatcher(this)),
    mReloadTilesetsOnChange(false)
{
#ifdef ZOMBOID
    const int TILE_WIDTH = 64;
    const int TILE_HEIGHT = 128;

    mMissingTileset = new Tileset(QLatin1String("missing"), TILE_WIDTH, TILE_HEIGHT);
    mMissingTileset->setManager(this); // hack
    mMissingTileset->setTransparentColor(Qt::white);
    mMissingTileset->setMissing(true);
    QString fileName = QLatin1String(":/images/missing-tile.png");
    if (!mMissingTileset->loadFromImage(QImage(fileName), fileName)) {
        QImage image(TILE_WIDTH, TILE_HEIGHT, QImage::Format_ARGB32);
        image.fill(Qt::red);
        mMissingTileset->loadFromImage(image, fileName);
    }
    mMissingTile = mMissingTileset->tileAt(0);
    mTilesets.insert(mMissingTileset, 1); //addReference(mMissingTileset);

    mNoBlendTileset = new Tileset(QLatin1String("noblend"), TILE_WIDTH, TILE_HEIGHT);
    mNoBlendTileset->setManager(this); // hack
    mNoBlendTileset->setTransparentColor(Qt::white);
    mNoBlendTileset->setMissing(true);
    fileName = QLatin1String(":/images/noblend.png");
    if (!mNoBlendTileset->loadFromImage(QImage(fileName), fileName)) {
        QImage image(TILE_WIDTH, TILE_HEIGHT, QImage::Format_ARGB32);
        image.fill(Qt::red);
        mNoBlendTileset->loadFromImage(image, fileName);
    }
    mNoBlendTile = mNoBlendTileset->tileAt(0);
    mTilesets.insert(mNoBlendTileset, 1); //addReference(mNoBlendTileset);

    qRegisterMetaType<Tileset*>("Tileset*");

#if 0
    mImageReaderThreads.resize(8);
    mImageReaderWorkers.resize(mImageReaderThreads.size());
    mNextThreadForJob = 0;
    for (int i = 0; i < mImageReaderWorkers.size(); i++) {
        mImageReaderThreads[i] = new InterruptibleThread;
        mImageReaderWorkers[i] = new TilesetImageReaderWorker(i, mImageReaderThreads[i]);
        mImageReaderWorkers[i]->moveToThread(mImageReaderThreads[i]);
        connect(mImageReaderWorkers[i], SIGNAL(imageLoaded(Tiled::Tileset*,Tiled::Tileset*)),
                SLOT(imageLoaded(Tiled::Tileset*,Tiled::Tileset*)));
        mImageReaderThreads[i]->start();
    }
#endif

#ifndef WORLDED
    mReloadTilesetsOnChange = Preferences::instance()->reloadTilesetsOnChange();
#endif
#endif

    connect(mWatcher, SIGNAL(fileChanged(QString)),
            this, SLOT(fileChanged(QString)));

    mChangedFilesTimer.setInterval(500);
    mChangedFilesTimer.setSingleShot(true);

    connect(&mChangedFilesTimer, SIGNAL(timeout()),
            this, SLOT(fileChangedTimeout()));
}

TilesetManager::~TilesetManager()
{
#ifdef ZOMBOID
    removeReference(mMissingTileset);
    removeReference(mNoBlendTileset);
#if 0
    for (int i = 0; i < mImageReaderThreads.size(); i++) {
        mImageReaderThreads[i]->interrupt();
        mImageReaderThreads[i]->quit();
        mImageReaderThreads[i]->wait();
        delete mImageReaderWorkers[i];
        delete mImageReaderThreads[i];
    }
#endif
    delete mTilesetImageCache;
#endif

    // Since all MapDocuments should be deleted first, we assert that there are
    // no remaining tileset references.
    Q_ASSERT(mTilesets.size() == 0);

#ifdef ZOMBOID_TILE_LAYER_NAMES
    foreach (ZTileLayerNames *tln, mTileLayerNames)
        writeTileLayerNames(tln);
#endif
}

#ifdef ZOMBOID
Asset *TilesetManager::createAsset(AssetPath path, AssetParams *params)
{
    return new Tileset(path, this, params);
}

void TilesetManager::destroyAsset(Asset *asset)
{
    Q_UNUSED(asset);
}

void TilesetManager::startLoading(Asset *asset)
{

}
#endif

#ifndef ZOMBOID
TilesetManager *TilesetManager::instance()
{
    if (!mInstance)
        mInstance = new TilesetManager;

    return mInstance;
}

void TilesetManager::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}
#endif

Tileset *TilesetManager::findTileset(const QString &fileName) const
{
    foreach (Tileset *tileset, tilesets())
        if (tileset->fileName() == fileName)
            return tileset;

    return 0;
}

Tileset *TilesetManager::findTileset(const TilesetSpec &spec) const
{
    foreach (Tileset *tileset, tilesets()) {
        if (tileset->imageSource() == spec.imageSource
            && tileset->tileWidth() == spec.tileWidth
            && tileset->tileHeight() == spec.tileHeight
            && tileset->tileSpacing() == spec.tileSpacing
            && tileset->margin() == spec.margin)
        {
            return tileset;
        }
    }

    return 0;
}

void TilesetManager::addReference(Tileset *tileset)
{
    if (mTilesets.contains(tileset)) {
        mTilesets[tileset]++;
    } else {
        mTilesets.insert(tileset, 1);
#ifdef ZOMBOID
#else
        if (!tileset->imageSource().isEmpty())
            mWatcher->addPath(tileset->imageSource());
#endif
    }
#ifdef ZOMBOID_TILE_LAYER_NAMES
    if (!tileset->imageSource().isEmpty() && !tileset->isMissing())
        readTileLayerNames(tileset);
#endif

#ifdef ZOMBOID
    loadTileset(tileset, tileset->imageSource());
#endif
}

void TilesetManager::removeReference(Tileset *tileset)
{
    Q_ASSERT(mTilesets.value(tileset) > 0);
    mTilesets[tileset]--;

    if (mTilesets.value(tileset) == 0) {
        mTilesets.remove(tileset);
#ifdef ZOMBOID
#else
        if (!tileset->imageSource().isEmpty())
            mWatcher->removePath(tileset->imageSource());
#endif

        delete tileset;
    }
}

void TilesetManager::addReferences(const QList<Tileset*> &tilesets)
{
    foreach (Tileset *tileset, tilesets)
        addReference(tileset);
}

void TilesetManager::removeReferences(const QList<Tileset*> &tilesets)
{
    foreach (Tileset *tileset, tilesets)
        removeReference(tileset);
}

QList<Tileset*> TilesetManager::tilesets() const
{
    return mTilesets.keys();
}

void TilesetManager::setReloadTilesetsOnChange(bool enabled)
{
    mReloadTilesetsOnChange = enabled;
    // TODO: Clear the file system watcher when disabled
}

void TilesetManager::fileChanged(const QString &path)
{
#ifndef ZOMBOID
    if (!mReloadTilesetsOnChange)
        return;
#endif

    /*
     * Use a one-shot timer since GIMP (for example) seems to generate many
     * file changes during a save, and some of the intermediate attempts to
     * reload the tileset images actually fail (at least for .png files).
     */
    mChangedFiles.insert(path);
    mChangedFilesTimer.start();
}

void TilesetManager::fileChangedTimeout()
{
#ifdef ZOMBOID
    qDebug() << "fileChangedTimeout " << mChangedFiles;
    foreach (Tileset *tileset, mTilesetImageCache->mTilesets) {
        QString fileName = tileset->imageSource2x().isEmpty() ? tileset->imageSource() : tileset->imageSource2x();
        if (mChangedFiles.contains(fileName)) {
            if (QImageReader(fileName).size().isValid()) {
                tileset->loadFromImage(QImage(fileName), tileset->imageSource());
                tileset->setMissing(false);
            } else {
                if (tileset->tileHeight() == mMissingTile->width() && tileset->tileWidth() == mMissingTile->height()) {
                    for (int i = 0; i < tileset->tileCount(); i++)
                        tileset->tileAt(i)->setImage(mMissingTile);
                }
                tileset->setMissing(true);
            }
        }
    }
    foreach (Tileset *tileset, tilesets()) {
        QString fileName = tileset->imageSource();
        QString fileName2 = tileset->imageSource2x().isEmpty() ? tileset->imageSource() : tileset->imageSource2x();
        if (mChangedFiles.contains(fileName2)) {
            if (Tileset *cached = mTilesetImageCache->findMatch(tileset, fileName, fileName2)) {
                if (tileset->loadFromCache(cached)) {
                    tileset->setMissing(cached->isMissing());
#ifdef ZOMBOID_TILE_LAYER_NAMES
                    syncTileLayerNames(tileset);
#endif
                    emit tilesetChanged(tileset);
                }
            }
        }
    }
#else

    foreach (Tileset *tileset, tilesets()) {
        QString fileName = tileset->imageSource();
        if (mChangedFiles.contains(fileName))
            if (tileset->loadFromImage(QImage(fileName), fileName))
#ifdef ZOMBOID
            {
                syncTileLayerNames(tileset);
                emit tilesetChanged(tileset);
            }
#else
                emit tilesetChanged(tileset);
#endif
    }
#endif

    mChangedFiles.clear();
}

#ifdef ZOMBOID
void TilesetManager::imageLoaded(QImage *image, Tileset *tileset)
{
    Q_ASSERT(mTilesetImageCache->mTilesets.contains(tileset));

    // This updates a tileset in the cache.
    tileset->loadFromImage(*image, tileset->imageSource());

    // Watch the image file for changes.
    mWatcher->addPath(tileset->imageSource2x().isEmpty() ? tileset->imageSource() : tileset->imageSource2x());

    // Now update every tileset using this image.
    foreach (Tileset *candidate, tilesets()) {
        if (candidate->isLoaded())
            continue;
        if (((candidate->imageSource() == tileset->imageSource()) || (!tileset->imageSource2x().isEmpty() && (candidate->imageSource2x() == tileset->imageSource2x())))
                && candidate->tileWidth() == tileset->tileWidth()
                && candidate->tileHeight() == tileset->tileHeight()
                && candidate->tileSpacing() == tileset->tileSpacing()
                && candidate->margin() == tileset->margin()
                && candidate->transparentColor() == tileset->transparentColor()) {
            candidate->loadFromCache(tileset);
            candidate->setMissing(false);
            emit tilesetChanged(candidate);
        }
    }
    delete image;
}

void TilesetManager::imageLoaded(Tileset *fromThread, Tileset *tileset)
{
    Q_ASSERT(mTilesetImageCache->mTilesets.contains(tileset));

    // This updates a tileset in the cache.
    // HACK - 'fromThread' is not in the cache, 'tileset' is
    tileset->loadFromCache(fromThread);

    // Watch the image file for changes.
    mWatcher->addPath(tileset->imageSource2x().isEmpty() ? tileset->imageSource() : tileset->imageSource2x());

    // Now update every tileset using this image.
    QList<Tileset*> tilesets = this->tilesets();
    for (Tileset *candidate : tilesets)
    {
        if (candidate->isLoaded())
            continue;
        if (((candidate->imageSource() == tileset->imageSource()) ||
                (!tileset->imageSource2x().isEmpty() && (candidate->imageSource2x() == tileset->imageSource2x())))
                && candidate->tileWidth() == tileset->tileWidth()
                && candidate->tileHeight() == tileset->tileHeight()
                && candidate->tileSpacing() == tileset->tileSpacing()
                && candidate->margin() == tileset->margin()
                && candidate->transparentColor() == tileset->transparentColor())
        {
            candidate->loadFromCache(tileset);
            candidate->setMissing(false);

            // Abusing the Asset-loading system
            if (candidate->isEmpty())
                candidate->onCreated(AssetState::READY);

            emit tilesetChanged(candidate);
        }
    }
}

#include "assettask.h"
#include "assettaskmanager.h"

class AssetTask_LoadTileset : public BaseAsyncAssetTask
{
public:
    AssetTask_LoadTileset(Tileset* asset, QString imageSource2x, QString imageSource)
        : BaseAsyncAssetTask(asset)
        , mCached(asset)
        , mImageSource2x(imageSource2x)
        , mImageSource(imageSource)
    {

    }

    void run() override
    {
        if (QImageReader(mImageSource2x).size().isValid())
        {
            qDebug() << "2x YES " << mImageSource;
            QImage image(mImageSource2x);
            if (image.isNull())
            {
                return;
            }
            mTileset = new Tileset(QStringLiteral("temp2x"), 64, 128);
            mTileset->setManager(TilesetManager::instancePtr()); // hack
            mTileset->setImageSource2x(mImageSource2x);
            bLoaded = mTileset->loadFromImage(image, mImageSource);
        }
        else if (QImageReader(mImageSource).size().isValid())
        {
            qDebug() << "2x NO " << mImageSource;
            QImage image(mImageSource);
            if (image.isNull())
            {
                return;
            }
            mTileset = new Tileset(QStringLiteral("temp"), 64, 128);
            mTileset->setManager(TilesetManager::instancePtr()); // hack
            mTileset->setImageSource2x(QString());
            bLoaded = mTileset->loadFromImage(image, mImageSource);
        }
        else
        {
        }
    }

    void handleResult() override
    {
        TilesetManager::instance().loadTilesetTaskFinished(this);
    }

    void release() override
    {
        delete mTileset;
        delete this;
    }

    Tileset* mCached;
    Tileset* mTileset = nullptr; // in mTilesetImageCache
    QString mImageSource2x;
    QString mImageSource;
    bool bLoaded = false;
    QString mError;
};

void TilesetManager::loadTilesetTaskFinished(AssetTask_LoadTileset *task)
{
    task->mCached->setManager(this); // hack

    if (task->bLoaded)
    {
        imageLoaded(task->mTileset, task->mCached);
        onLoadingSucceeded(task->mCached);
    }
    else
    {
        for (Tileset *candidate : tilesets())
        {
            if (candidate->isLoaded())
                continue;
            if (candidate->imageSource() == task->mImageSource)
            {
                if (candidate->tileHeight() == mMissingTile->height() && candidate->tileWidth() == mMissingTile->width())
                {
                    for (int i = 0; i < candidate->tileCount(); i++)
                    {
                        candidate->tileAt(i)->setImage(mMissingTile);
                    }
                }
                changeTilesetSource(candidate, task->mImageSource, true);
                candidate->setImageSource2x(QString());
                if (candidate->isEmpty())
                    onLoadingFailed(candidate);
            }
        }

        onLoadingFailed(task->mCached);
    }
}

void TilesetManager::loadTileset(Tileset *tileset, const QString &imageSource_)
{
    tileset->setManager(this); // hack

    // Hack to ignore TileMetaInfoMgr's tilesets that haven't been loaded,
    // their paths are relative to the Tiles Directory.
    if (QDir(imageSource_).isRelative())
        return;

    if (!tileset->isLoaded() /*&& !tileset->isMissing()*/) {
        QString tilesDir = Preferences::instance()->tilesDirectory();
        QString tiles2xDir = Preferences::instance()->tiles2xDirectory();
        QString imageSource = QDir(tilesDir).filePath(tileset->name() + QLatin1Literal(".png"));
        QString imageSource2x = QDir(tiles2xDir).filePath(tileset->name() + QLatin1Literal(".png"));
        if (Tileset *cached = mTilesetImageCache->findMatch(tileset, imageSource, imageSource2x)) {
            // If it !isLoaded(), a thread is reading the image.
            // FIXME: 1) load TMX with tilesets from not-TilesDirectory -> no 2x images loaded
            //        2) switch TilesDirectory to the same not-TilesDirectory in 1)
            //        3) 2x images remain unloaded
            if (cached->isLoaded()) {
                tileset->loadFromCache(cached);
                tileset->setMissing(false);
                tileset->onCreated(AssetState::READY);
                emit tilesetChanged(tileset);
            } else {
                changeTilesetSource(tileset, imageSource, false);
                tileset->setImageSource2x(cached->imageSource2x());
            }
        }
#if 1
        else
        {
            changeTilesetSource(tileset, imageSource, false);
            cached = mTilesetImageCache->addTileset(tileset);

            AssetTask_LoadTileset* assetTask = new AssetTask_LoadTileset(cached, imageSource2x, imageSource);
//            assetTask->connect(this, [this, assetTask]{ loadTilesetTaskFinished(assetTask); });
            setTask(cached, assetTask);
            AssetTaskManager::instance().submit(assetTask);
#else
        } else if (QImageReader(imageSource2x).size().isValid()) {
            qDebug() << "2x YES " << imageSource;
            changeTilesetSource(tileset, imageSource, false);
            tileset->setImageSource2x(imageSource2x);
            cached = mTilesetImageCache->addTileset(tileset);
#if 1 /* QT_POINTER_SIZE == 8 */
            QMetaObject::invokeMethod(mImageReaderWorkers[mNextThreadForJob],
                                      "addJob", Qt::QueuedConnection,
                                      Q_ARG(Tileset*,cached));
            mNextThreadForJob = (mNextThreadForJob + 1) % mImageReaderWorkers.size();
#else
            QImage *image = new QImage(tileset->imageSource2x());
            imageLoaded(image, cached);
#endif
        } else if (QImageReader(imageSource).size().isValid()) {
            qDebug() << "2x NO " << imageSource;
            changeTilesetSource(tileset, imageSource, false);
            tileset->setImageSource2x(QString());
            cached = mTilesetImageCache->addTileset(tileset);
#if 1 /* QT_POINTER_SIZE == 8 */
            QMetaObject::invokeMethod(mImageReaderWorkers[mNextThreadForJob],
                                      "addJob", Qt::QueuedConnection,
                                      Q_ARG(Tileset*,cached));
            mNextThreadForJob = (mNextThreadForJob + 1) % mImageReaderWorkers.size();
            qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
#else
            QImage *image = new QImage(tileset->imageSource());
            imageLoaded(image, cached);
#endif
        } else {
            if (tileset->tileHeight() == mMissingTile->height() && tileset->tileWidth() == mMissingTile->width()) {
                for (int i = 0; i < tileset->tileCount(); i++)
                    tileset->tileAt(i)->setImage(mMissingTile);
            }
            changeTilesetSource(tileset, imageSource, true);
            tileset->setImageSource2x(QString());
#endif
        }
    }
}

void TilesetManager::waitForTilesets(const QList<Tileset *> &tilesets)
{
    while (true) {
        bool busy = false;
        for (TilesetImageReaderWorker *worker : mImageReaderWorkers) {
            if (worker->busy()) {
                busy = true;
                break;
            }
        }
        if (!busy)
            break;
        Sleep::msleep(10);
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);

    foreach (Tileset *ts, tilesets) {
        if (ts->isLoaded())
            continue;
        // Missing tilesets aren't in mTilesetImageCache
        if (ts->isMissing())
            continue;
        // There may be a thread already reading or about to read this image.
        QImage *image = new QImage(ts->imageSource2x().isEmpty() ? ts->imageSource() : ts->imageSource2x());
        Tileset *cached = mTilesetImageCache->findMatch(ts, ts->imageSource(), ts->imageSource2x());
        Q_ASSERT(cached != 0 && !cached->isLoaded());
        if (cached) {
            imageLoaded(image, cached); // deletes image
        }
    }
}

void TilesetManager::changeTilesetSource(Tileset *tileset, const QString &source,
                                         bool missing)
{
    tileset->setImageSource(source);
    tileset->setMissing(missing);
    if (!tileset->imageSource().isEmpty() && !tileset->isMissing()) {
#ifdef ZOMBOID_TILE_LAYER_NAMES
        readTileLayerNames(tileset);
#endif
    }
    tileset->setLoaded(false);
    if (missing)
        emit tilesetChanged(tileset);
}
#endif

#ifdef ZOMBOID_TILE_LAYER_NAMES
#include "tile.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QXmlStreamReader>

namespace Tiled {
namespace Internal {

struct ZTileLayerName
{
    ZTileLayerName()
    {}
    QString mLayerName;
};

struct ZTileLayerNames
{
    ZTileLayerNames()
        : mColumns(0)
        , mRows(0)
        , mModified(false)
    {}
    ZTileLayerNames(const QString& filePath, int columns, int rows)
        : mColumns(columns)
        , mRows(rows)
        , mFilePath(filePath)
        , mModified(false)
    {
        mTiles.resize(columns * rows);
    }
    void enforceSize(int columns, int rows)
    {
        if (columns == mColumns && rows == mRows)
            return;
        if (columns == mColumns) {
            // Number of rows changed, tile ids are still valid
            mTiles.resize(columns * rows);
            return;
        }
        // Number of columns (and maybe rows) changed.
        // Copy over the preserved part.
        QRect oldBounds(0, 0, mColumns, mRows);
        QRect newBounds(0, 0, columns, rows);
        QRect preserved = oldBounds & newBounds;
        QVector<ZTileLayerName> tiles(columns * rows);
        for (int y = 0; y < preserved.height(); y++) {
            for (int x = 0; x < preserved.width(); x++) {
                tiles[y * columns + x] = mTiles[y * mColumns + x];
            }
        }
        mColumns = columns;
        mRows = rows;
        mTiles = tiles;
    }

    int mColumns;
    int mRows;
    QString mFilePath;
    QVector<ZTileLayerName> mTiles;
    bool mModified;
};

} // namespace Internal
} // namespace Tiled

void TilesetManager::setLayerName(Tile *tile, const QString &name)
{
    Tileset *ts = tile->tileset();
    if (ZTileLayerNames *tln = layerNamesForTileset(ts)) {
        // Prevent an error if two tilesets share the same image file but have
        // different tile dimensions.
        if ((tile->id() >= 0) && (tile->id() < tln->mRows * tln->mColumns)) {
            tln->mTiles[tile->id()].mLayerName = name;
            tln->mModified = true;
            emit tileLayerNameChanged(tile);
        }
    }
}

QString TilesetManager::layerName(Tile *tile)
{
    Tileset *ts = tile->tileset();
    if (mTileLayerNames.contains(ts->imageSource())) {
        ZTileLayerNames *tln =  mTileLayerNames[ts->imageSource()];
        // Prevent an error if two tilesets share the same image file but have
        // different tile dimensions.
        if ((tile->id() >= 0) && (tile->id() < tln->mRows * tln->mColumns))
            return tln->mTiles[tile->id()].mLayerName;
    }
    return QString();
}

class ZTileLayerNamesReader
{
public:
    bool read(const QString &filePath)
    {
        mError.clear();

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            mError = QCoreApplication::translate("TileLayerNames", "Could not open file.");
            return false;
        }

        xml.setDevice(&file);

        if (xml.readNextStartElement() && xml.name() == QLatin1String("tileset")) {
            mTLN.mFilePath = filePath;
            return readTileset();
        } else {
            mError = QCoreApplication::translate("TileLayerNames", "File doesn't contain <tilesets>.");
            return false;
        }
    }

    bool readTileset()
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("tileset"));

        const QXmlStreamAttributes atts = xml.attributes();
        const QString tilesetName = atts.value(QLatin1String("name")).toString();
        uint columns = atts.value(QLatin1String("columns")).toString().toUInt();
        uint rows = atts.value(QLatin1String("rows")).toString().toUInt();

        mTLN.mTiles.resize(columns * rows);

        mTLN.mColumns = columns;
        mTLN.mRows = rows;

        while (xml.readNextStartElement()) {
            if (xml.name() == QLatin1String("tile")) {
                const QXmlStreamAttributes atts = xml.attributes();
                uint id = atts.value(QLatin1String("id")).toString().toUInt();
                if (id >= columns * rows) {
                    qDebug() << "<tile> " << id << " out-of-bounds: ignored";
                } else {
                    const QString layerName(atts.value(QLatin1String("layername")).toString());
                    mTLN.mTiles[id].mLayerName = layerName;
                }
            }
            xml.skipCurrentElement();
        }

        return true;
    }

    QString errorString() const { return mError; }
    ZTileLayerNames &result() { return mTLN; }

private:
    QString mError;
    QXmlStreamReader xml;
    ZTileLayerNames mTLN;
};

class ZTileLayerNamesWriter
{
public:
    bool write(const ZTileLayerNames *tln)
    {
        mError.clear();

        QFile file(tln->mFilePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            mError = QCoreApplication::translate(
                        "TileLayerNames", "Could not open file for writing.");
            return false;
        }

        QXmlStreamWriter writer(&file);

        writer.setAutoFormatting(true);
        writer.setAutoFormattingIndent(1);

        writer.writeStartDocument();
        writer.writeStartElement(QLatin1String("tileset"));
        writer.writeAttribute(QLatin1String("columns"), QString::number(tln->mColumns));
        writer.writeAttribute(QLatin1String("rows"), QString::number(tln->mRows));

        int id = 0;
        foreach (const ZTileLayerName &tile, tln->mTiles) {
            if (!tile.mLayerName.isEmpty()) {
                writer.writeStartElement(QLatin1String("tile"));
                writer.writeAttribute(QLatin1String("id"), QString::number(id));
                writer.writeAttribute(QLatin1String("layername"), tile.mLayerName);
                writer.writeEndElement();
            }
            ++id;
        }

        writer.writeEndElement();
        writer.writeEndDocument();

        if (file.error() != QFile::NoError) {
            mError = file.errorString();
            return false;
        }

        return true;
    }

    QString errorString() const { return mError; }

private:
    QString mError;
};

QFileInfo TilesetManager::tileLayerNamesFile(Tileset *ts)
{
    QString imageSource = ts->imageSource();
    QFileInfo fileInfoImgSrc(imageSource);
    QDir dir = fileInfoImgSrc.absoluteDir();
    return QFileInfo(dir, fileInfoImgSrc.completeBaseName() + QLatin1String(".tilelayers.xml"));
}

ZTileLayerNames *TilesetManager::layerNamesForTileset(Tileset *ts)
{
    QString imageSource = ts->imageSource();
    QMap<QString,ZTileLayerNames*>::iterator it = mTileLayerNames.find(imageSource);
    if (it != mTileLayerNames.end())
        return *it;

    int columns = ts->columnCount();
    int rows = columns ? ts->tileCount() / columns : 0;

    QFileInfo fileInfo = tileLayerNamesFile(ts);
    QString filePath = fileInfo.absoluteFilePath();
    return mTileLayerNames[imageSource] = new ZTileLayerNames(filePath, columns, rows);
}

void TilesetManager::readTileLayerNames(Tileset *ts)
{
    QString imageSource = ts->imageSource();
    if (mTileLayerNames.contains(imageSource))
        return;

    int columns = ts->columnCount();
    int rows = columns ? ts->tileCount() / columns : 0;

    QFileInfo fileInfo = tileLayerNamesFile(ts);
    if (fileInfo.exists()) {
        QString filePath = fileInfo.absoluteFilePath();
//        qDebug() << "Reading: " << filePath;
        ZTileLayerNamesReader reader;
        if (reader.read(filePath)) {
            mTileLayerNames[imageSource] = new ZTileLayerNames(reader.result());
            // Handle the source image being resized
            mTileLayerNames[imageSource]->enforceSize(columns, rows);
        } else {
            QMessageBox::critical(0, tr("Error Reading Tile Layer Names"),
                                  filePath + QLatin1String("\n") + reader.errorString());
        }
    }
}

void TilesetManager::writeTileLayerNames(ZTileLayerNames *tln)
{
    if (tln->mModified == false)
        return;
//    qDebug() << "Writing: " << tln->mFilePath;
    ZTileLayerNamesWriter writer;
    if (writer.write(tln) == false) {
        QMessageBox::critical(0, tr("Error Writing Tile Layer Names"),
            tln->mFilePath + QLatin1String("\n") + writer.errorString());
    }
}

void TilesetManager::syncTileLayerNames(Tileset *ts)
{
    if (mTileLayerNames.contains(ts->imageSource())) {
        ZTileLayerNames *tln = mTileLayerNames[ts->imageSource()];
        int columns = ts->columnCount();
        int rows = columns ? ts->tileCount() / columns : 0;
        tln->enforceSize(columns, rows);
    }
}
#endif // ZOMBOID

#ifdef ZOMBOID
/////

TilesetImageReaderWorker::TilesetImageReaderWorker(int id, InterruptibleThread *thread) :
    BaseWorker(thread),
    mID(id),
    mHasJobs(false)
{
}

TilesetImageReaderWorker::~TilesetImageReaderWorker()
{
}

bool TilesetImageReaderWorker::busy()
{
    QMutexLocker locker(&mJobsMutex);
    return mHasJobs;
}

void TilesetImageReaderWorker::work()
{
    IN_WORKER_THREAD

    while (mJobs.size()) {
        if (aborted()) {
            mJobs.clear();
            break;
        }

        Job job = mJobs.takeAt(0);

        QImage *image = new QImage(job.tileset->imageSource2x().isEmpty() ? job.tileset->imageSource() : job.tileset->imageSource2x());
#if 0
        Sleep::msleep(500);
        qDebug() << "TilesetImageReaderThread #" << mID << "loaded" << job.tileset->imageSource();
#endif
        Tileset *fromThread = new Tileset(job.tileset->name(), 64, 128);
        fromThread->setManager(TilesetManager::instancePtr()); // hack
        fromThread->setImageSource2x(job.tileset->imageSource2x());
        fromThread->loadFromImage(*image, job.tileset->imageSource());
        delete image;
        emit imageLoaded(fromThread, job.tileset);
    }

    QMutexLocker locker(&mJobsMutex);
    mHasJobs = false;
}

void TilesetImageReaderWorker::addJob(Tileset *tileset)
{
    IN_WORKER_THREAD

    QMutexLocker locker(&mJobsMutex);
    mHasJobs = true;
    locker.unlock();

    mJobs += Job(tileset);
    scheduleWork();
}
#endif // ZOMBOID
