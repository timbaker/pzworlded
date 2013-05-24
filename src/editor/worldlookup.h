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

#ifndef WORLDLOOKUP_H
#define WORLDLOOKUP_H

#include <QList>
#include <QMap>
#include <QPolygonF>
#include <QRectF>
#include <QVector>

typedef unsigned long LookupCoordType;

class QuadRect
{
public:
    QuadRect() :
        x(0), y(0), width(0), height(0)
    {}
    QuadRect(LookupCoordType x, LookupCoordType y, LookupCoordType width, LookupCoordType height) :
        x(x), y(y), width(width), height(height)
    {}

    bool contains(LookupCoordType x1, LookupCoordType y1) const
    {
        return (x1 >= x && x1 < x + width)
                && (y1 >= y && y1 < y + height);
    }

    bool contains(const QuadRect &other) const
    {
        return (other.x >= x) && (other.x + other.width <= x + width)
                && (other.y >= y) && (other.y + other.height <= y + height);
    }

    bool intersects(const QuadRect &other) const
    {
        return (other.x + other.width > x) && (other.x < x + width) &&
                (other.y + other.height > y) && (other.y < y + height);
    }

    LookupCoordType x;
    LookupCoordType y;
    LookupCoordType width;
    LookupCoordType height;
};

class WorldNode;
class WorldPath;
class WorldPathLayer;
class WorldScript;

template<class T> class WorldQuadSubTree;

template<class T>
class WorldQuadTreeObject
{
public:
    WorldQuadTreeObject(int index, T *data);

    void updateBounds(WorldNode *node);
    void updateBounds(WorldPath *path);
    void updateBounds(WorldScript *script);

    bool intersects(const QuadRect &rect);
    bool intersects(const QPolygonF &poly);
    bool contains(LookupCoordType x, LookupCoordType y);

    WorldQuadSubTree<T> *owner;
    int index;
    T *data;
    QuadRect bounds;
};

// Yuck.
template<class T>
struct ObjectList
{
    typedef QList<WorldQuadTreeObject<T> *> type;
};


// http://gamedev.stackexchange.com/questions/20607/quad-tree-with-a-lot-of-moving-objects
template<class T>
class WorldQuadSubTree {
public:
    WorldQuadSubTree(LookupCoordType x, LookupCoordType y,
                  LookupCoordType width, LookupCoordType height);
    WorldQuadSubTree(WorldQuadSubTree *parent,
                  LookupCoordType x, LookupCoordType y,
                  LookupCoordType width, LookupCoordType height);

    ~WorldQuadSubTree();

    void Add(WorldQuadTreeObject<T> *object);
    void Remove(WorldQuadTreeObject<T> *object);
    void Subdivide();
    WorldQuadSubTree *GetDestinationTree(WorldQuadTreeObject<T> *object);
    void Relocate(WorldQuadTreeObject<T> *object);
    void CleanUpwards();
    void Delete(WorldQuadTreeObject<T> *object, bool clean);
    void Insert(WorldQuadTreeObject<T> *object);

    typedef QList<WorldQuadTreeObject<T> *> ObjectList;

    void GetObjects(LookupCoordType x, LookupCoordType y,
                    LookupCoordType width, LookupCoordType height,
                    ObjectList &ret)
    { GetObjects(QuadRect(x, y, width, height), ret); }
    void GetObjects(const QuadRect &rect, ObjectList &ret);
    void GetObjects(const QPolygonF &poly, ObjectList &ret);
    void GetObjects(LookupCoordType x, LookupCoordType y, ObjectList &ret);
    void GetAllObjects(ObjectList &ret);

    void Move(WorldQuadTreeObject<T> *object);

    void Clear();

    int ObjectCount();

private:
    QuadRect bounds;
    ObjectList objects;
    int depth;

    WorldQuadSubTree<T> *parent;
    WorldQuadSubTree<T> *NW;
    WorldQuadSubTree<T> *NE;
    WorldQuadSubTree<T> *SW;
    WorldQuadSubTree<T> *SE;

    static const int MaxObjectsPerNode = 50;

    bool contains(const QuadRect &rect);
    bool contains(LookupCoordType x, LookupCoordType y);
    bool intersects(const QuadRect &rect);
    bool intersects(const QPolygonF &poly);
    bool contains(WorldQuadTreeObject<T> *object);

};

template<class T>
class WorldQuadTree
{
public:
    WorldQuadTree(LookupCoordType x, LookupCoordType y,
                  LookupCoordType width, LookupCoordType height) :
        mRoot(new WorldQuadSubTree<T>(x, y, width, height))
    {

    }

    void Add(int index, T *object)
    {
        Q_ASSERT(!mMap.contains(object));
        WorldQuadTreeObject<T> *wrapped = new WorldQuadTreeObject<T>(index, object);
        mMap[object] = wrapped;
        mRoot->Insert(wrapped);
    }

    void Remove(T *object)
    {
        Q_ASSERT(mMap.contains(object));
        if (mMap.contains(object)) {
            mRoot->Delete(mMap[object], true);
            mMap.remove(object);
        }
    }

    void Move(T *object)
    {
        Q_ASSERT(mMap.contains(object));
        if (mMap.contains(object)) {
            mMap[object]->updateBounds(object);
            mRoot->Move(mMap[object]);
        }
    }

    typedef QList<WorldQuadTreeObject<T> *> ObjectList;

    void GetObjects(LookupCoordType x, LookupCoordType y,
                    LookupCoordType width, LookupCoordType height,
                    ObjectList &ret)
    {
        GetObjects(QuadRect(x, y, width, height), ret);
    }

    void GetObjects(const QuadRect &rect, ObjectList &ret)
    {
        mRoot->GetObjects(rect, ret);
    }

    void GetObjects(const QPolygonF &poly, ObjectList &ret)
    {
        mRoot->GetObjects(poly, ret);
    }

    void GetObjects(LookupCoordType x, LookupCoordType y, ObjectList &ret)
    {
        mRoot->GetObjects(x, y, ret);
    }

    WorldQuadSubTree<T> *mRoot;
    QMap<T*,WorldQuadTreeObject<T>*> mMap;
};

class WorldLookup
{
public:
    WorldLookup(WorldPathLayer *layer);
    ~WorldLookup();

    QList<WorldScript*> scripts(const QRectF &bounds) const;
    QList<WorldPath*> paths(const QRectF &bounds) const;
    QList<WorldPath*> paths(const QPolygonF &poly) const;
    QList<WorldNode*> nodes(const QRectF &bounds) const;
    QList<WorldNode*> nodes(const QPolygonF &bounds) const;

    void nodeAdded(WorldNode *node);
    void nodeRemoved(WorldPathLayer *layer, WorldNode *node);
    void nodeMoved(WorldNode *node);

    void pathAdded(WorldPath *path);
    void pathRemoved(WorldPathLayer *layer, WorldPath *path);
    void pathChanged(WorldPath *path);

    void scriptAdded(WorldScript *script);
    void scriptRemoved(WorldScript *script);
    void scriptRegionChanged(WorldScript *script);

private:
    WorldPathLayer *mLayer;
    WorldQuadTree<WorldNode> *mNodeTree;
    WorldQuadTree<WorldPath> *mPathTree;
    WorldQuadTree<WorldScript> *mScriptTree;
};

#endif // WORLDLOOKUP_H
