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

#include "bmptotmx.h"
#include "imagelayer.h"
#include "isometricrenderer.h"
#include "mainwindow.h"
#include "map.h"
#include "mapcomposite.h"
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

using namespace Tiled;
using namespace Tiled::Internal;

const int IMAGE_WIDTH = 512;

MapImageManager *MapImageManager::mInstance = NULL;

MapImageManager::MapImageManager() :
    QObject(),
    mExpectMapImage(0),
    mDeferralDepth(0),
    mDeferralQueued(false)
{
    mImageReaderThreads.resize(4);
    mImageReaderWorkers.resize(mImageReaderThreads.size());
    mNextThreadForJob = 0;
    for (int i = 0; i < mImageReaderWorkers.size(); i++) {
        mImageReaderThreads[i] = new InterruptibleThread;
        mImageReaderWorkers[i] = new MapImageReaderWorker;
        mImageReaderWorkers[i]->moveToThread(mImageReaderThreads[i]);
        connect(mImageReaderWorkers[i], SIGNAL(imageLoaded(QImage*,MapImage*)),
                SLOT(imageLoadedByThread(QImage*,MapImage*)));
        mImageReaderThreads[i]->start();
    }

    mImageRenderThread = new InterruptibleThread;
    mImageRenderWorker = new MapImageRenderWorker(mImageRenderThread->var());
    mImageRenderWorker->moveToThread(mImageRenderThread);
    connect(mImageRenderWorker, SIGNAL(mapNeeded(MapImage*)),
            SLOT(renderThreadNeedsMap(MapImage*)));
    qRegisterMetaType<MapImageData>("MapImageData");
    qRegisterMetaType<MapImage*>("MapImage*");
    qRegisterMetaType<MapComposite*>("MapComposite*");
    connect(mImageRenderWorker,
            SIGNAL(imageRendered(MapImageData,MapImage*)),
            SLOT(imageRenderedByThread(MapImageData,MapImage*)));
    connect(mImageRenderWorker, SIGNAL(jobDone(MapComposite*)),
            SLOT(renderJobDone(MapComposite*)));
    mImageRenderThread->start();

    connect(MapManager::instance(), SIGNAL(mapFileChanged(MapInfo*)),
            SLOT(mapFileChanged(MapInfo*)));
    connect(MapManager::instance(), SIGNAL(mapLoaded(MapInfo*)),
            SLOT(mapLoaded(MapInfo*)));
    connect(MapManager::instance(), SIGNAL(mapFailedToLoad(MapInfo*)),
            SLOT(mapFailedToLoad(MapInfo*)));
}

MapImageManager::~MapImageManager()
{
    for (int i = 0; i < mImageReaderThreads.size(); i++) {
        mImageReaderThreads[i]->interrupt();
        mImageReaderThreads[i]->quit();
        mImageReaderThreads[i]->wait();
        delete mImageReaderWorkers[i];
        delete mImageReaderThreads[i];
    }

    mImageRenderThread->interrupt();
    mImageRenderThread->quit();
    mImageRenderThread->wait();
    delete mImageRenderWorker;
    delete mImageRenderThread;
}

MapImageManager *MapImageManager::instance()
{
    if (mInstance == NULL)
        mInstance = new MapImageManager;
    return mInstance;
}

void MapImageManager::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

MapImage *MapImageManager::getMapImage(const QString &mapName, const QString &relativeTo)
{
    // Do not emit mapImageChanged as a result of worker threads finishing
    // loading any images while we are creating a new thumbnail image.
    // Any time QCoreApplication::processEvents() gets called (as is done
    // by MapManager's EditorMapReader class and the PROGRESS class) a
    // worker-thread's signal to us may be processed.
    MapImageManagerDeferral deferral;

#if 1
    QString suffix = QFileInfo(mapName).suffix();
    if (BMPToTMX::supportedImageFormats().contains(suffix)) {
        QString keyName = QFileInfo(mapName).canonicalFilePath();
        if (mMapImages.contains(keyName))
            return mMapImages[keyName];
        ImageData data = generateBMPImage(mapName);
        if (!data.valid)
            return 0;
        // Abusing the MapInfo struct
        MapInfo *mapInfo = new MapInfo(Map::Isometric,
                                       data.levelZeroBounds.width(),
                                       data.levelZeroBounds.height(), 1, 1);
        MapImage *mapImage = new MapImage(data.image, data.scale,
                                          data.levelZeroBounds, mapInfo);
        mapImage->mLoaded = true;
        mMapImages[keyName] = mapImage;
        return mapImage;
    }
#endif

    QString mapFilePath = MapManager::instance()->pathForMap(mapName, relativeTo);
    if (mapFilePath.isEmpty())
        return 0;

    if (mMapImages.contains(mapFilePath))
        return mMapImages[mapFilePath];

    ImageData data = generateMapImage(mapFilePath);
    if (!data.valid)
        return 0;

    MapInfo *mapInfo = MapManager::instance()->mapInfo(mapFilePath);
    if (data.threadLoad || data.threadRender)
        paintDummyImage(data, mapInfo);
    MapImage *mapImage = new MapImage(data.image, data.scale, data.levelZeroBounds, mapInfo);
    mapImage->mMissingTilesets = data.missingTilesets;
    mapImage->mLoaded = !(data.threadLoad || data.threadRender);

    if (data.threadLoad || data.threadRender) {
        if (data.threadLoad) {
            QString imageFileName = imageFileInfo(mapFilePath).canonicalFilePath();
            QMetaObject::invokeMethod(mImageReaderWorkers[mNextThreadForJob],
                                      "addJob", Qt::QueuedConnection,
                                      Q_ARG(QString,imageFileName),
                                      Q_ARG(MapImage*,mapImage));
            mNextThreadForJob = (mNextThreadForJob + 1) % mImageReaderWorkers.size();
        }
        if (data.threadRender) {
            QMetaObject::invokeMethod(mImageRenderWorker,
                                      "addJob", Qt::QueuedConnection,
                                      Q_ARG(MapImage*,mapImage));
        }
    }

    // Set up file modification tracking on each TMX that makes
    // up this image.
    QList<MapInfo*> sources;
    foreach (QString source, data.sources)
        if (MapInfo *sourceInfo = MapManager::instance()->mapInfo(source))
            sources += sourceInfo;
    mapImage->setSources(sources);

    mMapImages.insert(mapFilePath, mapImage);
    return mapImage;
}

MapImageManager::ImageData MapImageManager::generateMapImage(const QString &mapFilePath, bool force)
{
#if 0
    if (mapFilePath == QLatin1String("<fail>")) {
        QImage image(IMAGE_WIDTH, 256, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setFont(QFont(QLatin1String("Helvetica"), 48, 1, true));
        painter.drawText(0, 0, image.width(), image.height(), Qt::AlignCenter, QLatin1String("FAIL"));
        return image;
    }
#endif

    QFileInfo fileInfo(mapFilePath);
    QFileInfo imageInfo = imageFileInfo(mapFilePath);
    QFileInfo imageDataInfo = imageDataFileInfo(imageInfo);
    if (!force && imageInfo.exists() && imageDataInfo.exists() && (fileInfo.lastModified() < imageInfo.lastModified())) {
        QImageReader reader(imageInfo.absoluteFilePath());
        if (!reader.size().isValid())
            QMessageBox::warning(MainWindow::instance(), tr("Error Loading Image"),
                                 tr("An error occurred trying to read a map thumbnail image.\n") + imageInfo.absoluteFilePath());
        if (reader.size().width() == IMAGE_WIDTH) {
            ImageData data = readImageData(imageDataInfo);
            // If the image was originally created with some tilesets missing,
            // try to recreate the image in case those tileset issues were
            // resolved.
            if (data.missingTilesets)
                data.valid = false;
            if (data.valid) {
                foreach (QString source, data.sources) {
                    QFileInfo sourceInfo(source);
                    if (sourceInfo.exists() && (sourceInfo.lastModified() > imageInfo.lastModified())) {
                        data.valid = false;
                        break;
                    }
                }
            }
            if (data.valid) {
                data.threadLoad = true;
                data.size = reader.size();
                return data;
            }
        }
    }

#if 1
    // Create ImageData appropriate for paintDummyImage().
    MapInfo *mapInfo = MapManager::instance()->mapInfo(mapFilePath);
    if (!mapInfo) {
        mError = MapManager::instance()->errorString();
        return ImageData();
    }

    MapRenderer *renderer = NULL;
    Map staticMap(mapInfo->orientation(), mapInfo->width(), mapInfo->height(),
            mapInfo->tileWidth(), mapInfo->tileHeight());
    Map *map = &staticMap;

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
        return ImageData();
    }

    QRectF sceneRect = renderer->boundingRect(QRect(0, 0, map->width(), map->height()));
    QSize mapSize = sceneRect.size().toSize();
    if (mapSize.isEmpty())
        return ImageData();

    qreal scale = IMAGE_WIDTH / qreal(mapSize.width());
    mapSize *= scale;

    ImageData data;
    data.threadRender = true;
    data.size = mapSize;
    data.scale = scale;
    data.levelZeroBounds = sceneRect;
//    data.sources = mapComposite->getMapFileNames();
    data.valid = true;
    return data;
#else
    PROGRESS progress(tr("Generating thumbnail for %1").arg(fileInfo.completeBaseName()));

    MapInfo *mapInfo = MapManager::instance()->loadMap(mapFilePath);
    if (!mapInfo) {
        mError = MapManager::instance()->errorString();
        return ImageData(); // TODO: Add error handling
    }

    progress.update(tr("Generating thumbnail for %1").arg(fileInfo.completeBaseName()));

    MapComposite mapComposite(mapInfo);

    // Wait for TilesetManager's threads to finish loading the tilesets.
    QSet<Tileset*> usedTilesets;
    foreach (MapComposite *mc, mapComposite.maps()) {
        foreach (TileLayer *tl, mc->map()->tileLayers())
            usedTilesets += tl->usedTilesets();
    }
    usedTilesets.remove(TilesetManager::instance()->missingTileset());
    TilesetManager::instance()->waitForTilesets(usedTilesets.toList());

    ImageData data = generateMapImage(&mapComposite);

    foreach (MapComposite *mc, mapComposite.maps()) {
        if (mc->map()->hasUsedMissingTilesets()) {
            data.missingTilesets = true;
            break;
        }
    }

    data.image.save(imageInfo.absoluteFilePath());
    writeImageData(imageDataInfo, data);

    return data;
#endif
}

#if 0
MapImageManager::ImageData MapImageManager::generateMapImage(MapComposite *mapComposite)
{
    Map *map = mapComposite->map();

    MapRenderer *renderer = NULL;

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
        return ImageData();
    }

    mapComposite->saveVisibility();
    mapComposite->saveOpacity();
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

    // Don't draw empty levels
    int maxLevel = 0;
    foreach (CompositeLayerGroup *layerGroup, mapComposite->sortedLayerGroups()) {
        if (!layerGroup->bounds().isEmpty())
            maxLevel = layerGroup->level();
    }
    renderer->setMaxLevel(maxLevel);

    QRectF sceneRect = mapComposite->boundingRect(renderer);
    QSize mapSize = sceneRect.size().toSize();
    if (mapSize.isEmpty())
        return ImageData();

    qreal scale = IMAGE_WIDTH / qreal(mapSize.width());
    mapSize *= scale;

#if 1
#else
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
    }
#endif

    mapComposite->restoreVisibility();
    mapComposite->restoreOpacity();
    foreach (CompositeLayerGroup *layerGroup, mapComposite->sortedLayerGroups())
        layerGroup->synch();

    ImageData data;
#if 1
    data.threadRender = true;
    data.size = mapSize;
#else
    data.image = image;
#endif
    data.scale = scale;
    data.levelZeroBounds = renderer->boundingRect(QRect(0, 0, map->width(), map->height()));
    data.levelZeroBounds.translate(-sceneRect.topLeft());
    data.sources = mapComposite->getMapFileNames();
    data.valid = true;

    delete renderer;

    return data;
}
#endif

void MapImageManager::paintDummyImage(ImageData &data, MapInfo *mapInfo)
{
    Q_ASSERT(data.size.isValid());
    data.image = QImage(data.size, QImage::Format_ARGB32);
//        data.image.setColorTable(QVector<QRgb>() << qRgb(255,255,255));
    QPainter p(&data.image);
    QPolygonF poly;
    MapImage mapImage(data.image, data.scale, data.levelZeroBounds, mapInfo);
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

    QImage bmpRecolored(images->mBmp);
    for (int x = 0; x < images->mBmp.width(); x++) {
        for (int y = 0; y < images->mBmp.height(); y++) {
            if (images->mBmpVeg.pixel(x, y) == qRgb(255, 0, 0))
                bmpRecolored.setPixel(x, y, qRgb(47, 76, 64));
        }
    }

    delete images; // ***** ***** *****

    ImageData data;
    data.image = bmpRecolored.transformed(xform);;
    data.scale = 1.0f;
    data.levelZeroBounds = QRectF(0, 0, imageSize.width() / 300, imageSize.height() / 300);
    data.valid = true;

    data.image.save(imageInfo.absoluteFilePath());
    writeImageData(imageDataInfo, data);

    return data;
}

#define IMAGE_DATA_MAGIC 0xB15B00B5
#define IMAGE_DATA_VERSION 3

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

    // TODO: sanity-check the values
    data.valid = true;

    return data;
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
    foreach (QString source, data.sources)
        out << source;
    out << data.missingTilesets;
}

void MapImageManager::mapFileChanged(MapInfo *mapInfo)
{
    QMap<QString,MapImage*>::iterator it_begin = mMapImages.begin();
    QMap<QString,MapImage*>::iterator it_end = mMapImages.end();
    QMap<QString,MapImage*>::iterator it;

    MapImageManagerDeferral deferral;

    for (it = it_begin; it != it_end; it++) {
        MapImage *mapImage = it.value();
        if (mapImage->sources().contains(mapInfo)) {
            // When tilesets are added or removed from TileMetaInfoMgr,
            // MapManager generates the mapFileChanged signal for .tbx lots.
            // The generateMapImage() call below would then return the same
            // data for the image that was already loaded with image.size=0
            // to indicate we should load the image in a worker thread.
            // So force the image to be recreated in every case.
            bool force = true;
            ImageData data = generateMapImage(mapImage->mapInfo()->path(), force);
            if (!data.valid) {
                // We had a valid image, but now it's bogus, so blank it out.
                data.image = mapImage->image();
                data.image.fill(Qt::white);
                data.scale = mapImage->scale();
                data.levelZeroBounds = mapImage->levelZeroBounds();
                data.sources.clear();
                foreach (MapInfo *sourceInfo, mapImage->sources())
                    data.sources += sourceInfo->path();
            }
            if (/*data.threadLoad ||*/ data.threadRender) {
                paintDummyImage(data, mapInfo);
                QMetaObject::invokeMethod(mImageRenderWorker,
                                          "addJob", Qt::QueuedConnection,
                                          Q_ARG(MapImage*,mapImage));
            }
            mapImage->mapFileChanged(data.image, data.scale,
                                     data.levelZeroBounds);

            // Set up file modification tracking on each TMX that makes
            // up this image.
            QList<MapInfo*> sources;
            foreach (QString source, data.sources)
                if (MapInfo *sourceInfo = MapManager::instance()->mapInfo(source))
                    sources += sourceInfo;
            mapImage->setSources(sources);

            mapImage->mLoaded = !(data.threadLoad || data.threadRender);

            emit mapImageChanged(mapImage);
        }
    }
}

void MapImageManager::imageLoadedByThread(QImage *image, MapImage *mapImage)
{
    mapImage->setImage(*image);
    mapImage->mLoaded = true;
    delete image;

    if (mDeferralDepth > 0)
        mDeferredMapImages += mapImage;
    else
        emit mapImageChanged(mapImage);
}

void MapImageManager::renderThreadNeedsMap(MapImage *mapImage)
{
    bool asynch = true;
    Q_ASSERT(mExpectMapImage == 0);
    MapInfo *mapInfo = MapManager::instance()->loadMap(mapImage->mapInfo()->path(),
                                                       QString(), asynch);
    if (!mapInfo) {
        // The map file went away since MapImage's MapInfo was created.
        QMetaObject::invokeMethod(mImageRenderWorker,
                                  "mapFailedToLoad", Qt::QueuedConnection);
        emit mapImageFailedToLoad(mapImage);
        return;
    }
    mExpectMapImage = mapImage;
    mExpectSubMaps.clear();
    mReferencedMaps.clear();
    Q_ASSERT(mapInfo == mapImage->mapInfo());
    if (!mapInfo->isLoading())
        mapLoaded(mapInfo);
}

void MapImageManager::imageRenderedByThread(MapImageData imgData, MapImage *mapImage)
{
    qDebug() << "imageRenderedByThread" << mapImage->mapInfo()->path();

    mapImage->mImage = imgData.image;
    mapImage->mLevelZeroBounds = imgData.levelZeroBounds;
    mapImage->mScale = imgData.scale;
    mapImage->mMissingTilesets = imgData.missingTilesets;
    mapImage->mSources = imgData.sources;
    mapImage->mLoaded = true;

    ImageData data;
    data.image = mapImage->image();
    data.levelZeroBounds = mapImage->levelZeroBounds();
    data.scale = mapImage->scale();
    foreach (MapInfo *mapInfo, mapImage->sources())
        data.sources += mapInfo->path();
    data.missingTilesets = mapImage->isMissingTilesets();

    QFileInfo imageInfo = imageFileInfo(mapImage->mapInfo()->path());
    QFileInfo imageDataInfo = imageDataFileInfo(imageInfo);
    data.image.save(imageInfo.absoluteFilePath());
    writeImageData(imageDataInfo, data);

    if (mDeferralDepth > 0)
        mDeferredMapImages += mapImage;
    else
        emit mapImageChanged(mapImage);
}

void MapImageManager::renderJobDone(MapComposite *mapComposite)
{
    delete mapComposite;
}

#include "mapobject.h"
QStringList getSubMapFileNames(const MapInfo *mapInfo)
{
    QStringList ret;
    const QString relativeTo = QFileInfo(mapInfo->path()).absolutePath();
    foreach (ObjectGroup *objectGroup, mapInfo->map()->objectGroups()) {
        foreach (MapObject *object, objectGroup->objects()) {
            if (object->name() == QLatin1String("lot") && !object->type().isEmpty()) {
                QString path = MapManager::instance()->pathForMap(object->type(), relativeTo);
                if (!path.isEmpty())
                    ret += path;
            }
        }
    }
    return ret;
}

void MapImageManager::mapLoaded(MapInfo *mapInfo)
{
    if (!mExpectMapImage)
        return;

    if (mExpectMapImage->mapInfo() == mapInfo) {
        MapManager::instance()->addReferenceToMap(mapInfo), mReferencedMaps += mapInfo;
        foreach (const QString &path, getSubMapFileNames(mapInfo)) {
            bool async = true;
            if (MapInfo *subMapInfo = MapManager::instance()->loadMap(path, QString(), async)) {
                if (!mExpectSubMaps.contains(subMapInfo)) {
                    if (subMapInfo->isLoading())
                        mExpectSubMaps += subMapInfo;
                    else
                        MapManager::instance()->addReferenceToMap(subMapInfo), mReferencedMaps += subMapInfo;
                }
            }
        }
    } else if (mExpectSubMaps.contains(mapInfo)) {
        MapManager::instance()->addReferenceToMap(mapInfo), mReferencedMaps += mapInfo;
        mExpectSubMaps.removeAll(mapInfo);
        foreach (const QString &path, getSubMapFileNames(mapInfo)) {
            bool async = true;
            if (MapInfo *subMapInfo = MapManager::instance()->loadMap(path, QString(), async)) {
                if (!mExpectSubMaps.contains(subMapInfo)) {
                    if (subMapInfo->isLoading())
                        mExpectSubMaps += subMapInfo;
                    else
                        MapManager::instance()->addReferenceToMap(subMapInfo), mReferencedMaps += subMapInfo;
                }
            }
        }
        mapInfo = mExpectMapImage->mapInfo();
    } else {
        return;
    }

    if (mExpectSubMaps.size())
        return;

    mExpectMapImage = 0;

    // FIXME: load all the submaps needed for this rendering job.
    MapComposite *mapComposite = new MapComposite(mapInfo);

    // Now that mapComposite is referencing the maps...
    foreach (MapInfo *mapInfo, mReferencedMaps)
        MapManager::instance()->removeReferenceToMap(mapInfo);

    // Wait for TilesetManager's threads to finish loading the tilesets.
    // FIXME: this shouldn't block the gui.
    QSet<Tileset*> usedTilesets;
    foreach (MapComposite *mc, mapComposite->maps()) {
        foreach (TileLayer *tl, mc->map()->tileLayers())
            usedTilesets += tl->usedTilesets();
    }
    usedTilesets.remove(TilesetManager::instance()->missingTileset());
    TilesetManager::instance()->waitForTilesets(usedTilesets.toList());

    QMetaObject::invokeMethod(mImageRenderWorker,
                              "mapLoaded", Qt::QueuedConnection,
                              Q_ARG(MapComposite*,mapComposite));
}

void MapImageManager::mapFailedToLoad(MapInfo *mapInfo)
{
    // Failing to load a submap of the one we want to paint doesn't stop us
    // creating the map image.
    if (mExpectSubMaps.contains(mapInfo))
        mExpectSubMaps.removeAll(mapInfo);

    // The render thread was waiting for a map to load, but that failed.
    // Tell the render thread to continue on with the next job.
    if (mExpectMapImage && (mapInfo == mExpectMapImage->mapInfo())) {
        foreach (MapInfo *mapInfo, mReferencedMaps)
            MapManager::instance()->removeReferenceToMap(mapInfo);
        mReferencedMaps.clear();

        MapImage *mapImage = mExpectMapImage;
        mapImage->mImage.fill(Qt::transparent);
        mapImage->mLoaded = true; // FIXME: delete bogus MapImage???
        mExpectMapImage = 0;
        QMetaObject::invokeMethod(mImageRenderWorker,
                                  "mapFailedToLoad", Qt::QueuedConnection);
        emit mapImageFailedToLoad(mapImage);
    }
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
//        qDebug() << "MapImageManager::deferThreadResults depth++ =" << mDeferralDepth;
    } else {
        Q_ASSERT(mDeferralDepth > 0);
//        qDebug() << "MapImageManager::deferThreadResults depth-- =" << mDeferralDepth - 1;
        if (--mDeferralDepth == 0) {
            if (!mDeferralQueued && mDeferredMapImages.size()) {
                QMetaObject::invokeMethod(this, "processDeferrals", Qt::QueuedConnection);
                mDeferralQueued = true;
            }
        }
    }
}

void MapImageManager::processDeferrals()
{
    QList<MapImage*> mapImages = mDeferredMapImages;
    mDeferredMapImages.clear();
    mDeferralQueued = false;
    foreach (MapImage *mapImage, mapImages)
        emit mapImageChanged(mapImage);
}

///// ///// ///// ///// /////

MapImage::MapImage(QImage image, qreal scale, const QRectF &levelZeroBounds, MapInfo *mapInfo)
    : mImage(image)
    , mInfo(mapInfo)
    , mLevelZeroBounds(levelZeroBounds)
    , mScale(scale)
    , mMissingTilesets(false)
    , mLoaded(false)
{
}

QPointF MapImage::tileToPixelCoords(qreal x, qreal y)
{
    const int tileWidth = mInfo->tileWidth();
    const int tileHeight = mInfo->tileHeight();
    const int originX = mInfo->height() * tileWidth / 2;

    return QPointF((x - y) * tileWidth / 2 + originX,
                   (x + y) * tileHeight / 2);
}

QRectF MapImage::tileBoundingRect(const QRect &rect)
{
    const int tileWidth = mInfo->tileWidth();
    const int tileHeight = mInfo->tileHeight();

    const int originX = mInfo->height() * tileWidth / 2;
    const QPoint pos((rect.x() - (rect.y() + rect.height()))
                     * tileWidth / 2 + originX,
                     (rect.x() + rect.y()) * tileHeight / 2);

    const int side = rect.height() + rect.width();
    const QSize size(side * tileWidth / 2,
                     side * tileHeight / 2);

    return QRect(pos, size);
}

QRectF MapImage::bounds()
{
    int mapWidth = mInfo->width(), mapHeight = mInfo->height();
    return tileBoundingRect(QRect(0,0,mapWidth,mapHeight));
}

qreal MapImage::scale()
{
    return mScale;
}

QPointF MapImage::tileToImageCoords(qreal x, qreal y)
{
    QPointF pos = tileToPixelCoords(x, y);
    pos += mLevelZeroBounds.topLeft();
    return pos * scale();
}

void MapImage::mapFileChanged(QImage image, qreal scale, const QRectF &levelZeroBounds)
{
    mImage = image;
    mScale = scale;
    mLevelZeroBounds = levelZeroBounds;
}

/////

MapImageReaderWorker::MapImageReaderWorker() :
    BaseWorker(),
    mWorkPending(false)
{
}

MapImageReaderWorker::~MapImageReaderWorker()
{
}

void MapImageReaderWorker::work()
{
    IN_WORKER_THREAD

    mWorkPending = false;

    while (mJobs.size()) {

        if (aborted()) {
            mJobs.clear();
            return;
        }

        Job job = mJobs.takeAt(0);

        QImage *image = new QImage(job.imageFileName);

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
    if (!mWorkPending) {
        mWorkPending = true;
        QMetaObject::invokeMethod(this, "work", Qt::QueuedConnection);
    }
}

/////

MapImageRenderWorker::MapImageRenderWorker(bool *abortPtr) :
    BaseWorker(abortPtr),
    mWorkPending(false)
{
}

MapImageRenderWorker::~MapImageRenderWorker()
{
}

void MapImageRenderWorker::work()
{
    IN_WORKER_THREAD

    mWorkPending = false;

    while (mJobs.size()) {
        if (aborted()) {
            mJobs.clear();
            return;
        }

        if (!mJobs.at(0).mapComposite) {
            mWorkPending = true; // so addJob() doesn't resume work() before mapLoaded()
            emit mapNeeded(mJobs.at(0).mapImage);
            return;
        }

        Job job = mJobs.takeFirst();

        MapImageData data = generateMapImage(job.mapComposite);

        emit jobDone(job.mapComposite); // main thread needs to delete this

        if (data.valid())
            emit imageRendered(data, job.mapImage);
    }
}

void MapImageRenderWorker::addJob(MapImage *mapImage)
{
    IN_WORKER_THREAD

    mJobs += Job(mapImage);
    if (!mWorkPending) {
        mWorkPending = true;
        QMetaObject::invokeMethod(this, "work", Qt::QueuedConnection);
    }
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
    Q_ASSERT(mJobs[0].mapComposite == 0);
    mJobs[0].mapComposite = mapComposite;
    work();
}

void MapImageRenderWorker::mapFailedToLoad()
{
    IN_WORKER_THREAD

    mJobs.takeFirst();
    work();
}

MapImageData MapImageRenderWorker::generateMapImage(MapComposite *mapComposite)
{
    Map *map = mapComposite->map();

    MapRenderer *renderer = NULL;

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

    // Don't draw empty levels
    int maxLevel = 0;
    foreach (CompositeLayerGroup *layerGroup, mapComposite->sortedLayerGroups()) {
        if (!layerGroup->bounds().isEmpty())
            maxLevel = layerGroup->level();
    }
    renderer->setMaxLevel(maxLevel);

    QRectF sceneRect = mapComposite->boundingRect(renderer);
    QSize mapSize = sceneRect.size().toSize();
    if (mapSize.isEmpty())
        return MapImageData();

    qreal scale = IMAGE_WIDTH / qreal(mapSize.width());
    mapSize *= scale;

    QImage *image = new QImage(mapSize, QImage::Format_ARGB32);
    image->fill(Qt::transparent);
    QPainter painter(image);

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
            delete image;
            delete renderer;
            return MapImageData();
        }
    }

    MapImageData data;
    data.image = *image;
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

    delete renderer;

    return data;
}

MapImageRenderWorker::Job::Job(MapImage *mapImage) :
    mapComposite(0),
    mapImage(mapImage)
{
}
