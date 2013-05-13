#include "osmfile.h"

#include <QDebug>
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
        } else
            qDebug() << "unknown osm element" << xml.name();
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

    ProjectedCoordinate botLeft(mGPSMin);
    ProjectedCoordinate topRight(mGPSMax);
    mPCMin = ProjectedCoordinate(botLeft.x, topRight.y);
    mPCMax = ProjectedCoordinate(topRight.x, botLeft.y);

    foreach (Relation *relation, mRelations) {
        GPSCoordinate min( std::numeric_limits< double >::max(), std::numeric_limits< double >::max() );
        GPSCoordinate max( -std::numeric_limits< double >::max(), -std::numeric_limits< double >::max() );
        for (int i = 0; i < relation->members.size(); i++) {
            Relation::Member &m = relation->members[i];
            if (m.wayId && mWayByID.contains(m.wayId)) {
                m.way = mWayByID[m.wayId];
                min.latitude = std::min(min.latitude, m.way->minGPS.latitude);
                min.longitude = std::min(min.longitude, m.way->minGPS.longitude);
                max.latitude = std::max(max.latitude, m.way->maxGPS.latitude);
                max.longitude = std::max(max.longitude, m.way->maxGPS.longitude);
            } else if (m.nodeId && mNodeByID.contains(m.nodeId))
                m.node = mNodeByID[m.nodeId];
        }
        relation->minGPS = min;
        relation->maxGPS = max;
        if (relation->type == Relation::MultiPolygon) {
            QList<Way*> outer, inner;
            foreach (Relation::Member m, relation->members) {
                if (m.way && (m.role.isEmpty() || m.role == QLatin1String("outer"))) outer += m.way;
                if (m.way && m.role == QLatin1String("inner")) inner += m.way;
            }
            relation->outer = makeChains(outer);
            relation->inner = makeChains(inner);
            qDebug() << "relation" << relation->id << relation->outer.size() << relation->inner.size();
        }

    }

//    LoadCoastlines();

    return true;
}

bool File::readNode(QXmlStreamReader &xml)
{
    Node *node = new Node();

    const QXmlStreamAttributes atts = xml.attributes();
    if (atts.hasAttribute(QLatin1String("id"))) {
        bool ok;
        node->id = atts.value(QLatin1String("id")).toString().toULong(&ok);
        Q_ASSERT(ok);
    }
    if (atts.hasAttribute(QLatin1String("lat")))
        node->gps.latitude = atts.value(QLatin1String("lat")).toString().toDouble();
    else
        return true;
    if (atts.hasAttribute(QLatin1String("lon")))
        node->gps.longitude = atts.value(QLatin1String("lon")).toString().toDouble();
    else
        return true;
    node->pc = ProjectedCoordinate(node->gps);

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
        } else
            qDebug() << "unknown osm node element" << xml.name();
    }

    mNodes += node;
    mNodeByID[node->id] = node;
    return true;
}

bool File::readWay(QXmlStreamReader &xml)
{
    Way *way = new Way;
    way->polygon = false; // determined by rendering rules

    const QXmlStreamAttributes atts = xml.attributes();
    if (atts.hasAttribute(QLatin1String("id"))) {
        bool ok;
        way->id = atts.value(QLatin1String("id")).toString().toULong(&ok);
        Q_ASSERT(ok);
    }

    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("tag")) {
            const QXmlStreamAttributes atts = xml.attributes();
            if (atts.hasAttribute(QLatin1String("k")) && atts.hasAttribute(QLatin1String("v"))) {
                QStringRef key = atts.value(QLatin1String("k"));
                way->tags[key.toString()] = atts.value(QLatin1String("v")).toString();
            }
        } else if (xml.name() == QLatin1String("nd")) {
            const QXmlStreamAttributes atts = xml.attributes();
            if (atts.hasAttribute(QLatin1String("ref"))) {
                way->nodeIds += atts.value(QLatin1String("ref")).toString().toULong();
            }
        } else
            qDebug() << "unknown osm way element" << xml.name();
        xml.skipCurrentElement();
    }

    mWays += way;
    mWayByID[way->id] = way;

    return true;
}

bool File::readRelation(QXmlStreamReader &xml)
{
    Relation *relation = new Relation;

    const QXmlStreamAttributes atts = xml.attributes();
    if (atts.hasAttribute(QLatin1String("id"))) {
        bool ok;
        relation->id = atts.value(QLatin1String("id")).toString().toULong(&ok);
        Q_ASSERT(ok);
    }

    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("tag")) {
            const QXmlStreamAttributes atts = xml.attributes();
            if (atts.hasAttribute(QLatin1String("k")) && atts.hasAttribute(QLatin1String("v"))) {
                QString k = atts.value(QLatin1String("k")).toString();
                QString v = atts.value(QLatin1String("v")).toString();
                if (k == QLatin1String("type")) {
                    if (v == QLatin1String("boundary"))
                        relation->type = Relation::Boundary;
                    else if (v == QLatin1String("multipolygon")) {
                        relation->type = Relation::MultiPolygon;
                    } else if (v == QLatin1String("restriction"))
                        relation->type = Relation::Restriction;
                    else if (v == QLatin1String("route"))
                        relation->type = Relation::Route;
                    else
                        qDebug() << "unknown relation type" << v;
                } else
                    relation->tags[k] = v;
            }
        } else if (xml.name() == QLatin1String("member")) {
            const QXmlStreamAttributes atts = xml.attributes();
            Relation::Member member;
            if (atts.hasAttribute(QLatin1String("type")) && atts.hasAttribute(QLatin1String("ref"))) {
                unsigned long ref = atts.value(QLatin1String("ref")).toString().toULong();
                if (atts.value(QLatin1String("type")) == QLatin1String("way"))
                    member = Relation::Member(ref, 0);
                else if (atts.value(QLatin1String("type")) == QLatin1String("node"))
                    member = Relation::Member(0, ref);
                else
                    qDebug() << "unknown relation member type" << atts.value(QLatin1String("type"));
            }
            if (atts.hasAttribute(QLatin1String("role")))
                member.role = atts.value(QLatin1String("role")).toString();
            if (member.wayId || member.nodeId)
                relation->members += member;
        } else
            qDebug() << "unknown osm relation element" << xml.name();
        xml.skipCurrentElement();
    }

    mRelations += relation;

    return true;
}

bool File::readPBF()
{
    return false;
}

void File::LoadCoastlines()
{
    QList<Way*> coastSegs;
    QMap<Node*,Way*> segStartNodes;
    foreach (Way *way, mWays) {
        if (way->tags.contains(QLatin1String("natural")) &&
                (way->tags[QLatin1String("natural")] == QLatin1String("coastline") ||
                 way->tags[QLatin1String("natural")] == QLatin1String("beach"))) {
            if (way->nodes.size() > 1 &&
                    way->nodes.first() != way->nodes.last()) { // some coasts are areas already
                coastSegs += way;
                segStartNodes[way->nodes.first()] = way;
                way->ignore = true;
            }
        }
    }

    QMap<Node*,Way*> merged; // start node -> new way
    foreach (Way *way, coastSegs) {
        // this segment ends where another begins -> chain other to this one
        if (segStartNodes.contains(way->nodes.last())) {
            if (merged.contains(way->nodes.last())) {
                Way *head = merged[way->nodes.last()];
                head->minGPS.latitude = qMin(head->minGPS.latitude, way->minGPS.latitude);
                head->minGPS.longitude = qMin(head->minGPS.longitude, way->minGPS.longitude);
                head->maxGPS.latitude = qMin(head->maxGPS.latitude, way->maxGPS.latitude);
                head->maxGPS.longitude = qMin(head->maxGPS.longitude, way->maxGPS.longitude);
                head->nodeIds = way->nodeIds + head->nodeIds;
                head->nodes = way->nodes + head->nodes;
                segStartNodes.remove(way->nodes.last());
                segStartNodes[head->nodes.first()] = head;
                merged[head->nodes.first()] = head;
            } else {
                Way *head = new Way;
                head->nodes = way->nodes;
                head->nodeIds = way->nodeIds;
                head->tags[QLatin1String("natural")] = QLatin1String("coastline");
                merged[head->nodes.first()] = head;
                segStartNodes.remove(way->nodes.last());
                segStartNodes[head->nodes.first()] = head;
            }
        }
    }

    foreach (Way *way, merged) {
//        way->polygon = true;
        way->nodeIds += way->nodeIds.first();
        way->nodes += way->nodes.first();
    }

    mCoastlines = merged.values();

    qDebug() << "merged " << coastSegs.size() << " segments into " << merged.values().size() << "coastlines";
}

QList<WayChain*> File::makeChains(const QList<Way *> &ways)
{
    QList<WayChain*> chains;

    foreach (Way *way, ways) {
        if (way->nodes.size() > 1) {
            // Every way is in a chain by itself to begin.
            // Some ways may form a closed polygon by themselves.
            WayChain *chain = new WayChain;
            chain->ways += way;
            chain->reverse += false;
            chains += chain;
        }
    }

    QList<WayChain*> ret = chains;
    QList<WayChain*> remaining = chains;
    while (remaining.size()) {
        WayChain *chain = remaining.first();
        remaining.removeFirst();
        if (chain->isClosed()) continue;
        foreach (WayChain *other, remaining) {
            if (other->isClosed()) continue;
            if (chain->couldJoin(other)) {
                chain->join(other);
                remaining.removeOne(other);
                ret.removeOne(other);
                delete other;
            }
        }
    }

    return ret;
}

/////

QList<Node *> WayChain::toPolygon() const
{
    QList<Node*> p;
    for (int i = 0; i < ways.size(); i++) {
        Way *way = ways[i];
        if (reverse[i]) {
            for (int j = way->nodes.size() - 1; j >= 0; j--)
                p += way->nodes[j];
        } else {
            p += way->nodes;
        }
    }
    return p;
}

void WayChain::print()
{
    QString out;
    QTextStream ts(&out);
    for (int i = 0; i < ways.size(); i++) {
        ts << "[node=" << firstNode(i)->id << " < way=" << ways[i]->id << " > node="<< lastNode(i)->id << "]";
    }
    qDebug() << out;
}
