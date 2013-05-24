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

#include "BuildingEditor/buildingtiles.h"

#if defined(Q_OS_WIN) && (_MSC_VER == 1600)
// Hmmmm.  libtiled.dll defines the Properties class as so:
// class TILEDSHARED_EXPORT Properties : public QMap<QString,QString>
// Suddenly I'm getting a 'multiply-defined symbol' error.
// I found the solution here:
// http://www.archivum.info/qt-interest@trolltech.com/2005-12/00242/RE-Linker-Problem-while-using-QMap.html
template class __declspec(dllimport) QMap<QString, QString>;
#endif

PathWorld::PathWorld(int width, int height) :
    mWidth(width),
    mHeight(height),
    mNextPathId(InvalidId),
    mNextNodeId(InvalidId)
{
}

PathWorld::~PathWorld()
{
    qDeleteAll(mLevels);
}

void PathWorld::insertLevel(int index, WorldLevel *level)
{
    level->setWorld(this);
    mLevels.insert(index, level);
    foreach (WorldTileLayer *wtl, level->tileLayers())
        mTileLayerByName[wtl->name()] = wtl;
}

WorldLevel *PathWorld::levelAt(int index) const
{
    if (index >= 0 && index <= mLevels.size())
        return mLevels[index];
    return 0;
}

WorldNode *PathWorld::allocNode(const QPointF &pos)
{
    return new WorldNode(mNextNodeId++, pos);
}

WorldPath *PathWorld::allocPath()
{
    return new WorldPath(mNextPathId++);
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

WorldTile *PathWorld::tile(const QString &tileName)
{
    QString tilesetName;
    int index;
    if (BuildingEditor::BuildingTilesMgr::parseTileName(tileName, tilesetName, index))
        return tile(tilesetName, index);
    return 0;
}

WorldTileLayer *PathWorld::tileLayer(const QString &layerName)
{
    if (mTileLayerByName.contains(layerName))
        return mTileLayerByName[layerName];
    return 0;
}

void PathWorld::initClone(PathWorld *clone)
{
    clone->mWidth = mWidth;
    clone->mHeight = mHeight;

    foreach (WorldTileset *ts, tilesets())
        clone->insertTileset(clone->tilesetCount(), ts->clone());

    foreach (WorldScript *script, scripts())
        clone->insertScript(clone->scriptCount(), script->clone(clone));

    foreach (WorldLevel *level, levels())
        clone->insertLevel(clone->levelCount(), level->clone());
}

/////

WorldLevel::WorldLevel(int level) :
    mWorld(0),
    mLevel(level),
    mPathLayersVisible(true),
    mTileLayersVisible(true)
{
}

WorldLevel::~WorldLevel()
{
    qDeleteAll(mPathLayers);
    qDeleteAll(mTileLayers);
}

void WorldLevel::insertPathLayer(int index, WorldPathLayer *layer)
{
    layer->setLevel(this);
    mPathLayers.insert(index, layer);
}

WorldPathLayer *WorldLevel::removePathLayer(int index)
{
    WorldPathLayer *layer = mPathLayers.takeAt(index);
    layer->setLevel(0);
    return layer;
}

WorldPathLayer *WorldLevel::pathLayerAt(int index)
{
    return mPathLayers.at(index);
}

int WorldLevel::pathLayerCount() const
{
    return mPathLayers.size();
}

int WorldLevel::indexOf(WorldPathLayer *wtl)
{
    return mPathLayers.indexOf(wtl);
}

void WorldLevel::insertTileLayer(int index, WorldTileLayer *layer)
{
    layer->setLevel(this);
    mTileLayers.insert(index, layer);
    if (world()) world()->tileLayerMap()[layer->name()] = layer;
}

WorldTileLayer *WorldLevel::removeTileLayer(int index)
{
    WorldTileLayer *layer = mTileLayers.takeAt(index);
    layer->setLevel(0);
    if (world()) world()->tileLayerMap().remove(layer->name());
    return layer;
}

WorldTileLayer *WorldLevel::tileLayerAt(int index)
{
    if (index >= 0 && index < mTileLayers.size())
        return mTileLayers[index];
    return 0;
}

int WorldLevel::indexOf(WorldTileLayer *wtl)
{
    return mTileLayers.indexOf(wtl);
}

WorldLevel *WorldLevel::clone() const
{
    WorldLevel *clone = new WorldLevel(mLevel);

    foreach (WorldPathLayer *layer, pathLayers())
        clone->insertPathLayer(clone->pathLayerCount(), layer->clone());

    foreach (WorldTileLayer *layer, tileLayers())
        clone->insertTileLayer(clone->tileLayerCount(), layer->clone());

    return clone;
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

void WorldTileset::resize(int columns, int rows)
{
    if (columns != mColumns || rows != mRows) {
        qDeleteAll(mTiles);
        mTiles.clear();
        mColumns = columns;
        mRows = rows;
        for (int i = 0; i < mColumns * mRows; i++)
            mTiles += new WorldTile(this, i);
    }
}

WorldTileset *WorldTileset::clone() const
{
    WorldTileset *clone = new WorldTileset(mName, mColumns, mRows);
    for (int i = 0; i < mColumns * mRows; i++) {
        clone->mTiles[i]->mTiledTile = mTiles[i]->mTiledTile;
    }
    return clone;
}

/////

int WorldTileLayer::level() const
{
    return mLevel ? mLevel->level() : 0;
}

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

WorldTileLayer *WorldTileLayer::clone() const
{
    WorldTileLayer *clone = new WorldTileLayer(mName);
    // don't clone the sinks
    return clone;
}

/////

WorldScript *WorldScript::clone(PathWorld *owner) const
{
    Q_UNUSED(owner)

    WorldScript *clone = new WorldScript;
    clone->mFileName = mFileName;
    clone->mParams = mParams;
    foreach (WorldPath *path, mPaths) {
        if (WorldPath *clonePath = path->layer()->path(path->id))
            clone->mPaths += clonePath;
    }
    clone->mRegion = mRegion;
    return clone;
}
