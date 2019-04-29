/*
 * Copyright 2019, Tim Baker <treectrl@users.sf.net>
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

#ifndef MAPINFO_H
#define MAPINFO_H

#include "asset.h"

#include "map.h"

#include <QRect>

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
    bool mPlaceholder;
    bool mBeingEdited;
#ifdef WORLDED
    int mMapRefCount;
    int mReferenceEpoch;
#endif
    // Temporary used during loading.
    MapInfo* mLoaded = nullptr;

    friend class MapInfoManager;
    friend class MapManager;
};

#endif // MAPINFO_H
