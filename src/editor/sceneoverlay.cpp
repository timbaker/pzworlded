/*
 * Copyright 2014, Tim Baker <treectrl@users.sf.net>
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

#include "sceneoverlay.h"

#include "celldocument.h"
#include "cellscene.h"
#include "map.h"
#include "mapbuildings.h"
#include "mapcomposite.h"
#include "preferences.h"
#include "world.h"
#include "worldcell.h"

#include "maprenderer.h"
#include "tile.h"
//#include "tilelayer.h"
#include "tileset.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QStyleOptionGraphicsItem>
#include <QPainter>
#include <QSettings>

using namespace Tiled;

SceneOverlay::SceneOverlay(BaseGraphicsScene *scene, QGraphicsItem *parent) :
    QGraphicsItem(parent),
    mScene(scene)
{

}

/////

QImage LightSwitchOverlay::mImage;

LightSwitchOverlay::LightSwitchOverlay(CellScene *scene, qreal x, qreal y, int z, QGraphicsItem *parent) :
    SceneOverlay(scene, parent),
    mCellScene(scene->asCellScene()),
    mX(x),
    mY(y),
    mZ(z)
{
    setZValue(CellScene::ZVALUE_GRID);
    setAcceptHoverEvents(true);

//    setFlag(ItemIgnoresTransformations);

    if (mImage.isNull())
        mImage = QImage(QLatin1String(":/images/idea.png"));
}

QRectF LightSwitchOverlay::boundingRect() const
{
    QRectF r = mCellScene->renderer()->boundingRect(mRoomRegion.boundingRect(), mZ);
    return r | QRectF(r.center().x() - mImage.width() / 2, r.center().y() - mImage.height() / 2, mImage.width(), mImage.height());
}

void LightSwitchOverlay::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
//    mCellScene->renderer()->drawFancyRectangle(painter, QRect(mX, mY, 1, 1), Qt::lightGray, mZ);
    if (option->state & QStyle::State_MouseOver) {
        QPainterPath path;
        for (const QRect &r : mRoomRegion) {
            path.addPolygon(mCellScene->renderer()->tileToPixelCoords(r, mZ));
        }
        //    painter->setPen(QPen(QColor(128, 128, 128, 200), 3));
        //    QPainterPathStroker stroker;
        //    path = stroker.createStroke(path);
        painter->fillPath(path, QColor(128, 128, 128, 200));
    }

    QPointF scenePos = mCellScene->renderer()->tileToPixelCoords(QPointF(mX, mY), mZ);
    painter->drawImage(scenePos - QPoint(mImage.width() / 2, mImage.height() / 2), mImage);
}

QPainterPath LightSwitchOverlay::shape() const
{
    QPainterPath path;
    QPointF scenePos = mCellScene->renderer()->tileToPixelCoords(QPointF(mX, mY), mZ);
    path.addRect(QRectF(scenePos.x() - mImage.width() / 2, scenePos.y() - mImage.height() / 2, mImage.width(), mImage.height()));
    return path;
}

/////

LightSwitchOverlays::LightSwitchOverlays(CellScene *scene) :
    mScene(scene)
{
    QSettings settings;
//    QString d = settings.value(QLatin1String("LootWindow/GameDirectory")).toString();
    QString fileName = Preferences::instance()->tilesDirectory() + QLatin1String("/newtiledefinitions.tiles");
    if (QFileInfo(fileName).exists()) {
        mTileDefFile.read(fileName);

        qDebug() << "CellSceneOverlays parsing tiledef...";
        QString lightswitch(QLatin1String("lightswitch"));
        foreach (TileDefTileset *ts, mTileDefFile.tilesets()) {
            foreach (TileDefTile *tdt, ts->mTiles) {
                foreach (QString key, tdt->mProperties.keys()) {
                    if (key == lightswitch) {
                        mTileDefTiles += tdt;
                    }
                }
            }
        }
        qDebug() << "CellSceneOverlays parsing tiledef DONE";
    }

    if (!LightbulbsMgr::hasInstance())
        new LightbulbsMgr();
    connect(LightbulbsMgr::instancePtr(), SIGNAL(changed()), SLOT(update()));
}

#include <QElapsedTimer>

void LightSwitchOverlays::update()
{
    QElapsedTimer elapsed;
    elapsed.start();
    qDebug() << "CellSceneOverlays update...";

    qDeleteAll(mOverlays);
    mOverlays.clear();

    QVector<const Cell*> cells(40);

#if 0
    qDebug() << "CellSceneOverlays prepareDrawing2...";
    // This is quite slow due to BmpBlender::flush() on the entire map (incl adjacent maps).
    foreach (CompositeLayerGroup *lg, mScene->mapComposite()->layerGroups())
        lg->prepareDrawing2();
    qDebug() << "CellSceneOverlays prepareDrawing2 DONE";
#endif

    QSet<Tile*> lightSwitchTiles;
    foreach (Tileset *ts, mScene->mapComposite()->usedTilesets()) {
        foreach (TileDefTile *tdt, mTileDefTiles) {
            int col = tdt->id() % tdt->mTileset->mColumns;
            int row = tdt->id() / tdt->mTileset->mColumns;
            if ((tdt->mTileset->mName == ts->name()) && ts->tileAt(col + row * ts->columnCount()))
                lightSwitchTiles.insert(ts->tileAt(col + row * ts->columnCount()));
        }
    }

    QStringList rooms = LightbulbsMgr::instance().rooms();
    QSet<QString> ignoreRooms(rooms.constBegin(), rooms.constEnd());
    QStringList maps = LightbulbsMgr::instance().maps();
    QSet<QString> ignoreBuildings(maps.constBegin(), maps.constEnd());

    mMapBuildings = mScene->mMapBuildings;
    foreach (MapBuildingsNS::Building *building, mMapBuildings->buildings()) {
        if (!building->region().boundingRect().intersects(QRect(0, 0, 300, 300)))
            continue;
        foreach (MapBuildingsNS::Room *room, building->RoomList) {
            if (ignoreRooms.contains(room->name)) continue;
            bool hasSwitch = false;
            CompositeLayerGroup *lg = mScene->mapComposite()->layerGroupForLevel(room->floor);
            if (!lg) continue;
            lg->prepareDrawingNoBmpBlender(mScene->renderer(), mScene->renderer()->boundingRect(building->region().boundingRect(), lg->level()));
            QRectF biggestRoomRect;
            foreach (MapBuildingsNS::RoomRect *rect, room->rects) {
                if (ignoreBuildings.contains(rect->buildingName)) continue; // FIXME: check this sooner
                if (rect->w * rect->h > biggestRoomRect.width() * biggestRoomRect.height())
                    biggestRoomRect = rect->bounds();
                for (int y = rect->y; y < rect->y + rect->h; y++) {
                    for (int x = rect->x; x < rect->x + rect->w; x++) {
                        cells.clear();
                        lg->orderedCellsAt2(QPoint(x, y), cells);
                        foreach (const Cell *cell, cells) {
                            Tile *tile = cell->tile;
                            if (!tile) continue;
                            hasSwitch = lightSwitchTiles.contains(tile);
                            if (hasSwitch) break;
                        }
                        if (hasSwitch) break;
                    }
                    if (hasSwitch) break;
                }
                if (hasSwitch) break;
            }
            if (!hasSwitch && !biggestRoomRect.isEmpty()) {
                LightSwitchOverlay *overlay = new LightSwitchOverlay(mScene,
                                                                     biggestRoomRect.center().x(),
                                                                     biggestRoomRect.center().y(),
                                                                     room->floor);
                overlay->mRoomRegion = room->region();
                overlay->mRoomName = room->name;
                mScene->addItem(overlay);
                mOverlays += overlay;
            }
        }
    }

    updateCurrentLevelHighlight();

    qDebug() << "CellSceneOverlays update DONE in" << elapsed.elapsed() << "ms";
}

void LightSwitchOverlays::updateCurrentLevelHighlight()
{
    int currentLevel = mScene->document()->currentLevel();
    foreach (SceneOverlay *overlay, mOverlays) {
        LightSwitchOverlay *lso = (LightSwitchOverlay*)overlay;
        lso->setVisible(!mScene->mHighlightCurrentLevel || (lso->mZ == currentLevel));
    }
}

void LightSwitchOverlays::removeOverlays()
{
    qDeleteAll(mOverlays);
    mOverlays.clear();
}

/////

#include "preferences.h"
#include "simplefile.h"

#define VERSION_LATEST 0

LightbulbsFile::LightbulbsFile()
{

}

LightbulbsFile::~LightbulbsFile()
{

}

QString LightbulbsFile::txtName()
{
    return QLatin1String(QLatin1String("lightbulbs.txt"));
}

QString LightbulbsFile::txtPath()
{
    return Preferences::instance()->configPath(txtName());
}

bool LightbulbsFile::readTxt()
{
    QFileInfo info(txtPath());
    if (!info.exists()) {
        mError = tr("The %1 file doesn't exist.").arg(txtName());
        return false;
    }

    QString path = info.canonicalFilePath();
    SimpleFile simple;
    if (!simple.read(path)) {
        mError = simple.errorString();
        return false;
    }

    if (simple.version() != VERSION_LATEST) {
        mError = tr("Expected %1 version %2, got %3")
                .arg(txtName()).arg(VERSION_LATEST).arg(simple.version());
        return false;
    }

    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("rooms")) {
            foreach (SimpleFileKeyValue kv, block.values) {
                mRooms += kv.value;
            }
        } else if (block.name == QLatin1String("maps")) {
            foreach (SimpleFileKeyValue kv, block.values) {
                mMaps += kv.value;
            }
        } else {
            mError = tr("Unknown block name '%1'.\n%2")
                    .arg(block.name)
                    .arg(path);
            return false;
        }
    }

    return true;
}

bool LightbulbsFile::writeTxt()
{
    SimpleFile simpleFile;

    SimpleFileBlock roomsBlock;
    roomsBlock.name = QLatin1String("rooms");
    foreach (QString name, mRooms) {
        roomsBlock.addValue(QLatin1String("room"), name);
    }
    simpleFile.blocks += roomsBlock;

    SimpleFileBlock mapsBlock;
    mapsBlock.name = QLatin1String("maps");
    foreach (QString name, mMaps) {
        mapsBlock.addValue(QLatin1String("map"), name);
    }
    simpleFile.blocks += mapsBlock;

    simpleFile.setVersion(VERSION_LATEST);
    if (!simpleFile.write(txtPath())) {
        mError = simpleFile.errorString();
        return false;
    }
    return true;
}

/////

SINGLETON_IMPL(LightbulbsMgr)

LightbulbsMgr::LightbulbsMgr()
{
    if (QFileInfo(mFile.txtPath()).exists())
        mFile.readTxt();
}

void LightbulbsMgr::addRoom(const QString &room)
{
    if (!mFile.rooms().contains(room)) {
        mFile.addRoom(room);
        mFile.writeTxt();
        emit changed();
    }
}

void LightbulbsMgr::addMap(const QString &map)
{
    if (!mFile.maps().contains(map)) {
        mFile.addMap(map);
        mFile.writeTxt();
        emit changed();
    }
}

void LightbulbsMgr::toggleRoom(const QString &room)
{
    if (mFile.rooms().contains(room))
        mFile.removeRoom(room);
    else
        mFile.addRoom(room);
    mFile.writeTxt();
    emit changed();
}

void LightbulbsMgr::toggleMap(const QString &map)
{
    if (mFile.maps().contains(map))
        mFile.removeMap(map);
    else
        mFile.addMap(map);
    mFile.writeTxt();
    emit changed();
}

bool LightbulbsMgr::ignoreRoom(const QString &room)
{
    return mFile.rooms().contains(room);
}

bool LightbulbsMgr::ignoreMap(const QString &map)
{
    return mFile.maps().contains(map);
}

// // // // //

WaterFlowOverlay::WaterFlowOverlay(CellScene *scene, QGraphicsItem *parent)
    : SceneOverlay(scene, parent)
    , mCellScene(scene->asCellScene())
{
}

QRectF WaterFlowOverlay::boundingRect() const
{
    QRectF r = mCellScene->renderer()->boundingRect(QRect(0, 0, 300, 300), 0);
    return r;
}

#include <QtMath>

static void resolveProperties(PropertyHolder *ph, PropertyList &result)
{
    for (PropertyTemplate *pt : ph->templates()) {
        resolveProperties(pt, result);
    }
    for (Property *p : ph->properties()) {
        result.removeAll(p->mDefinition);
        result += p;
    }
}

void WaterFlowOverlay::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    ObjectType* objectType = mCellScene->world()->objectType(QStringLiteral("WaterFlow"));
    if (objectType == nullptr)
        return;
    PropertyDef* pdefDir = mCellScene->world()->propertyDefinition(QStringLiteral("WaterDirection"));
    PropertyDef* pdefSpeed = mCellScene->world()->propertyDefinition(QStringLiteral("WaterSpeed"));

    QPen pen(Qt::red);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    pen.setWidth(2);
    painter->setPen(pen);
    painter->setRenderHint(QPainter::Antialiasing);

    for (WorldCellObject* object : mCellScene->cell()->objects()) {
        if (object->type() != objectType)
            continue;

        QRectF tileBounds(object->pos(), object->size());
        mCellScene->renderer()->drawFancyRectangle(painter, tileBounds, Qt::red, object->level());

        PropertyList properties;
        resolveProperties(object, properties);
        Property* propertyDir = properties.find(pdefDir);
        Property* propertySpeed = properties.find(pdefSpeed);
        if (propertyDir != nullptr && propertySpeed != nullptr) {
            // positive is clockwise, zero degrees is at 12 o'clock (north in isometric coords)
            qreal directionDegrees = propertyDir->mValue.toDouble();
//            float directionRadians = qDegreesToRadians(directionDegrees);
            qreal speed = propertySpeed->mValue.toDouble();

            if (speed <= 0.0)
                continue;

            QLineF line(tileBounds.center(), tileBounds.center() + QPointF(qMax(5 * speed, 0.5), 0));
            // positive is counter-clockwise, zero degrees is at the 3 o'clock position
            qreal angle1 = -int(directionDegrees - 90) % 360;
            line.setAngle(angle1);

            QPointF start = mCellScene->renderer()->tileToPixelCoords(tileBounds.center());
            QPointF end = mCellScene->renderer()->tileToPixelCoords(line.p2());

            qreal arrowSize = 0.5;
            qreal angle = std::atan2(line.dy(), -line.dx());
            QPointF arrowP1 = line.p2() + QPointF(sin(angle + M_PI / 3) * arrowSize, cos(angle + M_PI / 3) * arrowSize);
            QPointF arrowP2 = line.p2() + QPointF(sin(angle + M_PI - M_PI / 3) * arrowSize, cos(angle + M_PI - M_PI / 3) * arrowSize);
            QPolygonF arrowHead;
            arrowHead << line.p2() << arrowP1 << arrowP2;
            arrowHead = mCellScene->renderer()->tileToPixelCoords(arrowHead);

            painter->drawLine(start, end);

            painter->setBrush(Qt::red);
            painter->drawPolygon(arrowHead);
            painter->setBrush(Qt::transparent);

        }
    }
}

