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

#include "worldchanger.h"

#include "path.h"

namespace WorldChanges
{

class MoveNode : public WorldChange
{
public:
    MoveNode(WorldChanger *changer, WorldNode *node, const QPointF &pos) :
        WorldChange(changer),
        mNode(node),
        mNewPos(pos),
        mOldPos(node->pos())
    {

    }

    void redo()
    {
        mNode->setPos(mNewPos);
        mChanger->afterMoveNode(mNode, mOldPos);
    }

    void undo()
    {
        mNode->setPos(mOldPos);
        mChanger->afterMoveNode(mNode, mNewPos);
    }

    QString text() const
    {
        return mChanger->tr("Move Node");
    }

    WorldNode *mNode;
    QPointF mNewPos;
    QPointF mOldPos;
};

} // namespace WorldChanges;

/////

WorldChange::WorldChange(WorldChanger *changer) :
    mChanger(changer)
{
}

WorldChange::~WorldChange()
{
}

void WorldChange::setChanger(WorldChanger *changer)
{
    mChanger = changer;
}

using namespace WorldChanges;

/////

WorldChanger::WorldChanger()
{
}

WorldChanger::~WorldChanger()
{
    qDeleteAll(mChanges);
}

void WorldChanger::doMoveNode(WorldNode *node, const QPointF &pos)
{
    MoveNode *c = new MoveNode(this, node, pos);
    mChanges += c;
    mChangesReversed.insert(0, c);
    c->redo();
}

void WorldChanger::afterMoveNode(WorldNode *node, const QPointF &prev)
{
    emit afterMoveNodeSignal(node, prev);
}

WorldChangeList WorldChanger::takeChanges()
{
    undo(); /////
    WorldChangeList changes = mChanges;
    mChanges.clear();
    mChangesReversed.clear();
    return changes;
}

void WorldChanger::undoAndForget()
{
    qDeleteAll(takeChanges());
}

void WorldChanger::undo()
{
    foreach (WorldChange *c, mChangesReversed)
        c->undo();
}

