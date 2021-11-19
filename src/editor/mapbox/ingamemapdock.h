/*
 * Copyright 2018, Tim Baker <treectrl@users.sf.net>
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

#ifndef INGAMEMAPDOCK_H
#define INGAMEMAPDOCK_H

#include <QDockWidget>
#include <QTreeView>

class CellDocument;
class Document;
class WorldCell;
class WorldDocument;

class InGameMapFeature;
class InGameMapPropertiesForm;

namespace Tiled {
class Map;
}

class InGameMapModel;
class InGameMapView;

class InGameMapDock : public QDockWidget
{
    Q_OBJECT
public:
    explicit InGameMapDock(QWidget *parent = nullptr);

    void changeEvent(QEvent *e);
    void retranslateUi();

    void setDocument(Document *doc);
    void clearDocument();

private slots:
    void selectionChanged();
    void itemClicked(const QModelIndex &index);
    void itemDoubleClicked(const QModelIndex &index);
    void trashItem(const QModelIndex &index);
    void selectedFeaturesChanged();

private:
    InGameMapView *mView;
    CellDocument *mCellDoc;
    WorldDocument *mWorldDoc;
    friend class InGameMapView;
    InGameMapPropertiesForm* mPropertiesForm;
};

class InGameMapView : public QTreeView
{
    Q_OBJECT
public:
    explicit InGameMapView(QWidget *parent = nullptr);

    void mousePressEvent(QMouseEvent *event);

    void setDocument(Document *doc);

    InGameMapModel *model() const { return mModel; }

    void setSynchingSelection(bool synching) { mSynchingSelection = synching; }
    bool synchingSelection() const;

signals:
    void trashItem(const QModelIndex &index);

private slots:
    void selectedCellsChanged();
    void selectedFeaturesChanged();
    void modelSynched();

private:
    InGameMapModel *mModel;
    WorldCell *mCell;
    CellDocument *mCellDoc;
    WorldDocument *mWorldDoc;
    bool mSynching;
    bool mSynchingSelection;
};

class InGameMapModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    InGameMapModel(QObject *parent = nullptr);
    ~InGameMapModel();

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

    QModelIndex index(InGameMapFeature* feature) const;

    InGameMapFeature *toFeature(const QModelIndex &index) const;

    void setDocument(Document *doc);
    void setCell(WorldCell *cell);

    bool synching() const { return mSynching; }
signals:
    void synched();

private slots:
    void featureAdded(WorldCell *cell, int index);
    void featureAboutToBeRemoved(WorldCell *cell, int index);
    void propertiesChanged(WorldCell* cell, int featureIndex);
    void cellContentsAboutToChange(WorldCell *cell);
    void cellContentsChanged(WorldCell *cell);

private:
    void setModelData();

    class Item
    {
    public:
        Item()
            : parent(nullptr)
            , feature(nullptr)
        {

        }

        ~Item()
        {
            qDeleteAll(children);
        }

        Item(Item *parent, int indexInParent)
            : parent(parent)
            , feature(nullptr)
        {
            parent->children.insert(indexInParent, this);
        }

        Item(Item *parent, int indexInParent, InGameMapFeature* feature)
            : parent(parent)
            , feature(feature)
        {
            parent->children.insert(indexInParent, this);
        }

        Item *parent;
        QList<Item*> children;
        InGameMapFeature *feature;
    };

    Item *toItem(const QModelIndex &index) const;
    Item *toItem(InGameMapFeature *feature) const;

    WorldCell *mCell;
    CellDocument *mCellDoc;
    WorldDocument *mWorldDoc;
    Item *mRootItem;
    bool mSynching;

    static QString InGameMapModelMimeType;
};

#endif // INGAMEMAPDOCK_H
