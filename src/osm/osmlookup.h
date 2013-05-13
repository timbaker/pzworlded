#ifndef OSMLOOKUP_H
#define OSMLOOKUP_H

#include "coordinates.h"

#include <QHash>
#include <QRect>
#include <QVector>

namespace OSM {

class File;
class Node;
class Relation;
class Way;

typedef unsigned long LookupCoordType;

class QuadTreeObject
{
public:
    QuadTreeObject(Node *node);
    QuadTreeObject(Way *way);
    QuadTreeObject(Relation *relation);

    bool contains(LookupCoordType x, LookupCoordType y);

    Node *node;
    Way *way;
    Relation *relation;
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

    struct Object
    {
        Object(Way *w, Node *n, Relation *r) :
            way(w), node(n), relation(r)
        {}

        Way *way;
        Node *node;
        Relation *relation;
    };

    QList<Object> objects(const ProjectedCoordinate &min, const ProjectedCoordinate &max);
    QList<Object> objects(const ProjectedCoordinate &pc);

    QList<Way*> ways(const ProjectedCoordinate &min, const ProjectedCoordinate &max);
    QList<Way*> ways(const ProjectedCoordinate &pc);

    QuadTree *mTree;
};

inline bool operator==(const Lookup::Object &o1, const Lookup::Object &o2)
{
    return o1.way == o2.way &&
            o1.node == o2.node &&
            o1.relation == o2.relation;
}

inline uint qHash(const Lookup::Object &key)
{
    if (key.node) return ::qHash(key.node);
    if (key.way) return ::qHash(key.way);
    if (key.relation) return ::qHash(key.relation);
    return ::qHash(0);
}

} // namespace OSM

#endif // OSMLOOKUP_H
