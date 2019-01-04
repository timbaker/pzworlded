/*
 * Copyright 2018, Tim Baker <treectrl@users.sf.net>
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

#include "mapboxgeojsongenerator.h"

#include "mapboxwriter.h"

#include "lotfilesmanager.h"
#include "mainwindow.h"
#include "mapcomposite.h"
#include "mapmanager.h"
#include "progress.h"
#include "world.h"
#include "worldcell.h"
#include "worlddocument.h"

#include "mapobject.h"
#include "objectgroup.h"

#include <qmath.h>
#include <QApplication>
#include <QDebug>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QProcess>

using namespace Tiled;

SINGLETON_IMPL(MapBoxGeojsonGenerator)

// https://gis.stackexchange.com/questions/2951/algorithm-for-offsetting-a-latitude-longitude-by-some-amount-of-meters
// If your displacements aren't too great (less than a few kilometers) and you're not right at the poles,
// use the quick and dirty estimate that 111,111 meters (111.111 km) in the y direction is 1 degree (of latitude)
// and 111,111 * cos(latitude) meters in the x direction is 1 degree (of longitude).
static double latitude = 0.0; // north/south
static double longitude = 0.0; // west/east
static double toLat(double worldX, double worldY) {
    return latitude + -worldY / 111111.0;
}
static double toLong(double worldX, double worldY) {
    return longitude + worldX / (111111.0 * qCos(qDegreesToRadians(toLat(worldX, worldY))));
}

MapBoxGeojsonGenerator::MapBoxGeojsonGenerator(QObject *parent) :
    QObject(parent)
{
}

bool MapBoxGeojsonGenerator::generateWorld(WorldDocument *worldDoc, MapBoxGeojsonGenerator::GenerateMode mode)
{
    mWorldDoc = worldDoc;
    World *world = mWorldDoc->world();

    MapManager::instance()->purgeUnreferencedMaps();

    PROGRESS progress(QStringLiteral("Creating GeoJSON files"));

#if 0
    if (mode == GenerateSelected) {
        foreach (WorldCell *cell, worldDoc->selectedCells())
            if (!generateCell(cell))
                goto errorExit;
    } else {
        for (int y = 0; y < world->height(); y++) {
            for (int x = 0; x < world->width(); x++) {
                if (!generateCell(world->cellAt(x, y)))
                    goto errorExit;
            }
        }
    }

    MapManager::instance()->purgeUnreferencedMaps();
#endif

    QString worldName = QFileInfo(worldDoc->fileName()).baseName();
    {
        MapboxWriter writer;
        if (!writer.writeWorld(world, QStringLiteral("D:/pz/worktree/build40-weather/pzmapbox/data/%1-features.xml").arg(worldName))) {
            qWarning("Failed to write features.xml.");
            goto errorExit;
        }
    }

    if (false) {
        QJsonObject object;
        object[QLatin1Literal("type")] = QLatin1Literal("FeatureCollection");
        object[QLatin1Literal("features")] = mJsonFeatures;
        QFile saveFile(QStringLiteral("D:/pz/worktree/build40-weather/pzmapbox/data/WorldEd-buildings.geojson"));
        if (!saveFile.open(QIODevice::WriteOnly)) {
             qWarning("Couldn't open save file.");
             goto errorExit;
        }
        QJsonDocument json(object);
        saveFile.write(json.toJson());
    }

    {
        auto& generateLotSettings = world->getGenerateLotsSettings();
        const int worldOriginX = generateLotSettings.worldOrigin.x();
        const int worldOriginY = generateLotSettings.worldOrigin.y();

        QJsonArray buildings, roads, water, combined;

        // Create a background polygon covering the entire world.
        // This is used to hide cells from other maps beneath this one (if any).
        {
            QJsonObject feature;
            feature[QLatin1Literal("type")] = QLatin1Literal("Feature");
            QJsonObject geometry;
            geometry[QLatin1Literal("type")] = QStringLiteral("Polygon");
            QJsonArray coordinates;
            QJsonArray ring;
            QJsonArray point;
            // nw
            point.append(toLong(worldOriginX * 300, worldOriginY * 300));
            point.append(toLat(worldOriginX * 300, worldOriginY * 300));
            ring.append(point);
            // sw
            point[0] = toLong(worldOriginX * 300, (worldOriginY + world->height()) * 300);
            point[1] = toLat(worldOriginX * 300, (worldOriginY + world->height()) * 300);
            ring.append(point);
            // se
            point[0] = toLong((worldOriginX + world->width()) * 300, (worldOriginY + world->height()) * 300);
            point[1] = toLat((worldOriginX + world->width()) * 300, (worldOriginY + world->height()) * 300);
            ring.append(point);
            // ne
            point[0] = toLong((worldOriginX + world->width()) * 300, worldOriginY * 300);
            point[1] = toLat((worldOriginX + world->width()) * 300, worldOriginY * 300);
            ring.append(point);
            ring += ring[0];
            coordinates.append(ring);
            geometry[QLatin1Literal("coordinates")] = coordinates;
            feature[QLatin1Literal("geometry")] = geometry;

            // Feature without "properties" produces a warning by tippecanoe.
            QJsonObject properties;
            feature[QLatin1Literal("properties")] = properties;

            QJsonObject tippecanoe;
            tippecanoe[QStringLiteral("layer")] = QStringLiteral("background");
            feature[QStringLiteral("tippecanoe")] = tippecanoe;

            combined += feature;
        }

        for (int y = 0; y < world->height(); y++) {
            for (int x = 0; x < world->width(); x++) {
                if (WorldCell* cell = world->cellAt(x, y)) {
                    for (auto* mbFeature : cell->mapBox().mFeatures) {
                        QJsonObject feature;

                        feature[QLatin1Literal("type")] = QLatin1Literal("Feature");

                        QJsonObject properties;
                        for (auto& mbProperty : mbFeature->mProperties) {
                            properties[mbProperty.mKey] = mbProperty.mValue;
                        }
                        feature[QLatin1Literal("properties")] = properties;

                        QJsonObject geometry;
                        geometry[QLatin1Literal("type")] = mbFeature->mGeometry.mType;
                        bool bPoint = mbFeature->mGeometry.mType == QLatin1Literal("Point");
                        bool bPolygon = mbFeature->mGeometry.mType == QLatin1Literal("Polygon");
                        QJsonArray coordinates;
                        for (auto& mbCoords : mbFeature->mGeometry.mCoordinates) {
                            QJsonArray ring; // counter-clockwise, longitude,latitude,elevation
                            for (auto& mbPoint : mbCoords) {
                                QJsonArray point;
                                point.append(toLong((worldOriginX + cell->x()) * 300 + mbPoint.x, (worldOriginY + cell->y()) * 300 + mbPoint.y));
                                point.append(toLat((worldOriginX + cell->x()) * 300 + mbPoint.x, (worldOriginY + cell->y()) * 300 + mbPoint.y));
                                if (bPolygon)
                                    ring.append(point);
                                else {
                                    coordinates += point;
                                }
                            }
                            if (bPolygon) {
                                ring += ring[0];
                                coordinates += ring;
                            }
                        }
                        if (bPoint) {
                            coordinates = coordinates.first().toArray();
                        }
                        geometry[QLatin1Literal("coordinates")] = coordinates;
                        feature[QLatin1Literal("geometry")] = geometry;

                        QString layerName = QStringLiteral("generic");

                        if (bPolygon) {
                            if (mbFeature->properties().containsKey(QStringLiteral("building"))) {
                                buildings.append(feature);
                                layerName = QStringLiteral("buildings");
                            }
                            if (mbFeature->properties().containsKey(QStringLiteral("water"))) {
                                water.append(feature);
                                layerName = QStringLiteral("water");
                            }
                            if (mbFeature->properties().containsKey(QStringLiteral("natural"))) {
                                layerName = QStringLiteral("landuse");
                            }
                        } else if (bPoint) {
                            if (mbFeature->properties().containsKey(QStringLiteral("place")))
                                layerName = QStringLiteral("place_label");
                        } else {
                            roads.append(feature);
                            layerName = QStringLiteral("roads");
                        }

                        QJsonObject tippecanoe;
                        tippecanoe[QStringLiteral("layer")] = layerName;
                        if (bPoint && mbFeature->properties().containsKey(QStringLiteral("place")))
                            tippecanoe[QStringLiteral("minzoom")] = QStringLiteral("11");
                        feature[QStringLiteral("tippecanoe")] = tippecanoe;

                        combined += feature;
                    }
                }
            }
        }

        auto writeJSON = [](const QString& name, const QJsonArray& array) {
            QJsonObject object;
            object[QLatin1Literal("type")] = QLatin1Literal("FeatureCollection");
            object[QLatin1Literal("features")] = array;
            QFile saveFile(QStringLiteral("D:/pz/worktree/build40-weather/pzmapbox/data/%1.geojson").arg(name));
            if (!saveFile.open(QIODevice::WriteOnly)) {
                 qWarning("Couldn't open save file.");
                 return false;
            }
            QJsonDocument json(object);
            saveFile.write(json.toJson());
            return true;
        };

        QProcess tippecanoe;
        tippecanoe.setProcessChannelMode(QProcess::MergedChannels);
        QStringList args;

#if 1
        if (!writeJSON(worldName, combined))
            goto errorExit;

         progress.update(QStringLiteral("Running tippecanoe"));

//        args << QStringLiteral("-e") << QStringLiteral("D:/pz/worktree/build40-weather/pzmapbox/data/tippecanoe-%1").arg(worldName);
        args << QStringLiteral("-o") << QStringLiteral("D:/pz/worktree/build40-weather/pzmapbox/data/tippecanoe-%1.mbtiles").arg(worldName);
        args << QStringLiteral("-f"); // force overwrite existing
        args << QStringLiteral("--temporary-directory") << QStringLiteral("D:/pz/worktree/build40-weather/pzmapbox/data/tippecanoe-tmp");
        args << QStringLiteral("--minimum-zoom") << QStringLiteral("11");
        args << QStringLiteral("--maximum-zoom") << QStringLiteral("18");
        args << QStringLiteral("--no-tile-compression");
        args << QStringLiteral("D:/pz/worktree/build40-weather/pzmapbox/data/%1.geojson").arg(worldName);
#else
        if (!writeJSON(QStringLiteral("buildings"), buildings))
            goto errorExit;
        if (!writeJSON(QStringLiteral("roads"), roads))
            goto errorExit;
        if (!writeJSON(QStringLiteral("water"), water))
            goto errorExit;

        args << QStringLiteral("-e D:/pz/worktree/build40-weather/pzmapbox/data/tippecanoe");
        args << QStringLiteral("D:/pz/worktree/build40-weather/pzmapbox/data/WorldEd-buildings.geojson");
        args << QStringLiteral("D:/pz/worktree/build40-weather/pzmapbox/data/WorldEd-roads.geojson");
        args << QStringLiteral("D:/pz/worktree/build40-weather/pzmapbox/data/WorldEd-water.geojson");
#endif
        tippecanoe.start(QCoreApplication::applicationDirPath() + QStringLiteral("/tippecanoe"),
                         args);
        tippecanoe.waitForStarted();
        tippecanoe.waitForFinished(10 * 60 * 1000);
        QString output = QString::fromLocal8Bit(tippecanoe.readAllStandardOutput());
        qDebug() << "tippecanoe exit code=" << tippecanoe.exitCode();
        qDebug() << "tippecanoe:" << output << tippecanoe.exitCode();
    }

    // While displaying this, the MapManager's FileSystemWatcher might see some
    // changed .tmx files, which results in the PROGRESS dialog being displayed.
    // It's a bit odd to see the PROGRESS dialog blocked behind this messagebox.
    QMessageBox::information(MainWindow::instance(),
                             tr("Mapbox"), tr("Finished!"));

    return true;

errorExit:
    return false;
}

bool MapBoxGeojsonGenerator::shouldGenerateCell(WorldCell *cell, int &bmpIndex)
{
    return true;
}

bool MapBoxGeojsonGenerator::generateCell(WorldCell *cell)
{
    int bmpIndex;
    if (!shouldGenerateCell(cell, bmpIndex))
        return true;

    if (cell->mapFilePath().isEmpty()) {
        return true;
    }

    MapInfo *mapInfo = MapManager::instance()->loadMap(cell->mapFilePath(),
                                                       mWorldDoc->fileName());
    if (!mapInfo) {
        mError = MapManager::instance()->errorString();
        return false;
    }

    MapManager::instance()->addReferenceToMap(mapInfo);

    bool ok = true;
    ok = doBuildings(cell, mapInfo);

    MapManager::instance()->removeReferenceToMap(mapInfo);

    return ok;
}

bool MapBoxGeojsonGenerator::doBuildings(WorldCell *cell, MapInfo *mapInfo)
{
    DelayedMapLoader mapLoader;
    mapLoader.addMap(mapInfo);

    foreach (WorldCellLot *lot, cell->lots()) {
        if (MapInfo *info = MapManager::instance()->loadMap(lot->mapName(),
                                                            QString(), true,
                                                            MapManager::PriorityMedium)) {
            mapLoader.addMap(info);
        } else {
            mError = MapManager::instance()->errorString();
            return false;
        }
    }

    // The cell map must be loaded before creating the MapComposite, which will
    // possibly load embedded lots.
    while (mapInfo->isLoading())
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);

    MapComposite staticMapComposite(mapInfo);
    MapComposite *mapComposite = &staticMapComposite;
    while (mapComposite->waitingForMapsToLoad() || mapLoader.isLoading())
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    if (!mapLoader.errorString().isEmpty()) {
        mError = mapLoader.errorString();
        return false;
    }

    foreach (WorldCellLot *lot, cell->lots()) {
        MapInfo *info = MapManager::instance()->mapInfo(lot->mapName());
        Q_ASSERT(info && info->map());
        mapComposite->addMap(info, lot->pos(), lot->level());
    }

    return processObjectGroups(cell, mapComposite);
}

bool MapBoxGeojsonGenerator::processObjectGroups(WorldCell *cell, MapComposite *mapComposite)
{
    foreach (Layer *layer, mapComposite->map()->layers()) {
        if (ObjectGroup *og = layer->asObjectGroup()) {
            if (!processObjectGroup(cell, og, mapComposite->levelRecursive(),
                                    mapComposite->originRecursive()))
                return false;
        }
    }

    foreach (MapComposite *subMap, mapComposite->subMaps())
        if (!processObjectGroups(cell, subMap))
            return false;

    return true;
}

bool MapBoxGeojsonGenerator::processObjectGroup(WorldCell *cell, ObjectGroup *objectGroup,
                                int levelOffset, const QPoint &offset)
{
    int level;
    if (!MapComposite::levelForLayer(objectGroup, &level))
        return true;
    level += levelOffset;

    if (level != 0)
        return true;

    QRect buildingBounds;

    foreach (const MapObject *mapObject, objectGroup->objects()) {
#if 0
        if (mapObject->name().isEmpty() || mapObject->type().isEmpty())
            continue;
#endif
        if (mapObject->width() * mapObject->height() <= 0)
            continue;

        int x = qFloor(mapObject->x());
        int y = qFloor(mapObject->y());
        int w = qCeil(mapObject->x() + mapObject->width()) - x;
        int h = qCeil(mapObject->y() + mapObject->height()) - y;

        QString name = mapObject->name();
        if (name.isEmpty())
            name = QLatin1String("unnamed");

        if (objectGroup->map()->orientation() == Map::Isometric) {
            x += 3 * level;
            y += 3 * level;
        }

        // Apply the MapComposite offset in the top-level map.
        x += offset.x();
        y += offset.y();

        if (objectGroup->name().contains(QLatin1String("RoomDefs"))) {
            if (x < 0 || y < 0 || x + w > 300 || y + h > 300) {
                x = qBound(0, x, 300);
                y = qBound(0, y, 300);
                mError = tr("A RoomDef in cell %1,%2 overlaps cell boundaries.\nNear x,y=%3,%4")
                        .arg(cell->x()).arg(cell->y()).arg(x).arg(y);
                return false;
            }
            buildingBounds |= QRect(x, y, w, h);
        }
    }

    if (!buildingBounds.isEmpty()) {
        buildingBounds.translate(cell->x() * 300, cell->y() * 300);

        QJsonObject feature;
        feature[QLatin1Literal("type")] = QLatin1Literal("Feature");

        QJsonObject properties;
        properties[QLatin1Literal("building")] = QLatin1Literal("yes");
        feature[QLatin1Literal("properties")] = properties;

        QJsonObject geometry;
        geometry[QLatin1Literal("type")] = QLatin1Literal("Polygon");

        QJsonArray exteriorRing; // counter-clockwise, longitude,latitude,elevation
        QJsonArray point;
        point.append(toLong(buildingBounds.left(), buildingBounds.top()));
        point.append(toLat(buildingBounds.left(), buildingBounds.top()));
        exteriorRing.append(point);
        point = QJsonArray();

        point.append(toLong(buildingBounds.left(), buildingBounds.bottom() + 1));
        point.append(toLat(buildingBounds.left(), buildingBounds.bottom() + 1));
        exteriorRing.append(point);
        point = QJsonArray();

        point.append(toLong(buildingBounds.right() + 1, buildingBounds.bottom() + 1));
        point.append(toLat(buildingBounds.right() + 1, buildingBounds.bottom() + 1));
        exteriorRing.append(point);
        point = QJsonArray();

        point.append(toLong(buildingBounds.right() + 1, buildingBounds.top()));
        point.append(toLat(buildingBounds.right() + 1, buildingBounds.top()));
        exteriorRing.append(point);

        exteriorRing.append(exteriorRing.at(0));

        QJsonArray coordinates;
        coordinates += exteriorRing;
        geometry[QLatin1Literal("coordinates")] = coordinates;

        feature[QLatin1Literal("geometry")] = geometry;

        mJsonFeatures.append(feature);
    }

    return true;
}
