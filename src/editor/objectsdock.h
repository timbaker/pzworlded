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

#ifndef OBJECTSDOCK_H
#define OBJECTSDOCK_H

#include <QDockWidget>
#include <QStyledItemDelegate>
#include <QTreeView>

class CellDocument;
class Document;
class ObjectType;
class WorldCell;
class WorldCellObject;
class WorldDocument;
class WorldObjectGroup;

namespace Tiled {
class Map;
}

class ObjectsModel;
class ObjectsView;

class ObjectsDock : public QDockWidget
{
    Q_OBJECT
public:
    explicit ObjectsDock(QWidget *parent = 0);

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
    ObjectsView *mView;
    CellDocument *mCellDoc;
    WorldDocument *mWorldDoc;
};

class ObjectsView : public QTreeView
{
    Q_OBJECT
public:
    explicit ObjectsView(QWidget *parent = 0);

    void mousePressEvent(QMouseEvent *event);

    void setDocument(Document *doc);

    ObjectsModel *model() const { return mModel; }

    void setSynchingSelection(bool synching) { mSynchingSelection = synching; }
    bool synchingSelection() const;

signals:
    void trashItem(const QModelIndex &index);

private slots:
    void rowsInserted(const QModelIndex &parent, int start, int end);
    void selectedCellsChanged();
    void selectedObjectsChanged();
    void modelSynched();
    void closeComboBoxEditor();

private:
    void saveExpandedLevels();
    void restoreExpandedLevels();

private:
    ObjectsModel *mModel;
    WorldCell *mCell;
    CellDocument *mCellDoc;
    WorldDocument *mWorldDoc;
    bool mSynching;
    bool mSynchingSelection;

    struct Level
    {
        bool expanded;
        QSet<WorldObjectGroup*> expandedGroups;
    };

    Level mExpandedLevels[16];
};

class ObjectsViewDelegate : public QStyledItemDelegate
{
    Q_OBJECT

 public:
     ObjectsViewDelegate(ObjectsView *view, QObject *parent = 0);

     void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;

     QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                           const QModelIndex &index) const;

     void setEditorData(QWidget *editor, const QModelIndex &index) const;

     void setModelData(QWidget *editor, QAbstractItemModel *model,
                       const QModelIndex &index) const;

     void updateEditorGeometry(QWidget *editor,
                               const QStyleOptionViewItem &option,
                               const QModelIndex &index) const;

     bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index);

     QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;

     QRect closeButtonRect(const QRect &itemViewRect) const;

private slots:

private:
     ObjectsView *mView;
     ObjectsModel *mModel;
     QPixmap mTrashPixmap;
 };

class ObjectsModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    ObjectsModel(QObject *parent = 0);
    ~ObjectsModel();

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
    };

    QModelIndex index(Level *level) const;
    QModelIndex index(WorldCellObject *obj, int column = 0) const;

    Level *toLevel(const QModelIndex &index) const;
    WorldObjectGroup *toGroup(const QModelIndex &index) const;
    WorldCellObject *toObject(const QModelIndex &index) const;

    void setDocument(Document *doc);
    void setCell(WorldCell *cell);
    WorldCell *cell() const { return mCell; }

    bool synching() const { return mSynching; }

    int nameColumn() const { return 0; }
    int typeColumn() const { return 1; }

    bool isEditable(const QModelIndex &index) const;
    bool isDeletable(const QModelIndex &index) const;
signals:
    void synched();

private slots:
    void objectTypeNameChanged(ObjectType *objType);

    void objectGroupAdded(int index);
    void objectGroupAboutToBeRemoved(int index);
    void objectGroupNameChanged(WorldObjectGroup *og);
    void objectGroupReordered(int index);

    void cellContentsAboutToChange(WorldCell *cell);
    void cellContentsChanged(WorldCell *cell);
    void cellObjectAdded(WorldCell *cell, int index);
    void cellObjectAboutToBeRemoved(WorldCell *cell, int index);
    void cellObjectXXXXChanged(WorldCellObject *obj);
    void cellObjectGroupAboutToChange(WorldCellObject *obj);
    void cellObjectGroupChanged(WorldCellObject *obj);
    void objectLevelAboutToChange(WorldCellObject *obj);
    void objectLevelChanged(WorldCellObject *obj);
    void cellObjectReordered(WorldCellObject *obj);

    void layerGroupAdded(int level);

private:
    void setModelData();

    class Item
    {
    public:
        Item()
            : parent(0)
            , level(0)
            , group(0)
            , object(0)
        {

        }

        ~Item()
        {
            qDeleteAll(children);
        }

        Item(Item *parent, int indexInParent, Level *level)
            : parent(parent)
            , level(level)
            , group(0)
            , object(0)
        {
            Q_ASSERT(!parent->children.contains(this));
            parent->children.insert(indexInParent, this);
        }

        Item(Item *parent, int indexInParent, WorldObjectGroup *og)
            : parent(parent)
            , level(0)
            , group(og)
            , object(0)
        {
            Q_ASSERT(!parent->children.contains(this));
            parent->children.insert(indexInParent, this);
        }

        Item(Item *parent, int indexInParent, WorldCellObject *obj)
            : parent(parent)
            , level(0)
            , group(0)
            , object(obj)
        {
            Q_ASSERT(!parent->children.contains(this));
            parent->children.insert(indexInParent, this);
        }

        Item *findChild(WorldCellObject *obj)
        {
            foreach (Item *item, children)
                if (item->object == obj)
                    return item;
            return 0;
        }

        int rowOf(WorldObjectGroup *og)
        {
            int row = 0;
            foreach (Item *item, children) {
                if (item->group == og)
                    return row;
                ++row;
            }
            return -1;
        }

        int numDescendants() const
        {
            int count = 0;
            foreach (Item *child, children)
                count += 1 + child->numDescendants();
            return count;
        }
        int flattenedRowInParent() const
        {
            if (!parent)
                return -1; // root item isn't displayed
            int row = 0;
            foreach (Item *sibling, parent->children) {
                if (sibling == this)
                    break;
                row += 1 + sibling->numDescendants();
            }

            return row;
        }
        int flattenedRowInList() const
        {
            int row = flattenedRowInParent();
            if (parent)
                row += 1 + parent->flattenedRowInList();
            return row;
        }

        Item *parent;
        QList<Item*> children;
        Level *level;
        WorldObjectGroup *group;
        WorldCellObject *object;
    };

    Item *toItem(const QModelIndex &index) const;
    Item *toItem(Level *level) const;
    Item *toItem(int level) const;
    Item *toItem(int level, WorldObjectGroup *og) const;
    Item *toItem(WorldCellObject *obj) const;

    QModelIndex index(Item *item) const;

    void addItemToModel(Item *parentItem, int indexInParent, Item *item);
    void removeItemFromModel(Item *item);

    WorldCell *mCell;
    CellDocument *mCellDoc;
    WorldDocument *mWorldDoc;
    Item *mRootItem;
    QList<Level*> mLevels;
    bool mSynching;
    QPixmap mObjectPixmap;
    static QString ObjectsModelMimeType;
};

#endif // OBJECTSDOCK_H
