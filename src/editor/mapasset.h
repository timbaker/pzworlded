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

#ifndef MAPASSET_H
#define MAPASSET_H

#include "asset.h"

#include "map.h"

class AssetManager;

namespace BuildingEditor {
class Building;
}

class MapAsset : public Asset
{
public:
    MapAsset(AssetPath path, AssetManager* manager)
        : Asset(path, manager)
    {}

    ~MapAsset() override;

    MapAsset(Tiled::Map* map, AssetPath path, AssetManager* manager);

    Tiled::Map* map() const { return mMap; }

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

    Tiled::Map::Orientation mOrientation;
    int mWidth;
    int mHeight;
    int mTileWidth;
    int mTileHeight;
    Tiled::Map* mMap = nullptr;
    QString mFilePath;
    bool mPlaceholder = false;
    bool mBeingEdited = false;
#ifdef WORLDED
    int mMapRefCount = 0;
    int mReferenceEpoch = 0;
#endif

    // Temporaries used during loading
    BuildingEditor::Building* mLoadedBuilding = nullptr;
    Tiled::Map* mLoadedMap = nullptr;
};

#endif // MAPASSET_H
