#include "mapimage.h"

#include "mapimagemanager.h"

MapImage::MapImage(AssetPath path, AssetManager *manager)
    : Asset(path, manager)
    , mImage(nullptr)
    , mInfo(nullptr)
    , mScale(0)
    , mMissingTilesets(false)
{

}

MapImage::MapImage(QImage image, qreal scale, const QRectF &levelZeroBounds, const QSize &mapSize, const QSize &tileSize, MapInfo *mapInfo)
    : Asset(AssetPath(), MapImageManager::instancePtr())
    , mImage(image)
    , mInfo(mapInfo)
    , mLevelZeroBounds(levelZeroBounds)
    , mScale(scale)
    , mMissingTilesets(false)
    , mMapSize(mapSize)
    , mTileSize(tileSize)
#ifdef WORLDED
    , mImageSize(image.size())
#endif
{
    onCreated(AssetState::READY);
}

QPointF MapImage::tileToPixelCoords(qreal x, qreal y)
{
    const int tileWidth = mTileSize.width();
    const int tileHeight = mTileSize.height();
    const int originX = mMapSize.height() * tileWidth / 2;

    return QPointF((x - y) * tileWidth / 2 + originX,
                   (x + y) * tileHeight / 2);
}

QRectF MapImage::tileBoundingRect(const QRect &rect)
{
    const int tileWidth = mTileSize.width();
    const int tileHeight = mTileSize.height();

    const int originX = mMapSize.height() * tileWidth / 2;
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
    return tileBoundingRect(QRect(QPoint(), mMapSize));
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

void MapImage::mapFileChanged(QImage image, qreal scale, const QRectF &levelZeroBounds, const QSize &mapSize, const QSize &tileSize)
{
    mImage = image;
    mScale = scale;
    mLevelZeroBounds = levelZeroBounds;
    mMapSize = mapSize;
    mTileSize = tileSize;
}

#ifdef WORLDED
void MapImage::chopIntoPieces()
{
    int columns = subImageColumns();
    int rows = subImageRows();
    mSubImages.resize(columns * rows);
    QRect r(QPoint(), image().size());
    for (int x = 0; x < columns; x++) {
        for (int y = 0; y < rows; y++) {
            QRect subr = QRect(x * 512, y * 512, 512, 512) & r;
            mSubImages[x + y * columns] = image().copy(subr).convertToFormat(QImage::Format_ARGB4444_Premultiplied);
        }
    }
    mMiniMapImage = mImage.scaledToWidth(512);
    mImage = QImage();
}
#endif /* WORLDED */
