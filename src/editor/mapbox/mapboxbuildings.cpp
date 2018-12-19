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

#include "mapboxbuildings.h"

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
#include <QDebug>
#include <QFileInfo>
#include <QMessageBox>

using namespace Tiled;

MapboxBuildings::MapboxBuildings(QObject *parent) :
    QObject(parent)
{
}

bool MapboxBuildings::generateWorld(WorldDocument *worldDoc, MapboxBuildings::GenerateMode mode)
{
    mWorldDoc = worldDoc;
    World *world = mWorldDoc->world();

    MapManager::instance()->purgeUnreferencedMaps();

    PROGRESS progress(QLatin1String("Generating building features"));

    if (mode == GenerateSelected) {
        for (WorldCell *cell : worldDoc->selectedCells())
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

    // While displaying this, the MapManager's FileSystemWatcher might see some
    // changed .tmx files, which results in the PROGRESS dialog being displayed.
    // It's a bit odd to see the PROGRESS dialog blocked behind this messagebox.
    QMessageBox::information(MainWindow::instance(),
                             tr("Mapbox Buildings"), tr("Finished!"));

    return true;

errorExit:
    return false;
}

bool MapboxBuildings::shouldGenerateCell(WorldCell *cell)
{
    return true;
}

bool MapboxBuildings::generateCell(WorldCell *cell)
{
    if (!shouldGenerateCell(cell))
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

    // Remove all "building=" features
    auto& features = cell->mapBox().features();
    for (int i = features.size() - 1; i >= 0; i--) {
        auto* feature = features[i];
        for (auto& property : feature->properties()) {
            if (property.mKey == QStringLiteral("building")) {
                mWorldDoc->removeMapboxFeature(cell, feature->index());
            }
        }
    }

    bool ok = true;
    ok = doBuildings(cell, mapInfo);

    MapManager::instance()->removeReferenceToMap(mapInfo);

    return ok;
}

bool MapboxBuildings::doBuildings(WorldCell *cell, MapInfo *mapInfo)
{
    DelayedMapLoader mapLoader;
    mapLoader.addMap(mapInfo);

    for (WorldCellLot *lot : cell->lots()) {
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

bool MapboxBuildings::processObjectGroups(WorldCell *cell, MapComposite *mapComposite)
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

bool MapboxBuildings::processObjectGroup(WorldCell *cell, ObjectGroup *objectGroup,
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
        MapBoxFeature* feature = new MapBoxFeature(&cell->mapBox());

        MapBoxProperty property;
        property.mKey = QStringLiteral("building");
        property.mValue = QStringLiteral("yes");
        feature->properties() += property;

        feature->mGeometry.mType = QStringLiteral("Polygon");
        MapBoxCoordinates coords;
        coords += MapBoxPoint(buildingBounds.left(), buildingBounds.top());
        coords += MapBoxPoint(buildingBounds.right() + 1, buildingBounds.top());
        coords += MapBoxPoint(buildingBounds.right() + 1, buildingBounds.bottom() + 1);
        coords += MapBoxPoint(buildingBounds.left(), buildingBounds.bottom() + 1);
        feature->mGeometry.mCoordinates += coords;

        mWorldDoc->addMapboxFeature(cell, cell->mapBox().features().size(), feature);
    }

    return true;
}
