#ifndef OSMFILE_H
#define OSMFILE_H

#include "utils/coordinates.h"

#include <QMap>
#include <QPolygon>
#include <QString>
#include <QXmlStreamReader>

namespace OSM {

class Node
{
public:
    unsigned long id;
    GPSCoordinate gps;
    ProjectedCoordinate pc;
};

class Way
{
public:
    Way() :
        id(0),
        polygon(false),
        ignore(false)
    {}

    Node *first() const
    { return nodes.size() ? nodes.first() : 0; }

    Node *last() const
    { return nodes.size() ? nodes.last() : 0; }

    bool isClosed() const
    { return first() == last(); }

    unsigned long id;
    GPSCoordinate minGPS;
    GPSCoordinate maxGPS;
    QList<unsigned long> nodeIds;
    QList<Node*> nodes;

    QMap<QString,QString> tags;
    bool polygon;
    bool ignore;
};

class WayChain
{
public:
    Node *firstNode() const
    {
        if (ways.size())
             return reverse.first() ? ways.first()->last() : ways.first()->first();
        return 0;
    }

    Node *lastNode() const
    {
        if (ways.size())
            return reverse.last() ? ways.last()->first() : ways.last()->last();
        return 0;
    }

    Node *firstNode(int i) const
    {
        return reverse[i] ? ways[i]->last() : ways[i]->first();
    }

    Node *lastNode(int i) const
    {
        return reverse[i] ? ways[i]->first() : ways[i]->last();
    }

    bool isClosed() const
    { return firstNode() == lastNode(); }

    bool couldJoin(WayChain *other)
    {
        return !(isClosed() || this == other || other->isClosed());
    }

    void join(WayChain *other)
    {
        Q_ASSERT(couldJoin(other));
        if (lastNode() == other->firstNode()) {
            ways += other->ways;
            reverse += other->reverse;
        } else if (lastNode() == other->lastNode()) {
            for (int i = other->ways.size() - 1; i >= 0; --i) {
                ways += other->ways[i];
                reverse += !other->reverse[i];
            }
        } else if (firstNode() == other->firstNode()) {
            for (int i = 0; i < other->ways.size(); i++) {
                ways.prepend(other->ways[i]);
                reverse.prepend(!other->reverse[i]);
            }
        } else if (firstNode() == other->lastNode()) {
            ways = other->ways + ways;
            reverse = other->reverse + reverse;
        }
    }

    QList<Node*> toPolygon() const;

    void print();

    QList<Way*> ways;
    QList<bool> reverse;
};

class Relation
{
public:
    enum Type
    {
        Boundary,
        MultiPolygon,
        Restriction,
        Route
    };

    struct Member
    {
        Member() :
            way(0), node(0), wayId(0), nodeId(0) {}
        Member(unsigned long wayId, unsigned long nodeId) :
            way(0), node(0), wayId(wayId), nodeId(nodeId) {}
        Way *way;
        Node *node;
        unsigned long wayId;
        unsigned long nodeId;
        QString role; // MultiPolygon
    };

    unsigned long id;
    Type type;
    QList<Member> members;

    QMap<QString,QString> tags;

    QList<WayChain*> outer, inner; // MultiPolygon
};

class File
{
public:
    File();

    bool read(const QString &fileName);

    const QList<Way*> &ways() const
    { return mWays; }

    const QList<Way*> &coastlines() const
    { return mCoastlines; }

    const QList<Relation*> &relations() const
    { return mRelations; }

    Node *node(unsigned long id)
    { return mNodeByID.contains(id) ? mNodeByID[id] : 0; }

    GPSCoordinate minGPS() const { return mGPSMin; }
    GPSCoordinate maxGPS() const { return mGPSMax; }

    double northMostLatitude() const { return mGPSMax.latitude; }
    double southMostLatitude() const { return mGPSMin.latitude; }
    double westMostLongitude() const { return mGPSMin.longitude; }
    double eastMostLongitude() const { return mGPSMax.longitude; }

    ProjectedCoordinate minProjected() const { return mPCMin; }
    ProjectedCoordinate maxProjected() const { return mPCMax; }

    static QList<WayChain *> makeChains(const QList<Way*> &ways);

private:
    bool readOSM();
    bool readNode(QXmlStreamReader &xml);
    bool readWay(QXmlStreamReader &xml);
    bool readRelation(QXmlStreamReader &xml);

    bool readPBF();

    void LoadCoastlines();

    QString mFileName;
    QString mError;

    GPSCoordinate mGPSMin;
    GPSCoordinate mGPSMax;
    ProjectedCoordinate mPCMin;
    ProjectedCoordinate mPCMax;

    QList<Node*> mNodes;
    QMap<unsigned long,Node*> mNodeByID;

    QList<Way*> mWays;
    QMap<unsigned long,Way*> mWayByID;

    QList<Relation*> mRelations;

    QList<Way*> mCoastlines;
};

} // namespace WorldGen

#endif // OSMFILE_H
