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
#include <QFileInfo>
#include <QMessageBox>
#include <QPainter>

using namespace Tiled;

SINGLETON_IMPL(TMXToBMP)

TMXToBMP::TMXToBMP(QObject *parent) :
    QObject(parent)
{
}

#include "threads.h"
bool TMXToBMP::generateWorld(WorldDocument *worldDoc, TMXToBMP::GenerateMode mode)
{
    mWorldDoc = worldDoc;
    World *world = mWorldDoc->world();

    const TMXToBMPSettings &settings = world->getTMXToBMPSettings();
    mDoMain = settings.doMain;
    mDoVeg = settings.doVegetation;
    mDoBldg = settings.doBuildings;

    MapManager::instance().purgeUnreferencedMaps();

    PROGRESS progress(QLatin1String("Setting up images"));

    foreach (WorldBMP *bmp, world->bmps()) {
        BMPToTMXImages *images = BMPToTMX::instance()->getImages(bmp->filePath(), bmp->pos());
        if (!images) {
            goto errorExit;
        }
        mImages += images;
    }

    if (mode == GenerateSelected) {
#if 0
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
#endif
        if (mDoBldg) {
            if (QFileInfo(settings.buildingsFile).exists()) {
                mImageBldg = QImage(settings.buildingsFile);
                if (mImageBldg.isNull() || (mImageBldg.size() != world->size() * 300)) {
                    mError = tr("The existing 'buildings' image file could not be loaded or is the wrong size, can't merge.");
                    goto errorExit;
                }
            } else {
                mImageBldg = QImage(world->size() * 300, QImage::Format_ARGB32);
                mImageBldg.fill(Qt::transparent);
            }
        }
    } else {
        // Don't bother loading the existing image since it will be replaced.
#if 0
        if (mDoMain)
            mImageMain = QImage(world->size() * 300, QImage::Format_ARGB32);
        if (mDoVeg)
            mImageVeg = QImage(world->size() * 300, QImage::Format_ARGB32);
#endif
        if (mDoBldg) {
            mImageBldg = QImage(world->size() * 300, QImage::Format_ARGB32);
            mImageBldg.fill(Qt::transparent);
        }
    }

    if (/*(mDoMain && mImageMain.isNull()) ||
            (mDoVeg && mImageVeg.isNull()) ||*/
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

    mModifiedImages.clear();

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

    MapManager::instance().purgeUnreferencedMaps();

#if 1
    foreach (BMPToTMXImages *images, mImages) {
        if (!mModifiedImages.contains(images))
            continue;
        QFileInfo info(images->mPath);
        if (mDoMain) {
            progress.update(QLatin1String("Saving ") + info.fileName());
            images->mBmp.save(images->mPath);
//            Sleep::sleep(2);
        }
        if (mDoVeg) {
            progress.update(QLatin1String("Saving ") + info.baseName() + QLatin1String("_veg.") + info.suffix());
            images->mBmpVeg.save(info.absolutePath() + QLatin1String("/") + info.baseName() + QLatin1String("_veg.") + info.suffix());
//            Sleep::sleep(2);
        }
    }

#else
    if (mDoMain) {
        progress.update(QLatin1String("Saving main image"));
        mImageMain.save(settings.mainFile);
    }
    if (mDoVeg) {
        progress.update(QLatin1String("Saving vegetation image"));
        mImageVeg.save(settings.vegetationFile);
    }
#endif
    if (mDoBldg) {
        progress.update(QLatin1String("Saving buildings image"));
        mImageBldg.save(settings.buildingsFile);
//        Sleep::sleep(2);
    }

    qDeleteAll(mImages);
    mImages.clear();
    if (mDoBldg) {
        mBldgPainter.end();
        mImageBldg = QImage();
    }

    // While displaying this, the MapManager's FileSystemWatcher might see some
    // changed .tmx files, which results in the PROGRESS dialog being displayed.
    // It's a bit odd to see the PROGRESS dialog blocked behind this messagebox.
    QMessageBox::information(MainWindow::instance(),
                             tr("TMP To BMP"), tr("Finished!"));

    return true;

errorExit:
    qDeleteAll(mImages);
    mImages.clear();
    if (mDoBldg) {
        mBldgPainter.end();
        mImageBldg = QImage();
    }
    return false;
}

bool TMXToBMP::shouldGenerateCell(WorldCell *cell, int &bmpIndex)
{
    // Get the top-most BMP covering the cell
    int n = 0;
    bmpIndex = -1;
    foreach (WorldBMP *bmp, cell->world()->bmps()) {
        if (bmp->bounds().contains(cell->pos()))
            bmpIndex = n;
        n++;
    }
    if (bmpIndex == -1)
        return false;

    return true;
}

bool TMXToBMP::generateCell(WorldCell *cell)
{
    int bmpIndex;
    if (!shouldGenerateCell(cell, bmpIndex))
        return true;
    BMPToTMXImages *images = mImages[bmpIndex];

    if (cell->mapFilePath().isEmpty()) {
        if (mDoMain) {
            QPainter painter(&images->mBmp);
            painter.fillRect(cell->x() * 300, cell->y() * 300, 300, 300, Qt::black);
            mModifiedImages.insert(images);
        }
        if (mDoVeg) {
            QPainter painter(&images->mBmpVeg);
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.fillRect(cell->x() * 300, cell->y() * 300, 300, 300, Qt::transparent);
            mModifiedImages.insert(images);
        }
        if (mDoBldg) {
            mBldgPainter.fillRect(cell->x() * 300, cell->y() * 300, 300, 300, Qt::transparent);
        }
        return true;
    }

    MapInfo *mapInfo = MapManager::instance().loadMap(cell->mapFilePath(),
                                                       mWorldDoc->fileName());
    if (!mapInfo) {
        mError = MapManager::instance().errorString();
        return false;
    }

    MapManager::instance().addReferenceToMap(mapInfo);

    QRgb black = qRgba(0, 0, 0, 255);
    QRgb transparent = qRgba(0, 0, 0, 0);

    if (mDoMain) {
        QPainter painter(&images->mBmp);
        painter.drawImage((cell->pos() - images->mBounds.topLeft()) * 300, mapInfo->map()->bmpMain().image());
        mModifiedImages.insert(images);
    }

    if (mDoVeg) {
        QPainter painter(&images->mBmpVeg);
        QPoint p = (cell->pos() - images->mBounds.topLeft()) * 300;
        painter.drawImage(p, mapInfo->map()->bmpVeg().image());
        for (int y = 0; y < 300; y++) {
            for (int x = 0; x < 300; x++) {
                if (images->mBmpVeg.pixel(p + QPoint(x, y)) == black)
                    images->mBmpVeg.setPixel(p + QPoint(x, y), transparent);
            }
        }
        mModifiedImages.insert(images);
    }

    bool ok = true;
    if (mDoBldg) {
        ok = doBuildings(cell, mapInfo);
    }

    MapManager::instance().removeReferenceToMap(mapInfo);

    return ok;
}

bool TMXToBMP::doBuildings(WorldCell *cell, MapInfo *mapInfo)
{
    DelayedMapLoader mapLoader;
    mapLoader.addMap(mapInfo);

    foreach (WorldCellLot *lot, cell->lots()) {
        if (MapInfo *info = MapManager::instance().loadMap(lot->mapName(),
                                                            QString(), true,
                                                            MapManager::PriorityMedium)) {
            mapLoader.addMap(info);
        } else {
            mError = MapManager::instance().errorString();
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
        MapInfo *info = MapManager::instance().mapInfo(lot->mapName());
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
