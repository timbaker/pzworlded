#include "osmlookup.h"

#include "osmfile.h"

#include <QSet>

using namespace OSM;

Lookup::Lookup() :
    mTree(0)
{
}

Lookup::~Lookup()
{
    delete mTree;
}

#define FUDGE 10e6
#define QTREE_DEPTH 6

class LookupCoordinate
{
public:
    LookupCoordinate(const ProjectedCoordinate &pc)
    {
        // x: 0 -> 1  left-right
        // y: 0 -> 1  top-bottom
        x = pc.x * FUDGE;
        y = pc.y * FUDGE;
    }

    LookupCoordinate(const GPSCoordinate &gps)
    {
        ProjectedCoordinate pc(gps);
        // x: 0 -> 1  left-right
        // y: 0 -> 1  top-bottom
        x = pc.x * FUDGE;
        y = pc.y * FUDGE;
    }

    LookupCoordType x, y;
};

void Lookup::fromFile(const File &file)
{
    LookupCoordinate bottomLeft(file.minGPS());
    LookupCoordinate topRight(file.maxGPS());

    Q_ASSERT(bottomLeft.x <= topRight.x && topRight.y <= bottomLeft.y);
    mTree = new QuadTree(bottomLeft.x, topRight.y,
                         topRight.x - bottomLeft.x,
                         bottomLeft.y - topRight.y,
                         0, QTREE_DEPTH);

    foreach (Way *way, file.coastlines())
        mTree->AddObject(new QuadTreeObject(way));

    foreach (Way *way, file.ways()) {
        if (way->ignore)
            continue; // coastline
        mTree->AddObject(new QuadTreeObject(way));
    }

    {
    int max = 0;
    foreach (QuadTree *t, mTree->nonEmptyQuads())
        max = qMax(max, t->count());
    return;
    }
}

QList<Way *> Lookup::ways(const ProjectedCoordinate &min, const ProjectedCoordinate &max)
{
    Q_ASSERT(min.x <= max.x && min.y <= max.y);

    LookupCoordinate lmin(min), lmax(max);

    QList<Way*> ways;
    foreach (QuadTreeObject *qto, mTree->GetObjectsAt(lmin.x, lmin.y, lmax.x - lmin.x, lmax.y - lmin.y))
        ways += qto->way;
    return ways;
}

QList<Way *> Lookup::ways(const ProjectedCoordinate &pc)
{
    LookupCoordinate lc(pc);

    QList<Way*> ways;
    foreach (QuadTreeObject *qto, mTree->GetObjectsAt(lc.x, lc.y))
        ways += qto->way;
    return ways;
}

/////

QuadTreeObject::QuadTreeObject(Way *way) :
    node(0), way(way)
{
    x = LookupCoordinate(way->minGPS).x;
    y = LookupCoordinate(way->maxGPS).y;
    width = LookupCoordinate(way->maxGPS).x - x;
    height = LookupCoordinate(way->minGPS).y - y;
}

#include <QPolygonF>
bool QuadTreeObject::contains(LookupCoordType x, LookupCoordType y)
{
    if (x >= this->x && x < this->x + this->width
            && y >= this->y && y < this->y + this->height) {
        if (way->nodes.size() < 2 || !way->polygon)
            return true;
        QPolygonF pf;
        foreach (Node *node, way->nodes)
            pf += QPointF(node->pc.x * FUDGE, node->pc.y * FUDGE);
        return pf.containsPoint(QPointF(x, y), Qt::WindingFill);
    }
    return false;
}


/////

QuadTree::QuadTree(LookupCoordType _x, LookupCoordType _y,
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

    NW = new QuadTree(x, y, width / 2, height / 2, level+1, maxLevel);
    NE = new QuadTree(x + width / 2, y, width - width / 2, height / 2, level+1, maxLevel);
    SW = new QuadTree(x, y + height / 2, width / 2, height - height / 2, level+1, maxLevel);
    SE = new QuadTree(x + width / 2, y + height / 2, width - width / 2, height - height / 2, level+1, maxLevel);
}

QuadTree::~QuadTree()
{
    if (level == maxLevel)
        return;

    delete NW;
    delete NE;
    delete SW;
    delete SE;
}

void QuadTree::AddObject(QuadTreeObject *object) {
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

QList<QuadTreeObject *> QuadTree::GetObjectsAt(LookupCoordType x, LookupCoordType y,
                                               LookupCoordType width, LookupCoordType height)
{
    QList<QuadTreeObject*> ret;
    if (level == maxLevel)
        return objects.toList();

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

QList<QuadTreeObject *> QuadTree::GetObjectsAt(LookupCoordType x, LookupCoordType y)
{
    QList<QuadTreeObject*> ret;
    if (level == maxLevel) {
        foreach (QuadTreeObject *qto, objects) {
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

void QuadTree::Clear() {
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

bool QuadTree::contains(QuadTree *child, QuadTreeObject *object) {
    return child->contains(object->x,object->y,object->width,object->height);
}

int QuadTree::count()
{
    if (level == maxLevel)
        return objects.size();
    return NW->count() + NE->count() + SW->count() + SE->count();
}

QList<QuadTree *> QuadTree::nonEmptyQuads()
{
    if (level == maxLevel)
        return objects.size() ? QList<QuadTree*>() << this : QList<QuadTree*>();

    return NW->nonEmptyQuads() + NE->nonEmptyQuads() + SW->nonEmptyQuads() + SE->nonEmptyQuads();
}


bool QuadTree::contains(LookupCoordType x, LookupCoordType y,
                        LookupCoordType width, LookupCoordType height)
{
    return (x + width > this->x) && (x < this->x + this->width) &&
            (y + height > this->y) && (y < this->y + this->height);
}

bool QuadTree::contains(LookupCoordType x, LookupCoordType y)
{
    return (x >= this->x) && (x < this->x + this->width) &&
            (y >= this->y) && (y < this->y + this->height);
}
