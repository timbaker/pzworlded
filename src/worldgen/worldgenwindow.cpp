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

#include "worldgenwindow.h"
#include "ui_worldgenwindow.h"

#include "worldgenview.h"

#include <QFileDialog>

using namespace WorldGen;

WorldGenWindow *WorldGenWindow::mInstance = 0;

WorldGenWindow *WorldGenWindow::instance()
{
    if (!mInstance)
        mInstance = new WorldGenWindow;
    return mInstance;
}

#include <QDebug>
#include <QSettings>

#define TILE_SIZE (256*2)
#define TILE_POWER (8+1)

WorldGenWindow::WorldGenWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::WorldGenWindow)
{
    ui->setupUi(this);

    connect(ui->actionOpen, SIGNAL(triggered()), SLOT(open()));

    connect(ui->actionIncreaseDepth, SIGNAL(triggered()),
            ui->view->scene(), SLOT(depthIncr()));
    connect(ui->actionDecreaseDepth, SIGNAL(triggered()),
            ui->view->scene(), SLOT(depthDecr()));

    ui->actionShowGrids->setChecked(true);
    connect(ui->actionShowGrids, SIGNAL(toggled(bool)),
            ui->view->scene(), SLOT(showGrids(bool)));

    connect(ui->actionOsm, SIGNAL(triggered()), SLOT(osm()));


}

WorldGenWindow::~WorldGenWindow()
{
    delete ui;
}

void WorldGenWindow::open()
{
    QString f = QFileDialog::getOpenFileName(this, tr("Open L-System"),
                                             QLatin1String("C:/Programming/WorldGen/Lse/Examples"));
    if (f.isEmpty())
        return;
    ui->view->LoadFile(f);
}

#include "osmfile.h"
#include "osmlookup.h"
#include "osmrulefile.h"

#include <qmath.h>
#include <QGraphicsSceneHoverEvent>
#include <QMultiMap>

namespace WorldGen {

class OpenStreetMapItem2 : public QGraphicsItem
{
public:
    OpenStreetMapItem2(QString osmFile) :
        QGraphicsItem(),
        zoom(14),
        magnification(1)
    {
        setFlag(ItemUsesExtendedStyleOption);
        setAcceptHoverEvents(true);

        if (mFile.read(osmFile))
            mLookup.fromFile(mFile);

        if (!mRuleFile.read(QLatin1String("C:/Programming/Tiled/PZWorldEd/PZWorldEd/OSM_Rules.txt")))
            qDebug() << mRuleFile.errorString();

        foreach (OSM::RuleFile::Line line, mRuleFile.mLines) {
            // range from rule1 to rule2
            int s = mRuleFile.mRuleIDs.indexOf(line.rule1);
            int e = mRuleFile.mRuleIDs.indexOf(line.rule2);
            for (int i = s; i <= e; i++)
                mLineByRuleID[mRuleFile.mRuleIDs[i]] = line;
        }
        foreach (OSM::RuleFile::Area area, mRuleFile.mAreas)
            mAreaByRuleID[area.rule_id] = area;

        foreach (OSM::Relation *relation, mFile.relations()) {
            if (relation->type != OSM::Relation::MultiPolygon)
                continue;
            foreach (OSM::Relation::Member m, relation->members) {
                if (m.way) mWayToRelations.insert(m.way, relation);
            }
            foreach (QString tagKey, relation->tags.keys()) {
                QString tagValue = relation->tags[tagKey];
                if (mRuleFile.mWayToRule[tagKey].valueToRule.contains(tagValue)) {
                    QString rule_id = mRuleFile.mWayToRule[tagKey].valueToRule[tagValue];
                    if (mAreaByRuleID.contains(rule_id)) {
                        mRelationToArea[relation] = &mAreaByRuleID[rule_id];
                        break;
                    }
                    if (mLineByRuleID.contains(rule_id)) {
                        mRelationToLine[relation] = &mLineByRuleID[rule_id];
                        break;
                    }
                }
            }
        }

        foreach (OSM::Way *way, mFile.ways() + mFile.coastlines()) {
            // Get tags from relations if any
            QMap<QString,QString> tags = way->tags;
            if (mWayToRelations.contains(way)) {
                bool ignore = false;
                foreach (OSM::Relation *relation, mWayToRelations.values(way)) {
                    if (mRelationToArea.contains(relation) ||
                            mRelationToLine.contains(relation)) {
                        ignore = true;
                        break;
                    }
                    if (ignore) continue;
                }
            }
            foreach (QString tagKey, tags.keys()) {
                if (mRuleFile.mWayToRule.contains(tagKey)) {
                    QString tagValue = tags[tagKey];
                    if (mRuleFile.mWayToRule[tagKey].valueToRule.contains(tagValue)) {
                        QString rule_id = mRuleFile.mWayToRule[tagKey].valueToRule[tagValue];
                        if (mAreaByRuleID.contains(rule_id) && way->isClosed()) {
                            mWayToArea[way] = &mAreaByRuleID[rule_id];
                            way->polygon = true;
                            break;
                        }
                        if (mLineByRuleID.contains(rule_id)) {
                            mWayToLine[way] = &mLineByRuleID[rule_id];
                            break;
                        }
                    }
                }
            }
        }
    }

    QRectF boundingRect() const
    {
        int worldGridX1 = qFloor(mFile.minProjected().x * (1U << zoom));
        int worldGridY1 = qFloor(mFile.minProjected().y * (1U << zoom));
        int worldGridX2 = qCeil(mFile.maxProjected().x * (1U << zoom));
        int worldGridY2 = qCeil(mFile.maxProjected().y * (1U << zoom));

        return QRectF(0, 0,
                      (worldGridX2 - worldGridX1) * TILE_SIZE,
                      (worldGridY2 - worldGridY1) * TILE_SIZE);
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
    {
        int tileMinX = option->exposedRect.x() / TILE_SIZE;
        int tileMinY = option->exposedRect.y() / TILE_SIZE;
        int tileMaxX = (option->exposedRect.right() + TILE_SIZE - 1) / TILE_SIZE;
        int tileMaxY = (option->exposedRect.bottom() + TILE_SIZE - 1) / TILE_SIZE;

        qDebug() << tileMinX << "->" << tileMaxX << "," << tileMinY << "->" << tileMaxY;

        double tileFactor = 1u << zoom; // 2^zoom tiles across the whole world

        GPSCoordinate topLeft(mFile.northMostLatitude(), mFile.westMostLongitude());
        int minX = ProjectedCoordinate(topLeft).x * tileFactor + tileMinX;
        int maxX = minX + (tileMaxX - tileMinX);
        int minY = ProjectedCoordinate(topLeft).y * tileFactor + tileMinY;
        int maxY = minY + (tileMaxY - tileMinY);

        qDebug() << minX << "->" << maxX << "," << minY << "->" << maxY;

        int posX = tileMinX * TILE_SIZE;
        for ( int x = minX; x < maxX; ++x ) {
            int posY = tileMinY * TILE_SIZE;
            for ( int y = minY; y < maxY; ++y ) {
                drawTile(painter, x, y, posX, posY);
                posY += TILE_SIZE;
            }
            posX += TILE_SIZE;
        }
    }

    void drawTile(QPainter *painter, int x, int y, int posX, int posY)
    {
        painter->setClipRect(posX,posY,TILE_SIZE*magnification,TILE_SIZE*magnification);

        double tileFactor = 1u << zoom; // 2^zoom tiles across the whole world

        ProjectedCoordinate min(x / tileFactor, y / tileFactor);
        ProjectedCoordinate max((x+1) / tileFactor, (y+1) / tileFactor);

        QMultiMap<int,OSM::Lookup::Object> drawThis;
        foreach (OSM::Lookup::Object lo, mLookup.objects(min, max)) {
            QString rule_id;
            if (OSM::Way *way = lo.way) {
                if (way->polygon)
                    rule_id = mWayToArea[way]->rule_id;
                else if (mWayToLine.contains(way))
                    rule_id = mWayToLine[way]->rule1;
            }
            if (OSM::Relation *relation = lo.relation) {
                if (relation->type == OSM::Relation::MultiPolygon) {
                    if (mRelationToArea.contains(relation))
                        rule_id = mRelationToArea[relation]->rule_id;
                    else if (mRelationToLine.contains(relation))
                        rule_id = mRelationToLine[relation]->rule1;
                }
            }
            if (!rule_id.isEmpty())
                drawThis.insert(mRuleFile.mRuleIDs.indexOf(rule_id), lo);
            else
                drawThis.insert(1000, lo);
        }

        foreach (OSM::Lookup::Object lo, drawThis) {
            if (OSM::Way *way = lo.way) {
                QPainterPath p;
                int i = 0;
                foreach (OSM::Node *node, way->nodes) {
                    if (i++ == 0)
                        p.moveTo(projectedCoordToPixels(node->pc));
                    else
                        p.lineTo(projectedCoordToPixels(node->pc));

                    if (node == mClosestNode) {
                        painter->setPen(Qt::blue);
                        painter->setBrush(Qt::NoBrush);
                        painter->drawEllipse(projectedCoordToPixels(node->pc), 8, 8);
                    }
                }
                if (way->polygon) {
                    painter->setBrush(mWayToArea[way]->color);
                    painter->setPen(Qt::NoPen);
                } else {
                    painter->setBrush(Qt::NoBrush);
                    if (mWayToLine.contains(way)) {
                        if (mWayToLine[way]->color1 == QColor(6,6,6,200)) // hack nature_reserve
                            painter->setPen(Qt::NoPen);
                        else
                            painter->setPen(QPen(mWayToLine[way]->color1, mWayToLine[way]->width1 * 0));
                    } else if (mWayToRelations.contains(way)) {
                        painter->setPen(Qt::red);
                        bool willDrawLater = false;
                        foreach (OSM::Relation *relation, mWayToRelations.values(way)) {
                             if (mRelationToArea.contains(relation)
                                     || mRelationToLine.contains(relation)) {
                                 willDrawLater = true;
                                 break;
                             }
                        }
                        if (willDrawLater) continue;
                    } else
                        painter->setPen(Qt::red);
                }
                painter->drawPath(p);
                if (mHoverObjects.contains(OSM::Lookup::Object(way,0,0)) && way->isClosed())
                    painter->fillPath(p, QColor(128,128,128,128));
            }
            if (OSM::Relation *relation = lo.relation) {
                QPainterPath path;
                foreach (OSM::WayChain *chain, relation->outer) {
                    QPolygonF poly;
                    foreach (OSM::Node *node, chain->toPolygon())
                        poly += projectedCoordToPixels(node->pc);
                    path.addPolygon(poly);
                }
                foreach (OSM::WayChain *chain, relation->inner) {
                    QPolygonF poly;
                    foreach (OSM::Node *node, chain->toPolygon())
                        poly += projectedCoordToPixels(node->pc);
                    path.addPolygon(poly);
                }
                if (mRelationToArea.contains(relation)) {
                    painter->setPen(Qt::NoPen);
                    if (mRelationToArea[relation]->color == QColor(6,6,6,200)) // hack nature_reserve
                        painter->setBrush(Qt::CrossPattern);
                    else
                        painter->setBrush(mRelationToArea[relation]->color);
                } else if (mRelationToLine.contains(relation)) {
                    painter->setBrush(Qt::NoBrush);
                    if (mRelationToLine[relation]->color1 == QColor(6,6,6,200)) // hack nature_reserve
                        continue;
                    painter->setPen(QPen(mRelationToLine[relation]->color1, mRelationToLine[relation]->width1 * 0));
                } else {
                    if (relation->tags.contains(QLatin1String("amenity"))) continue;
                    painter->setBrush(QColor(128,0,0,128));
                    qDebug() << "*** multipolygon with no color" << relation->id << relation->tags;
                    continue;
                }
                painter->drawPath(path);
                if (mHoverObjects.contains(OSM::Lookup::Object(0,0,relation)))
                    painter->fillPath(path, QColor(128,128,128,128));
            }
        }

        painter->setPen(Qt::black);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(posX,posY,TILE_SIZE*magnification,TILE_SIZE*magnification);
    }

    QPointF gpsToPixels(const GPSCoordinate &gps)
    {
        ProjectedCoordinate pc(gps);
        return projectedCoordToPixels(pc);
    }

    QPointF projectedCoordToPixels(const ProjectedCoordinate &pc)
    {
        int worldGridX1 = qFloor(mFile.minProjected().x * (1U << zoom)) * TILE_SIZE;
        int worldGridY1 = qFloor(mFile.minProjected().y * (1U << zoom)) * TILE_SIZE;

        double worldInPixels = (1u << zoom) * TILE_SIZE; // size of world in pixels
        return QPointF(pc.x * worldInPixels - worldGridX1,
                       pc.y * worldInPixels - worldGridY1);
    }

    ProjectedCoordinate itemToProjected(const QPointF &itemPos)
    {
        int worldGridX1 = qFloor(mFile.minProjected().x * (1U << zoom)) * TILE_SIZE;
        int worldGridY1 = qFloor(mFile.minProjected().y * (1U << zoom)) * TILE_SIZE;

        double worldInPixels = (1u << zoom) * TILE_SIZE; // size of world in pixels
        return ProjectedCoordinate((itemPos.x() + worldGridX1) / worldInPixels,
                (itemPos.y() + worldGridY1) / worldInPixels);
    }

    void hoverMoveEvent(QGraphicsSceneHoverEvent *event)
    {
        ProjectedCoordinate pc = itemToProjected(event->pos());

        QSet<OSM::Lookup::Object> objects = mLookup.objects(pc).toSet();
        if (objects != mHoverObjects) {
            qDebug() << "hoverMoveEvent #objects=" << objects.size();
            foreach (OSM::Lookup::Object lo, objects) {
                if (OSM::Way *way = lo.way) {
                    if (mWayToArea.contains(way))
                        qDebug() << "way-area" << way->id << way->tags;
                    if (mWayToLine.contains(way))
                        qDebug() << "way-line" << way->id << way->tags;
                    if (!mWayToArea.contains(way) && !mWayToLine.contains(way))
                        qDebug() << "way NO LINE/AREA" << way->id << way->tags;
                }
                if (OSM::Relation *relation = lo.relation) {
                    if (mRelationToArea.contains(relation))
                        qDebug() << "relation-area" << relation->id << relation->tags;
                    if (mRelationToLine.contains(relation))
                        qDebug() << "relation-line" << relation->id << relation->tags;
                    if (!mRelationToArea.contains(relation) && !mRelationToLine.contains(relation))
                        qDebug() << "relation NO LINE/AREA" << relation->id << relation->tags;
                }
            }
            mHoverObjects = objects;
            update();
        }

        qreal minDist = 100000;
        OSM::Node *closest = 0;
        QSet<OSM::Node*> nodes;
        foreach (OSM::Lookup::Object lo, objects) {
            if (lo.way) nodes += lo.way->nodes.toSet();
        }
        foreach (OSM::Node *node, nodes) {
            qreal dist = QLineF(event->pos(), gpsToPixels(node->gps)).length();
            if (dist < minDist) {
                closest = node;
                minDist = dist;
            }
        }
        if (closest != mClosestNode) {
            mClosestNode = closest;
            if (mClosestNode)
                qDebug() << "closest node is" << mClosestNode->id;
            update();
        }
    }

    OSM::File mFile;
    OSM::Lookup mLookup;
    OSM::RuleFile mRuleFile;
    int zoom;
    int magnification;
    QSet<OSM::Lookup::Object> mHoverObjects;
    OSM::Node *mClosestNode;
    QMap<QString,OSM::RuleFile::Area> mAreaByRuleID;
    QMap<OSM::Way*,OSM::RuleFile::Area*> mWayToArea;
    QMap<QString,OSM::RuleFile::Line> mLineByRuleID;
    QMap<OSM::Way*,OSM::RuleFile::Line*> mWayToLine;
    QMap<OSM::Relation*,OSM::RuleFile::Area*> mRelationToArea;
    QMap<OSM::Relation*,OSM::RuleFile::Line*> mRelationToLine;
    QMultiMap<OSM::Way*,OSM::Relation*> mWayToRelations; // can be > 1 relation per way
};

} // namespace WorldGen

void WorldGenWindow::osm()
{
    ui->view->scene()->clear();

    OSM::Node n1; n1.id = 1;
    OSM::Node n2; n2.id = 2;
    OSM::Node n3; n3.id = 3;
    OSM::Node n4; n4.id = 4;
    OSM::Way w1; w1.id = 1; w1.nodes << &n2 << &n3;
    OSM::Way w2; w2.id = 2; w2.nodes << &n1 << &n2;
    OSM::Way w3; w3.id = 3; w3.nodes << &n3 << &n4;
    QList<OSM::WayChain*> chains = OSM::File::makeChains(QList<OSM::Way*>() << &w1 << &w2 << &w3);
    foreach (OSM::WayChain *chain, chains) chain->print(); qDeleteAll(chains);

    w2.nodes.clear(); w2.nodes << &n2 << &n1;
    chains = OSM::File::makeChains(QList<OSM::Way*>() << &w1 << &w2 << &w3);
    foreach (OSM::WayChain *chain, chains) chain->print(); qDeleteAll(chains);
//    return;

    OpenStreetMapItem2 *item = new OpenStreetMapItem2(QLatin1String("C:\\Programming\\OpenStreetMap\\Vancouver2.osm"));
    ui->view->scene()->addItem(item);
    ui->view->scene()->setSceneRect(QRectF());
}
