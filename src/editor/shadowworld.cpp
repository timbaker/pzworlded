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

#include "shadowworld.h"

#include "path.h"
#include "pathworld.h"
#include "worldlookup.h"

#include <QDebug>

namespace ShadowWorldModifiers {
class MoveNode;
class OffsetNode;
}

class WorldModifier
{
public:
    virtual ~WorldModifier() {}

    virtual void redo() = 0;
    virtual void undo() = 0;

    virtual ShadowWorldModifiers::MoveNode *asMoveNode() { return 0; }
    virtual ShadowWorldModifiers::OffsetNode *asOffsetNode() { return 0; }
};

namespace ShadowWorldModifiers {

class MoveNode : public WorldModifier
{
public:
    MoveNode(WorldNode *node, const QPointF &pos) :
        mNode(node),
        mPos(pos)
    {
    }

    void redo() { swap(); }
    void undo() { swap(); }

    void swap()
    {
        QPointF old = mNode->p;
        mNode->p = mPos;
        mPos = old;
    }

    MoveNode *asMoveNode() { return this; }

    WorldNode *mNode;
    QPointF mPos;
};

class OffsetNode : public WorldModifier
{
public:
    OffsetNode(WorldNode *node, const QPointF &offset) :
        mNode(node),
        mOffset(offset)
    {
    }

    void redo() { mNode->p += mOffset; }
    void undo() { mNode->p -= mOffset; }

    OffsetNode *asOffsetNode() { return this; }

    WorldNode *mNode;
    QPointF mOffset;
};

} // namespace ShadowWorldModifiers

class WorldChange
{
public:
    virtual void apply(ShadowWorld *world) = 0;
};

namespace ShadowWorldChanges {

class NodeMoved : public WorldChange
{
public:
    NodeMoved(WorldNode *node) : id(node->id), p(node->pos()) {}
    virtual void apply(ShadowWorld *world)
    {
        foreach (WorldPathLayer *wpl, world->pathLayers()) {
            if (WorldNode *node = wpl->node(id)) {
                world->nodeMoved(node->p, p);
                node->p = p;
                world->lookup()->nodeMoved(node);
                break;
            }
        }
    }

    id_t id;
    QPointF p;
};

}

using namespace ShadowWorldModifiers;
using namespace ShadowWorldChanges;

/////

ShadowWorld::ShadowWorld(PathWorld *orig) :
    PathWorld(orig->width(), orig->height()),
    mOrig(orig)
{
    mOrig->initClone(this);
    mLookup = new WorldLookup(this);
}

void ShadowWorld::addMoveNode(WorldNode *node, const QPointF &pos)
{
    foreach (WorldModifier *m, mModifiers) {
        if (m->asMoveNode() && m->asMoveNode()->mNode == node) {
            m->asMoveNode()->mNode->p = pos;
            return;
        }
    }
    mModifiers += new MoveNode(node, pos);
    mModifiers.last()->redo();
}

void ShadowWorld::removeMoveNode(WorldNode *node)
{
    foreach (WorldModifier *m, mModifiers) {
        if (m->asMoveNode() && m->asMoveNode()->mNode == node) {
            m->undo();
            mModifiers.removeOne(m);
            delete m;
        }
    }
}

void ShadowWorld::addOffsetNode(WorldNode *node, const QPointF &offset)
{
    foreach (WorldModifier *m, mModifiers) {
        if (m->asOffsetNode() && m->asOffsetNode()->mNode == node) {
            m->asOffsetNode()->undo();
            m->asOffsetNode()->mOffset = offset;
            m->asOffsetNode()->redo();
            return;
        }
    }
    mModifiers += new OffsetNode(node, offset);
    mModifiers.last()->redo();
}

void ShadowWorld::removeOffsetNode(WorldNode *node)
{
    foreach (WorldModifier *m, mModifiers) {
        if (m->asOffsetNode() && m->asOffsetNode()->mNode == node) {
            m->undo();
            mModifiers.removeOne(m);
            delete m;
        }
    }
}

void ShadowWorld::nodeMoved(WorldNode *node)
{
    if (mChanges.isEmpty())
        QMetaObject::invokeMethod(this, "processChanges", Qt::QueuedConnection);
    mChanges += new NodeMoved(node);
}

void ShadowWorld::nodeMoved(const QPointF &oldPos, const QPointF &newPos)
{
    if (mMovedNodeArea.isNull()) {
        mMovedNodeArea = QRectF(oldPos, newPos).normalized();
    } else {
        mMovedNodeArea |= QRectF(oldPos, newPos).normalized();
    }
}

void ShadowWorld::processChanges()
{
    mMovedNodeArea = QRectF();

    foreach (WorldChange *c, mChanges) {
        c->apply(this);
        delete c;
    }
    mChanges.clear();

    if (!mMovedNodeArea.isNull()) {
        // Tell WorldLookup about all the Paths that are affected

        emit nodesMoved(mMovedNodeArea);
    }
}
