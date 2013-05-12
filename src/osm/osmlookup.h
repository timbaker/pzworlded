#ifndef OSMLOOKUP_H
#define OSMLOOKUP_H

#include "utils/coordinates.h"

#include <QRect>
#include <QVector>

namespace OSM {

class File;
class Node;
class Way;

typedef unsigned long LookupCoordType;

class QuadTreeObject
{
public:
    QuadTreeObject(Node *node) :
        node(node), way(0)
    {

    }

    bool contains(LookupCoordType x, LookupCoordType y);

    QuadTreeObject(Way *way);

    Node *node;
    Way *way;
    LookupCoordType x, y, width, height;
};

class QuadTree {
public:
    QuadTree(LookupCoordType x, LookupCoordType y, LookupCoordType w, LookupCoordType h,
             int level, int maxLevel);

    ~QuadTree();

    void AddObject(QuadTreeObject *object);
    QList<QuadTreeObject *> GetObjectsAt(LookupCoordType x, LookupCoordType y,
                                         LookupCoordType width, LookupCoordType height);
    QList<QuadTreeObject *> GetObjectsAt(LookupCoordType x, LookupCoordType y);
    void Clear();

    int count();
    QList<QuadTree*> nonEmptyQuads();

private:
    LookupCoordType x, y, width, height;
    int level;
    int maxLevel;
    QVector<QuadTreeObject*> objects;

    QuadTree *parent;
    QuadTree *NW;
    QuadTree *NE;
    QuadTree *SW;
    QuadTree *SE;

    bool contains(LookupCoordType x, LookupCoordType y,
                  LookupCoordType width, LookupCoordType height);
    bool contains(LookupCoordType x, LookupCoordType y);
    bool contains(QuadTree *child, QuadTreeObject *object);

};

class Lookup
{
public:
    Lookup();
    ~Lookup();

    void fromFile(const File &file);

    QList<Way*> ways(const ProjectedCoordinate &min, const ProjectedCoordinate &max);
    QList<Way*> ways(const ProjectedCoordinate &pc);

    QuadTree *mTree;
};

} // namespace OSM

#endif // OSMLOOKUP_H
