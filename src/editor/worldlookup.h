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
#include <QRectF>
#include <QVector>

typedef unsigned long LookupCoordType;

class World;
class WorldScript;

namespace WorldPath {
class Node;
class Path;
}

class WorldQuadTreeObject
{
public:
    WorldQuadTreeObject(WorldPath::Node *node);
    WorldQuadTreeObject(WorldPath::Path *path);
    WorldQuadTreeObject(WorldScript *script);

    bool contains(LookupCoordType x, LookupCoordType y);

    WorldPath::Node *node;
    WorldPath::Path *path;
    WorldScript *script;

    LookupCoordType x, y, width, height;
};

class WorldQuadTree {
public:
    WorldQuadTree(LookupCoordType x, LookupCoordType y, LookupCoordType w, LookupCoordType h,
             int level, int maxLevel);

    ~WorldQuadTree();

    void AddObject(WorldQuadTreeObject *object);
    QList<WorldQuadTreeObject *> GetObjectsAt(LookupCoordType x, LookupCoordType y,
                                         LookupCoordType width, LookupCoordType height);
    QList<WorldQuadTreeObject *> GetObjectsAt(LookupCoordType x, LookupCoordType y);
    void Clear();

    int count();
    QList<WorldQuadTree*> nonEmptyQuads();

private:
    LookupCoordType x, y, width, height;
    int level;
    int maxLevel;
    QVector<WorldQuadTreeObject*> objects;

    WorldQuadTree *parent;
    WorldQuadTree *NW;
    WorldQuadTree *NE;
    WorldQuadTree *SW;
    WorldQuadTree *SE;

    bool contains(LookupCoordType x, LookupCoordType y,
                  LookupCoordType width, LookupCoordType height);
    bool contains(LookupCoordType x, LookupCoordType y);
    bool contains(WorldQuadTree *child, WorldQuadTreeObject *object);

};

class WorldLookup
{
public:
    WorldLookup(World *world);
    ~WorldLookup();

    QList<WorldScript*> scripts(const QRectF &bounds) const;

private:
    World *mWorld;
    WorldQuadTree *mQTree;
};

#endif // WORLDLOOKUP_H
