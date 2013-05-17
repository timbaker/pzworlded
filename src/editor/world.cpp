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

#include "world.h"

#include "bmptotmx.h" // FIXME: remove from this file
#include "path.h"
#include "worldcell.h"

#include <QStringList>

World::World(int width, int height)
    : mWidth(width)
    , mHeight(height)
    , mNullObjectType(new ObjectType())
    , mNullObjectGroup(new WorldObjectGroup(this))
{
    mCells.resize(mWidth * mHeight);

    for (int y = 0; y < mHeight; y++) {
        for (int x = 0; x < mWidth; x++) {
            mCells[y * mWidth + x] = new WorldCell(this, x, y);
        }
    }

    // The nameless default group for WorldCellObjects
    mObjectGroups.append(mNullObjectGroup);

    // The nameless default type for WorldCellObjects
    mObjectTypes.append(mNullObjectType);
}

World::~World()
{
    qDeleteAll(mCells);
    qDeleteAll(mPropertyTemplates);
    qDeleteAll(mPropertyDefs);
    qDeleteAll(mObjectTypes);
    qDeleteAll(mBMPs);
    qDeleteAll(mPathLayers);
}

void World::swapCells(QVector<WorldCell *> &cells)
{
    mCells.swap(cells);
}

void World::setSize(const QSize &newSize)
{
    mWidth = newSize.width();
    mHeight = newSize.height();
}

PropertyDef *World::removePropertyDefinition(int index)
{
    return mPropertyDefs.takeAt(index);
}

void World::insertObjectGroup(int index, WorldObjectGroup *og)
{
    mObjectGroups.insert(index, og);
}

WorldObjectGroup *World::removeObjectGroup(int index)
{
    return mObjectGroups.takeAt(index);
}

void World::insertObjectType(int index, ObjectType *ot)
{
    mObjectTypes.insert(index, ot);
}

ObjectType *World::removeObjectType(int index)
{
    return mObjectTypes.takeAt(index);
}

void World::insertRoad(int index, Road *road)
{
    mRoads.insert(index, road);
}

Road *World::removeRoad(int index)
{
    return mRoads.takeAt(index);
}

void World::insertBmp(int index, WorldBMP *bmp)
{
    mBMPs.insert(index, bmp);
}

WorldBMP *World::removeBmp(int index)
{
    return mBMPs.takeAt(index);
}

void World::insertLayer(int index, WorldPath::Layer *layer)
{
    mPathLayers.insert(index, layer);
}

WorldPath::Layer *World::removeLayer(int index)
{
    return mPathLayers.takeAt(index);
}

WorldPath::Layer *World::layerAt(int index)
{
    return mPathLayers.at(index);
}

int World::layerCount() const
{
    return mPathLayers.size();
}

void World::insertTileLayer(int index, WorldTileLayer *layer)
{
    mTileLayers.insert(index, layer);
    mTileLayerByName[layer->mName] = layer;
}

WorldTileLayer *World::removeTileLayer(int index)
{
    return mTileLayers.takeAt(index);
}

WorldTileLayer *World::tileLayerAt(int index)
{
    if (index >= 0 && index < mTileLayers.size())
        return mTileLayers[index];
    return 0;
}

WorldTileLayer *World::tileLayer(const QString &layerName)
{
    if (mTileLayerByName.contains(layerName))
        return mTileLayerByName[layerName];
    return 0;
}

int World::indexOf(WorldTileLayer *wtl)
{
    return mTileLayers.indexOf(wtl);
}

void World::insertScript(int index, WorldScript *script)
{
    mScripts.insert(index, script);
}

WorldScript *World::removeScript(int index)
{
    return mScripts.takeAt(index);
}

WorldScript *World::scriptAt(int index)
{
    return (index >= 0 && index < mScripts.size()) ? mScripts[index] : 0;
}

void World::insertTileset(int index, WorldTileset *tileset)
{
    mTilesets.insert(index, tileset);
    mTilesetByName[tileset->mName] = tileset;
}

WorldTileset *World::tileset(const QString &tilesetName)
{
    if (mTilesetByName.contains(tilesetName))
        return mTilesetByName[tilesetName];
    return 0;
}

WorldTile *World::tile(const QString &tilesetName, int index)
{
    if (WorldTileset *ts = tileset(tilesetName))
        return ts->tileAt(index);
    return 0;
}

/////

bool ObjectGroupList::contains(const QString &name) const
{
    return find(name) != 0;
}

WorldObjectGroup *ObjectGroupList::find(const QString &name) const
{
    const_iterator it = constBegin();
    while (it != constEnd()) {
        if ((*it)->name() == name)
            return *it;
        it++;
    }
    return 0;
}

QStringList ObjectGroupList::names() const
{
    QStringList result;
    const_iterator it = constBegin();
    while (it != constEnd()) {
        result += (*it)->name();
        ++it;
    }
    return result;
}

/////

bool ObjectTypeList::contains(const QString &name) const
{
    return find(name) != 0;
}

ObjectType *ObjectTypeList::find(const QString &name) const
{
    const_iterator it = constBegin();
    while (it != constEnd()) {
        if ((*it)->name() == name)
            return *it;
        it++;
    }
    return 0;
}

QStringList ObjectTypeList::names() const
{
    QStringList result;
    const_iterator it = constBegin();
    while (it != constEnd()) {
        result += (*it)->name();
        ++it;
    }
    return result;
}

/////

WorldBMP::WorldBMP(World *world, int x, int y, int width, int height, const QString &filePath) :
    mWorld(world),
    mX(x),
    mY(y),
    mWidth(width),
    mHeight(height),
    mFilePath(filePath)
{
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
