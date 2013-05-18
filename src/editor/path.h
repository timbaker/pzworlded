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

#ifndef PATH_H
#define PATH_H

#include <QList>
#include <QMap>
#include <QRectF>
#include <QRegion>
#include <QPolygonF>
#include <QString>

class WorldPathLayer;

typedef unsigned long id_t;
const id_t InvalidId = 0;

class WorldNode
{
public:
    WorldNode();
    WorldNode(id_t id, qreal x, qreal y);
    WorldNode(id_t id, const QPointF &p);
    ~WorldNode();

    const QPointF &pos() const
    { return p; }

    WorldNode *clone() const;

    id_t id;
    QPointF p;
};

class WorldPath
{
public:
    WorldPath();
    WorldPath(id_t id);
    ~WorldPath();

    WorldPathLayer *owner() const
    { return mOwner; }

    void insertNode(int index, WorldNode *node);
    WorldNode *removeNode(int index);

    bool isClosed() const;

    QRectF bounds();
    QPolygonF polygon();

    QRegion region();

    void setOwner(WorldPathLayer *owner)
    { mOwner = owner; }
    WorldPath *clone(WorldPathLayer *owner) const;

    id_t id;
    QList<WorldNode*> nodes;
    QMap<QString,QString> tags;

    QRectF mBounds;
    QPolygonF mPolygon;

    WorldPathLayer *mOwner;
};

QPolygonF strokePath(WorldPath *path, qreal thickness);
QPolygonF offsetPath(WorldPath *path, qreal offset);
QRegion polygonRegion(const QPolygonF &poly);

class WorldPathLayer
{
public:
    WorldPathLayer();
    WorldPathLayer(const QString &mName, int mLevel);
    ~WorldPathLayer();

    const QString &name() const
    { return mName; }

    int level() const
    { return mLevel; }

    void insertNode(int index, WorldNode *node);
    WorldNode *removeNode(int index);
    const QList<WorldNode*> &nodes() const
    { return mNodes; }
    int nodeCount() const
    { return mNodes.size(); }
    WorldNode *node(id_t id);

    void insertPath(int index, WorldPath *path);
    WorldPath *removePath(int index);
    const QList<WorldPath*> &paths() const
    { return mPaths; }
    int pathCount() const
    { return mPaths.size(); }
    WorldPath *path(id_t id);

//    QList<WorldPath*> paths(QRectF &bounds);
//    QList<WorldPath*> paths(qreal x, qreal y, qreal width, qreal height);

    WorldPathLayer *clone() const;

    QString mName;
    int mLevel;
    QList<WorldNode*> mNodes;
    QMap<id_t,WorldNode*> mNodeByID;
    QList<WorldPath*> mPaths;
    QMap<id_t,WorldPath*> mPathByID;
};

#endif // PATH_H
