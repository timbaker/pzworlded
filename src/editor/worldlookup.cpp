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

#include "worldlookup.h"

#include "path.h"
#include "pathworld.h"

#include <QSet>

#define FUDGE 10e1

class LookupCoordinate
{
public:
    LookupCoordinate(const QPointF &wc)
    {
        x = wc.x() * FUDGE;
        y = wc.y() * FUDGE;
    }

    QPointF toPointF() const { return QPointF(x, y); }

    LookupCoordType x, y;
};

static LookupCoordType LOOKUP_LENGTH(qreal w)
{
    return w * FUDGE;
}

WorldLookup::WorldLookup(PathWorld *world) :
    mWorld(world),
    mScriptTree(new WorldQuadTree<WorldScript>(0, 0,
                                  LOOKUP_LENGTH(world->width()),
                                  LOOKUP_LENGTH(world->height())))
{
    int index = 0;
    foreach (WorldPathLayer *layer, mWorld->pathLayers()) {
        mNodeTree += new WorldQuadTree<WorldNode>(0, 0,
                                      LOOKUP_LENGTH(world->width()),
                                      LOOKUP_LENGTH(world->height()));
        mPathTree += new WorldQuadTree<WorldPath>(0, 0,
                                      LOOKUP_LENGTH(world->width()),
                                      LOOKUP_LENGTH(world->height()));
        index = 0;
        foreach (WorldNode *node, layer->nodes())
            mNodeTree.last()->Add(index++, node);

        index = 0;
        foreach (WorldPath *path, layer->paths())
            mPathTree.last()->Add(index++, path);
    }

    index = 0;
    foreach (WorldScript *ws, mWorld->scripts()) {
        if (ws->mRegion.isEmpty()) {
            qCritical("script has empty region - IGNORING");
            continue;
        }
        mScriptTree->Add(index++, ws);
    }
}

WorldLookup::~WorldLookup()
{
    qDeleteAll(mNodeTree);
    qDeleteAll(mPathTree);
    delete mScriptTree;
}

QList<WorldScript *> WorldLookup::scripts(const QRectF &bounds) const
{
    LookupCoordinate topLeft(bounds.topLeft());
    QMap<int,WorldScript *> ret;
    WorldQuadSubTree::ObjectList objects;
    mScriptTree->GetObjects(topLeft.x, topLeft.y,
                            LOOKUP_LENGTH(bounds.width()),
                            LOOKUP_LENGTH(bounds.height()),
                            objects);
    foreach (WorldQuadTreeObject *o, objects) {
        Q_ASSERT(o->script);
        ret[o->index] = o->script;
    }
    return ret.values();
}

QList<WorldPath *> WorldLookup::paths(WorldPathLayer *layer, const QRectF &bounds) const
{
    LookupCoordinate topLeft(bounds.topLeft());
    QMap<int,WorldPath *> ret;
    WorldQuadSubTree::ObjectList objects;
    mPathTree[mWorld->indexOf(layer)]->GetObjects(topLeft.x, topLeft.y,
                                                  LOOKUP_LENGTH(bounds.width()),
                                                  LOOKUP_LENGTH(bounds.height()),
                                                  objects);
    foreach (WorldQuadTreeObject *o, objects) {
        Q_ASSERT(o->path);
        ret[o->index] = o->path;
    }
    return ret.values();
}

QList<WorldPath *> WorldLookup::paths(WorldPathLayer *layer, const QPolygonF &poly) const
{
    static QPolygonF lpoly;
    lpoly.resize(0);
    foreach (QPointF p, poly)
        lpoly += LookupCoordinate(p).toPointF();

    QMap<int,WorldPath *> ret;
    WorldQuadSubTree::ObjectList objects;
    mPathTree[mWorld->indexOf(layer)]->GetObjects(lpoly, objects);
    foreach (WorldQuadTreeObject *o, objects) {
        Q_ASSERT(o->path);
        ret[o->index] = o->path;
    }
    return ret.values();
}

QList<WorldNode *> WorldLookup::nodes(WorldPathLayer *layer, const QRectF &bounds) const
{
    LookupCoordinate topLeft(bounds.topLeft());
    WorldQuadSubTree::ObjectList objects;
    mNodeTree[mWorld->indexOf(layer)]->GetObjects(topLeft.x, topLeft.y,
                                                  LOOKUP_LENGTH(bounds.width()),
                                                  LOOKUP_LENGTH(bounds.height()),
                                                  objects);
    QMap<int,WorldNode *> ret;
    foreach (WorldQuadTreeObject *o, objects) {
        Q_ASSERT(o->node);
        ret[o->index] = o->node;
    }
    return ret.values();
}

QList<WorldNode *> WorldLookup::nodes(WorldPathLayer *layer, const QPolygonF &poly) const
{
    static QPolygonF lpoly;
    lpoly.resize(0);
    foreach (QPointF p, poly)
        lpoly += LookupCoordinate(p).toPointF();
    WorldQuadSubTree::ObjectList objects;
    mNodeTree[mWorld->indexOf(layer)]->GetObjects(lpoly, objects);
    QMap<int,WorldNode *> ret;
    foreach (WorldQuadTreeObject *o, objects) {
        Q_ASSERT(o->node);
        ret[o->index] = o->node;
    }
    return ret.values();
}

void WorldLookup::nodeMoved(WorldNode *node, const QPointF &oldPos)
{
    mNodeTree[mWorld->indexOf(node->layer)]->Move(node);
}

/////

WorldQuadTreeObject::WorldQuadTreeObject(int index, WorldNode *node) :
    owner(0), index(index), node(node), path(0), script(0)
{
    bounds.x = LookupCoordinate(node->pos()).x;
    bounds.y = LookupCoordinate(node->pos()).y;
    bounds.width = 0;
    bounds.height = 0;
}

WorldQuadTreeObject::WorldQuadTreeObject(int index, WorldPath *path) :
    owner(0), index(index), node(0), path(path), script(0)
{
    bounds.x = LookupCoordinate(path->bounds().topLeft()).x;
    bounds.y = LookupCoordinate(path->bounds().topLeft()).y;
    bounds.width = LOOKUP_LENGTH(path->bounds().width());
    bounds.height = LOOKUP_LENGTH(path->bounds().height());
}

WorldQuadTreeObject::WorldQuadTreeObject(int index, WorldScript *script) :
    owner(0), index(index), node(0), path(0), script(script)
{
    bounds.x = LookupCoordinate(script->mRegion.boundingRect().topLeft()).x;
    bounds.y = LookupCoordinate(script->mRegion.boundingRect().topLeft()).y;
    bounds.width = LOOKUP_LENGTH(script->mRegion.boundingRect().width());
    bounds.height = LOOKUP_LENGTH(script->mRegion.boundingRect().height());
}

bool WorldQuadTreeObject::intersects(const QuadRect &rect)
{
    if (node) return rect.contains(bounds.x, bounds.y);

    return bounds.intersects(rect);
}

bool WorldQuadTreeObject::intersects(const QPolygonF &poly)
{
    if (node) return poly.boundingRect().contains(bounds.x, bounds.y);

    QRectF selfRect(bounds.x, bounds.y, bounds.width, bounds.height);
    return poly.boundingRect().intersects(selfRect);
}

bool WorldQuadTreeObject::contains(LookupCoordType x, LookupCoordType y)
{
    if (node) return false;

    if (bounds.contains(x, y)) {
        return true; // TODO: proper hit-testing
    }
    return false;
}

/////

WorldQuadSubTree::WorldQuadSubTree(LookupCoordType x, LookupCoordType y,
                             LookupCoordType width, LookupCoordType height) :
    parent(0),
    bounds(x, y, width, height),
    depth(0),
    NW(0), NE(0), SW(0), SE(0)
{
}

WorldQuadSubTree::WorldQuadSubTree(WorldQuadSubTree *parent,
                             LookupCoordType x, LookupCoordType y,
                             LookupCoordType width, LookupCoordType height) :
    parent(parent),
    bounds(x, y, width, height),
    depth(parent->depth + 1),
    NW(0), NE(0), SW(0), SE(0)
{
}

WorldQuadSubTree::~WorldQuadSubTree()
{
    delete NW;
    delete NE;
    delete SW;
    delete SE;
}

void WorldQuadSubTree::Add(WorldQuadTreeObject *object)
{
    objects += object;
    object->owner = this;
}

void WorldQuadSubTree::Remove(WorldQuadTreeObject *object)
{
    int n = objects.indexOf(object);
    if (n >= 0) {
        objects.removeAt(n);
        object->owner = 0;
    }
}

void WorldQuadSubTree::Subdivide()
{
    LookupCoordType w1 = bounds.width / 2, w2 = bounds.width - w1;
    LookupCoordType h1 = bounds.height / 2, h2 = bounds.height - h1;
    NW = new WorldQuadSubTree(this, bounds.x, bounds.y, w1, h1);
    NE = new WorldQuadSubTree(this, bounds.x + w1, bounds.y, w2, h1);
    SW = new WorldQuadSubTree(this, bounds.x, bounds.y + h1, w1, h2);
    SE = new WorldQuadSubTree(this, bounds.x + w1, bounds.y + h1, w2, h2);

    for (int i = 0; i < objects.size(); i++) {
        WorldQuadTreeObject *o = objects[i];
        WorldQuadSubTree *dest = GetDestinationTree(o);
        if (dest != this) {
            Remove(o);
            dest->Insert(o);
            i--;
        }
    }
}

WorldQuadSubTree *WorldQuadSubTree::GetDestinationTree(WorldQuadTreeObject *object)
{
    if (NW->contains(object)) return NW;
    if (NE->contains(object)) return NE;
    if (SW->contains(object)) return SW;
    if (SE->contains(object)) return SE;
    return this;
}

void WorldQuadSubTree::Relocate(WorldQuadTreeObject *object)
{
    if (contains(object)) {
        if (NW) {
            WorldQuadSubTree *dest = GetDestinationTree(object);
            Q_ASSERT(object->owner);
            if (dest != object->owner) {
                WorldQuadSubTree *former = object->owner;
                Delete(object, false);
                dest->Insert(object);

                former->CleanUpwards();
            }
        }
    } else if (parent) {
        parent->Relocate(object);
    }
}

void WorldQuadSubTree::CleanUpwards()
{
    if (NW) {
        if (NW->objects.isEmpty() &&
                NE->objects.isEmpty() &&
                SW->objects.isEmpty() &&
                SE->objects.isEmpty()) {
            delete NW;
            delete NE;
            delete SW;
            delete SE;
            NW = NE = SW = SE = 0;
            if (parent && objects.isEmpty())
                parent->CleanUpwards();
        }
    } else {
        if (parent && objects.isEmpty())
            parent->CleanUpwards();
    }
}

void WorldQuadSubTree::Delete(WorldQuadTreeObject *object, bool clean)
{
    if (object->owner) {
        if (object->owner == this) {
            Remove(object);
            if (clean) CleanUpwards();
        } else {
            object->owner->Delete(object, clean);
        }
    }
}

void WorldQuadSubTree::Insert(WorldQuadTreeObject *object)
{
    if (!contains(object)) {
        Q_ASSERT(parent == 0);
        if (parent == 0)
            Add(object);
        else
            return;
    }

    if (objects.isEmpty() || (!NW && objects.size() + 1 <= MaxObjectsPerNode)) {
        Add(object);
    } else {
        if (!NW)
            Subdivide();

        WorldQuadSubTree *dest = GetDestinationTree(object);
        if (dest == this)
            Add(object);
        else
            dest->Insert(object);
    }
}

void WorldQuadSubTree::GetObjects(const QuadRect &rect, ObjectList &ret)
{
    if (rect.contains(bounds))
        GetAllObjects(ret);
    else if (intersects(rect)) {
        if (objects.size()) {
            foreach (WorldQuadTreeObject *o, objects) {
                if (o->intersects(rect))
                    ret += o;
            }
        }
        if (NW) {
            NW->GetObjects(rect, ret);
            NE->GetObjects(rect, ret);
            SW->GetObjects(rect, ret);
            SE->GetObjects(rect, ret);
        }
    }
}

void WorldQuadSubTree::GetObjects(const QPolygonF &poly, ObjectList &ret)
{
    if (poly.boundingRect().contains(QRectF(bounds.x, bounds.y, bounds.width, bounds.height)))
        GetAllObjects(ret);
    else if (intersects(poly)) {
        if (objects.size()) {
            foreach (WorldQuadTreeObject *o, objects) {
                if (o->intersects(poly))
                    ret += o;
            }
        }
        if (NW) {
            NW->GetObjects(poly, ret);
            NE->GetObjects(poly, ret);
            SW->GetObjects(poly, ret);
            SE->GetObjects(poly, ret);
        }
    }
}

void WorldQuadSubTree::GetObjects(LookupCoordType x, LookupCoordType y, ObjectList &ret)
{
    if (contains(x, y)) {
        if (objects.size()) {
            foreach (WorldQuadTreeObject *o, objects) {
                if (o->contains(x, y))
                    ret += o;
            }
        }
        if (NW) {
            NW->GetObjects(x, y, ret);
            NE->GetObjects(x, y, ret);
            SW->GetObjects(x, y, ret);
            SE->GetObjects(x, y, ret);
        }
    }
}

void WorldQuadSubTree::GetAllObjects(ObjectList &ret)
{
    if (objects.size())
        ret += objects;
    if (NW) {
        NW->GetAllObjects(ret);
        NE->GetAllObjects(ret);
        SW->GetAllObjects(ret);
        SE->GetAllObjects(ret);
    }
}

void WorldQuadSubTree::Move(WorldQuadTreeObject *object)
{
    if (object->owner)
        object->owner->Relocate(object);
    else
        Relocate(object);
}

void WorldQuadSubTree::Clear() {
    if (NW) {
        NW->Clear();
        NE->Clear();
        SW->Clear();
        SE->Clear();
        NW = NE = SW = SE = 0;
    }
    objects.clear();
}

bool WorldQuadSubTree::contains(WorldQuadTreeObject *object) {
    if (object->node) return contains(object->bounds.x, object->bounds.y);
    return contains(object->bounds);
}

int WorldQuadSubTree::ObjectCount()
{
    int count = objects.size();
    if (NW) {
        count += NW->ObjectCount() + NE->ObjectCount() + SW->ObjectCount() + SE->ObjectCount();
    }
    return count;
}

bool WorldQuadSubTree::contains(const QuadRect &rect)
{
    return bounds.contains(rect);
}

bool WorldQuadSubTree::contains(LookupCoordType x, LookupCoordType y)
{
    return bounds.contains(x, y);
}

bool WorldQuadSubTree::intersects(const QuadRect &rect)
{
    return bounds.intersects(rect);
}

bool WorldQuadSubTree::intersects(const QPolygonF &poly)
{
    QRectF selfRect(bounds.x, bounds.y, bounds.width, bounds.height);
    return poly.boundingRect().intersects(selfRect);
}
