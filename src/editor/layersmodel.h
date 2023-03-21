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

#ifndef LAYERSMODEL_H
#define LAYERSMODEL_H

#include <QAbstractItemModel>
#include <QIcon>

class CellDocument;
class CompositeLayerGroup;
class MapComposite;
class WorldCell;

namespace Tiled {
class Map;
class MapLevel;
class Layer;
class TileLayer;
class ZTileLayerGroup;
}

class LayersModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum UserRoles {
        OpacityRole = Qt::UserRole
    };

    LayersModel(QObject *parent = 0);
    ~LayersModel();

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &index) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

    bool setData(const QModelIndex &index, const QVariant &value, int role);
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    Qt::ItemFlags flags(const QModelIndex &index) const;

    QModelIndex index(Tiled::MapLevel *g) const;
    QModelIndex index(Tiled::TileLayer *tl) const;

    Tiled::MapLevel *toMapLevel(const QModelIndex &index) const;
    Tiled::TileLayer *toTileLayer(const QModelIndex &index) const;

    void setCellDocument(CellDocument *doc);
    CellDocument *document() const { return mCellDocument; }

private slots:
    void layerVisibilityChanged(Tiled::Layer *layer);
    void layerGroupAdded(int level);
    void layerGroupVisibilityChanged(Tiled::ZTileLayerGroup *layerGroup);
    void cellMapFileAboutToChange();
    void cellMapFileChanged();

private:
    void setModelData();

    class Item
    {
    public:
        Item()
            : parent(nullptr)
            , mapLevel(nullptr)
            , layer(nullptr)
        {

        }

        ~Item()
        {
            qDeleteAll(children);
        }

        Item(Item *parent, int indexInParent, Tiled::MapLevel *mapLevel)
            : parent(parent)
            , mapLevel(mapLevel)
            , layer(nullptr)
        {
            parent->children.insert(indexInParent, this);
        }

        Item(Item *parent, int indexInParent, Tiled::TileLayer *tl)
            : parent(parent)
            , mapLevel(nullptr)
            , layer(tl)
        {
            parent->children.insert(indexInParent, this);
        }

        Item *parent;
        QList<Item*> children;
        Tiled::MapLevel *mapLevel;
        Tiled::TileLayer *layer;
    };

    Item *toItem(const QModelIndex &index) const;
    Item *toItem(Tiled::MapLevel *mapLevel) const;
    Item *toItem(Tiled::TileLayer *tl) const;

    CellDocument *mCellDocument;
    MapComposite *mMapComposite;
    Tiled::Map *mMap;
    Item *mRootItem;
};

#endif // LAYERSMODEL_H
