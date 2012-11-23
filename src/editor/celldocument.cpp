/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
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

#include "celldocument.h"

#include "basegraphicsview.h"
#include "cellscene.h"
#include "mapcomposite.h"
#include "mapmanager.h"
#include "world.h"
#include "worldcell.h"
#include "worlddocument.h"

#include "layer.h"
#include "map.h"
#include "tilelayer.h"
#include "ztilelayergroup.h"

using namespace Tiled;

CellDocument::CellDocument(WorldDocument *worldDoc, WorldCell *cell)
    : Document(CellDocType)
    , mCell(cell)
    , mWorldDocument(worldDoc)
    , mCellScene(0)
    , mCurrentLayerIndex(-1)
    , mCurrentLevel(0)
    , mCurrentObjectGroup(world()->nullObjectGroup())
{
    mUndoStack = worldDoc->undoStack();

    connect(mWorldDocument, SIGNAL(cellContentsAboutToChange(WorldCell*)), SLOT(cellContentsAboutToChange(WorldCell*)));
    connect(mWorldDocument, SIGNAL(cellContentsChanged(WorldCell*)), SLOT(cellContentsChanged(WorldCell*)));
    connect(mWorldDocument, SIGNAL(cellMapFileAboutToChange(WorldCell*)), SLOT(cellMapFileAboutToChange(WorldCell*)));
    connect(mWorldDocument, SIGNAL(cellMapFileChanged(WorldCell*)), SLOT(cellMapFileChanged(WorldCell*)));

    connect(mWorldDocument, SIGNAL(cellLotAdded(WorldCell*,int)), SLOT(cellLotAdded(WorldCell*,int)));
    connect(mWorldDocument, SIGNAL(cellLotAboutToBeRemoved(WorldCell*,int)), SLOT(cellLotAboutToBeRemoved(WorldCell*,int)));
    connect(mWorldDocument, SIGNAL(cellLotMoved(WorldCellLot*)), SLOT(cellLotMoved(WorldCellLot*)));

    connect(mWorldDocument, SIGNAL(objectGroupAboutToBeRemoved(int)),
            SLOT(objectGroupAboutToBeRemoved(int)));

    connect(MapManager::instance(), SIGNAL(mapAboutToChange(MapInfo*)),
            SLOT(mapAboutToChange(MapInfo*)));
    connect(MapManager::instance(), SIGNAL(mapFileChanged(MapInfo*)),
            SLOT(mapFileChanged(MapInfo*)));
}

void CellDocument::setFileName(const QString &fileName)
{
    mWorldDocument->setFileName(fileName);
}

const QString &CellDocument::fileName() const
{
    return mWorldDocument->fileName();
}

bool CellDocument::save(const QString &filePath, QString &error)
{
    return mWorldDocument->save(filePath, error);
}

World *CellDocument::world() const
{
    return mWorldDocument->world();
}

void CellDocument::setScene(CellScene *scene)
{
    mCellScene = scene;

    mMiniMapItem = new CellMiniMapItem(mCellScene);
    view()->addMiniMapItem(mMiniMapItem);

    if (mCurrentLayerIndex == -1)
        setCurrentLevel(0);
}

void CellDocument::setSelectedLots(const QList<WorldCellLot *> &selected)
{
    mSelectedLots.clear();
    foreach (WorldCellLot *lot, selected) {
        if (!mSelectedLots.contains(lot))
            mSelectedLots.append(lot);
        else
            qWarning("duplicate lots passed to setSelectedLots");
    }
    emit selectedLotsChanged();
}

void CellDocument::setSelectedObjects(const QList<WorldCellObject *> &selected)
{
    mSelectedObjects.clear();
    foreach (WorldCellObject *obj, selected) {
        if (!mSelectedObjects.contains(obj))
            mSelectedObjects.append(obj);
        else
            qWarning("duplicate lots passed to setSelectedObjects");
    }
    emit selectedObjectsChanged();
    if (mSelectedObjects.size() == 1) {
        WorldCellObject *obj = mSelectedObjects.first();
        if (obj->level() != mCurrentLevel)
            setCurrentLevel(obj->level());
        setCurrentObjectGroup(obj->group());
    }
}

void CellDocument::setLayerVisibility(Layer *layer, bool visible)
{
    if (TileLayer *tl = layer->asTileLayer()) {
        CompositeLayerGroup *layerGroup = mCellScene->mapComposite()->layerGroupForLayer(tl);
        layerGroup->setLayerVisibility(tl, visible);
        emit layerVisibilityChanged(layer);
    }
}

void CellDocument::setLayerGroupVisibility(ZTileLayerGroup *layerGroup, bool visible)
{
    layerGroup->setVisible(visible);
    emit layerGroupVisibilityChanged(layerGroup);
}

void CellDocument::setLotLevelVisible(int level, bool visible)
{
    if (mLotLevelVisible.contains(level) && mLotLevelVisible[level] == visible)
        return;
    mLotLevelVisible[level] = visible;
    emit lotLevelVisibilityChanged(level);
}

bool CellDocument::isLotLevelVisible(int level)
{
    if (!mLotLevelVisible.contains(level)) // need a good place to init this
        mLotLevelVisible[level] = true;
    return mLotLevelVisible[level];
}

void CellDocument::setObjectLevelVisible(int level, bool visible)
{
    if (mObjectLevelVisible.contains(level) && mObjectLevelVisible[level] == visible)
        return;
    mObjectLevelVisible[level] = visible;
    emit objectLevelVisibilityChanged(level);
}

bool CellDocument::isObjectLevelVisible(int level)
{
    if (!mObjectLevelVisible.contains(level)) // need a good place to init this
        mObjectLevelVisible[level] = true;
    return mObjectLevelVisible[level];
}

void CellDocument::setObjectGroupVisible(WorldObjectGroup *og, int level, bool visible)
{
    if (mObjectGroupVisible.contains(og)
            && mObjectGroupVisible[og].contains(level)
            && mObjectGroupVisible[og][level] == visible)
        return;
    mObjectGroupVisible[og][level] = visible;
    emit objectGroupVisibilityChanged(og, level);
}

bool CellDocument::isObjectGroupVisible(WorldObjectGroup *og, int level)
{
    if (!mObjectGroupVisible.contains(og) ||
            !mObjectGroupVisible[og].contains(level)) // need a good place to init this
        mObjectGroupVisible[og][level] = true;
    return mObjectGroupVisible[og][level];
}

void CellDocument::setCurrentLayerIndex(int index)
{
    Q_ASSERT(index >= -1 && index < scene()->map()->layerCount());
    mCurrentLayerIndex = index;

    mCurrentLevel = (index == -1) ? 0 : currentLayer()->level();
    emit currentLevelChanged(mCurrentLevel);

    /* This function always sends the following signal, even if the index
     * didn't actually change. This is because the selected index in the layer
     * table view might be out of date anyway, and would otherwise not be
     * properly updated.
     *
     * This problem happens due to the selection model not sending signals
     * about changes to its current index when it is due to insertion/removal
     * of other items. The selected item doesn't change in that case, but our
     * layer index does.
     */
    emit currentLayerIndexChanged(mCurrentLayerIndex);
}

Tiled::Layer *CellDocument::currentLayer() const
{
    if (mCurrentLayerIndex == -1)
        return 0;

    return scene()->map()->layerAt(mCurrentLayerIndex);
}

TileLayer *CellDocument::currentTileLayer() const
{
    if (Layer *layer = currentLayer())
        return layer->asTileLayer();
    return 0;
}

void CellDocument::setCurrentLevel(int level)
{
    Q_ASSERT(level >= 0 && level < scene()->mapComposite()->layerGroupCount());
    mCurrentLevel = level;
    mCurrentLayerIndex = -1;
    emit currentLevelChanged(mCurrentLevel);
    emit currentLayerIndexChanged(mCurrentLayerIndex);
}

CompositeLayerGroup *CellDocument::currentLayerGroup() const
{
    return scene()->mapComposite()->tileLayersForLevel(mCurrentLevel);
}

void CellDocument::setCurrentObjectGroup(WorldObjectGroup *og)
{
    Q_ASSERT(og);
    if (!og || og == mCurrentObjectGroup)
        return;
    mCurrentObjectGroup = og;
    emit currentObjectGroupChanged(mCurrentObjectGroup);
}

WorldObjectGroup *CellDocument::currentObjectGroup() const
{
    Q_ASSERT(mCurrentObjectGroup);
    Q_ASSERT(world()->objectGroups().indexOf(mCurrentObjectGroup) != -1);
    return mCurrentObjectGroup;
}

void CellDocument::cellContentsAboutToChange(WorldCell *cell)
{
    if (cell == mCell) {
        mCurrentLayerIndex = -1;
        mCurrentLevel = 0;
        setSelectedLots(QList<WorldCellLot*>());
        emit cellContentsAboutToChange();

        mMiniMapItem->cellContentsAboutToChange();
    }
}

void CellDocument::cellContentsChanged(WorldCell *cell)
{
    if (cell == mCell) {
        emit cellContentsChanged();
        mMiniMapItem->cellContentsChanged();
    }
}

void CellDocument::cellMapFileAboutToChange(WorldCell *cell)
{
    if (cell == mCell)
        emit cellMapFileAboutToChange();
}

void CellDocument::cellMapFileChanged(WorldCell *cell)
{
    if (cell == mCell) {
        emit cellMapFileChanged();
        mMiniMapItem->updateCellImage();
    }
}

void CellDocument::cellLotAdded(WorldCell *cell, int index)
{
    if (cell == mCell)
        mMiniMapItem->lotAdded(index);
}

void CellDocument::cellLotAboutToBeRemoved(WorldCell *cell, int index)
{
    if (cell == mCell) {
        QList<WorldCellLot*> selection = mSelectedLots;
        selection.removeAll(cell->lots().at(index));
        setSelectedLots(selection);

        mMiniMapItem->lotRemoved(index);
    }
}

void CellDocument::cellLotMoved(WorldCellLot *lot)
{
    if (lot->cell() == mCell) {
        int index = mCell->lots().indexOf(lot);
        mMiniMapItem->lotMoved(index);
    }
}

void CellDocument::objectGroupAboutToBeRemoved(int index)
{
    WorldObjectGroup *og = world()->objectGroups().at(index);
    if (og == mCurrentObjectGroup)
        setCurrentObjectGroup(world()->nullObjectGroup());
}

// Called by MapManager when an already-loaded TMX changes on disk
void CellDocument::mapAboutToChange(MapInfo *mapInfo)
{
    if (scene()->mapAboutToChange(mapInfo))
        worldDocument()->emitCellMapFileAboutToChange(mCell);
}

// Called by MapManager when an already-loaded TMX changes on disk
void CellDocument::mapFileChanged(MapInfo *mapInfo)
{
    if (scene()->mapFileChanged(mapInfo))
        worldDocument()->emitCellMapFileChanged(mCell);
}
