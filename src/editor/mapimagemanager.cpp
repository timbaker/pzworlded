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

#include "mapimagemanager.h"

#include "assettask.h"
#include "assettaskmanager.h"
#ifdef WORLDED
#include "bmptotmx.h"
#endif // WORLDED
#include "bmpblender.h"
#include "idletasks.h"
#include "imagelayer.h"
#include "isometricrenderer.h"
#include "mainwindow.h"
#include "map.h"
#include "mapasset.h"
#include "mapcomposite.h"
#include "mapimage.h"
#include "mapinfo.h"
#include "mapinfomanager.h"
#include "mapimagemanager.h"
#include "mapmanager.h"
#include "objectgroup.h"
#include "orthogonalrenderer.h"
#include "progress.h"
#include "staggeredrenderer.h"
#include "tilelayer.h"
#include "tilesetmanager.h"
#include "zlevelrenderer.h"

#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QMessageBox>

#ifdef QT_NO_DEBUG
inline QNoDebug noise() { return QNoDebug(); }
#else
inline QDebug noise() { return QDebug(QtDebugMsg); }
#endif

using namespace Tiled;
using namespace Tiled::Internal;

const int IMAGE_WIDTH = 512;

SINGLETON_IMPL(MapImageManager)

MapImageManager::MapImageManager()
    : mDeferralDepth(0)
{
    qRegisterMetaType<MapImageData>("MapImageData");
    qRegisterMetaType<MapImage*>("MapImage*");
    qRegisterMetaType<MapComposite*>("MapComposite*");

    connect(MapManager::instancePtr(), &MapManager::mapAboutToChange,
            this, &MapImageManager::mapAboutToChange);
    connect(MapManager::instancePtr(), &MapManager::mapChanged,
            this, &MapImageManager::mapChanged);
    connect(MapManager::instancePtr(), &MapManager::mapFileChanged,
            this, &MapImageManager::mapFileChanged);
    connect(MapManager::instancePtr(), &MapManager::mapLoaded,
            this, &MapImageManager::mapLoaded);
    connect(MapManager::instancePtr(), &MapManager::mapFailedToLoad,
            this, &MapImageManager::mapFailedToLoad);

    connect(IdleTasks::instancePtr(), &IdleTasks::idleTime, this, &MapImageManager::processDeferrals);
    connect(IdleTasks::instancePtr(), &IdleTasks::idleTime, this, &MapImageManager::processWaitingTasks);
}

MapImageManager::~MapImageManager()
{
}

MapImage *MapImageManager::getMapImage(const QString &mapName, const QString &relativeTo)
{
    // Do not emit mapImageChanged as a result of worker threads finishing
    // loading any images while we are creating a new thumbnail image.
    // Any time QCoreApplication::processEvents() gets called (as is done
    // by MapManager's EditorMapReader class and the PROGRESS class) a
    // worker-thread's signal to us may be processed.
    MapImageManagerDeferral deferral; // FIXME: optimized out?

#ifdef WORLDED
    QString suffix = QFileInfo(mapName).suffix();
    if (BMPToTMX::supportedImageFormats().contains(suffix)) {
        QString keyName = QFileInfo(mapName).canonicalFilePath();
        if (MapImage* mapImage = static_cast<MapImage*>(get(keyName)))
            return mapImage;
        ImageData data = generateBMPImage(mapName);
        if (!data.valid)
            return nullptr;
        // Abusing the MapInfo struct
        MapInfo *mapInfo = new MapInfo(Map::Isometric,
                                       data.levelZeroBounds.width(),
                                       data.levelZeroBounds.height(), 1, 1);
        mapInfo->setManager(MapInfoManager::instancePtr());
        MapImage *mapImage = new MapImage(data.image, data.scale,
                                          data.levelZeroBounds, data.mapSize, data.tileSize,
                                          mapInfo);
        mapImage->setManager(this);
        m_assets[keyName] = mapImage;
        mapImage->chopIntoPieces();
        return mapImage;
    }
#endif

    QString mapFilePath = MapManager::instance().pathForMap(mapName, relativeTo);
    if (mapFilePath.isEmpty())
        return nullptr;

    if (MapImage* mapImage = static_cast<MapImage*>(get(mapFilePath)))
        return mapImage;

    return static_cast<MapImage*>(load(AssetPath(mapFilePath)));
}

void MapImageManager::recreateImage(const QString &mapName)
{
    MapImage* mapImage = getMapImage(mapName);
    if (mapImage == nullptr)
        return;
    if (mapImage->isEmpty())
        return;
    QFileInfo fileInfo = imageFileInfo(mapName);
    if (fileInfo.exists())
    {
        QFile(fileInfo.absoluteFilePath()).remove();
        mapImage->mSources.clear();
        mapImage->mSources += mapImage->mapInfo();
        reload(mapImage);
    }
}

void MapImageManager::paintDummyImage(ImageData &data, MapInfo *mapInfo)
{
    Q_ASSERT(data.size.isValid());
    data.image = QImage(data.size, QImage::Format_ARGB32);
//        data.image.setColorTable(QVector<QRgb>() << qRgb(255,255,255));
    QPainter p(&data.image);
    QPolygonF poly;
    MapImage mapImage(data.image, data.scale, data.levelZeroBounds, data.mapSize, data.tileSize, mapInfo);
    poly += mapImage.tileToImageCoords(0, 0);
    poly += mapImage.tileToImageCoords(mapInfo->width(), 0);
    poly += mapImage.tileToImageCoords(mapInfo->width(), mapInfo->height());
    poly += mapImage.tileToImageCoords(0, mapInfo->height());
    poly += poly.first();
    QPainterPath path;
    path.addPolygon(poly);
    data.image.fill(Qt::transparent);
    p.fillPath(path, QColor(100,100,100));
}

#ifdef WORLDED
// BMP To TMX image thumbnail
MapImageManager::ImageData MapImageManager::generateBMPImage(const QString &bmpFilePath)
{
    QSize imageSize = BMPToTMX::instance()->validateImages(bmpFilePath);
    if (imageSize.isEmpty()) {
        mError = BMPToTMX::instance()->errorString();
        return ImageData();
    }

    // Transform the image to the isometric view
    QTransform xform;
    xform.scale(1.0 / 2, 0.5 / 2);
    xform.shear(-1, 1);
    QRect skewedImageBounds = xform.mapRect(QRect(QPoint(0, 0), imageSize));

    QFileInfo fileInfo(bmpFilePath);
    QFileInfo imageInfo = imageFileInfo(bmpFilePath);
    QFileInfo imageDataInfo = imageDataFileInfo(imageInfo);
    if (imageInfo.exists() && imageDataInfo.exists() &&
            (fileInfo.lastModified() < imageInfo.lastModified())) {
        QImage image(imageInfo.absoluteFilePath());
        if (image.isNull())
            QMessageBox::warning(MainWindow::instance(), tr("Error Loading Image"),
                                 tr("An error occurred trying to read a BMP thumbnail image.\n")
                                 + imageInfo.absoluteFilePath());
        if (image.size() == skewedImageBounds.size()) {
            ImageData data = readImageData(imageDataInfo);
            if (data.valid) {
                data.image = image;
                return data;
            }
        }
    }

    PROGRESS progress(tr("Generating thumbnail for %1").arg(fileInfo.completeBaseName()));

    BMPToTMXImages *images = BMPToTMX::instance()->getImages(bmpFilePath, QPoint());
    if (!images)
        return ImageData();

#if 1
    QImage bmpRecolored = images->mBmp.convertToFormat(QImage::Format_ARGB32);
    images->mBmp = QImage();
    QRgb ruleColor = qRgb(255, 0, 0);
    QRgb treeColor = qRgb(47, 76, 64);
    for (int cy = 0; cy < images->mBmpVeg.height() / 300; cy++) {
        for (int cx = 0; cx < images->mBmpVeg.width() / 300; cx++) {
            QImage bmpVeg = images->mBmpVeg.copy(cx * 300, cy * 300, 300, 300).convertToFormat(QImage::Format_ARGB32);
            for (int y = 0; y < 300; y++) {
                for (int x = 0; x < 300; x++) {
                    if (bmpVeg.pixel(x, y) == ruleColor)
                        bmpRecolored.setPixel(cx * 300 + x, cy * 300 + y, treeColor);
                }
            }
        }
    }
#else
    QImage bmpRecolored(images->mBmp);
    for (int x = 0; x < images->mBmp.width(); x++) {
        for (int y = 0; y < images->mBmp.height(); y++) {
            if (images->mBmpVeg.pixel(x, y) == qRgb(255, 0, 0))
                bmpRecolored.setPixel(x, y, qRgb(47, 76, 64));
        }
    }
#endif

    delete images; // ***** ***** *****

    ImageData data;
    data.image = bmpRecolored.transformed(xform);
    data.scale = 1.0f;
    data.levelZeroBounds = QRectF(0, 0, imageSize.width() / 300, imageSize.height() / 300);
    data.valid = true;

    data.image.save(imageInfo.absoluteFilePath());
    writeImageData(imageDataInfo, data);

    return data;
}
#endif // WORLDED

#define IMAGE_DATA_MAGIC 0xB15B00B5
#define IMAGE_DATA_VERSION 4

MapImageManager::ImageData MapImageManager::readImageData(const QFileInfo &imageDataFileInfo)
{
    ImageData data;
    QFile file(imageDataFileInfo.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return data;

    QDataStream in(&file);

    quint32 magic;
    in >> magic;
    if (magic != IMAGE_DATA_MAGIC)
        return data;

    quint32 version;
    in >> version;
    if (version != IMAGE_DATA_VERSION)
        return data;

    in >> data.scale;

    qreal x, y, w, h;
    in >> x >> y >> w >> h;
    data.levelZeroBounds.setCoords(x, y, x + w, y + h);

    qint32 count;
    in >> count;
    for (int i = 0; i < count; i++) {
        QString source;
        in >> source;
        data.sources += source;
    }

    in >> data.missingTilesets;

    qint32 wid, hgt;
    in >> wid >> hgt;
    data.mapSize = QSize(wid, hgt);

    in >> wid >> hgt;
    data.tileSize = QSize(wid, hgt);

    // TODO: sanity-check the values
    data.valid = true;

    return data;
}

void MapImageManager::writeImageData(MapImageData imgData, MapImage* mapImage)
{
    ImageData data;
    data.image = mapImage->image();
    data.levelZeroBounds = mapImage->levelZeroBounds();
    data.scale = mapImage->scale();
    for (MapInfo *mapInfo : mapImage->sources())
    {
        data.sources += mapInfo->path();
    }
    data.missingTilesets = mapImage->isMissingTilesets();
    data.mapSize = imgData.mapSize;
    data.tileSize = imgData.tileSize;

    QFileInfo imageInfo = imageFileInfo(mapImage->mapInfo()->path());
    QFileInfo imageDataInfo = imageDataFileInfo(imageInfo);
    data.image.save(imageInfo.absoluteFilePath());
    writeImageData(imageDataInfo, data);
}

void MapImageManager::writeImageData(const QFileInfo &imageDataFileInfo, const MapImageManager::ImageData &data)
{
    QFile file(imageDataFileInfo.absoluteFilePath());
    if (!file.open(QIODevice::WriteOnly))
        return;

    QDataStream out(&file);
    out << quint32(IMAGE_DATA_MAGIC);
    out << quint32(IMAGE_DATA_VERSION);
    out.setVersion(QDataStream::Qt_4_0);
    out << data.scale;
    QRectF r = data.levelZeroBounds;
    out << r.x() << r.y() << r.width() << r.height();
    out << qint32(data.sources.length());
    for (QString source : data.sources)
    {
        out << source;
    }
    out << data.missingTilesets;
    out << (qint32)data.mapSize.width() << (qint32)data.mapSize.height();
    out << (qint32)data.tileSize.width() << (qint32)data.tileSize.height();
}

void MapImageManager::mapAboutToChange(MapAsset *mapInfo)
{
    // FIXME: need to wait or interrupt AssetTask_LoadMapImage
#if 0
    if (!mRenderMapComposite)
        return;
    // Caution: mRenderMapComposite is being used right now by the render thread.
    foreach (MapComposite *mc, mRenderMapComposite->maps()) {
        if (mc->mapInfo() == mapInfo) {
            mImageRenderThread->interrupt(true);
            MapImage *mapImage = mMapImages[mRenderMapComposite->mapInfo()->path()];
            Q_ASSERT(mapImage);
            mapImage->mLoaded = false;
            break;
        }
    }
#endif
}

void MapImageManager::mapChanged(MapAsset *mapInfo)
{
    // FIXME: need to wait or interrupt AssetTask_LoadMapImage
#if 0
    if (!mRenderMapComposite)
        return;
    // Caution: mRenderMapComposite is being used right now by the render thread.
    foreach (MapComposite *mc, mRenderMapComposite->maps()) {
        if (mc->mapInfo() == mapInfo) {
            MapImage *mapImage = mMapImages[mRenderMapComposite->mapInfo()->path()];
            Q_ASSERT(mapImage);
            mImageRenderThread->resume();
            QMetaObject::invokeMethod(mImageRenderWorker,
                                      "resume", Qt::QueuedConnection,
                                      Q_ARG(MapImage*,mapImage));
            break;
        }
    }
#endif
}

void MapImageManager::mapFileChanged(MapInfo *mapInfo)
{
    MapImageManagerDeferral deferral; // FIXME: optimized out?

    AssetTable assets = m_assets;
    for (Asset* asset : assets)
    {
        MapImage *mapImage = static_cast<MapImage*>(asset);
        if (mapImage->sources().contains(mapInfo)) {
            if (mapImage->isEmpty() == false) {
#if 0
                bool force = true;
                ImageData data = generateMapImage(mapImage->mapInfo()->path(), force);
                paintDummyImage(data, mapInfo);
                mapImage->mapFileChanged(data.image, data.scale,
                                         data.levelZeroBounds,
                                         data.mapSize, data.tileSize);
#endif
                recreateImage(mapImage->mapInfo()->path());
            }
        }
    }
}

void MapImageManager::imageLoadedByThread(QImage *image, MapImage *mapImage)
{
    mapImage->setImage(*image);
    onLoadingSucceeded(mapImage);
//    delete image;
}

void MapImageManager::imageRenderedByThread(MapImageData imgData, MapImage *mapImage)
{
    noise() << "imageRenderedByThread" << mapImage->mapInfo()->path();

    mapImage->mImage = imgData.image;
    mapImage->mLevelZeroBounds = imgData.levelZeroBounds;
    mapImage->mScale = imgData.scale;
    mapImage->mMissingTilesets = imgData.missingTilesets;
    mapImage->mSources = imgData.sources;
    mapImage->mMapSize = imgData.mapSize;
    mapImage->mTileSize = imgData.tileSize;
#if 0 // moved to thread
    ImageData data;
    data.image = mapImage->image();
    data.levelZeroBounds = mapImage->levelZeroBounds();
    data.scale = mapImage->scale();
    for (MapInfo *mapInfo : mapImage->sources())
        data.sources += mapInfo->path();
    data.missingTilesets = mapImage->isMissingTilesets();
    data.mapSize = imgData.mapSize;
    data.tileSize = imgData.tileSize;

    QFileInfo imageInfo = imageFileInfo(mapImage->mapInfo()->path());
    QFileInfo imageDataInfo = imageDataFileInfo(imageInfo);
    data.image.save(imageInfo.absoluteFilePath());
    writeImageData(imageDataInfo, data);
#endif
    onLoadingSucceeded(mapImage);
}

#include "mapobject.h"

QStringList getSubMapFileNames(const MapAsset *mapInfo)
{
    QStringList ret;
    const QString relativeTo = QFileInfo(mapInfo->path()).absolutePath();
    foreach (ObjectGroup *objectGroup, mapInfo->map()->objectGroups()) {
        foreach (MapObject *object, objectGroup->objects()) {
            if (object->name() == QLatin1String("lot") && !object->type().isEmpty()) {
                QString path = MapManager::instance().pathForMap(object->type(), relativeTo);
                if (!path.isEmpty())
                    ret += path;
            }
        }
    }
    return ret;
}

QFileInfo MapImageManager::imageFileInfo(const QString &mapFilePath)
{
    QFileInfo mapFileInfo(mapFilePath);
    QDir mapDir = mapFileInfo.absoluteDir();
    if (!mapDir.exists())
        return QFileInfo();
    QFileInfo imagesDirInfo(mapDir, QLatin1String(".pzeditor"));
    if (!imagesDirInfo.exists()) {
        if (!mapDir.mkdir(QLatin1String(".pzeditor")))
            return QFileInfo();
    }

    // Need to distinguish BMPToTMX image formats, so include .png or .bmp
    // in the file name.
    QString suffix;
    if (mapFileInfo.suffix() != QLatin1String("tmx"))
        suffix = QLatin1String("_") + mapFileInfo.suffix();

    return QFileInfo(imagesDirInfo.absoluteFilePath() + QLatin1Char('/') +
                     mapFileInfo.completeBaseName() + suffix + QLatin1String(".png"));
}

QFileInfo MapImageManager::imageDataFileInfo(const QFileInfo &imageFileInfo)
{
    return QFileInfo(imageFileInfo.absolutePath() + QLatin1Char('/') +
                     imageFileInfo.completeBaseName() + QLatin1String(".dat"));
}

void MapImageManager::deferThreadResults(bool defer)
{
    if (defer) {
        ++mDeferralDepth;
//        noise() << "MapImageManager::deferThreadResults depth++ =" << mDeferralDepth;
    } else {
        Q_ASSERT(mDeferralDepth > 0);
//        noise() << "MapImageManager::deferThreadResults depth-- =" << mDeferralDepth - 1;
        if (--mDeferralDepth == 0) {
        }
    }
}

void MapImageManager::processDeferrals()
{
    if (mDeferralDepth > 0 || mDeferredMapImages.isEmpty())
        return;
    QList<MapImage*> mapImages = mDeferredMapImages;
    mDeferredMapImages.clear();
    for (MapImage *mapImage : mapImages)
    {
        emit mapImageChanged(mapImage);
    }
}

class AssetTask_LoadMapImage : public BaseAsyncAssetTask
{
public:
    AssetTask_LoadMapImage(MapImage* asset)
        : BaseAsyncAssetTask(asset)
        , mMapImage(asset)
    {

    }

    void run() override
    {
        QString mapFilePath = m_asset->getPath().getString();

        switch (mState)
        {
        case State::Init:
            runInit(mapFilePath);
            break;
        case State::LoadedCache:
            break;
        case State::FailedCache:
            break;
        case State::WaitForMaps:
            break;
        case State::WaitForTilesets:
            break;
        case State::GenerateImage:
            runGenerateImage(mapFilePath);
            break;
        case State::GeneratedImage:
            break;
        }
    }

    void handleResult() override
    {
        QString mapFilePath = m_asset->getPath().getString();

        switch (mState)
        {
        case State::Init:
            Q_ASSERT(false);
            break;
        case State::LoadedCache:
            handleLoadedCache();
            break;
        case State::FailedCache:
            handleFailedCache(mapFilePath);
            break;
        case State::WaitForMaps:
            Q_ASSERT(false);
            break;
        case State::WaitForTilesets:
            Q_ASSERT(false);
            break;
        case State::GenerateImage:
            Q_ASSERT(false);
            break;
        case State::GeneratedImage:
            MapImageManager::instance().imageRenderedByThread(mMapImageData, mMapImage);
            break;
        }
    }

    void release() override
    {
        switch (mState)
        {
        case State::Init:
            break;
        case State::LoadedCache:
            delete this;
            break;
        case State::FailedCache:
            break;
        case State::WaitForMaps:
            break;
        case State::WaitForTilesets:
            break;
        case State::GenerateImage:
            break;
        case State::GeneratedImage:
            delete mMapComposite;
            delete this;
            break;
        }
    }

    void runInit(QString mapFilePath)
    {
        if (!bForce && tryCachedImageData(mapFilePath))
        {
            QString imageFileName = MapImageManager::instance().imageFileInfo(mapFilePath).canonicalFilePath();
            mImage = QImage(imageFileName);
#ifdef WORLDED
            if (!mImage.isNull())
                mImage = mImage.convertToFormat(QImage::Format_ARGB4444_Premultiplied);
#endif // WORLDED

#ifndef QT_NO_DEBUG
//        Sleep::msleep(250);
#endif

            mMapImage->mImage = mImageData.image;
            mMapImage->mScale = mImageData.scale;
            mMapImage->mLevelZeroBounds = mImageData.levelZeroBounds;
            mMapImage->mMapSize = mImageData.mapSize;
            mMapImage->mTileSize = mImageData.tileSize;
            mMapImage->mImageSize = mMapImage->mImage.size();

            mMapImage->mMissingTilesets = mMapImage->mMissingTilesets;

            mState = State::LoadedCache;
            bLoaded = true;
            return;
        }
        mState = State::FailedCache;
    }

    void runGenerateImage(QString mapFilePath)
    {
        mMapImageData = generateMapImage(mMapComposite);

        mMapImage->mImage = mMapImageData.image;
        mMapImage->mScale = mMapImageData.scale;
        mMapImage->mLevelZeroBounds = mMapImageData.levelZeroBounds;
        mMapImage->mMapSize = mMapImageData.mapSize;
        mMapImage->mTileSize = mMapImageData.tileSize;
        mMapImage->mImageSize = mMapImage->mImage.size();

        mMapImage->mMissingTilesets = mMapImageData.missingTilesets;

        // Set up file-modification tracking on each TMX that makes up this image.
        mMapImage->setSources(mMapImageData.sources);

        MapImageManager::instance().writeImageData(mMapImageData, mMapImage);

        mState = State::GeneratedImage;
        bLoaded = true;
    }

    void handleLoadedCache()
    {
        // Set up file modification tracking on each TMX that makes
        // up this image.
        QList<MapInfo*> sources;
        for (QString source : mImageData.sources)
        {
            if (MapInfo *sourceInfo = MapInfoManager::instance().mapInfo(source))
            {
                sources += sourceInfo;
            }
        }
        mMapImage->setSources(sources);

        MapImageManager::instance().imageLoadedByThread(&mImage, mMapImage);
    }

    void handleFailedCache(QString mapFilePath)
    {
        Q_ASSERT(mMapComposite == nullptr);
        bool asynch = true;
        mMapAsset = MapManager::instance().loadMap(mapFilePath, QString(), asynch, MapManager::PriorityLow);
        if (mMapAsset == nullptr)
        {
            // The map file went away since MapImage's MapInfo was created.
            emit MapImageManager::instance().mapImageFailedToLoad(mMapImage);
            // FIXME: delete this task
            return;
        }

//        Q_ASSERT(mMapAsset == mMapImage->mapInfo());
        mState = State::WaitForMaps;
        MapImageManager::instance().mWaitingTasks << this;
        if (mMapAsset->isEmpty())
        {
        }
        else
        {
            mapLoaded(mMapAsset);
        }
    }

    void handleWaitForMaps()
    {
        // mapLoaded() will update the state eventually
    }

    void handleWaitForTilesets()
    {
        QList<Tileset*> usedTilesets = mMapComposite->usedTilesets();
        usedTilesets.removeAll(TilesetManager::instance().missingTileset());
        for (Tileset* tileset : usedTilesets)
        {
            if (tileset->isEmpty())
            {
                return;
            }
        }

        // BmpBlender sends a signal to the MapComposite when it has finished
        // blending.  That needs to happen in the render thread.
        Q_ASSERT(mMapComposite->bmpBlender()->parent() == mMapComposite);
//        mMapComposite->moveToThread(mImageRenderThread);
        // Since we can't use moveToThread() from whatever thread the job runs on, we'll handle the result of the BmpBlender signal manually.
        mMapComposite->bmpBlender()->disconnect(mMapComposite);

        mState = State::GenerateImage;
    }

    void mapLoaded(MapAsset *mapInfo)
    {
        Q_ASSERT(mMapAsset != nullptr);
        if (mMapAsset == mapInfo)
        {
#ifdef WORLDED
            MapManager::instance().addReferenceToMap(mapInfo);
            mReferencedMaps += mapInfo;
#endif
            for (const QString &path : getSubMapFileNames(mapInfo))
            {
                bool async = true;
                if (MapAsset *subMapInfo = MapManager::instance().loadMap(path, QString(), async,
                                                                          MapManager::PriorityLow))
                {
                    if (!mExpectSubMaps.contains(subMapInfo))
                    {
                        if (subMapInfo->isEmpty())
                            mExpectSubMaps += subMapInfo;
#ifdef WORLDED
                        else
                        {
                            MapManager::instance().addReferenceToMap(subMapInfo);
                            mReferencedMaps += subMapInfo;
                        }
#endif
                    }
                }
            }
        }
        else if (mExpectSubMaps.contains(mapInfo))
        {
#ifdef WORLDED
            MapManager::instance().addReferenceToMap(mapInfo);
            mReferencedMaps += mapInfo;
#endif
            mExpectSubMaps.removeAll(mapInfo);
            for (const QString &path : getSubMapFileNames(mapInfo))
            {
                bool async = true;
                if (MapAsset *subMapInfo = MapManager::instance().loadMap(path, QString(), async, MapManager::PriorityLow))
                {
                    if (!mExpectSubMaps.contains(subMapInfo))
                    {
                        if (subMapInfo->isEmpty())
                            mExpectSubMaps += subMapInfo;
#ifdef WORLDED
                        else
                        {
                            MapManager::instance().addReferenceToMap(subMapInfo);
                            mReferencedMaps += subMapInfo;
                        }
#endif
                    }
                }
            }
        }
        else
        {
            return;
        }

        if (!mExpectSubMaps.isEmpty())
        {
            return;
        }

        allMapsLoaded();
    }

    void mapFailedToLoad(MapAsset *mapInfo)
    {
        // Failing to load a submap of the one we want to paint doesn't stop us
        // creating the map image.
        if (mExpectSubMaps.contains(mapInfo))
            mExpectSubMaps.removeAll(mapInfo);

        // The top-level map failed to load, don't create an image.
        if (mapInfo == mMapAsset)
        {
#ifdef WORLDED
            for (MapAsset *mapInfo : mReferencedMaps)
            {
                MapManager::instance().removeReferenceToMap(mapInfo);
            }
            mReferencedMaps.clear();
#endif
            MapImageManager::instance().mWaitingTasks.removeAll(this);

            MapImage *mapImage = mMapImage;
            release();

//            mapImage->mImage.fill(Qt::transparent);
            // FIXME: delete bogus MapImage???
            MapImageManager::instance().onLoadingFailed(mapImage);

            emit MapImageManager::instance().mapImageFailedToLoad(mapImage);
        }
    }

    void allMapsLoaded()
    {
        mMapComposite = new MapComposite(mMapAsset);
        Q_ASSERT(mMapComposite->waitingForMapsToLoad() == false);
#ifdef WORLDED
        // Now that mapComposite is referencing the maps...
        for (MapAsset *mapInfo : mReferencedMaps)
        {
            MapManager::instance().removeReferenceToMap(mapInfo);
        }
#endif
        mState = State::WaitForTilesets;
        handleWaitForTilesets();
    }

    bool tryCachedImageData(QString mapFilePath)
    {
        QFileInfo fileInfo(mapFilePath);
        QFileInfo imageInfo = MapImageManager::instance().imageFileInfo(mapFilePath);
        QFileInfo imageDataInfo = MapImageManager::instance().imageDataFileInfo(imageInfo);
        if (imageInfo.exists() && imageDataInfo.exists() && (fileInfo.lastModified() < imageInfo.lastModified()))
        {
            QImageReader reader(imageInfo.absoluteFilePath());
 /*
            if (!reader.size().isValid())
                QMessageBox::warning(MainWindow::instance(), tr("Error Loading Image"),
                                     tr("An error occurred trying to read a map thumbnail image.\n") + imageInfo.absoluteFilePath());
*/
            if (reader.size().width() == IMAGE_WIDTH)
            {
                MapImageManager::ImageData data = MapImageManager::instance().readImageData(imageDataInfo);
                // If the image was originally created with some tilesets missing,
                // try to recreate the image in case those tileset issues were
                // resolved.
                if (data.missingTilesets)
                {
                    data.valid = false;
                }
                if (data.valid)
                {
                    for (QString source : data.sources)
                    {
                        QFileInfo sourceInfo(source);
                        if (sourceInfo.exists() && (sourceInfo.lastModified() > imageInfo.lastModified()))
                        {
                            data.valid = false;
                            break;
                        }
                    }
                }
                if (data.valid)
                {
                    data.size = reader.size();
                    mImageData = data;
                    return true;
                }
            }
        }
        return false;
    }

    MapImageData generateMapImage(MapComposite *mapComposite)
    {
        Map *map = mapComposite->map();

        MapRenderer *renderer = nullptr;

        switch (map->orientation()) {
        case Map::Isometric:
            renderer = new IsometricRenderer(map);
            renderer->set2x(true);
            break;
        case Map::LevelIsometric:
            renderer = new ZLevelRenderer(map);
            break;
        case Map::Orthogonal:
            renderer = new OrthogonalRenderer(map);
            break;
        case Map::Staggered:
            renderer = new StaggeredRenderer(map);
            break;
        default:
            return MapImageData();
        }

//        renderer->mAbortDrawing = workerThread()->var();

        // Don't draw empty levels
        int maxLevel = 0;
        for (CompositeLayerGroup *layerGroup : mapComposite->sortedLayerGroups())
        {
            if (!layerGroup->bounds().isEmpty())
            {
                maxLevel = layerGroup->level();
            }
        }
        renderer->setMaxLevel(maxLevel);

        for (MapComposite *mc : mapComposite->maps())
        {
            if (mc->bmpBlender())
            {
                mc->bmpBlender()->flush(QRect(0, 0, mc->map()->width() - 1, mc->map()->height() - 1));
            }
        }

        // HACK: MapComposite::bmpBlenderLayersRecreated() would be called by a signal but this is a different thread.
       mapComposite->layerGroupForLevel(0)->setBmpBlendLayers(mapComposite->bmpBlender()->tileLayers());

        QRectF sceneRect = mapComposite->boundingRect(renderer);
        QSize mapSize = sceneRect.size().toSize();
        if (mapSize.isEmpty())
            return MapImageData();

        qreal scale = IMAGE_WIDTH / qreal(mapSize.width());
        mapSize *= scale;

        QImage image(mapSize, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        QPainter painter(&image);

        painter.setRenderHints(QPainter::SmoothPixmapTransform |
                               QPainter::HighQualityAntialiasing);
        painter.setTransform(QTransform::fromScale(scale, scale).translate(-sceneRect.left(), -sceneRect.top()));

        for (MapComposite::ZOrderItem zo : mapComposite->zOrder())
        {
            if (zo.group)
            {
                renderer->drawTileLayerGroup(&painter, zo.group);
            }
            else if (TileLayer *tl = zo.layer->asTileLayer())
            {
                if (tl->name().contains(QLatin1String("NoRender")))
                    continue;
                renderer->drawTileLayer(&painter, tl);
            }
            if (aborted())
            {
                painter.end();
                delete renderer;
                return MapImageData();
            }
        }

        MapImageData data;
#ifdef WORLDED
        data.image = image.convertToFormat(QImage::Format_ARGB4444_Premultiplied);
#else
        data.image = image;
#endif
        data.scale = scale;
        data.levelZeroBounds = renderer->boundingRect(QRect(0, 0, map->width(), map->height()));
        data.levelZeroBounds.translate(-sceneRect.topLeft());
        for (MapComposite *mc : mapComposite->maps())
            data.sources += mc->mapInfo();

        for (MapComposite *mc : mapComposite->maps())
        {
            if (mc->map()->hasUsedMissingTilesets())
            {
                data.missingTilesets = true;
                break;
            }
        }

        data.mapSize = map->size();
        data.tileSize = renderer->boundingRect(QRect(0, 0, 1, 1)).size();

        painter.end();
        delete renderer;

        return data;
    }

    void processWaitingTask()
    {
        QString mapFilePath = m_asset->getPath().getString();

        switch (mState)
        {
        case State::Init:
            Q_ASSERT(false);
            break;
        case State::LoadedCache:
            Q_ASSERT(false);
            break;
        case State::FailedCache:
            Q_ASSERT(false);
            break;
        case State::WaitForMaps:
            handleWaitForMaps();
            break;
        case State::WaitForTilesets:
            handleWaitForTilesets();
            break;
        case State::GenerateImage:
            MapImageManager::instance().mWaitingTasks.removeAll(this);
            AssetTaskManager::instance().submit(this);
            break;
        case State::GeneratedImage:
            Q_ASSERT(false);
            break;
        }
    }

    bool aborted()
    {
        // TODO
        return false;
    }

    enum struct State
    {
        Init,
        LoadedCache,
        FailedCache,
        WaitForMaps,
        WaitForTilesets,
        GenerateImage,
        GeneratedImage,
    };

    MapImage* mMapImage;
    bool bForce = false;
    State mState = State::Init;

    MapImageManager::ImageData mImageData;

    MapAsset* mMapAsset = nullptr;
    MapComposite* mMapComposite = nullptr;
    MapImageData mMapImageData;
    QList<MapAsset*> mExpectSubMaps;
#ifdef WORLDED
    QList<MapAsset*> mReferencedMaps;
#endif

    QImage mImage;
    bool bLoaded = false;
};

void MapImageManager::processWaitingTasks()
{
    QList<AssetTask*> waiting(mWaitingTasks);
    for (AssetTask* task : waiting)
    {
        static_cast<AssetTask_LoadMapImage*>(task)->processWaitingTask();
    }
}

void MapImageManager::mapLoaded(MapAsset *mapInfo)
{
    for (AssetTask* task : mWaitingTasks)
    {
        static_cast<AssetTask_LoadMapImage*>(task)->mapLoaded(mapInfo);
    }
}

void MapImageManager::mapFailedToLoad(MapAsset *mapInfo)
{
    for (AssetTask* task : mWaitingTasks)
    {
        static_cast<AssetTask_LoadMapImage*>(task)->mapFailedToLoad(mapInfo);
    }
}

Asset *MapImageManager::createAsset(AssetPath path, AssetParams *params)
{
    return new MapImage(path, this/*, params*/);
}

void MapImageManager::destroyAsset(Asset *asset)
{

}

void MapImageManager::startLoading(Asset *asset)
{
    MapImage* mapImage = static_cast<MapImage*>(asset);

    if (mapImage->mInfo == nullptr)
    {
        QString mapFilePath = asset->getPath().getString();
        mapImage->mInfo = MapInfoManager::instance().mapInfo(mapFilePath);
        if (mapImage->mapInfo() != nullptr)
            mapImage->addDependency(mapImage->mInfo);
    }

    AssetTask_LoadMapImage* assetTask = new AssetTask_LoadMapImage(mapImage);
    setTask(asset, assetTask);
    AssetTaskManager::instance().submit(assetTask);
}

void MapImageManager::onStateChanged(AssetState old_state, AssetState new_state, Asset *asset)
{
    AssetManager::onStateChanged(old_state, new_state, asset);

    // MapImage depends on MapInfo.  This handles MapInfo becoming READY.
    MapImage* mapImage = static_cast<MapImage*>(asset);
    if (new_state == AssetState::READY)
    {
        if (mDeferralDepth > 0)
            mDeferredMapImages += mapImage;
        else
            emit mapImageChanged(mapImage);
    }
}

///// ///// ///// ///// /////

/////

#if 0

MapImageReaderWorker::MapImageReaderWorker(InterruptibleThread *thread) :
    BaseWorker(thread)
{
}

MapImageReaderWorker::~MapImageReaderWorker()
{
}

void MapImageReaderWorker::work()
{
    IN_WORKER_THREAD

    while (mJobs.size()) {

        if (aborted()) {
            mJobs.clear();
            return;
        }

        Job job = mJobs.takeAt(0);

        QImage *image = new QImage(job.imageFileName);
#ifdef WORLDED
        if (!image->isNull())
            *image = image->convertToFormat(QImage::Format_ARGB4444_Premultiplied);
#endif // WORLDED

#ifndef QT_NO_DEBUG
        Sleep::msleep(250);
#endif
        emit imageLoaded(image, job.mapImage);
    }
}

void MapImageReaderWorker::addJob(const QString &imageFileName, MapImage *mapImage)
{
    IN_WORKER_THREAD

    mJobs += Job(imageFileName, mapImage);
    scheduleWork();
}

#endif

/////

#if 0

MapImageRenderWorker::MapImageRenderWorker(InterruptibleThread *thread) :
    BaseWorker(thread)
{
}

MapImageRenderWorker::~MapImageRenderWorker()
{
}

void MapImageRenderWorker::work()
{
    IN_WORKER_THREAD

    while (mJobs.size()) {
        if (aborted()) {
            return;
        }

        if (!mJobs.at(0).mapComposite) {
            preventWork(); // until mapLoaded() or mapFailedToLoad()
            emit mapNeeded(mJobs.at(0).mapImage);
            return;
        }

        Job job = mJobs.takeFirst();

        noise() << "MapImageRenderWorker started" << job.mapImage->mapInfo()->path();
#ifndef QT_NO_DEBUG
//        Sleep::msleep(1000);
#endif
        MapImageData data = generateMapImage(job.mapComposite);
        noise() << "MapImageRenderWorker" << (aborted() ? "aborted" : "finished") << job.mapImage->mapInfo()->path();

        emit jobDone(job.mapComposite); // main thread needs to delete this

        if (data.valid())
            emit imageRendered(data, job.mapImage);
    }
}

void MapImageRenderWorker::addJob(MapImage *mapImage)
{
    IN_WORKER_THREAD

    mJobs += Job(mapImage);
    scheduleWork();
}

void MapImageRenderWorker::mapLoaded(MapComposite *mapComposite)
{
    IN_WORKER_THREAD

    foreach (CompositeLayerGroup *layerGroup, mapComposite->sortedLayerGroups()) {
        foreach (TileLayer *tl, layerGroup->layers()) {
            bool isVisible = true;
            if (tl->name().contains(QLatin1String("NoRender")))
                isVisible = false;
            layerGroup->setLayerVisibility(tl, isVisible);
            layerGroup->setLayerOpacity(tl, 1.0f);
        }
        layerGroup->synch();
    }

    Q_ASSERT(mJobs.size());
    Q_ASSERT(mJobs[0].mapComposite == nullptr);
    mJobs[0].mapComposite = mapComposite;
    allowWork();
    scheduleWork();
}

void MapImageRenderWorker::mapFailedToLoad()
{
    IN_WORKER_THREAD

    mJobs.takeFirst();
    allowWork();
    scheduleWork();
}

void MapImageRenderWorker::resume(MapImage *mapImage)
{
    IN_WORKER_THREAD

    mJobs.prepend(Job(mapImage));
    scheduleWork();
}

MapImageData MapImageRenderWorker::generateMapImage(MapComposite *mapComposite)
{
    Map *map = mapComposite->map();

    MapRenderer *renderer = nullptr;

    switch (map->orientation()) {
    case Map::Isometric:
        renderer = new IsometricRenderer(map);
        break;
    case Map::LevelIsometric:
        renderer = new ZLevelRenderer(map);
        break;
    case Map::Orthogonal:
        renderer = new OrthogonalRenderer(map);
        break;
    case Map::Staggered:
        renderer = new StaggeredRenderer(map);
        break;
    default:
        return MapImageData();
    }

    renderer->mAbortDrawing = workerThread()->var();

    // Don't draw empty levels
    int maxLevel = 0;
    foreach (CompositeLayerGroup *layerGroup, mapComposite->sortedLayerGroups()) {
        if (!layerGroup->bounds().isEmpty())
            maxLevel = layerGroup->level();
    }
    renderer->setMaxLevel(maxLevel);

    foreach (MapComposite *mc, mapComposite->maps())
        if (mc->bmpBlender())
            mc->bmpBlender()->flush(QRect(0, 0, mc->map()->width() - 1, mc->map()->height() - 1));

    QRectF sceneRect = mapComposite->boundingRect(renderer);
    QSize mapSize = sceneRect.size().toSize();
    if (mapSize.isEmpty())
        return MapImageData();

    qreal scale = IMAGE_WIDTH / qreal(mapSize.width());
    mapSize *= scale;

    QImage image(mapSize, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter painter(&image);

    painter.setRenderHints(QPainter::SmoothPixmapTransform |
                           QPainter::HighQualityAntialiasing);
    painter.setTransform(QTransform::fromScale(scale, scale).translate(-sceneRect.left(), -sceneRect.top()));

    foreach (MapComposite::ZOrderItem zo, mapComposite->zOrder()) {
        if (zo.group) {
            renderer->drawTileLayerGroup(&painter, zo.group);
        } else if (TileLayer *tl = zo.layer->asTileLayer()) {
            if (tl->name().contains(QLatin1String("NoRender")))
                continue;
            renderer->drawTileLayer(&painter, tl);
        }
        if (aborted()) {
            painter.end();
            delete renderer;
            return MapImageData();
        }
    }

    MapImageData data;
#ifdef WORLDED
    data.image = image.convertToFormat(QImage::Format_ARGB4444_Premultiplied);
#else
    data.image = image;
#endif
    data.scale = scale;
    data.levelZeroBounds = renderer->boundingRect(QRect(0, 0, map->width(), map->height()));
    data.levelZeroBounds.translate(-sceneRect.topLeft());
    foreach (MapComposite *mc, mapComposite->maps())
        data.sources += mc->mapInfo();

    foreach (MapComposite *mc, mapComposite->maps()) {
        if (mc->map()->hasUsedMissingTilesets()) {
            data.missingTilesets = true;
            break;
        }
    }

    data.mapSize = map->size();
    data.tileSize = renderer->boundingRect(QRect(0, 0, 1, 1)).size();

    painter.end();
    delete renderer;

    return data;
}

MapImageRenderWorker::Job::Job(MapImage *mapImage) :
    mapComposite(nullptr),
    mapImage(mapImage)
{
}

#endif
