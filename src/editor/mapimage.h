#ifndef MAPIMAGE_H
#define MAPIMAGE_H

#include "asset.h"

#include <QImage>

class MapInfo;

class MapImage : public Asset
{
public:
    MapImage(AssetPath path, AssetManager* manager);

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

#ifdef WORLDED
    void chopIntoPieces();

    QSize imageSize() const
    { return mImageSize; }

    int imageWidth() const
    { return mImageSize.width(); }

    int imageHeight() const
    { return mImageSize.height(); }

    int subImageColumns() const
    { return (mImageSize.width() + 511) / 512; }

    int subImageRows() const
    { return (mImageSize.height() + 511) / 512; }

    const QVector<QImage> &subImages() const
    { return mSubImages; }

    const QImage &miniMapImage() const
    { return mMiniMapImage; }
#endif /* WORLDED */

private:
    QImage mImage;
    MapInfo *mInfo;
    QRectF mLevelZeroBounds;
    qreal mScale;
    QList<MapInfo*> mSources;
    bool mMissingTilesets;
    QSize mMapSize;
    QSize mTileSize;

#ifdef WORLDED
    // For WorldEd world images.
    QSize mImageSize;
    QVector<QImage> mSubImages;
    QImage mMiniMapImage;
#endif /* WORLDED */

    friend class MapImageManager;
    friend class AssetTask_LoadMapImage;
};

#endif // MAPIMAGE_H
