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
#define QTREE_DEPTH 6

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
    mQTree(new WorldQuadTree(0, 0,
                             LOOKUP_LENGTH(world->width() * 300),
                             LOOKUP_LENGTH(world->height() * 300),
                             0, QTREE_DEPTH))
{
    foreach (WorldPath::Layer *layer, mWorld->pathLayers())
        foreach (WorldPath::Path *path, layer->paths())
            mQTree->AddObject(new WorldQuadTreeObject(path));
    foreach (WorldScript *ws, mWorld->scripts()) {
        if (ws->mRegion.isEmpty()) {
            qCritical("script has empty region - IGNORING");
            continue;
        }
        mQTree->AddObject(new WorldQuadTreeObject(ws));
    }
}

WorldLookup::~WorldLookup()
{
    delete mQTree;
}

QList<WorldScript *> WorldLookup::scripts(const QRectF &bounds) const
{
    LookupCoordinate topLeft(bounds.topLeft());
    QList<WorldScript *> ret;
    foreach (WorldQuadTreeObject *o, mQTree->GetObjectsAt(topLeft.x, topLeft.y,
                                                          LOOKUP_LENGTH(bounds.width()),
                                                          LOOKUP_LENGTH(bounds.height()))) {
        if (o->script)
            ret += o->script;
    }
    return ret;
}

QList<WorldPath::Path *> WorldLookup::paths(const QRectF &bounds) const
{
    LookupCoordinate topLeft(bounds.topLeft());
    QList<WorldPath::Path *> ret;
    foreach (WorldQuadTreeObject *o, mQTree->GetObjectsAt(topLeft.x, topLeft.y,
                                                          LOOKUP_LENGTH(bounds.width()),
                                                          LOOKUP_LENGTH(bounds.height()))) {
        if (o->path)
            ret += o->path;
    }
    return ret;
}

QList<WorldPath::Path *> WorldLookup::paths(const QPolygonF &poly) const
{
    static QPolygonF lpoly;
    lpoly.resize(0);
    foreach (QPointF p, poly)
        lpoly += LookupCoordinate(p).toPointF();

    QList<WorldPath::Path *> ret;
    foreach (WorldQuadTreeObject *o, mQTree->GetObjectsAt(lpoly)) {
        if (o->path)
            ret += o->path;
    }
    return ret;
}

/////

WorldQuadTreeObject::WorldQuadTreeObject(WorldPath::Node *node) :
    node(node), path(0), script(0)
{
}

WorldQuadTreeObject::WorldQuadTreeObject(WorldPath::Path *path) :
    node(0), path(path), script(0)
{
    x = LookupCoordinate(path->bounds().topLeft()).x;
    y = LookupCoordinate(path->bounds().topLeft()).y;
    width = LOOKUP_LENGTH(path->bounds().width());
    height = LOOKUP_LENGTH(path->bounds().height());
}

WorldQuadTreeObject::WorldQuadTreeObject(WorldScript *script) :
    node(0), path(0), script(script)
{
    x = LookupCoordinate(script->mRegion.boundingRect().topLeft()).x;
    y = LookupCoordinate(script->mRegion.boundingRect().topLeft()).y;
    width = LOOKUP_LENGTH(script->mRegion.boundingRect().width());
    height = LOOKUP_LENGTH(script->mRegion.boundingRect().height());
}

bool WorldQuadTreeObject::intersects(LookupCoordType x, LookupCoordType y,
                                     LookupCoordType width, LookupCoordType height)
{
    return (x + width > this->x) && (x < this->x + this->width) &&
            (y + height > this->y) && (y < this->y + this->height);
}

bool WorldQuadTreeObject::intersects(const QPolygonF &poly)
{
    QRectF selfRect(QRectF(this->x, this->y, this->width, this->height));
    return poly.boundingRect().intersects(selfRect);
}

bool WorldQuadTreeObject::contains(LookupCoordType x, LookupCoordType y)
{
    if (x >= this->x && x < this->x + this->width
            && y >= this->y && y < this->y + this->height) {

        return true; // TODO: proper hit-testing
    }
    return false;
}

/////

WorldQuadTree::WorldQuadTree(LookupCoordType _x, LookupCoordType _y,
                   LookupCoordType _width, LookupCoordType _height,
                   int _level, int _maxLevel) :
    x		(_x),
    y		(_y),
    width	(_width),
    height	(_height),
    level	(_level),
    maxLevel(_maxLevel),
    NW(0), NE(0), SW(0), SE(0)
{
    if (level == maxLevel)
        return;

    NW = new WorldQuadTree(x, y, width / 2, height / 2, level+1, maxLevel);
    NE = new WorldQuadTree(x + width / 2, y, width - width / 2, height / 2, level+1, maxLevel);
    SW = new WorldQuadTree(x, y + height / 2, width / 2, height - height / 2, level+1, maxLevel);
    SE = new WorldQuadTree(x + width / 2, y + height / 2, width - width / 2, height - height / 2, level+1, maxLevel);
}

WorldQuadTree::~WorldQuadTree()
{
    if (level == maxLevel)
        return;

    delete NW;
    delete NE;
    delete SW;
    delete SE;
}

void WorldQuadTree::AddObject(WorldQuadTreeObject *object) {
    if (level == maxLevel) {
        objects.push_back(object);
        return;
    }

    if (contains(NW, object)) {
        NW->AddObject(object);
    }
    if (contains(NE, object)) {
        NE->AddObject(object);
    }
    if (contains(SW, object)) {
        SW->AddObject(object);
    }
    if (contains(SE, object)) {
        SE->AddObject(object);
    }
}

QList<WorldQuadTreeObject *> WorldQuadTree::GetObjectsAt(LookupCoordType x, LookupCoordType y,
                                                         LookupCoordType width, LookupCoordType height)
{
    QList<WorldQuadTreeObject*> ret;
    if (level == maxLevel) {
        foreach (WorldQuadTreeObject *qto, objects) {
            if (qto->intersects(x, y, width, height))
                ret += qto;
        }
        return ret;
    }

    if (NW->contains(x, y, width, height))
        ret += NW->GetObjectsAt(x, y, width, height);
    if (NE->contains(x, y, width, height))
        ret += NE->GetObjectsAt(x, y, width, height);
    if (SW->contains(x, y, width, height))
        ret += SW->GetObjectsAt(x, y, width, height);
    if (SE->contains(x, y, width, height))
        ret += SE->GetObjectsAt(x, y, width, height);

    return ret.toSet().toList();
}

QList<WorldQuadTreeObject *> WorldQuadTree::GetObjectsAt(const QPolygonF &poly)
{
    QList<WorldQuadTreeObject*> ret;
    if (level == maxLevel) {
        foreach (WorldQuadTreeObject *qto, objects) {
            if (qto->intersects(poly))
                ret += qto;
        }
        return ret;
    }

    if (NW->intersects(poly))
        ret += NW->GetObjectsAt(poly);
    if (NE->intersects(poly))
        ret += NE->GetObjectsAt(poly);
    if (SW->intersects(poly))
        ret += SW->GetObjectsAt(poly);
    if (SE->intersects(poly))
        ret += SE->GetObjectsAt(poly);

    return ret.toSet().toList();
}

QList<WorldQuadTreeObject *> WorldQuadTree::GetObjectsAt(LookupCoordType x, LookupCoordType y)
{
    QList<WorldQuadTreeObject*> ret;
    if (level == maxLevel) {
        foreach (WorldQuadTreeObject *qto, objects) {
            if (qto->contains(x, y))
                ret += qto;
        }
        return ret;
    }

    if (NW->contains(x, y))
        ret += NW->GetObjectsAt(x, y);
    if (NE->contains(x, y))
        ret += NE->GetObjectsAt(x, y);
    if (SW->contains(x, y))
        ret += SW->GetObjectsAt(x, y);
    if (SE->contains(x, y))
        ret += SE->GetObjectsAt(x, y);

    return ret.toSet().toList();
}

void WorldQuadTree::Clear() {
    if (level == maxLevel) {
        objects.clear();
        return;
    } else {
        NW->Clear();
        NE->Clear();
        SW->Clear();
        SE->Clear();
    }

    if (!objects.empty()) {
        objects.clear();
    }
}

bool WorldQuadTree::contains(WorldQuadTree *child, WorldQuadTreeObject *object) {
    return child->contains(object->x,object->y,object->width,object->height);
}

int WorldQuadTree::count()
{
    if (level == maxLevel)
        return objects.size();
    return NW->count() + NE->count() + SW->count() + SE->count();
}

QList<WorldQuadTree *> WorldQuadTree::nonEmptyQuads()
{
    if (level == maxLevel)
        return objects.size() ? QList<WorldQuadTree*>() << this : QList<WorldQuadTree*>();

    return NW->nonEmptyQuads() + NE->nonEmptyQuads() + SW->nonEmptyQuads() + SE->nonEmptyQuads();
}


bool WorldQuadTree::contains(LookupCoordType x, LookupCoordType y,
                             LookupCoordType width, LookupCoordType height)
{
    return (x + width > this->x) && (x < this->x + this->width) &&
            (y + height > this->y) && (y < this->y + this->height);
}

bool WorldQuadTree::contains(LookupCoordType x, LookupCoordType y)
{
    return (x >= this->x) && (x < this->x + this->width) &&
            (y >= this->y) && (y < this->y + this->height);
}

bool WorldQuadTree::intersects(const QPolygonF &poly)
{
    QRectF selfRect(QRectF(this->x, this->y, this->width, this->height));
    return poly.boundingRect().intersects(selfRect);
}
