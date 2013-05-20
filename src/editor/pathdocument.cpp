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

    connect(mChanger, SIGNAL(afterMoveNodeSignal(WorldNode*,QPointF)),
            SLOT(afterMoveNode(WorldNode*)));
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

void PathDocument::afterMoveNode(WorldNode *node)
{
    mLookup->nodeMoved(node);
}
