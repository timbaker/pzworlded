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

#ifndef MAPMANAGER_H
#define MAPMANAGER_H

#include "map.h"

#include <QMap>

class MapInfo
{
public:
    MapInfo(Tiled::Map::Orientation orientation,
            int width, int height,
            int tileWidth, int tileHeight)
        : mOrientation(orientation)
        , mWidth(width)
        , mHeight(height)
        , mTileWidth(tileWidth)
        , mTileHeight(tileHeight)
        , mMap(0)
        , mConverted(0)
        , mPlaceholder(false)
        , mBeingEdited(false)
    {

    }

    bool isValid() const { return mWidth > 0 && mHeight > 0; }

    Tiled::Map::Orientation orientation() const { return mOrientation; }
    int width() const { return mWidth; }
    int height() const { return mHeight; }
    QSize size() const { return QSize(mWidth, mHeight); }
    int tileWidth() const { return mTileWidth; }
    int tileHeight() const { return mTileHeight; }

    void setFilePath(const QString& path) { mFilePath = path; }
    const QString &path() { return mFilePath; }

    Tiled::Map *map() const { return mMap; }

    MapInfo *converted(Tiled::Map::Orientation orient);

    bool isBeingEdited() const { return mBeingEdited; }

private:
    Tiled::Map::Orientation mOrientation;
    int mWidth;
    int mHeight;
    int mTileWidth;
    int mTileHeight;
    QString mFilePath;
    Tiled::Map *mMap;
    MapInfo *mConverted;
    bool mPlaceholder;
    bool mBeingEdited;

    friend class MapManager;
};

class MapManager : public QObject
{
    Q_OBJECT
public:
    static MapManager *instance();
    static void deleteInstance();

    /**
     * Returns the canonical (absolute) path for a map file.
     * \a mapName may or may not include .tmx extension.
     * \a mapName may be relative or absolute.
     */
    QString pathForMap(const QString &mapName, const QString &relativeTo);

    MapInfo *loadMap(const QString &mapName,
                     Tiled::Map::Orientation orient = Tiled::Map::Unknown,
                     const QString &relativeTo = QString());

    MapInfo *newFromMap(Tiled::Map *map, const QString &mapFilePath = QString());

    MapInfo *mapInfo(const QString &mapFilePath,
                     Tiled::Map::Orientation orient = Tiled::Map::Unknown);

    /**
     * The "empty map" is used when a WorldCell has no map.
     * The user still needs to view the cell to place Lots etc.
     */
    MapInfo *getEmptyMap();

    /**
      * A "placeholder" map is used when a sub-map could not be loaded.
      * The user still needs to move/delete the sub-map.
      * A placeholder map may be updated to a real map when the
      * user changes the maptools directory.
      */
    MapInfo *getPlaceholderMap(const QString &mapName, Tiled::Map::Orientation orient, int width, int height);

signals:
    void mapMagicallyGotMoreLayers(Tiled::Map *map);

private:

    /**
     * Converts a map to Isometric or LevelIsometric.
     * A new map is returned if conversion occurred.
     */
    Tiled::Map *convertOrientation(Tiled::Map *map, Tiled::Map::Orientation orient);

    MapManager();
    QMap<QString,MapInfo*> mMapInfo;
    static MapManager *mInstance;
};

#endif // MAPMANAGER_H
