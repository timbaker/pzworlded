#include "mapinfo.h"

#include "mapassetmanager.h"
#include "mapmanager.h"

Tiled::Map *MapInfo::map() const
{
    return mMap != nullptr && mMap->isReady() ? mMap->map() : nullptr;
}

bool MapInfo::isLoading() const
{
    return mMap != nullptr && mMap->isEmpty();
}

void MapInfo::setFrom(MapInfo *other)
{
    mOrientation = other->mOrientation;
    mWidth = other->mWidth;
    mHeight = other->mHeight;
    mTileWidth = other->mTileWidth;
    mTileHeight = other->mTileHeight;
    mFilePath = other->mFilePath;
    mMap = other->mMap; // FIXME: addDepency()
    mPlaceholder = other->mPlaceholder;
    mBeingEdited = other->mBeingEdited;
    mReferenceEpoch = other->mReferenceEpoch;
}
