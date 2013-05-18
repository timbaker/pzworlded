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
        return (x1 >= x && x1 < x + this->width
                && y1 >= y && y1 < y + height);
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

class PathWorld;
class WorldNode;
class WorldPath;
class WorldPathLayer;
class WorldScript;

class WorldQuadSubTree;

class WorldQuadTreeObject
{
public:
    WorldQuadTreeObject(int index, WorldNode *node);
    WorldQuadTreeObject(int index, WorldPath *path);
    WorldQuadTreeObject(int index, WorldScript *script);

    bool intersects(const QuadRect &rect);
    bool intersects(const QPolygonF &poly);
    bool contains(LookupCoordType x, LookupCoordType y);

    WorldQuadSubTree *owner;

    int index;
    WorldNode *node;
    WorldPath *path;
    WorldScript *script;

    QuadRect bounds;
};

// http://gamedev.stackexchange.com/questions/20607/quad-tree-with-a-lot-of-moving-objects
class WorldQuadSubTree {
public:
    WorldQuadSubTree(LookupCoordType x, LookupCoordType y,
                  LookupCoordType width, LookupCoordType height);
    WorldQuadSubTree(WorldQuadSubTree *parent,
                  LookupCoordType x, LookupCoordType y,
                  LookupCoordType width, LookupCoordType height);

    ~WorldQuadSubTree();

    void Add(WorldQuadTreeObject *object);
    void Remove(WorldQuadTreeObject *object);
    void Subdivide();
    WorldQuadSubTree *GetDestinationTree(WorldQuadTreeObject *object);
    void Relocate(WorldQuadTreeObject *object);
    void CleanUpwards();
    void Delete(WorldQuadTreeObject *object, bool clean);
    void Insert(WorldQuadTreeObject *object);

    typedef QList<WorldQuadTreeObject *> ObjectList;

    void GetObjects(LookupCoordType x, LookupCoordType y,
                    LookupCoordType width, LookupCoordType height,
                    ObjectList &ret)
    { GetObjects(QuadRect(x, y, width, height), ret); }
    void GetObjects(const QuadRect &rect, ObjectList &ret);
    void GetObjects(const QPolygonF &poly, ObjectList &ret);
    void GetObjects(LookupCoordType x, LookupCoordType y, ObjectList &ret);
    void GetAllObjects(ObjectList &ret);

    void Move(WorldQuadTreeObject *object);

    void Clear();

    int ObjectCount();

private:
    QuadRect bounds;
    ObjectList objects;
    int depth;

    WorldQuadSubTree *parent;
    WorldQuadSubTree *NW;
    WorldQuadSubTree *NE;
    WorldQuadSubTree *SW;
    WorldQuadSubTree *SE;

    static const int MaxObjectsPerNode = 50;

    bool contains(const QuadRect &rect);
    bool contains(LookupCoordType x, LookupCoordType y);
    bool intersects(const QuadRect &rect);
    bool intersects(const QPolygonF &poly);
    bool contains(WorldQuadTreeObject *object);

};

template<class T>
class WorldQuadTree
{
public:
    WorldQuadTree(LookupCoordType x, LookupCoordType y,
                  LookupCoordType width, LookupCoordType height) :
        mRoot(new WorldQuadSubTree(x, y, width, height))
    {

    }

    void Add(int index, T *object)
    {
        WorldQuadTreeObject *wrapped = new WorldQuadTreeObject(index, object);
        mMap[object] = wrapped;
        mRoot->Insert(wrapped);
    }

    void Remove(T *object)
    {
        if (mMap.contains(object)) {
            mRoot->Delete(object, true);
            mMap.remove(object);
        }
    }

    void Move(T *object)
    {
        if (mMap.contains(object))
            mRoot->Move(mMap[object]);
    }


    void GetObjects(LookupCoordType x, LookupCoordType y,
                    LookupCoordType width, LookupCoordType height,
                    WorldQuadSubTree::ObjectList &ret)
    {
        GetObjects(QuadRect(x, y, width, height), ret);
    }

    void GetObjects(const QuadRect &rect, WorldQuadSubTree::ObjectList &ret)
    {
        mRoot->GetObjects(rect, ret);
    }

    void GetObjects(const QPolygonF &poly, WorldQuadSubTree::ObjectList &ret)
    {
        mRoot->GetObjects(poly, ret);
    }

    void GetObjects(LookupCoordType x, LookupCoordType y, WorldQuadSubTree::ObjectList &ret)
    {
        mRoot->GetObjects(x, y, ret);
    }

    WorldQuadSubTree *mRoot;
    QMap<T*,WorldQuadTreeObject*> mMap;
};

class WorldLookup
{
public:
    WorldLookup(PathWorld *world);
    ~WorldLookup();

    QList<WorldScript*> scripts(const QRectF &bounds) const;
    QList<WorldPath*> paths(WorldPathLayer *layer, const QRectF &bounds) const;
    QList<WorldPath*> paths(WorldPathLayer *layer, const QPolygonF &poly) const;
    QList<WorldNode*> nodes(WorldPathLayer *layer, const QRectF &bounds) const;
    QList<WorldNode*> nodes(WorldPathLayer *layer, const QPolygonF &bounds) const;

    void nodeMoved(WorldNode *node, const QPointF &oldPos);

private:
    PathWorld *mWorld;
    QVector<WorldQuadTree<WorldNode> *> mNodeTree;
    QVector<WorldQuadTree<WorldPath> *> mPathTree;
    WorldQuadTree<WorldScript> *mScriptTree;
};

#endif // WORLDLOOKUP_H
