#ifndef OSMLOOKUP_H
#define OSMLOOKUP_H

#include "utils/coordinates.h"

#include <QRect>
#include <QVector>

namespace OSM {

class File;
class Node;
class Way;

class QuadTreeObject
{
public:
    QuadTreeObject(Node *node) :
        node(node), way(0)
    {

    }

    QuadTreeObject(Way *way);

    Node *node;
    Way *way;
    unsigned int x, y, width, height;
};

class QuadTree {
public:
    QuadTree(unsigned int x, unsigned int y, unsigned int w, unsigned int h,
             int level, int maxLevel);

    ~QuadTree();

    void AddObject(QuadTreeObject *object);
    QList<QuadTreeObject *> GetObjectsAt(unsigned int x, unsigned int y,
                                         unsigned int width, unsigned int height);
    void Clear();

    int count();
    QList<QuadTree*> nonEmptyQuads();

private:
    unsigned int x, y, width, height;
    int level;
    int maxLevel;
    QVector<QuadTreeObject*> objects;

    QuadTree *parent;
    QuadTree *NW;
    QuadTree *NE;
    QuadTree *SW;
    QuadTree *SE;

    bool contains(unsigned int x, unsigned int y,
                  unsigned int width, unsigned int height);
    bool contains(QuadTree *child, QuadTreeObject *object);

};

class Lookup
{
public:
    Lookup();
    ~Lookup();

    void fromFile(const File &file);

    QList<Way*> ways(const ProjectedCoordinate &min, const ProjectedCoordinate &max);

    QuadTree *mTree;
};

} // namespace OSM

#endif // OSMLOOKUP_H
