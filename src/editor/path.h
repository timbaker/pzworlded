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

namespace WorldPath {

typedef unsigned long id_t;
typedef qreal coord_t;

typedef QPointF Point;
typedef QPolygonF Polygon;
typedef QRectF Rect;

const id_t InvalidId = 0;

class Tag
{
public:
    Tag(const QString &k, const QString &v) :
        k(k), v(v)
    {}
    QString k;
    QString v;
};

class Node
{
public:
    Node();
    Node(id_t id, coord_t x, coord_t y);
    Node(id_t id, const Point &p);
    ~Node();

    id_t id;
    Point p;
};

class Path
{
public:
    Path();
    Path(id_t id);
    ~Path();

    void insertNode(int index, Node *node);
    Node *removeNode(int index);

    bool isClosed() const;

    Rect bounds();
    Polygon polygon();

    QRegion region();

    id_t id;
    QList<Node*> nodes;
    QMap<QString,QString> tags;

    Rect mBounds;
    Polygon mPolygon;
};

QPolygonF strokePath(Path *path, qreal thickness);
QPolygonF offsetPath(Path *path, qreal offset);
QRegion polygonRegion(const QPolygonF &poly);

class Layer
{
public:
    Layer();
    Layer(const QString &mName, int mLevel);
    ~Layer();

    const QString &name() const
    { return mName; }

    int level() const
    { return mLevel; }

    void insertNode(int index, Node *node);
    Node *removeNode(int index);
    const QList<Node*> &nodes() const
    { return mNodes; }
    Node *node(id_t id);

    void insertPath(int index, Path *path);
    Path *removePath(int index);
    const QList<Path*> &paths() const
    { return mPaths; }

    QList<Path*> paths(Rect &bounds);
    QList<Path*> paths(coord_t x, coord_t y, coord_t width, coord_t height);

    QString mName;
    int mLevel;
    QList<Node*> mNodes;
    QMap<id_t,Node*> mNodeByID;
    QList<Path*> mPaths;
};

} // namespace WorldPath

#endif // PATH_H
