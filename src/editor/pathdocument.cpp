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

#include "pathdocument.h"

#include "luaworlded.h"
#include "path.h"
#include "pathworld.h"
#include "pathworldwriter.h"
#include "progress.h"
#include "tilemetainfomgr.h"
#include "worldchanger.h"
#include "worldlookup.h"

#include "tileset.h"

#include <QUndoStack>

PathDocument::PathDocument(PathWorld *world, const QString &fileName) :
    Document(PathDocType),
    mWorld(world),
    mFileName(fileName),
    mChanger(new WorldChanger(world)),
    mCurrentLevel(0),
    mCurrentPathLayer(0),
    mCurrentTileLayer(0)
{
    mUndoStack = new QUndoStack(this);

    QList<Tiled::Tileset*> tilesets;
    foreach (WorldTileset *wts, mWorld->tilesets()) {
        if (Tiled::Tileset *ts = Tiled::TileMetaInfoMgr::instance()->tileset(wts->mName)) {
            wts->resize(ts->columnCount(), ts->tileCount() / ts->columnCount());
            for (int i = 0; i < ts->tileCount(); i++)
                wts->tileAt(i)->mTiledTile = ts->tileAt(i); // HACK for convenience
        }
    }
    Tiled::TileMetaInfoMgr::instance()->loadTilesets(tilesets);

    PROGRESS progress(QLatin1String("Running scripts (region)"));

    foreach (WorldLevel *wlevel, mWorld->levels()) {
        foreach (WorldPathLayer *layer, wlevel->pathLayers()) {
            foreach (WorldPath *path, layer->paths()) {
                foreach (WorldScript *ws, path->scripts()) {
                    Tiled::Lua::LuaScript ls(mWorld, ws);
                    if (ls.runFunction("region")) {
                        QRegion rgn;
                        if (ls.getResultRegion(rgn))
                            ws->mRegion = rgn;
                    }
                }
            }
        }
    }

    foreach (WorldScript *ws, mWorld->scripts()) {
        Tiled::Lua::LuaScript ls(mWorld, ws);
        if (ls.runFunction("region")) {
            QRegion rgn;
            if (ls.getResultRegion(rgn))
                ws->mRegion = rgn;
        }
    }
#if 0
    // Now that the script regions are up-to-date...
    foreach (WorldLevel *wlevel, mWorld->levels())
        foreach (WorldPathLayer *layer, wlevel->pathLayers())
            layer->initLookup();
#endif

    setCurrentLevel(mWorld->levelAt(0));
    setCurrentPathLayer(mWorld->levelAt(0)->pathLayerAt(0));
    setCurrentTileLayer(mWorld->levelAt(0)->tileLayerAt(0));

    connect(mChanger, SIGNAL(afterAddNodeSignal(WorldNode*)),
            SLOT(afterAddNode(WorldNode*)));
    connect(mChanger, SIGNAL(afterRemoveNodeSignal(WorldPathLayer*,int,WorldNode*)),
            SLOT(afterRemoveNode(WorldPathLayer*,int,WorldNode*)));
    connect(mChanger, SIGNAL(afterAddPathSignal(WorldPathLayer*,int,WorldPath*)),
            SLOT(afterAddPath(WorldPathLayer*,int,WorldPath*)));
    connect(mChanger, SIGNAL(afterRemovePathSignal(WorldPathLayer*,int,WorldPath*)),
            SLOT(afterRemovePath(WorldPathLayer*,int,WorldPath*)));
    connect(mChanger, SIGNAL(afterAddNodeToPathSignal(WorldPath*,int,WorldNode*)),
            SLOT(afterAddNodeToPath(WorldPath*,int,WorldNode*)));
    connect(mChanger, SIGNAL(afterRemoveNodeFromPathSignal(WorldPath*,int,WorldNode*)),
            SLOT(afterRemoveNodeFromPath(WorldPath*,int,WorldNode*)));
    connect(mChanger, SIGNAL(afterAddScriptToPathSignal(WorldPath*,int,WorldScript*)),
            SLOT(afterAddScriptToPath(WorldPath*,int,WorldScript*)));
    connect(mChanger, SIGNAL(afterRemoveScriptFromPathSignal(WorldPath*,int,WorldScript*)),
            SLOT(afterRemoveScriptFromPath(WorldPath*,int,WorldScript*)));
    connect(mChanger, SIGNAL(afterMoveNodeSignal(WorldNode*,QPointF)),
            SLOT(afterMoveNode(WorldNode*,QPointF)));
    connect(mChanger, SIGNAL(afterSetPathVisibleSignal(WorldPath*,bool)),
            SLOT(afterSetPathVisible(WorldPath*,bool)));
}

PathDocument::~PathDocument()
{
    delete mChanger;
}

void PathDocument::setFileName(const QString &fileName)
{
    mFileName = fileName;
}

const QString &PathDocument::fileName() const
{
    return mFileName;
}

bool PathDocument::save(const QString &filePath, QString &error)
{
    PathWorldWriter writer;
    if (!writer.writeWorld(mWorld, filePath)) {
        error = writer.errorString();
        return false;
    }
    undoStack()->setClean();
    setFileName(filePath);
    return true;
}

void PathDocument::setCurrentLevel(WorldLevel *wlevel)
{
    if (wlevel == mCurrentLevel) return;
    mCurrentLevel = wlevel;
    emit currentLevelChanged(mCurrentLevel);
}

void PathDocument::setCurrentPathLayer(WorldPathLayer *layer)
{
    if (layer == mCurrentPathLayer) return;
    mCurrentPathLayer = layer;
    setCurrentLevel(layer ? layer->wlevel() : 0);
    emit currentPathLayerChanged(mCurrentPathLayer);
}

void PathDocument::setCurrentTileLayer(WorldTileLayer *layer)
{
    if (layer == mCurrentTileLayer) return;
    mCurrentTileLayer = layer;
    setCurrentLevel(layer ? layer->wlevel() : 0);
    emit currentTileLayerChanged(mCurrentTileLayer);
}

void PathDocument::afterAddNode(WorldNode *node)
{
    node->layer->lookup()->nodeAdded(node);
}

void PathDocument::afterRemoveNode(WorldPathLayer *layer, int index, WorldNode *node)
{
    Q_UNUSED(index)
    layer->lookup()->nodeRemoved(node);
}

void PathDocument::afterMoveNode(WorldNode *node, const QPointF &prev)
{
    Q_UNUSED(prev)
    node->layer->lookup()->nodeMoved(node);
}

void PathDocument::afterAddPath(WorldPathLayer *layer, int index, WorldPath *path)
{
    Q_UNUSED(layer)
    Q_UNUSED(index)
    layer->lookup()->pathAdded(path);
    if (!path->isVisible())
        mHiddenPaths += path;
}

void PathDocument::afterRemovePath(WorldPathLayer *layer, int index, WorldPath *path)
{
    Q_UNUSED(index)
    layer->lookup()->pathRemoved(path);
    if (!path->isVisible())
        mHiddenPaths.remove(path);
}

void PathDocument::afterAddNodeToPath(WorldPath *path, int index, WorldNode *node)
{
    Q_UNUSED(index)
    Q_UNUSED(node)
    path->layer()->lookup()->pathChanged(path);
}

void PathDocument::afterRemoveNodeFromPath(WorldPath *path, int index, WorldNode *node)
{
    Q_UNUSED(index)
    Q_UNUSED(node)
    path->layer()->lookup()->pathChanged(path);
}

void PathDocument::afterAddScriptToPath(WorldPath *path, int index, WorldScript *script)
{
    Q_UNUSED(path)
    Q_UNUSED(index)
    path->layer()->lookup()->scriptAdded(script);
}

void PathDocument::afterRemoveScriptFromPath(WorldPath *path, int index, WorldScript *script)
{
    Q_UNUSED(path)
    Q_UNUSED(index)
    path->layer()->lookup()->scriptRemoved(script);
}

void PathDocument::afterSetPathVisible(WorldPath *path, bool visible)
{
    if (visible)
        mHiddenPaths.remove(path);
    else
        mHiddenPaths += path;
}
