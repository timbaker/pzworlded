/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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

#ifndef PATHWORLD_H
#define PATHWORLD_H

#include <QList>
#include <QMap>
#include <QRegion>
#include <QString>

class WorldPathLayer;
class WorldPath;

namespace Tiled {
class Tile;
}

class PathWorld;
class WorldTileset;

class WorldTile
{
public:
    WorldTile(WorldTileset *tileset, int index) :
        mTileset(tileset),
        mIndex(index),
        mTiledTile(0)
    {}

    WorldTileset *mTileset;
    int mIndex;

    // HACK for convenience
    Tiled::Tile *mTiledTile;
};

class WorldTileset
{
public:
    WorldTileset(const QString &name, int columns, int rows);
    ~WorldTileset();

    WorldTile *tileAt(int index);

    WorldTileset *clone() const;

    QString mName;
    int mColumns;
    int mRows;
    QList<WorldTile*> mTiles;
};

class WorldTileLayer
{
public:
    WorldTileLayer(PathWorld *world, const QString &name) :
        mWorld(world),
        mName(name),
        mLevel(0)
    {}

    void putTile(int x, int y, WorldTile *tile);
    WorldTile *getTile(int x, int y);

    WorldTileLayer *clone(PathWorld *owner) const;

    class TileSink
    {
    public:
        virtual void putTile(int x, int y, WorldTile *tile) = 0;
        virtual WorldTile *getTile(int x, int y) = 0;
    };

    PathWorld *mWorld;
    QString mName;
    int mLevel;
    QList<TileSink*> mSinks;
};

class WorldScript
{
public:
    WorldScript *clone(PathWorld *owner) const;

    QString mFileName;
    QMap<QString,QString> mParams;
    QList<WorldPath*> mPaths;
    QRegion mRegion;
};

class PathWorld
{
public:
    PathWorld(int width, int height);
    ~PathWorld();

    int width() const { return mWidth; }
    int height() const { return mHeight; }
    QSize size() const { return QSize(mWidth, mHeight); }
    QRect bounds() const { return QRect(0, 0, mWidth, mHeight); }

    int tileHeight() const { return 32; }
    int tileWidth() const { return 64; }

    int maxLevel() const { return 16; }

    void insertPathLayer(int index, WorldPathLayer *layer);
    WorldPathLayer *removePathLayer(int index);
    const QList<WorldPathLayer*> &pathLayers() const
    { return mPathLayers; }
    WorldPathLayer *pathLayerAt(int index);
    int pathLayerCount() const;
    int indexOf(WorldPathLayer *wtl);

    void insertTileLayer(int index, WorldTileLayer *layer);
    WorldTileLayer *removeTileLayer(int index);
    const QList<WorldTileLayer*> &tileLayers() const
    { return mTileLayers; }
    WorldTileLayer *tileLayerAt(int index);
    WorldTileLayer *tileLayer(const QString &layerName);
    int tileLayerCount() const
    { return mTileLayers.size(); }
    int indexOf(WorldTileLayer *wtl);

    void insertScript(int index, WorldScript *script);
    WorldScript *removeScript(int index);
    const QList<WorldScript*> &scripts() const
    { return mScripts; }
    WorldScript *scriptAt(int index);
    int scriptCount() const
    { return mScripts.size(); }

    void insertTileset(int index, WorldTileset *tileset);
    const QList<WorldTileset*> &tilesets() const
    { return mTilesets; }
    WorldTileset *tileset(const QString &tilesetName);
    int tilesetCount() const
    { return mTilesets.size(); }

    WorldTile *tile(const QString &tilesetName, int index);

    void initClone(PathWorld *clone);

protected:
    int mWidth;
    int mHeight;

    QList<WorldTileset*> mTilesets;
    QMap<QString,WorldTileset*> mTilesetByName;

    QList<WorldTileLayer*> mTileLayers;
    QMap<QString,WorldTileLayer*> mTileLayerByName;

    QList<WorldPathLayer*> mPathLayers;
    QList<WorldScript*> mScripts;
};

#endif // PATHWORLD_H
