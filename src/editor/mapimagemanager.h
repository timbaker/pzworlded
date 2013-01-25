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

#include <QMutexLocker>
#include <QThread>
#include <QWaitCondition>
class MapImage;
class MapImageReaderThread : public QThread
{
    Q_OBJECT
public:
    MapImageReaderThread();

    ~MapImageReaderThread();

    void run();

    void addJob(const QString &imageFileName, MapImage *mapImage);

signals:
    void imageLoaded(QImage *image, MapImage *mapImage);

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

    QMutex mMutex;
    QWaitCondition mWaitCondition;
    bool mQuit;
};

class MapImage
{
public:
    MapImage(QImage image, qreal scale, const QRectF &levelZeroBounds, MapInfo *mapInfo);

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

    void mapFileChanged(QImage image, qreal scale, const QRectF &levelZeroBounds);

    void setSources(const QList<MapInfo*> &sources)
    { mSources = sources; }

    QList<MapInfo*> sources() const
    { return mSources; }

    QRectF levelZeroBounds() const
    { return mLevelZeroBounds; }

    bool isLoaded() const { return mLoaded; }

private:
    QImage mImage;
    MapInfo *mInfo;
    QRectF mLevelZeroBounds;
    qreal mScale;
    QList<MapInfo*> mSources;
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
            missingTilesets(false)
        {}
        qreal scale;
        QRectF levelZeroBounds;
        QImage image;
        bool valid;
        QStringList sources;
        bool missingTilesets;
        QSize size;
    };

    ImageData generateMapImage(const QString &mapFilePath, bool force = false);
    ImageData generateMapImage(MapComposite *mapComposite);

    ImageData generateBMPImage(const QString &bmpFilePath);

    ImageData readImageData(const QFileInfo &imageDataFileInfo);
    void writeImageData(const QFileInfo &imageDataFileInfo, const ImageData &data);

signals:
    void mapImageChanged(MapImage *mapImage);
    
public slots:
    void mapFileChanged(MapInfo *mapInfo);
    void imageLoaded(QImage *image, MapImage *mapImage);

private:
    MapImageManager();
    QFileInfo imageFileInfo(const QString &mapFilePath);
    QFileInfo imageDataFileInfo(const QFileInfo &imageFileInfo);

    QMap<QString,MapImage*> mMapImages;
    QString mError;

    QVector<MapImageReaderThread*> mImageReaderThread;
    int mNextThreadForJob;

    static MapImageManager *mInstance;
};

#endif // MAPIMAGEMANAGER_H
