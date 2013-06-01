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

#include <QDir>
#include <QFileInfo>
#include <QImageReader>
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
#if 0
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
#endif

#if 1
    addTexturesInDirectory(appDir() + QLatin1String("/../Textures"));
#else
    if (mWorld->textureList().isEmpty()) {

    WorldTexture *wtex = new WorldTexture;
    wtex->mGLid = 0;
    wtex->mFileName = QLatin1String("C:/Users/Tim/Desktop/ProjectZomboid/francegrassfull.jpg");
    wtex->mName = QFileInfo(wtex->mFileName).baseName();
    QImageReader ir(wtex->mFileName);
    wtex->mSize = ir.size();
    mWorld->textureList() += wtex;
    mWorld->textureMap()[wtex->mName] = wtex;

    wtex = new WorldTexture;
    wtex->mGLid = 0;
    wtex->mFileName = QLatin1String("C:/Users/Tim/Desktop/ProjectZomboid/pebbles.png");
    wtex->mName = QFileInfo(wtex->mFileName).baseName();
    ir.setFileName(wtex->mFileName);
    wtex->mSize = ir.size();
    mWorld->textureList() += wtex;
    mWorld->textureMap()[wtex->mName] = wtex;

    QDir fo(QLatin1String("C:/Users/Tim/Desktop/ProjectZomboid/street_lines"));
    QStringList filters(QString::fromLatin1("*.png"));
    foreach (QFileInfo info, fo.entryInfoList(filters)) {
        wtex = new WorldTexture;
        wtex->mGLid = 0;
        wtex->mFileName = info.filePath();
        wtex->mName = info.baseName();
        ir.setFileName(wtex->mFileName);
        wtex->mSize = ir.size();
        mWorld->textureList() += wtex;
        mWorld->textureMap()[wtex->mName] = wtex;
    }

    fo.setPath(QLatin1String("C:/Users/Tim/Desktop/ProjectZomboid/real_asphalt_texture_pack"));
//    QStringList filters(QString::fromLatin1("*.png"));
    foreach (QFileInfo info, fo.entryInfoList(filters)) {
        wtex = new WorldTexture;
        wtex->mGLid = 0;
        wtex->mFileName = info.filePath();
        wtex->mName = info.baseName();
        ir.setFileName(wtex->mFileName);
        wtex->mSize = ir.size();
        mWorld->textureList() += wtex;
        mWorld->textureMap()[wtex->mName] = wtex;
    }

    fo.setPath(QLatin1String("C:/Users/Tim/Desktop/ProjectZomboid/grass"));
//    QStringList filters(QString::fromLatin1("*.png"));
    foreach (QFileInfo info, fo.entryInfoList(filters)) {
        wtex = new WorldTexture;
        wtex->mGLid = 0;
        wtex->mFileName = info.filePath();
        wtex->mName = info.baseName();
        ir.setFileName(wtex->mFileName);
        wtex->mSize = ir.size();
        mWorld->textureList() += wtex;
        mWorld->textureMap()[wtex->mName] = wtex;
    }

    } else {
        foreach (WorldTexture *tex, mWorld->textureList()) {
            QImageReader ir(tex->mFileName);
            tex->mSize = ir.size(); // FIXME: do this IFF texture is needed
        }
    }
#endif

#if 0
    // Now that the script regions are up-to-date...
    foreach (WorldLevel *wlevel, mWorld->levels())
        foreach (WorldPathLayer *layer, wlevel->pathLayers())
            layer->initLookup();
#endif

    setCurrentLevel(mWorld->levelAt(0));
    setCurrentPathLayer(mWorld->levelAt(0)->pathLayerAt(0));
    setCurrentTileLayer(mWorld->levelAt(0)->tileLayerAt(0));

    connect(mChanger, SIGNAL(afterAddPathSignal(WorldPathLayer*,int,WorldPath*)),
            SLOT(afterAddPath(WorldPathLayer*,int,WorldPath*)));
    connect(mChanger, SIGNAL(afterRemovePathSignal(WorldPathLayer*,int,WorldPath*)),
            SLOT(afterRemovePath(WorldPathLayer*,int,WorldPath*)));
    connect(mChanger, SIGNAL(afterReorderPathSignal(WorldPath*,int)),
            SLOT(afterReorderPath(WorldPath*,int)));
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


void PathDocument::afterMoveNode(WorldNode *node, const QPointF &prev)
{
    Q_UNUSED(prev)
    node->layer()->lookup()->nodeMoved(node);
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

void PathDocument::afterReorderPath(WorldPath *path, int oldIndex)
{
    Q_UNUSED(oldIndex);
    path->layer()->lookup()->pathReordered(path, oldIndex);
}

void PathDocument::afterAddNodeToPath(WorldPath *path, int index, WorldNode *node)
{
    Q_UNUSED(index)
    Q_UNUSED(node)
    path->layer()->lookup()->nodeAdded(node);
    path->layer()->lookup()->pathChanged(path);
}

void PathDocument::afterRemoveNodeFromPath(WorldPath *path, int index, WorldNode *node)
{
    Q_UNUSED(index)
    Q_UNUSED(node)
    path->layer()->lookup()->nodeRemoved(node);
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

void PathDocument::addTexturesInDirectory(const QString &path)
{
    QDir dir(path);
    dir.setFilter(QDir::Files | QDir::AllDirs | QDir::NoDotAndDotDot);
    QStringList nameFilters;
    foreach (QByteArray format, QImageReader::supportedImageFormats())
        nameFilters += QLatin1String("*.") + QString::fromAscii(format);
    dir.setSorting(QDir::Name | QDir::DirsFirst);

    QFileInfoList fileInfoList = dir.entryInfoList(nameFilters);
    foreach (QFileInfo fileInfo, fileInfoList) {
        if (fileInfo.isDir()) {
            addTexturesInDirectory(fileInfo.absoluteFilePath());
            continue;
        }
        QImageReader ir(fileInfo.absoluteFilePath());
        QSize size = ir.size();
        if (size.isValid()) {
            if (mWorld->textureMap().contains(fileInfo.baseName())) {
                mWorld->textureMap()[fileInfo.baseName()]->mSize = size;
                continue;
            }
            WorldTexture *wtex = new WorldTexture;
            wtex->mFileName = fileInfo.filePath();
            wtex->mName = fileInfo.baseName();
            wtex->mSize = size;
            mWorld->textureList() += wtex;
            mWorld->textureMap()[wtex->mName] = wtex;
        }
    }
}
