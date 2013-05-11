#ifndef OSMFILE_H
#define OSMFILE_H

#include "utils/coordinates.h"

#include <QMap>
#include <QString>
#include <QXmlStreamReader>

namespace OSM {

class Node
{
public:
    unsigned long id;
    GPSCoordinate gps;
};

class Way
{
public:
    unsigned long id;
    GPSCoordinate minGPS;
    GPSCoordinate maxGPS;
    QList<unsigned long> nodeIds;
    QList<Node*> nodes;
};

class File
{
public:
    File();

    bool read(const QString &fileName);

    const QList<Way*> &ways() const
    { return mWays; }

    Node *node(unsigned long id)
    { return mNodeByID.contains(id) ? mNodeByID[id] : 0; }

    GPSCoordinate minGPS() const { return mGPSMin; }
    GPSCoordinate maxGPS() const { return mGPSMax; }

    double northMostLatitude() const { return mGPSMax.latitude; }
    double southMostLatitude() const { return mGPSMin.latitude; }
    double westMostLongitude() const { return mGPSMin.longitude; }
    double eastMostLongitude() const { return mGPSMax.longitude; }

private:
    bool readOSM();
    bool readNode(QXmlStreamReader &xml);
    bool readWay(QXmlStreamReader &xml);
    bool readRelation(QXmlStreamReader &xml);

    bool readPBF();

    QString mFileName;
    QString mError;

    GPSCoordinate mGPSMin;
    GPSCoordinate mGPSMax;

    QList<Node*> mNodes;
    QMap<unsigned long,Node*> mNodeByID;

    QList<Way*> mWays;
    QMap<unsigned long,Way*> mWayByID;
};

} // namespace WorldGen

#endif // OSMFILE_H
