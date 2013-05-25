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

#ifndef PATHLAYERSMODEL_H
#define PATHLAYERSMODEL_H

#include <QAbstractItemModel>
#include <QIcon>

class PathDocument;
class PathWorld;
class WorldLevel;
class WorldPathLayer;

class PathLayersModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    PathLayersModel(QObject *parent = 0);
    ~PathLayersModel();

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &index) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

    bool setData(const QModelIndex &index, const QVariant &value, int role);
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    Qt::ItemFlags flags(const QModelIndex &index) const;

    QModelIndex index(WorldPathLayer *layer) const;

    WorldPathLayer *toPathLayer(const QModelIndex &index) const;
    WorldLevel *toLevel(const QModelIndex &index) const;

    void setDocument(PathDocument *doc);
    PathDocument *document() const { return mDocument; }

private slots:
    void afterAddPathLayer(WorldLevel *wlevel, int index, WorldPathLayer *layer);
    void beforeRemovePathLayer(WorldLevel *wlevel, int index, WorldPathLayer *layer);
    void afterReorderPathLayer(WorldLevel *wlevel, WorldPathLayer *layer, int oldIndex);
    void afterSetPathLayerVisible(WorldPathLayer *layer, bool visible);

private:
    void setModelData();

    class Item
    {
    public:
        Item()
            : parent(0)
            , layer(0)
            , wlevel(0)
        {
        }

        Item(Item *parent, int indexInParent, WorldLevel *wlevel)
            : parent(parent)
            , wlevel(wlevel)
            , layer(0)
        {
            parent->children.insert(indexInParent, this);
        }

        Item(Item *parent, int indexInParent, WorldPathLayer *layer)
            : parent(parent)
            , wlevel(0)
            , layer(layer)
        {
            parent->children.insert(indexInParent, this);
        }

        ~Item()
        {
            qDeleteAll(children);
        }

        Item *parent;
        QList<Item*> children;
        WorldPathLayer *layer;
        WorldLevel *wlevel;
    };

    Item *toItem(const QModelIndex &index) const;
    Item *toItem(WorldPathLayer *layer) const;
    Item *toItem(WorldLevel *wlevel) const;

    QModelIndex index(Item *item) const;

    PathDocument *mDocument;
    Item *mRootItem;
};

#endif // PATHLAYERSMODEL_H
