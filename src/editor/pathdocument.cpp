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

#include "path.h"
#include "pathworld.h"
#include "worldchanger.h"
#include "worldlookup.h"

#include <QUndoStack>

PathDocument::PathDocument(PathWorld *world, const QString &fileName) :
    Document(PathDocType),
    mWorld(world),
    mFileName(fileName),
    mLookup(new WorldLookup(world)),
    mChanger(new WorldChanger)
{
    mUndoStack = new QUndoStack(this);

    connect(mChanger, SIGNAL(afterAddNodeSignal(WorldNode*)),
            SLOT(afterAddNode(WorldNode*)));
    connect(mChanger, SIGNAL(afterRemoveNodeSignal(WorldPathLayer*,int,WorldNode*)),
            SLOT(afterRemoveNode(WorldPathLayer*,int,WorldNode*)));
    connect(mChanger, SIGNAL(afterAddPathSignal(WorldPath*)),
            SLOT(afterAddPath(WorldPath*)));
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
}

PathDocument::~PathDocument()
{
    delete mLookup;
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
    return false;
}

void PathDocument::afterAddNode(WorldNode *node)
{
    lookup()->nodeAdded(node);
}

void PathDocument::afterRemoveNode(WorldPathLayer *layer, int index, WorldNode *node)
{
    lookup()->nodeRemoved(layer, node);
}

void PathDocument::afterMoveNode(WorldNode *node, const QPointF &prev)
{
    mLookup->nodeMoved(node);
}

void PathDocument::afterAddPath(WorldPath *path)
{
    lookup()->pathAdded(path);
}

void PathDocument::afterRemovePath(WorldPathLayer *layer, int index, WorldPath *path)
{
    lookup()->pathRemoved(layer, path);
}

void PathDocument::afterAddNodeToPath(WorldPath *path, int index, WorldNode *node)
{
    lookup()->pathChanged(path);
}

void PathDocument::afterRemoveNodeFromPath(WorldPath *path, int index, WorldNode *node)
{
    lookup()->pathChanged(path);
}

void PathDocument::afterAddScriptToPath(WorldPath *path, int index, WorldScript *script)
{
    lookup()->scriptAdded(script);
}

void PathDocument::afterRemoveScriptFromPath(WorldPath *path, int index, WorldScript *script)
{
    lookup()->scriptRemoved(script);
}
