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

#include "assetmanager.h"
#include "singleton.h"

#include <QFileInfo>
#include <QImage>
#include <QMap>
#include <QObject>
#include <QStringList>

class MapComposite;
class MapInfo;
class AssetTask_LoadMapImage;

namespace Tiled {
class Map;
}

#include "threads.h"
class MapImage;

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

class MapImageManager : public AssetManager, public Singleton<MapImageManager>
{
    Q_OBJECT

public:
    MapImageManager();
    ~MapImageManager();

    MapImage *getMapImage(const QString &mapName, const QString &relativeTo = QString());

    void recreateImage(const QString& mapName);

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
        QSize mapSize;
        QSize tileSize;
        QSize size;
    };

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

    void imageRenderedByThread(MapImageData imgData, MapImage *mapImage);

    void mapLoaded(MapInfo *mapInfo);
    void mapFailedToLoad(MapInfo *mapInfo);

    void processDeferrals();
    void processWaitingTasks();

protected:
    Asset* createAsset(AssetPath path, AssetParams* params) override;
    void destroyAsset(Asset* asset) override;
    void startLoading(Asset* asset) override;

    friend class AssetTask_LoadMapImage;

private:
    QFileInfo imageFileInfo(const QString &mapFilePath);
    QFileInfo imageDataFileInfo(const QFileInfo &imageFileInfo);

    QString mError;

    QList<AssetTask*> mWaitingTasks;

    friend class MapImageManagerDeferral;
    void deferThreadResults(bool defer);
    int mDeferralDepth;
    QList<MapImage*> mDeferredMapImages;
};

class MapImageManagerDeferral
{
public:
    MapImageManagerDeferral() :
        mReleased(false)
    {
        MapImageManager::instance().deferThreadResults(true);
    }

    ~MapImageManagerDeferral()
    {
        if (!mReleased)
            MapImageManager::instance().deferThreadResults(false);
    }

    void release()
    {
        MapImageManager::instance().deferThreadResults(false);
        mReleased = true;
    }

private:
    bool mReleased;
};

#endif // MAPIMAGEMANAGER_H
