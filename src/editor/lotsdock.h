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

#ifndef LOTSDOCK_H
#define LOTSDOCK_H

#include <QDockWidget>
#include <QTreeView>

class CellDocument;
class Document;
class WorldCell;
class WorldCellLot;
class WorldDocument;

namespace Tiled {
class Map;
}

class LotsModel;
class LotsView;

class LotsDock : public QDockWidget
{
    Q_OBJECT
public:
    explicit LotsDock(QWidget *parent = 0);

    void changeEvent(QEvent *e);
    void retranslateUi();

    void setDocument(Document *doc);
    void clearDocument();

private slots:
    void selectionChanged();
    void itemClicked(const QModelIndex &index);
    void itemDoubleClicked(const QModelIndex &index);
    void trashItem(const QModelIndex &index);

private:
    LotsView *mView;
    CellDocument *mCellDoc;
    WorldDocument *mWorldDoc;
};

class LotsView : public QTreeView
{
    Q_OBJECT
public:
    explicit LotsView(QWidget *parent = 0);

    void mousePressEvent(QMouseEvent *event);

    void setDocument(Document *doc);

    LotsModel *model() const { return mModel; }

    void setSynchingSelection(bool synching) { mSynchingSelection = synching; }
    bool synchingSelection() const;

signals:
    void trashItem(const QModelIndex &index);

private slots:
    void selectedCellsChanged();
    void selectedLotsChanged();
    void modelSynched();

private:
    LotsModel *mModel;
    WorldCell *mCell;
    CellDocument *mCellDoc;
    WorldDocument *mWorldDoc;
    bool mSynching;
    bool mSynchingSelection;
};

class LotsModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    LotsModel(QObject *parent = 0);
    ~LotsModel();

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &index) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

    bool setData(const QModelIndex &index, const QVariant &value, int role);
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    Qt::ItemFlags flags(const QModelIndex &index) const;

    Qt::DropActions supportedDropActions() const;
    QStringList mimeTypes() const;
    QMimeData *mimeData(const QModelIndexList &indexes) const;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent);

    class Level
    {
    public:
        Level(int level)
            : level(level)
        {
        }

        int level;
        QList<WorldCellLot*> lots;
    };

    QModelIndex index(Level *level) const;
    QModelIndex index(WorldCellLot *lot) const;

    Level *toLevel(const QModelIndex &index) const;
    WorldCellLot *toLot(const QModelIndex &index) const;

    void setDocument(Document *doc);
    void setCell(WorldCell *cell);

    bool synching() const { return mSynching; }
signals:
    void synched();

private slots:
    void cellLotAdded(WorldCell *cell, int index);
    void cellLotAboutToBeRemoved(WorldCell *cell, int index);
    void cellContentsAboutToChange(WorldCell *cell);
    void cellContentsChanged(WorldCell *cell);
    void lotLevelChanged(WorldCellLot *lot);
    void layerGroupAdded(int level);

private:
    void setModelData();

    class Item
    {
    public:
        Item()
            : parent(0)
            , level(0)
            , lot(0)
        {

        }

        Item(Item *parent, int indexInParent, Level *level)
            : parent(parent)
            , level(level)
            , lot(0)
        {
            parent->children.insert(indexInParent, this);
        }

        Item(Item *parent, int indexInParent, WorldCellLot *lot)
            : parent(parent)
            , level(0)
            , lot(lot)
        {
            parent->children.insert(indexInParent, this);
        }

        Item *parent;
        QList<Item*> children;
        Level *level;
        WorldCellLot *lot;
    };

    Item *toItem(const QModelIndex &index) const;
    Item *toItem(Level *level) const;
    Item *toItem(int level) const;
    Item *toItem(WorldCellLot *lot) const;

    WorldCell *mCell;
    CellDocument *mCellDoc;
    WorldDocument *mWorldDoc;
    Item *mRootItem;
    QList<Level*> mLevels;
    bool mSynching;
    static QString LotsModelMimeType;
};

#endif // LOTSDOCK_H
