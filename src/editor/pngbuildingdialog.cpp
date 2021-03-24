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

#include "pngbuildingdialog.h"
#include "ui_pngbuildingdialog.h"

#include "bmptotmx.h"
#include "lotfilesmanager.h"
#include "mapcomposite.h"
#include "mapmanager.h"
#include "progress.h"
#include "world.h"
#include "worldcell.h"

#include "mapobject.h"
#include "objectgroup.h"
#include "tile.h"
#include "tileset.h"

#include <qmath.h>
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <QSettings>

using namespace Tiled;

PNGBuildingDialog::PNGBuildingDialog(World *world, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PNGBuildingDialog),
    mWorld(world)
{
    ui->setupUi(this);

    mColor = Qt::red;
    ui->color->setColor(mColor);

    connect(ui->pngBrowse, SIGNAL(clicked()), SLOT(browse()));

    QSettings settings;
    ui->pngEdit->setText(settings.value(QLatin1String("PNGBuildingDialog/FileName")).toString());
}

PNGBuildingDialog::~PNGBuildingDialog()
{
    QSettings settings;
    settings.setValue(QLatin1String("PNGBuildingDialog/FileName"), ui->pngEdit->text());
    delete ui;
}

void PNGBuildingDialog::accept()
{
    if (ui->pngEdit->text().isEmpty())
        return;

    mColor = ui->color->color();

    if (!generateWorld(mWorld)) {
        QMessageBox::warning(this, tr("PNG Generation Failed"), mError);
    }

    QDialog::accept();
}

void PNGBuildingDialog::browse()
{
    QString f = QFileDialog::getSaveFileName(this, tr("Save PNG As..."),
                                             ui->pngEdit->text(), QLatin1String("PNG Files (*.png)"));
    if (f.isEmpty())
        return;
    ui->pngEdit->setText(QDir::toNativeSeparators(f));
}

bool PNGBuildingDialog::generateWorld(World *world)
{
    // Try to free up some memory before loading large images.
    MapManager::instance()->purgeUnreferencedMaps();

    PROGRESS progress(QLatin1String("Reading BMP images"));

    mImage = QImage(world->size() * 300, QImage::Format_RGB555);
    mImage.fill(Qt::black);
    QPainter p(&mImage);
    mPainter = &p;

    foreach (WorldBMP *bmp, world->bmps()) {
        BMPToTMXImages *images = BMPToTMX::instance()->getImages(bmp->filePath(),
                                                                 bmp->pos(),
                                                                 QImage::Format_RGB555);
        if (!images) {
            mError = BMPToTMX::instance()->errorString();
            goto errorExit;
        }
        p.drawImage(images->mBounds.topLeft() * 300, images->mBmp);
        delete images;
    }

    for (int y = 0; y < world->height(); y++) {
        for (int x = 0; x < world->width(); x++) {
            if (!generateCell(world->cellAt(x, y)))
                goto errorExit;
        }
    }

    if (!mImage.save(ui->pngEdit->text()))
        goto errorExit;

    return true;

errorExit:
    return false;
}

bool PNGBuildingDialog::generateCell(WorldCell *cell)
{
    if (cell->mapFilePath().isEmpty())
        return true;

    PROGRESS progress(tr("Processing cell %1,%2")
                      .arg(cell->x()).arg(cell->y()));

    MapInfo *mapInfo = MapManager::instance()->loadMap(cell->mapFilePath(),
                                                       QString(), true);
    if (!mapInfo) {
        mError = MapManager::instance()->errorString();
        return false;
    }

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

    processObjectGroups(cell, mapComposite);

    CompositeLayerGroup *layerGroup = mapComposite->layerGroupForLevel(0);
    QList<Tileset*> tilesets;
    foreach (Tileset *ts, mapInfo->map()->tilesets())
        if (ts->name().startsWith(QLatin1String("vegetation_trees_")))
            tilesets += ts;
    if (layerGroup && !tilesets.isEmpty()) {
        QVector<const Cell*> cells(40);
        layerGroup->prepareDrawing2();
        QRgb treeColor = qRgb(47, 76, 64); // same dark green as MapImageManager uses
        for (int y = 0; y < mapInfo->map()->height(); y++) {
            for (int x = 0; x < mapInfo->map()->width(); x++) {
                cells.resize(0);
                if (layerGroup->orderedCellsAt2(QPoint(x, y), cells)) {
                    foreach (const Cell *tileCell, cells) {
                        if (tilesets.contains(tileCell->tile->tileset())) {
                            mImage.setPixel(cell->x() * 300 + x, cell->y() * 300 + y, treeColor);
                            break;
                        }
                    }
                }
            }
        }
    }

    return true;
}

bool PNGBuildingDialog::processObjectGroups(WorldCell *cell, MapComposite *mapComposite)
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

bool PNGBuildingDialog::processObjectGroup(WorldCell *cell, ObjectGroup *objectGroup,
                                           int levelOffset, const QPoint &offset)
{
    int level = objectGroup->level();
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

            mPainter->fillRect(cell->x() * 300 + x,
                               cell->y() * 300 + y,
                               w, h, mColor);
        }
    }
    return true;
}
