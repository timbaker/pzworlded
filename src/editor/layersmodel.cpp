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

#include "layersmodel.h"

#include "celldocument.h"
#include "cellscene.h"
#include "mapcomposite.h"
#include "mapmanager.h"
#include "worlddocument.h"

#include "map.h"
#include "maplevel.h"
#include "tilelayer.h"

using namespace Tiled;

LayersModel::LayersModel(QObject *parent)
    : QAbstractItemModel(parent)
    , mCellDocument(0)
    , mMapComposite(0)
    , mMap(0)
    , mRootItem(0)
{
}

LayersModel::~LayersModel()
{
    delete mRootItem;
}

QModelIndex LayersModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!mMap)
        return QModelIndex();

    if (!parent.isValid()) {
        if (row < mRootItem->children.size())
            return createIndex(row, column, mRootItem->children.at(row));
        return QModelIndex();
    }

    if (Item *item = toItem(parent)) {
        if (row < item->children.size())
            return createIndex(row, column, item->children.at(row));
        return QModelIndex();
    }

    return QModelIndex();
}

QModelIndex LayersModel::parent(const QModelIndex &index) const
{
    if (Item *item = toItem(index)) {
        if (item->parent != mRootItem) {
            Item *grandParent = item->parent->parent;
            return createIndex(grandParent->children.indexOf(item->parent), 0, item->parent);
        }
    }
    return QModelIndex();
}

int LayersModel::rowCount(const QModelIndex &parent) const
{
    if (!mRootItem)
        return 0;
    if (!parent.isValid())
        return mRootItem->children.size();
    if (Item *item = toItem(parent))
        return item->children.size();
    return 0;
}

int LayersModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

QVariant LayersModel::data(const QModelIndex &index, int role) const
{
    if (TileLayer *tl = toTileLayer(index)) {
        switch (role) {
        case Qt::DisplayRole:
            return MapComposite::layerNameWithoutPrefix(tl);
        case Qt::EditRole:
            return tl->name();
        case Qt::DecorationRole:
            return QVariant();
        case Qt::CheckStateRole: {
            CompositeLayerGroup *g = mMapComposite->layerGroupForLayer(tl);
            return g->isLayerVisible(tl) ? Qt::Checked : Qt::Unchecked;
        }
        case OpacityRole:
            return qreal(1);
        default:
            return QVariant();
        }
    }
    if (MapLevel *mapLevel = toMapLevel(index)) {
        switch (role) {
        case Qt::DisplayRole:
        case Qt::EditRole:
            return QString(tr("Level %1")).arg(mapLevel->level());
        case Qt::DecorationRole:
            return QVariant();
        case Qt::CheckStateRole: {
            CompositeLayerGroup *g = mMapComposite->layerGroupForLevel(mapLevel->level());
            return g->isVisible() ? Qt::Checked : Qt::Unchecked;
        }
        case OpacityRole:
            return qreal(1);
        default:
            return QVariant();
        }
    }
    return QVariant();
}

bool LayersModel::setData(const QModelIndex &index, const QVariant &value,
                         int role)
{
    if (TileLayer *tl = toTileLayer(index)) {
        switch (role) {
        case Qt::CheckStateRole:
            {
            Qt::CheckState c = static_cast<Qt::CheckState>(value.toInt());
            mCellDocument->setLayerVisibility(tl, c == Qt::Checked);
            emit dataChanged(index, index);
            return true;
            }
        case Qt::EditRole:
            {
            return false;
            }
        }
        return false;
    }
    if (MapLevel *mapLevel = toMapLevel(index)) {
        switch (role) {
        case Qt::CheckStateRole:
            {
            Qt::CheckState c = static_cast<Qt::CheckState>(value.toInt());
            CompositeLayerGroup *g = mMapComposite->layerGroupForLevel(mapLevel->level());
            mCellDocument->setLayerGroupVisibility(g, c == Qt::Checked);
            emit dataChanged(index, index);
            return true;
            }
        case Qt::EditRole:
            {
            return false;
            }
        }
        return false;
    }
    return false;
}

Qt::ItemFlags LayersModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags rc = QAbstractItemModel::flags(index);
    rc |= Qt::ItemIsUserCheckable;
    return rc;
}

QVariant LayersModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
        case 0: return tr("Name");
        }
    }
    return QVariant();
}

QModelIndex LayersModel::index(MapLevel *mapLevel) const
{
    Item *item = toItem(mapLevel);
    int row = item->parent->children.indexOf(item);
    return createIndex(row, 0, item);
}

QModelIndex LayersModel::index(TileLayer *tl) const
{
    Item *item = toItem(tl);
    int row = item->parent->children.indexOf(item);
    return createIndex(row, 0, item);
}

MapLevel *LayersModel::toMapLevel(const QModelIndex &index) const
{
    if (index.isValid())
        return toItem(index)->mapLevel;
    return 0;
}

TileLayer *LayersModel::toTileLayer(const QModelIndex &index) const
{
    if (index.isValid())
        return toItem(index)->layer;
    return 0;
}

LayersModel::Item *LayersModel::toItem(const QModelIndex &index) const
{
    if (index.isValid())
        return static_cast<Item*>(index.internalPointer());
    return 0;
}

LayersModel::Item *LayersModel::toItem(MapLevel *mapLevel) const
{
    if (!mRootItem)
        return 0;
    foreach (Item *item, mRootItem->children)
        if (item->mapLevel == mapLevel)
            return item;
    return 0;
}

LayersModel::Item *LayersModel::toItem(TileLayer *tl) const
{
    MapLevel *mapLevel = mMap->mapLevelForZ(tl->level());
    Item *parent = toItem(mapLevel);
    foreach (Item *item, parent->children)
        if (item->layer == tl)
            return item;
    return 0;
}

void LayersModel::setCellDocument(CellDocument *doc)
{
    if (mCellDocument == doc)
        return;

    if (mCellDocument) {
        mCellDocument->disconnect(this);
    }

    beginResetModel();

    mCellDocument = doc;
    mMapComposite = 0;
    mMap = 0;

    if (mCellDocument) {
        mMapComposite = mCellDocument->scene()->mapComposite();
        mMap = mMapComposite->map();

        connect(mCellDocument, &CellDocument::layerVisibilityChanged,
                this, &LayersModel::layerVisibilityChanged);
        connect(mCellDocument, &CellDocument::layerGroupAdded,
                this, &LayersModel::layerGroupAdded);
        connect(mCellDocument, &CellDocument::layerGroupVisibilityChanged,
                this, &LayersModel::layerGroupVisibilityChanged);

        connect(mCellDocument, qOverload<>(&CellDocument::cellMapFileAboutToChange),
                this, &LayersModel::cellMapFileAboutToChange);
        connect(mCellDocument, qOverload<>(&CellDocument::cellMapFileChanged),
                this, &LayersModel::cellMapFileChanged);
        connect(mCellDocument, qOverload<>(&CellDocument::cellContentsAboutToChange),
                this, &LayersModel::cellMapFileAboutToChange);
        connect(mCellDocument, qOverload<>(&CellDocument::cellContentsChanged),
                this, &LayersModel::cellMapFileChanged);
    }

    setModelData();
    endResetModel();
}

void LayersModel::setModelData()
{
    delete mRootItem;
    mRootItem = 0;

    if (mCellDocument) {
        mRootItem = new Item();
        for (MapLevel *mapLevel : mMap->mapLevels()) {
            Item *parent = new Item(mRootItem, 0, mapLevel);
            foreach (TileLayer *tl, mapLevel->tileLayers())
                new Item(parent, 0, tl);
        }
    }
}

void LayersModel::layerVisibilityChanged(Layer *layer)
{
    if (TileLayer *tl = layer->asTileLayer()) {
        QModelIndex index = this->index(tl);
        emit dataChanged(index, index);
    }
}

// This is a bit of a hack.  When a lot is added to a cell, new layers
// and layergroups may be created to enable the new submap to be displayed.
// In that case, the model must be updated; really only need to add the new
// layergroups;
void LayersModel::layerGroupAdded(int level)
{
    Q_UNUSED(level);
    cellMapFileChanged();
}

void LayersModel::layerGroupVisibilityChanged(ZTileLayerGroup *layerGroup)
{
    MapLevel *mapLevel = mMap->mapLevelForZ(layerGroup->level());
    QModelIndex index = this->index(mapLevel);
    emit dataChanged(index, index);
}

void LayersModel::cellMapFileAboutToChange()
{
    beginResetModel();
    delete mRootItem;
    mRootItem = 0;
    mMap = 0;
    mMapComposite = 0;
    endResetModel();
}

void LayersModel::cellMapFileChanged()
{
    beginResetModel();
    mMapComposite = mCellDocument->scene()->mapComposite();
    mMap = mMapComposite->map();
    setModelData();
    endResetModel();
}
