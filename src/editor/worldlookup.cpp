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
    // FIXME: when inserting or deleting objects, the index becomes invalid
    int index = 0;
    int scriptIndex = 0;

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
        foreach (WorldPath *path, layer->paths()) {
            mPathTree.last()->Add(index++, path);

            foreach (WorldScript *ws, path->scripts()) {
                if (ws->mRegion.isEmpty()) {
                    qCritical("script has empty region - IGNORING");
                    continue;
                }
                mScriptTree->Add(scriptIndex++, ws);
            }
            scriptIndex += 10 - scriptIndex % 10; // FIXME: see scriptAdded()
        }
    }

    foreach (WorldScript *ws, mWorld->scripts()) {
        if (ws->mRegion.isEmpty()) {
            qCritical("script has empty region - IGNORING");
            continue;
        }
        mScriptTree->Add(scriptIndex++, ws);
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
    WorldQuadSubTree<WorldScript>::ObjectList objects;
    mScriptTree->GetObjects(topLeft.x, topLeft.y,
                            LOOKUP_LENGTH(bounds.width()),
                            LOOKUP_LENGTH(bounds.height()),
                            objects);
    foreach (WorldQuadTreeObject<WorldScript> *o, objects) {
        Q_ASSERT(o->data);
        ret[o->index] = o->data;
    }
    return ret.values();
}

QList<WorldPath *> WorldLookup::paths(WorldPathLayer *layer, const QRectF &bounds) const
{
    LookupCoordinate topLeft(bounds.topLeft());
    QMap<int,WorldPath *> ret;
    WorldQuadSubTree<WorldPath>::ObjectList objects;
    mPathTree[mWorld->indexOf(layer)]->GetObjects(topLeft.x, topLeft.y,
                                                  LOOKUP_LENGTH(bounds.width()),
                                                  LOOKUP_LENGTH(bounds.height()),
                                                  objects);
    foreach (WorldQuadTreeObject<WorldPath> *o, objects) {
        Q_ASSERT(o->data);
        ret[o->index] = o->data;
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
    WorldQuadSubTree<WorldPath>::ObjectList objects;
    mPathTree[mWorld->indexOf(layer)]->GetObjects(lpoly, objects);
    foreach (WorldQuadTreeObject<WorldPath> *o, objects) {
        Q_ASSERT(o->data);
        ret[o->index] = o->data;
    }
    return ret.values();
}

QList<WorldNode *> WorldLookup::nodes(WorldPathLayer *layer, const QRectF &bounds) const
{
    LookupCoordinate topLeft(bounds.topLeft());
    WorldQuadSubTree<WorldNode>::ObjectList objects;
    mNodeTree[mWorld->indexOf(layer)]->GetObjects(topLeft.x, topLeft.y,
                                                  LOOKUP_LENGTH(bounds.width()),
                                                  LOOKUP_LENGTH(bounds.height()),
                                                  objects);
    QMap<int,WorldNode *> ret;
    foreach (WorldQuadTreeObject<WorldNode> *o, objects) {
        Q_ASSERT(o->data);
        ret[o->index] = o->data;
    }
    return ret.values();
}

QList<WorldNode *> WorldLookup::nodes(WorldPathLayer *layer, const QPolygonF &poly) const
{
    static QPolygonF lpoly;
    lpoly.resize(0);
    foreach (QPointF p, poly)
        lpoly += LookupCoordinate(p).toPointF();
    WorldQuadSubTree<WorldNode>::ObjectList objects;
    mNodeTree[mWorld->indexOf(layer)]->GetObjects(lpoly, objects);
    QMap<int,WorldNode *> ret;
    foreach (WorldQuadTreeObject<WorldNode> *o, objects) {
        Q_ASSERT(o->data);
        ret[o->index] = o->data;
    }
    return ret.values();
}

void WorldLookup::nodeAdded(WorldNode *node)
{
    mNodeTree[mWorld->indexOf(node->layer)]->Add(node->layer->indexOf(node), node);
}

void WorldLookup::nodeRemoved(WorldPathLayer *layer, WorldNode *node)
{
    mNodeTree[mWorld->indexOf(layer)]->Remove(node);
}

void WorldLookup::nodeMoved(WorldNode *node)
{
    mNodeTree[mWorld->indexOf(node->layer)]->Move(node);
    foreach (WorldPath *path, node->mPaths.keys()) {// FIXME: do this once after all the nodes have moved
        path->nodeMoved();
        mPathTree[mWorld->indexOf(path->layer())]->Move(path);
    }
}

void WorldLookup::pathAdded(WorldPath *path)
{
    mPathTree[mWorld->indexOf(path->layer())]->Add(path->layer()->indexOf(path), path);
}

void WorldLookup::pathRemoved(WorldPathLayer *layer, WorldPath *path)
{
    mPathTree[mWorld->indexOf(layer)]->Remove(path);
}

void WorldLookup::pathChanged(WorldPath *path)
{
    mPathTree[mWorld->indexOf(path->layer())]->Move(path);
}

void WorldLookup::scriptAdded(WorldScript *script)
{
    // A script was added to a Path.  The 'index' should be the position in a
    // global list of scripts in the order they are applied.
    WorldPath *path = script->mPaths.first();
    int index = path->layer()->indexOf(path) * 10 + path->scripts().indexOf(script); // FIXME: totally bogus, must push following scripts up
    mScriptTree->Add(index, script);
}

void WorldLookup::scriptRemoved(WorldScript *script)
{
    mScriptTree->Remove(script);
}

void WorldLookup::scriptRegionChanged(WorldScript *script)
{
    mScriptTree->Move(script);
}

/////

template<class T>
WorldQuadTreeObject<T>::WorldQuadTreeObject(int index, T *data) :
    owner(0), index(index), data(data)
{
    updateBounds(data);
}

template<class T>
void WorldQuadTreeObject<T>::updateBounds(WorldNode *node)
{
    bounds.x = LookupCoordinate(node->pos()).x;
    bounds.y = LookupCoordinate(node->pos()).y;
    bounds.width = 1;
    bounds.height = 1;
}

template<class T>
void WorldQuadTreeObject<T>::updateBounds(WorldPath *path)
{
    bounds.x = LookupCoordinate(path->bounds().topLeft()).x;
    bounds.y = LookupCoordinate(path->bounds().topLeft()).y;
    bounds.width = qMax(1UL, LOOKUP_LENGTH(path->bounds().width()));
    bounds.height = qMax(1UL,LOOKUP_LENGTH(path->bounds().height()));
}

template<class T>
void WorldQuadTreeObject<T>::updateBounds(WorldScript *script)
{
    bounds.x = LookupCoordinate(script->mRegion.boundingRect().topLeft()).x;
    bounds.y = LookupCoordinate(script->mRegion.boundingRect().topLeft()).y;
    bounds.width = qMax(1UL,LOOKUP_LENGTH(script->mRegion.boundingRect().width()));
    bounds.height = qMax(1UL,LOOKUP_LENGTH(script->mRegion.boundingRect().height()));
}

template<class T>
bool WorldQuadTreeObject<T>::intersects(const QuadRect &rect)
{
    return bounds.intersects(rect);
}

template<class T>
bool WorldQuadTreeObject<T>::intersects(const QPolygonF &poly)
{
    QRectF selfRect(bounds.x, bounds.y, bounds.width, bounds.height);
    return poly.boundingRect().intersects(selfRect);
}

template<class T>
bool WorldQuadTreeObject<T>::contains(LookupCoordType x, LookupCoordType y)
{
    if (bounds.contains(x, y)) {
        return true; // TODO: proper hit-testing
    }
    return false;
}

/////

template<class T>
WorldQuadSubTree<T>::WorldQuadSubTree(LookupCoordType x, LookupCoordType y,
                                      LookupCoordType width, LookupCoordType height) :
    bounds(x, y, width, height),
    depth(0),
    parent(0),
    NW(0), NE(0), SW(0), SE(0)
{
}

template<class T>
WorldQuadSubTree<T>::WorldQuadSubTree(WorldQuadSubTree *parent,
                                      LookupCoordType x, LookupCoordType y,
                                      LookupCoordType width, LookupCoordType height) :
    bounds(x, y, width, height),
    depth(parent->depth + 1),
    parent(parent),
    NW(0), NE(0), SW(0), SE(0)
{
}

template<class T>
WorldQuadSubTree<T>::~WorldQuadSubTree()
{
    delete NW;
    delete NE;
    delete SW;
    delete SE;
}

template<class T>
void WorldQuadSubTree<T>::Add(WorldQuadTreeObject<T> *object)
{
    objects += object;
    object->owner = this;
}

template<class T>
void WorldQuadSubTree<T>::Remove(WorldQuadTreeObject<T> *object)
{
    int n = objects.indexOf(object);
    Q_ASSERT(n >= 0);
    if (n >= 0) {
        objects.removeAt(n);
        object->owner = 0;
    }
}

template<class T>
void WorldQuadSubTree<T>::Subdivide()
{
    LookupCoordType w1 = bounds.width / 2, w2 = bounds.width - w1;
    LookupCoordType h1 = bounds.height / 2, h2 = bounds.height - h1;
    NW = new WorldQuadSubTree<T>(this, bounds.x, bounds.y, w1, h1);
    NE = new WorldQuadSubTree<T>(this, bounds.x + w1, bounds.y, w2, h1);
    SW = new WorldQuadSubTree<T>(this, bounds.x, bounds.y + h1, w1, h2);
    SE = new WorldQuadSubTree<T>(this, bounds.x + w1, bounds.y + h1, w2, h2);

    for (int i = 0; i < objects.size(); i++) {
        WorldQuadTreeObject<T> *o = objects[i];
        WorldQuadSubTree<T> *dest = GetDestinationTree(o);
        if (dest != this) {
            Remove(o);
            dest->Insert(o);
            i--;
        }
    }
}

template<class T>
WorldQuadSubTree<T> *WorldQuadSubTree<T>::GetDestinationTree(WorldQuadTreeObject<T> *object)
{
    if (NW->contains(object)) return NW;
    if (NE->contains(object)) return NE;
    if (SW->contains(object)) return SW;
    if (SE->contains(object)) return SE;
    return this;
}

template<class T>
void WorldQuadSubTree<T>::Relocate(WorldQuadTreeObject<T> *object)
{
    if (contains(object)) {
        if (NW) {
            WorldQuadSubTree<T> *dest = GetDestinationTree(object);
            Q_ASSERT(object->owner);
            if (dest != object->owner) {
                WorldQuadSubTree<T> *former = object->owner;
                Delete(object, false);
                dest->Insert(object);

                former->CleanUpwards();
            }
        }
    } else if (parent) {
        parent->Relocate(object);
    }
}

template<class T>
void WorldQuadSubTree<T>::CleanUpwards()
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

template<class T>
void WorldQuadSubTree<T>::Delete(WorldQuadTreeObject<T> *object, bool clean)
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

template<class T>
void WorldQuadSubTree<T>::Insert(WorldQuadTreeObject<T> *object)
{
    Q_ASSERT(object->owner == 0);
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

template<class T>
void WorldQuadSubTree<T>::GetObjects(const QuadRect &rect, ObjectList &ret)
{
    if (rect.contains(bounds))
        GetAllObjects(ret);
    else if (intersects(rect)) {
        if (objects.size()) {
            foreach (WorldQuadTreeObject<T> *o, objects) {
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

template<class T>
void WorldQuadSubTree<T>::GetObjects(const QPolygonF &poly, ObjectList &ret)
{
    if (poly.boundingRect().contains(QRectF(bounds.x, bounds.y, bounds.width, bounds.height)))
        GetAllObjects(ret);
    else if (intersects(poly)) {
        if (objects.size()) {
            foreach (WorldQuadTreeObject<T> *o, objects) {
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

template<class T>
void WorldQuadSubTree<T>::GetObjects(LookupCoordType x, LookupCoordType y, ObjectList &ret)
{
    if (contains(x, y)) {
        if (objects.size()) {
            foreach (WorldQuadTreeObject<T> *o, objects) {
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

template<class T>
void WorldQuadSubTree<T>::GetAllObjects(ObjectList &ret)
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

template<class T>
void WorldQuadSubTree<T>::Move(WorldQuadTreeObject<T> *object)
{
    if (object->owner)
        object->owner->Relocate(object);
    else
        Relocate(object);
}

template<class T>
void WorldQuadSubTree<T>::Clear() {
    if (NW) {
        NW->Clear();
        NE->Clear();
        SW->Clear();
        SE->Clear();
        NW = NE = SW = SE = 0;
    }
    objects.clear();
}

template<class T>
bool WorldQuadSubTree<T>::contains(WorldQuadTreeObject<T> *object) {
    return contains(object->bounds);
}

template<class T>
int WorldQuadSubTree<T>::ObjectCount()
{
    int count = objects.size();
    if (NW) {
        count += NW->ObjectCount() + NE->ObjectCount() + SW->ObjectCount() + SE->ObjectCount();
    }
    return count;
}

template<class T>
bool WorldQuadSubTree<T>::contains(const QuadRect &rect)
{
    return bounds.contains(rect);
}

template<class T>
bool WorldQuadSubTree<T>::contains(LookupCoordType x, LookupCoordType y)
{
    return bounds.contains(x, y);
}

template<class T>
bool WorldQuadSubTree<T>::intersects(const QuadRect &rect)
{
    return bounds.intersects(rect);
}

template<class T>
bool WorldQuadSubTree<T>::intersects(const QPolygonF &poly)
{
    QRectF selfRect(bounds.x, bounds.y, bounds.width, bounds.height);
    return poly.boundingRect().intersects(selfRect);
}
