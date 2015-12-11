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

#ifndef MAPIMAGEMANAGER_H
#define MAPIMAGEMANAGER_H

#include <QFileInfo>
#include <QImage>
#include <QMap>
#include <QObject>
#include <QStringList>

class MapComposite;
class MapInfo;

namespace Tiled {
class Map;
}

#include "threads.h"
class MapImage;
class MapImageReaderWorker : public BaseWorker
{
    Q_OBJECT
public:
    MapImageReaderWorker(InterruptibleThread *thread);

    ~MapImageReaderWorker();

signals:
    void imageLoaded(QImage *image, MapImage *mapImage);

public slots:
    void work();
    void addJob(const QString &imageFileName, MapImage *mapImage);

private:
    class Job {
    public:
        Job(const QString &imageFileName, MapImage *mapImage) :
            imageFileName(imageFileName),
            mapImage(mapImage)
        {
        }

        QString imageFileName;
        MapImage *mapImage;
    };
    QList<Job> mJobs;
};

class MapImageData
{
public:
    MapImageData() :
        scale(0),
        missingTilesets(false)
    {

    }

    bool valid() const { return !image.isNull(); }

    QImage image;
    QRectF levelZeroBounds;
    qreal scale;
    QList<MapInfo*> sources;
    bool missingTilesets;
    QSize mapSize;
    QSize tileSize;
};

#include <QMetaType>
Q_DECLARE_METATYPE(MapImageData) // for QueuedConnection

class MapImageRenderWorker : public BaseWorker
{
    Q_OBJECT
public:
    MapImageRenderWorker(InterruptibleThread *thread);

    ~MapImageRenderWorker();

signals:
    void mapNeeded(MapImage *mapImage);
    void imageRendered(MapImageData data, MapImage *mapImage);
    void jobDone(MapComposite *mapComposite);

public slots:
    void work();
    void addJob(MapImage *mapImage);
    void mapLoaded(MapComposite *mapComposite);
    void mapFailedToLoad();
    void resume(MapImage *mapImage);

private:
    MapImageData generateMapImage(MapComposite *mapComposite);

    class Job {
    public:
        Job(MapImage *mapImage);

        MapComposite *mapComposite;
        MapImage *mapImage;
    };
    QList<Job> mJobs;
};

class MapImage
{
public:
    MapImage(QImage image, qreal scale, const QRectF &levelZeroBounds, const QSize &mapSize, const QSize &tileSize, MapInfo *mapInfo);

    void setImage(const QImage &image) { mImage = image; }
    QImage image() const {return mImage; }
    MapInfo *mapInfo() const { return mInfo; }

    QPointF tileToPixelCoords(qreal x, qreal y);

    QRectF tileBoundingRect(const QRect &rect);

    QRectF bounds();

    qreal scale();

    QPointF tileToImageCoords(qreal x, qreal y);

    QPointF tileToImageCoords(const QPoint &pos)
    { return tileToImageCoords(pos.x(), pos.y()); }

    void mapFileChanged(QImage image, qreal scale, const QRectF &levelZeroBounds, const QSize &mapSize, const QSize &tileSize);

    void setSources(const QList<MapInfo*> &sources)
    { mSources = sources; }

    QList<MapInfo*> sources() const
    { return mSources; }

    QRectF levelZeroBounds() const
    { return mLevelZeroBounds; }

    bool isMissingTilesets() const { return mMissingTilesets; }

    QSize tileSize() const
    { return mTileSize; }

    bool isLoaded() const { return mLoaded; }

private:
    QImage mImage;
    MapInfo *mInfo;
    QRectF mLevelZeroBounds;
    qreal mScale;
    QList<MapInfo*> mSources;
    bool mMissingTilesets;
    QSize mMapSize;
    QSize mTileSize;
    bool mLoaded;

    friend class MapImageManager;
};

class MapImageManager : public QObject
{
    Q_OBJECT

public:
    static MapImageManager *instance();
    static void deleteInstance();

    MapImage *getMapImage(const QString &mapName, const QString &relativeTo = QString());

    QString errorString() const
    { return mError; }

protected:
    struct ImageData
    {
        ImageData() :
            scale(0),
            valid(false),
            missingTilesets(false),
            threadLoad(false),
            threadRender(false)
        {}
        qreal scale;
        QRectF levelZeroBounds;
        QImage image;
        bool valid;
        QStringList sources;
        bool missingTilesets;
        QSize mapSize;
        QSize tileSize;
        QSize size;

        bool threadLoad;
        bool threadRender;
    };

    ImageData generateMapImage(const QString &mapFilePath, bool force = false);
#if 0
    ImageData generateMapImage(MapComposite *mapComposite);
#endif
    void paintDummyImage(ImageData &data, MapInfo *mapInfo);

#ifdef WORLDED
    ImageData generateBMPImage(const QString &bmpFilePath);
#endif

    ImageData readImageData(const QFileInfo &imageDataFileInfo);
    void writeImageData(const QFileInfo &imageDataFileInfo, const ImageData &data);

signals:
    void mapImageChanged(MapImage *mapImage);
    void mapImageFailedToLoad(MapImage *mapImage);
    
private slots:
    void mapAboutToChange(MapInfo *mapInfo);
    void mapChanged(MapInfo *mapInfo);
    void mapFileChanged(MapInfo *mapInfo);

private slots:
    void imageLoadedByThread(QImage *image, MapImage *mapImage);

    void renderThreadNeedsMap(MapImage *mapImage);
    void imageRenderedByThread(MapImageData imgData, MapImage *mapImage);
    void renderJobDone(MapComposite *mapComposite);

    void mapLoaded(MapInfo *mapInfo);
    void mapFailedToLoad(MapInfo *mapInfo);

    void processDeferrals();

private:
    Q_DISABLE_COPY(MapImageManager)
    MapImageManager();
    ~MapImageManager();

    QFileInfo imageFileInfo(const QString &mapFilePath);
    QFileInfo imageDataFileInfo(const QFileInfo &imageFileInfo);

    QMap<QString,MapImage*> mMapImages;
    QString mError;

    QVector<InterruptibleThread*> mImageReaderThreads;
    QVector<MapImageReaderWorker*> mImageReaderWorkers;
    int mNextThreadForJob;

    InterruptibleThread *mImageRenderThread;
    MapImageRenderWorker *mImageRenderWorker;
    MapImage *mExpectMapImage;
    QList<MapInfo*> mExpectSubMaps;
#ifdef WORLDED
    QList<MapInfo*> mReferencedMaps;
#endif
    MapComposite *mRenderMapComposite;

    friend class MapImageManagerDeferral;
    void deferThreadResults(bool defer);
    int mDeferralDepth;
    QList<MapImage*> mDeferredMapImages;
    bool mDeferralQueued;

    static MapImageManager *mInstance;
};

class MapImageManagerDeferral
{
public:
    MapImageManagerDeferral() :
        mReleased(false)
    {
        MapImageManager::instance()->deferThreadResults(true);
    }

    ~MapImageManagerDeferral()
    {
        if (!mReleased)
            MapImageManager::instance()->deferThreadResults(false);
    }

    void release()
    {
        MapImageManager::instance()->deferThreadResults(false);
        mReleased = true;
    }

private:
    bool mReleased;
};

#endif // MAPIMAGEMANAGER_H
