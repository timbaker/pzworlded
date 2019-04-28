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

#ifndef CELLDOCUMENT_H
#define CELLDOCUMENT_H

#include "document.h"

#include <QMap>

class CellMiniMapItem;
class CellScene;
class CompositeLayerGroup;
class MapAsset;
class MapInfo;
class World;
class WorldCell;
class WorldCellLot;
class WorldCellObject;
class WorldDocument;
class WorldObjectGroup;

namespace Tiled {
class Layer;
class TileLayer;
class ZTileLayerGroup;
}

class CellDocument : public Document
{
    Q_OBJECT

public:
    explicit CellDocument(WorldDocument *worldDoc, WorldCell *cell);

    void setFileName(const QString &fileName);
    const QString &fileName() const;
    bool save(const QString &filePath, QString &error);

    bool isModified() const { return false; }

    WorldCell *cell() const { return mCell; }

    WorldDocument *worldDocument() const { return mWorldDocument; }
    World *world() const;

    void setScene(CellScene *scene);
    CellScene *scene() const { return mCellScene; }

    void setSelectedLots(const QList<WorldCellLot*> &selected);
    const QList<WorldCellLot*> &selectedLots() const { return mSelectedLots; }
    int selectedLotCount() const { return mSelectedLots.count(); }

    void setSelectedObjects(const QList<WorldCellObject*> &selected);
    const QList<WorldCellObject*> &selectedObjects() const { return mSelectedObjects; }
    int selectedObjectCount() const { return mSelectedObjects.count(); }

    void setLayerVisibility(Tiled::Layer *layer, bool visible);
    void setLayerGroupVisibility(Tiled::ZTileLayerGroup *layerGroup, bool visible);

    void setLotLevelVisible(int level, bool visible);
    bool isLotLevelVisible(int level);

    void setObjectLevelVisible(int level, bool visible);
    bool isObjectLevelVisible(int level);

    void setObjectGroupVisible(WorldObjectGroup *og, int level, bool visible);
    bool isObjectGroupVisible(WorldObjectGroup *og, int level);

    void setCurrentLayerIndex(int index);
    int currentLayerIndex() const { return mCurrentLayerIndex; }
    Tiled::Layer *currentLayer() const;
    Tiled::TileLayer *currentTileLayer() const;

    void setCurrentLevel(int level);
    int currentLevel() const { return mCurrentLevel; }
    CompositeLayerGroup *currentLayerGroup() const;

    void setCurrentObjectGroup(WorldObjectGroup *og);
    WorldObjectGroup *currentObjectGroup() const;

signals:
    void layerVisibilityChanged(Tiled::Layer *layer);
    void layerGroupAdded(int level);
    void layerGroupVisibilityChanged(Tiled::ZTileLayerGroup *layerGroup);
    void lotLevelVisibilityChanged(int level);
    void objectLevelVisibilityChanged(int level);
    void objectGroupVisibilityChanged(WorldObjectGroup *og, int level);
    void selectedLotsChanged();
    void selectedObjectsChanged();
    void cellContentsAboutToChange();
    void cellContentsChanged();
    void cellMapFileAboutToChange();
    void cellMapFileChanged();
    void currentLayerIndexChanged(int);
    void currentLevelChanged(int);
    void currentObjectGroupChanged(WorldObjectGroup *og);

private slots:
    void mapAboutToChange(MapAsset *mapInfo);
    void mapChanged(MapAsset *mapInfo);

    void cellContentsAboutToChange(WorldCell *cell);
    void cellContentsChanged(WorldCell *cell);
    void cellMapFileAboutToChange(WorldCell *cell);
    void cellMapFileChanged(WorldCell *cell);

    void cellLotAdded(WorldCell *cell, int index);
    void cellLotAboutToBeRemoved(WorldCell *cell, int index);
    void cellLotMoved(WorldCellLot *lot);

    void objectGroupAboutToBeRemoved(int index);

private:
    WorldCell *mCell;
    WorldDocument *mWorldDocument;
    CellScene *mCellScene;
    QList<WorldCellLot*> mSelectedLots;
    QList<WorldCellObject*> mSelectedObjects;
    int mCurrentLayerIndex;
    int mCurrentLevel;
    WorldObjectGroup *mCurrentObjectGroup;
    QMap<int,bool> mLotLevelVisible;
    QMap<int,bool> mObjectLevelVisible;
    QMap<WorldObjectGroup*,QMap<int,bool> > mObjectGroupVisible;
    CellMiniMapItem *mMiniMapItem;
};

#endif // CELLDOCUMENT_H
