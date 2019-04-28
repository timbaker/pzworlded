#ifndef MAPINFO_H
#define MAPINFO_H

#include "asset.h"

#include "map.h"

#include <QRect>

class MapAsset;

class MapInfo : public Asset
{
public:
    MapInfo(AssetPath path, AssetManager* manager)
        : Asset(path, manager)
        , mOrientation(Tiled::Map::Orientation::Unknown)
        , mWidth(-1)
        , mHeight(-1)
        , mTileWidth(-1)
        , mTileHeight(-1)
        , mMap(nullptr)
        , mPlaceholder(false)
        , mBeingEdited(false)
#ifdef WORLDED
        , mMapRefCount(0)
        , mReferenceEpoch(0)
#endif
    {

    }

    MapInfo(Tiled::Map::Orientation orientation,
            int width, int height,
            int tileWidth, int tileHeight)
        : Asset(AssetPath(), nullptr)
        , mOrientation(orientation)
        , mWidth(width)
        , mHeight(height)
        , mTileWidth(tileWidth)
        , mTileHeight(tileHeight)
        , mMap(nullptr)
        , mPlaceholder(false)
        , mBeingEdited(false)
#ifdef WORLDED
        , mMapRefCount(0)
        , mReferenceEpoch(0)
#endif
    {
        onCreated(AssetState::READY);
    }

    bool isValid() const { return mWidth > 0 && mHeight > 0; }

    Tiled::Map::Orientation orientation() const { return mOrientation; }
    int width() const { return mWidth; }
    int height() const { return mHeight; }
    QSize size() const { return QSize(mWidth, mHeight); }
    QRect bounds() const { return QRect(0, 0, mWidth, mHeight); }
    int tileWidth() const { return mTileWidth; }
    int tileHeight() const { return mTileHeight; }

    void setFilePath(const QString& path) { mFilePath = path; }
    const QString &path() const { return mFilePath; }

    Tiled::Map* map() const;

    void setBeingEdited(bool edited) { mBeingEdited = edited; }
    bool isBeingEdited() const { return mBeingEdited; }

    bool isLoading() const; // is *mMap* loading

    void setFrom(MapInfo* other);

private:
    Tiled::Map::Orientation mOrientation;
    int mWidth;
    int mHeight;
    int mTileWidth;
    int mTileHeight;
    QString mFilePath;
    MapAsset *mMap;
    bool mPlaceholder;
    bool mBeingEdited;
#ifdef WORLDED
    int mMapRefCount;
    int mReferenceEpoch;
#endif

    friend class MapInfoManager;
    friend class MapManager;
};

#endif // MAPINFO_H
