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

#include "tmxtobmp.h"

#include "bmptotmx.h"
#include "lotfilesmanager.h"
#include "mapcomposite.h"
#include "mapmanager.h"
#include "progress.h"
#include "world.h"
#include "worldcell.h"
#include "worlddocument.h"

#include "mapobject.h"
#include "objectgroup.h"

#include <qmath.h>
#include <QFileInfo>
#include <QPainter>

using namespace Tiled;

SINGLETON_IMPL(TMXToBMP)

TMXToBMP::TMXToBMP(QObject *parent) :
    QObject(parent)
{
}

bool TMXToBMP::generateWorld(WorldDocument *worldDoc, TMXToBMP::GenerateMode mode)
{
    mWorldDoc = worldDoc;
    World *world = mWorldDoc->world();

    const TMXToBMPSettings &settings = world->getTMXToBMPSettings();
    mDoMain = settings.doMain;
    mDoVeg = settings.doVegetation;
    mDoBldg = settings.doBuildings;

    MapManager::instance()->purgeUnreferencedMaps();

    PROGRESS progress(QLatin1String("Setting up images"));

//    foreach (WorldBMP *bmp, world->bmps()) {
//        BMPToTMXImages *images = BMPToTMX::instance()->getImages(bmp->filePath(), bmp->pos());
//        if (!images) {
//            goto errorExit;
//        }
//        mImages += images;
//    }

    if (mode == GenerateSelected) {
        // Merge the selected map BMP data with existing images
        if (mDoMain) {
            if (QFileInfo(settings.mainFile).exists()) {
                mImageMain = QImage(settings.mainFile);
                if (mImageMain.isNull() || (mImageMain.size() != world->size() * 300)) {
                    mError = tr("The existing 'main' image file could not be loaded or is the wrong size, can't merge.");
                    goto errorExit;
                }
            } else {
                mImageMain = QImage(world->size() * 300, QImage::Format_ARGB32);
            }
        }
        if (mDoVeg) {
            if (QFileInfo(settings.vegetationFile).exists()) {
                mImageVeg = QImage(settings.vegetationFile);
                if (mImageVeg.isNull() || (mImageVeg.size() != world->size() * 300)) {
                    mError = tr("The existing 'vegetation' image file could not be loaded or is the wrong size, can't merge.");
                    goto errorExit;
                }
            } else {
                mImageVeg = QImage(world->size() * 300, QImage::Format_ARGB32);
            }
        }
        if (mDoBldg) {
            if (QFileInfo(settings.buildingsFile).exists()) {
                mImageBldg = QImage(settings.buildingsFile);
                if (mImageBldg.isNull() || (mImageBldg.size() != world->size() * 300)) {
                    mError = tr("The existing 'buildings' image file could not be loaded or is the wrong size, can't merge.");
                    goto errorExit;
                }
            } else {
                mImageBldg = QImage(world->size() * 300, QImage::Format_ARGB32);
            }
        }
    } else {
        if (mDoMain)
            mImageMain = QImage(world->size() * 300, QImage::Format_ARGB32);
        if (mDoVeg)
            mImageVeg = QImage(world->size() * 300, QImage::Format_ARGB32);
        if (mDoBldg)
            mImageBldg = QImage(world->size() * 300, QImage::Format_ARGB32);
    }

    if ((mDoMain && mImageMain.isNull()) ||
            (mDoVeg && mImageVeg.isNull()) ||
            (mDoBldg && mImageBldg.isNull())) {
        mError = tr("Failed to create images.  There might not be enough memory.\nTry closing any open cell documents or restart the application.");
        goto errorExit;
    }

    if (mDoBldg) {
        if (!mBldgPainter.begin(&mImageBldg)) {
            mError = tr("Can't paint to the buildings image for some reason.");
            goto errorExit;
        }
        mBldgPainter.setCompositionMode(QPainter::CompositionMode_Source);
    }

    progress.update(QLatin1String("Reading maps"));

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

    if (mDoMain) {
        progress.update(QLatin1String("Saving main image"));
        mImageMain.save(settings.mainFile);
    }
    if (mDoVeg) {
        progress.update(QLatin1String("Saving vegetation image"));
        mImageVeg.save(settings.vegetationFile);
    }
    if (mDoBldg) {
        progress.update(QLatin1String("Saving buildings image"));
        mImageBldg.save(settings.buildingsFile);
    }

    mImageMain = QImage();
    mImageVeg = QImage();
    if (mDoBldg) {
        mBldgPainter.end();
        mImageBldg = QImage();
    }
    return true;

errorExit:
    mImageMain = QImage();
    mImageVeg = QImage();
    if (mDoBldg) {
        mBldgPainter.end();
        mImageBldg = QImage();
    }
    return false;
}

bool TMXToBMP::generateCell(WorldCell *cell)
{
    if (cell->mapFilePath().isEmpty()) {
        if (mDoMain) {
            QPainter painter(&mImageMain);
            painter.fillRect(cell->x() * 300, cell->y() * 300, 300, 300, Qt::black);
        }
        if (mDoVeg) {
            QPainter painter(&mImageVeg);
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.fillRect(cell->x() * 300, cell->y() * 300, 300, 300, Qt::transparent);
        }
        if (mDoBldg) {
            mBldgPainter.fillRect(cell->x() * 300, cell->y() * 300, 300, 300, Qt::transparent);
        }
        return true;
    }

    MapInfo *mapInfo = MapManager::instance()->loadMap(cell->mapFilePath(),
                                                       mWorldDoc->fileName());
    if (!mapInfo) {
        mError = MapManager::instance()->errorString();
        return false;
    }

    MapManager::instance()->addReferenceToMap(mapInfo);

    QRgb black = qRgba(0, 0, 0, 255);
    QRgb transparent = qRgba(0, 0, 0, 0);

    if (mDoMain) {
        QPainter painter(&mImageMain);
        painter.drawImage(cell->x() * 300, cell->y() * 300, mapInfo->map()->bmpMain().image());
    }

    if (mDoVeg) {
        QPainter painter(&mImageVeg);
        painter.drawImage(cell->x() * 300, cell->y() * 300, mapInfo->map()->bmpVeg().image());
        for (int y = 0; y < 300; y++) {
            for (int x = 0; x < 300; x++) {
                if (mImageVeg.pixel(cell->x() * 300 + x, cell->y() * 300 + y) == black)
                    mImageVeg.setPixel(cell->x() * 300 + x, cell->y() * 300 + y, transparent);
            }
        }
    }

    bool ok = true;
    if (mDoBldg) {
        ok = doBuildings(cell, mapInfo);
    }

    MapManager::instance()->removeReferenceToMap(mapInfo);

    return ok;
}

bool TMXToBMP::doBuildings(WorldCell *cell, MapInfo *mapInfo)
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

    mBldgPainter.fillRect(cell->x() * 300, cell->y() * 300, 300, 300, Qt::transparent);

    return processObjectGroups(cell, mapComposite);
}

bool TMXToBMP::processObjectGroups(WorldCell *cell, MapComposite *mapComposite)
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

bool TMXToBMP::processObjectGroup(WorldCell *cell, ObjectGroup *objectGroup,
                                  int levelOffset, const QPoint &offset)
{
    int level;
    if (!MapComposite::levelForLayer(objectGroup, &level))
        return true;
    level += levelOffset;

    foreach (const MapObject *mapObject, objectGroup->objects()) {
#if 0
        if (mapObject->name().isEmpty() || mapObject->type().isEmpty())
            continue;
#endif
        if (!mapObject->width() || !mapObject->height())
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

            mBldgPainter.fillRect(cell->x() * 300 + x,
                                  cell->y() * 300 + y,
                                  w, h, mBldgColor);
        }
    }
    return true;
}
