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

#include "pathworld.h"

#include "path.h"

PathWorld::PathWorld(int width, int height) :
    mWidth(width),
    mHeight(height)
{
}

PathWorld::~PathWorld()
{
    qDeleteAll(mPathLayers);
    qDeleteAll(mTileLayers);
}

void PathWorld::insertPathLayer(int index, WorldPathLayer *layer)
{
    mPathLayers.insert(index, layer);
}

WorldPathLayer *PathWorld::removePathLayer(int index)
{
    return mPathLayers.takeAt(index);
}

WorldPathLayer *PathWorld::pathLayerAt(int index)
{
    return mPathLayers.at(index);
}

int PathWorld::pathLayerCount() const
{
    return mPathLayers.size();
}

int PathWorld::indexOf(WorldPathLayer *wtl)
{
    return mPathLayers.indexOf(wtl);
}

void PathWorld::insertTileLayer(int index, WorldTileLayer *layer)
{
    mTileLayers.insert(index, layer);
    mTileLayerByName[layer->mName] = layer;
}

WorldTileLayer *PathWorld::removeTileLayer(int index)
{
    return mTileLayers.takeAt(index);
}

WorldTileLayer *PathWorld::tileLayerAt(int index)
{
    if (index >= 0 && index < mTileLayers.size())
        return mTileLayers[index];
    return 0;
}

WorldTileLayer *PathWorld::tileLayer(const QString &layerName)
{
    if (mTileLayerByName.contains(layerName))
        return mTileLayerByName[layerName];
    return 0;
}

int PathWorld::indexOf(WorldTileLayer *wtl)
{
    return mTileLayers.indexOf(wtl);
}

void PathWorld::insertScript(int index, WorldScript *script)
{
    mScripts.insert(index, script);
}

WorldScript *PathWorld::removeScript(int index)
{
    return mScripts.takeAt(index);
}

WorldScript *PathWorld::scriptAt(int index)
{
    return (index >= 0 && index < mScripts.size()) ? mScripts[index] : 0;
}

void PathWorld::insertTileset(int index, WorldTileset *tileset)
{
    mTilesets.insert(index, tileset);
    mTilesetByName[tileset->mName] = tileset;
}

WorldTileset *PathWorld::tileset(const QString &tilesetName)
{
    if (mTilesetByName.contains(tilesetName))
        return mTilesetByName[tilesetName];
    return 0;
}

WorldTile *PathWorld::tile(const QString &tilesetName, int index)
{
    if (WorldTileset *ts = tileset(tilesetName))
        return ts->tileAt(index);
    return 0;
}

/////

WorldTileset::WorldTileset(const QString &name, int columns, int rows) :
    mName(name),
    mColumns(columns),
    mRows(rows)
{
    for (int i = 0; i < mColumns * mRows; i++)
        mTiles += new WorldTile(this, i);
}

WorldTileset::~WorldTileset()
{
    qDeleteAll(mTiles);
}

WorldTile *WorldTileset::tileAt(int index)
{
    if (index >= 0 && index < mTiles.size())
        return mTiles[index];
    return 0;
}

/////

void WorldTileLayer::putTile(int x, int y, WorldTile *tile)
{
    foreach (TileSink *sink, mSinks)
        sink->putTile(x, y, tile);
}

WorldTile *WorldTileLayer::getTile(int x, int y)
{
     // weird?
    foreach (TileSink *sink, mSinks) {
        if (WorldTile *tile = sink->getTile(x, y))
            return tile;
    }
    return 0;
}
