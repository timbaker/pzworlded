#include "osmfile.h"

#include <QFile>

using namespace OSM;

File::File()
{
}

bool File::read(const QString &fileName)
{
    mFileName = fileName;
    if (fileName.endsWith(QLatin1String(".osm")))
        return readOSM();
    if (fileName.endsWith(QLatin1String(".pbf")))
        return readPBF();
    return false;
}

bool File::readOSM()
{
    QFile file(mFileName);
    if (!file.open(QIODevice::ReadOnly)) {
        mError = file.errorString();
        return false;
    }

    QXmlStreamReader xml;
    xml.setDevice(&file);

    if (!xml.readNextStartElement())
        return false;

    if (xml.name() != QLatin1String("osm"))
        return false;

    while (xml.readNextStartElement()) {

        if (xml.name() == QLatin1String("bound")) {
            xml.skipCurrentElement();
        } else if (xml.name() == QLatin1String("bounds")) {
            xml.skipCurrentElement();
        } else if (xml.name() == QLatin1String("node")) {
            if (!readNode(xml))
                return false;
        } else if (xml.name() == QLatin1String("way")) {
            if (!readWay(xml))
                return false;
        } else if (xml.name() == QLatin1String("relation")) {
            if (!readRelation(xml))
                return false;
        }
    }

    mGPSMin = GPSCoordinate(std::numeric_limits< double >::max(), std::numeric_limits< double >::max());
    mGPSMax = GPSCoordinate(-std::numeric_limits< double >::max(), -std::numeric_limits< double >::max());

    for (int i = 0; i < mWays.size(); i++) {
        GPSCoordinate min( std::numeric_limits< double >::max(), std::numeric_limits< double >::max() );
        GPSCoordinate max( -std::numeric_limits< double >::max(), -std::numeric_limits< double >::max() );
        bool ignore = true;
        foreach (unsigned long id, mWays[i]->nodeIds) {
            if (mNodeByID.contains(id)) {
                Node *node = mNodeByID[id];
                mWays[i]->nodes += node;
                min.latitude = std::min(min.latitude, node->gps.latitude);
                min.longitude = std::min(min.longitude, node->gps.longitude);
                max.latitude = std::max(max.latitude, node->gps.latitude);
                max.longitude = std::max(max.longitude, node->gps.longitude);
                ignore = false;
            }
        }
        if (ignore) {
            mWays[i]->nodeIds.clear();
            mWays[i]->nodes.clear();
            continue;
        }
//        qSwap(min.latitude, max.latitude); // FIXME
        mWays[i]->minGPS = min;
        mWays[i]->maxGPS = max;

        // FIXME: get these from the 'bound' or 'bounds' <osm> attribute
        mGPSMin.latitude = std::min(mGPSMin.latitude, mWays[i]->minGPS.latitude);
        mGPSMin.longitude = std::min(mGPSMin.longitude, mWays[i]->minGPS.longitude);
        mGPSMax.latitude = std::max(mGPSMax.latitude, mWays[i]->maxGPS.latitude);
        mGPSMax.longitude = std::max(mGPSMax.longitude, mWays[i]->maxGPS.longitude);
    }

    return true;
}

bool File::readNode(QXmlStreamReader &xml)
{
    Node *node = new Node();

    const QXmlStreamAttributes atts = xml.attributes();
    if (atts.hasAttribute(QLatin1String("id")))
        node->id = atts.value(QLatin1String("id")).toString().toULong();
    if (atts.hasAttribute(QLatin1String("lat")))
        node->gps.latitude = atts.value(QLatin1String("lat")).toString().toDouble();
    else
        return true;
    if (atts.hasAttribute(QLatin1String("lon")))
        node->gps.longitude = atts.value(QLatin1String("lon")).toString().toDouble();
    else
        return true;
    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("tag")) {
#if 0
            const QXmlStreamAttributes atts = xml.attributes();
            if (atts.hasAttribute(QLatin1String("k")) && atts.hasAttribute(QLatin1String("v"))) {
                int tagID = m_nodeTags.value( atts.value(QLatin1String("k")).toString(), -1 );
                if ( tagID != -1 ) {
                    Tag tag;
                    tag.key = tagID;
                    tag.value = atts.value(QLatin1String("v")).toString();
                    node->tags.push_back( tag );
                }
            }
#endif
            xml.skipCurrentElement();
        }
    }

    mNodes += node;
    mNodeByID[node->id] = node;
    return true;
}

bool File::readWay(QXmlStreamReader &xml)
{
    Way *way = new Way;

    const QXmlStreamAttributes atts = xml.attributes();
    if (atts.hasAttribute(QLatin1String("id")))
        way->id = atts.value(QLatin1String("id")).toString().toULong();

    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("tag")) {
#if 0
            const QXmlStreamAttributes atts = xml.attributes();
            if (atts.hasAttribute(QLatin1String("k")) && atts.hasAttribute(QLatin1String("v"))) {
                int tagID = m_wayTags.value( atts.value(QLatin1String("k")).toString(), -1 );
                if ( tagID != -1 ) {
                    Tag tag;
                    tag.key = tagID;
                    tag.value = atts.value(QLatin1String("v")).toString();
                    way->tags.push_back( tag );
                }
            }
#endif
        } else if (xml.name() == QLatin1String("nd")) {
            const QXmlStreamAttributes atts = xml.attributes();
            if (atts.hasAttribute(QLatin1String("ref"))) {
                way->nodeIds += atts.value(QLatin1String("ref")).toString().toULong();
            }
        }
        xml.skipCurrentElement();
    }

    mWays += way;
    mWayByID[way->id] = way;

    return true;
}

bool File::readRelation(QXmlStreamReader &xml)
{
    xml.skipCurrentElement();
    return true;
}

bool File::readPBF()
{
    return false;
}
