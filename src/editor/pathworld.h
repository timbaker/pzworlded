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
#include <QStringList>
#include <QVector>

#include "global.h"

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

    void resize(int columns, int rows);

    WorldTileset *clone() const;

    QString mName;
    int mColumns;
    int mRows;
    QList<WorldTile*> mTiles;
};

class WorldTileLayer
{
public:
    WorldTileLayer(const QString &name) :
        mLevel(0),
        mName(name)
    {}

    const QString &name() const
    { return mName; }

    void setLevel(WorldLevel *wlevel)
    { mLevel = wlevel; }
    WorldLevel *wlevel() const
    { return mLevel; }
    int level() const;

    void putTile(int x, int y, WorldTile *tile);
    WorldTile *getTile(int x, int y);

    WorldTileLayer *clone() const;

    class TileSink
    {
    public:
        virtual QRect bounds() = 0;
        virtual void putTile(int x, int y, WorldTile *tile) = 0;
        virtual WorldTile *getTile(int x, int y) = 0;
    };

    void addSink(TileSink *sink)
    { mSinks += sink; }
    const QList<TileSink*> &sinks() const
    { return mSinks; }

private:
    WorldLevel *mLevel;
    QString mName;
    QList<TileSink*> mSinks;
};

class WorldLevel
{
public:
    WorldLevel(int level);
    ~WorldLevel();

    void setWorld(PathWorld *world)
    { mWorld = world; }
    PathWorld *world() const
    { return mWorld; }

    int level() const
    { return mLevel; }

    void insertPathLayer(int index, WorldPathLayer *layer);
    WorldPathLayer *removePathLayer(int index);
    const QList<WorldPathLayer*> &pathLayers() const
    { return mPathLayers; }
    WorldPathLayer *pathLayerAt(int index);
    WorldPathLayer *pathLayerByName(const QString &name);
    int pathLayerCount() const;
    int indexOf(WorldPathLayer *wtl);

    void insertTileLayer(int index, WorldTileLayer *layer);
    WorldTileLayer *removeTileLayer(int index);
    const QList<WorldTileLayer*> &tileLayers() const
    { return mTileLayers; }
    WorldTileLayer *tileLayerAt(int index);
    int tileLayerCount() const
    { return mTileLayers.size(); }
    int indexOf(WorldTileLayer *wtl);

    void setPathLayersVisible(bool visible)
    { mPathLayersVisible = visible; }
    bool isPathLayersVisible() const
    { return mPathLayersVisible; }

    void setTileLayersVisible(bool visible)
    { mTileLayersVisible = visible; }
    bool isTileLayersVisible() const
    { return mTileLayersVisible; }

    WorldLevel *clone() const;

private:
    PathWorld *mWorld;
    int mLevel;
    PathLayerList mPathLayers;
    TileLayerList mTileLayers;
    bool mPathLayersVisible;
    bool mTileLayersVisible;
};

class WorldTileAlias
{
public:
    QString mName;
    QStringList mTileNames;
    TileList mTiles;
};

class WorldTileRule
{
public:
    WorldTileRule() :
        mCondition(0)
    {}
    QString mName;
    QStringList mTileNames; // tile or alias names
    TileList mTiles;
    QString mLayer;
    WorldTileRule *mCondition;
};

#if 0
class WorldTileBlend
{
public:
    WorldTileBlend();

    class BlendDir
    {
    public:
        QString mLayer;
        QString mMainTile;
        QString mBlendTile;
        QStringList mExclude;
        QStringList mExclude2;
    };

    enum Dir {
        N, E, S, W, NW, NE, SE, SW
    };

    QString mName;
    QStringList mTileNames;
    TileList mTiles;
    QStringList mLayers;
};
#endif

class WorldScript
{
public:
    WorldScript *clone(PathWorld *owner) const;

    QString mFileName;
    ScriptParams mParams;
    QList<WorldPath*> mPaths;
    QList<WorldNode*> mNodes;
    WorldPathLayer *mCurrentPathLayer; // FIXME
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

    int maxLevel() const { return 16 - 1; }

    void insertLevel(int index, WorldLevel *level);
    const LevelList &levels() const
    { return mLevels; }
    WorldLevel *levelAt(int index) const;
    int levelCount() const
    { return mLevels.size(); }

    void setNextIds(id_t node, id_t path)
    { mNextNodeId = node, mNextPathId = path; }

    WorldNode *allocNode(const QPointF &pos);
    WorldPath *allocPath();
    WorldPathLayer *allocPathLayer();

    WorldTileLayer *tileLayer(const QString &layerName);
    QMap<QString,WorldTileLayer*> &tileLayerMap() // for use by WorldLevel only
    { return mTileLayerByName; }

    void insertScript(int index, WorldScript *script);
    WorldScript *removeScript(int index);
    const ScriptList &scripts() const
    { return mScripts; }
    WorldScript *scriptAt(int index);
    int scriptCount() const
    { return mScripts.size(); }

    void insertTileset(int index, WorldTileset *tileset);
    const TilesetList &tilesets() const
    { return mTilesets; }
    WorldTileset *tileset(const QString &tilesetName);
    int tilesetCount() const
    { return mTilesets.size(); }

    void addTileAlias(WorldTileAlias *alias)
    {
        mTileAliases += alias;
        mTileAliasByName[alias->mName] = alias;
    }
    const AliasList &tileAliases() const
    { return mTileAliases; }
    WorldTileAlias *tileAlias(const QString &name)
    {
        return mTileAliasByName.contains(name) ? mTileAliasByName[name] : 0;
    }

    void addTileRule(WorldTileRule *rule)
    {
        mTileRules += rule;
        mTileRuleByName[rule->mName] = rule;
    }
    const TileRuleList &tileRules() const
    { return mTileRules; }
    WorldTileRule *tileRule(const QString &name)
    {
        return mTileRuleByName.contains(name) ? mTileRuleByName[name] : 0;
    }

    WorldTile *tile(const QString &tilesetName, int index);
    WorldTile *tile(const QString &tileName);

    void initClone(PathWorld *clone);

    TextureList &textureList() { return mTextures; }
    TextureMap &textureMap() { return mTextureMap; }

protected:
    int mWidth;
    int mHeight;

    LevelList mLevels;

    TilesetList mTilesets;
    QMap<QString,WorldTileset*> mTilesetByName;

    QMap<QString,WorldTileLayer*> mTileLayerByName;

    AliasList mTileAliases;
    QMap<QString,WorldTileAlias*> mTileAliasByName;

    TileRuleList mTileRules;
    QMap<QString,WorldTileRule*> mTileRuleByName;

    ScriptList mScripts;

    TextureList mTextures;
    TextureMap mTextureMap;

    id_t mNextPathId;
    id_t mNextNodeId;
};

#endif // PATHWORLD_H
