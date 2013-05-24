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

#include "pathlayersmodel.h"

#include "mapcomposite.h"
#include "path.h"
#include "pathdocument.h"
#include "pathworld.h"
#include "worldchanger.h"

PathLayersModel::PathLayersModel(QObject *parent)
    : QAbstractItemModel(parent)
    , mDocument(0)
    , mRootItem(0)
{
}

PathLayersModel::~PathLayersModel()
{
    delete mRootItem;
}

QModelIndex PathLayersModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!mRootItem)
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

QModelIndex PathLayersModel::parent(const QModelIndex &index) const
{
    if (Item *item = toItem(index)) {
        if (item->parent != mRootItem) {
            Item *grandParent = item->parent->parent;
            return createIndex(grandParent->children.indexOf(item->parent), 0, item->parent);
        }
    }
    return QModelIndex();
}

int PathLayersModel::rowCount(const QModelIndex &parent) const
{
    if (!mRootItem)
        return 0;
    if (!parent.isValid())
        return mRootItem->children.size();
    if (Item *item = toItem(parent))
        return item->children.size();
    return 0;
}

int PathLayersModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

QVariant PathLayersModel::data(const QModelIndex &index, int role) const
{
    if (WorldPathLayer *layer = toPathLayer(index)) {
        switch (role) {
        case Qt::DisplayRole:
            return MapComposite::layerNameWithoutPrefix(layer->name());
        case Qt::EditRole:
            return layer->name();
        case Qt::DecorationRole:
            return QVariant();
        case Qt::CheckStateRole: {
            return layer->isVisible() ? Qt::Checked : Qt::Unchecked;
        }
        default:
            return QVariant();
        }
    }
    if (WorldLevel *wlevel = toLevel(index)) {
        switch (role) {
        case Qt::DisplayRole:
            return tr("Level %1").arg(wlevel->level());
        case Qt::CheckStateRole: {
            return wlevel->isPathLayersVisible() ? Qt::Checked : Qt::Unchecked;
        }
        default:
            return QVariant();
        }
    }
    return QVariant();
}

bool PathLayersModel::setData(const QModelIndex &index, const QVariant &value,
                         int role)
{
    if (WorldPathLayer *layer = toPathLayer(index)) {
        switch (role) {
        case Qt::CheckStateRole:  {
            Qt::CheckState c = static_cast<Qt::CheckState>(value.toInt());
            document()->changer()->doSetPathLayerVisible(layer, c == Qt::Checked);
            emit dataChanged(index, index);
            return true;
        }
        case Qt::EditRole:  {
            return false;
        }
        }
        return false;
    }
    if (WorldLevel *wlevel = toLevel(index)) {
        switch (role) {
        case Qt::CheckStateRole: {
            Qt::CheckState c = static_cast<Qt::CheckState>(value.toInt());
            document()->changer()->doSetPathLayersVisible(wlevel, c == Qt::Checked);
            emit dataChanged(index, index);
            return true;
        }
        case Qt::EditRole: {
            return false;
        }
        }
        return false;
    }
    return false;
}

Qt::ItemFlags PathLayersModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags rc = QAbstractItemModel::flags(index);
    rc |= Qt::ItemIsUserCheckable;
    return rc;
}

QVariant PathLayersModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
        case 0: return tr("Name");
        }
    }
    return QVariant();
}

QModelIndex PathLayersModel::index(WorldPathLayer *layer) const
{
    Item *item = toItem(layer);
    int row = item->parent->children.indexOf(item);
    return createIndex(row, 0, item);
}

WorldPathLayer *PathLayersModel::toPathLayer(const QModelIndex &index) const
{
    if (index.isValid())
        return toItem(index)->layer;
    return 0;
}

WorldLevel *PathLayersModel::toLevel(const QModelIndex &index) const
{
    if (index.isValid())
        return toItem(index)->wlevel;
    return false;
}

PathLayersModel::Item *PathLayersModel::toItem(const QModelIndex &index) const
{
    if (index.isValid())
        return static_cast<Item*>(index.internalPointer());
    return 0;
}

PathLayersModel::Item *PathLayersModel::toItem(WorldPathLayer *layer) const
{
    if (!mRootItem)
        return 0;
    foreach (Item *item, mRootItem->children)
        foreach (Item *item2, item->children)
            if (item2->layer == layer)
                return item2;
    return 0;
}

PathLayersModel::Item *PathLayersModel::toItem(WorldLevel *wlevel) const
{
    if (!mRootItem)
        return 0;
    foreach (Item *item, mRootItem->children)
        if (item->wlevel == wlevel)
            return item;
    return 0;
}

QModelIndex PathLayersModel::index(PathLayersModel::Item *item) const
{
    int row = item->parent->children.indexOf(item);
    return createIndex(row, 0, item);
}

void PathLayersModel::setDocument(PathDocument *doc)
{
    if (mDocument == doc)
        return;

    if (mDocument) {
        mDocument->disconnect(this);
        mDocument->changer()->disconnect(this);
    }

    mDocument = doc;

    if (mDocument) {
    }

    setModelData();

    reset();
}

void PathLayersModel::afterAddPathLayer(WorldLevel *wlevel, int index, WorldPathLayer *layer)
{
    if (Item *item = toItem(wlevel)) {
        int row = wlevel->pathLayerCount() - index - 1;
        beginInsertRows(this->index(item), row, row);
        new Item(mRootItem, row, layer);
        endInsertRows();
    }
}

void PathLayersModel::beforeRemovePathLayer(WorldLevel *wlevel, int index, WorldPathLayer *layer)
{
    if (Item *item = toItem(wlevel)) {
        int row = wlevel->pathLayerCount() - index - 1;
        beginRemoveRows(this->index(item), row, row);
        delete item->children.takeAt(row);
        endRemoveRows();
    }
}

void PathLayersModel::setModelData()
{
    delete mRootItem;
    mRootItem = 0;

    if (mDocument) {
        mRootItem = new Item();
        foreach (WorldLevel *wlevel, document()->world()->levels()) {
            Item *levelItem = new Item(mRootItem, 0, wlevel);
            foreach (WorldPathLayer *layer, wlevel->pathLayers())
                /*Item *parent =*/ new Item(levelItem, 0, layer);
        }
    }
}

void PathLayersModel::afterSetPathLayerVisible(WorldPathLayer *layer, bool visible)
{
    Q_UNUSED(visible)
    QModelIndex index = this->index(layer);
    emit dataChanged(index, index);
}
