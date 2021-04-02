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

#include "lotsdock.h"

#include "celldocument.h"
#include "cellscene.h"
#include "cellview.h"
#include "scenetools.h"
#include "mapcomposite.h"
#include "mapmanager.h"
#include "worldcell.h"
#include "worlddocument.h"

#include "maprenderer.h"
#include "tilelayer.h"

#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QHeaderView>
#include <QMimeData>
#include <QMouseEvent>
#include <QUndoStack>
#include <QVBoxLayout>

using namespace Tiled;

LotsDock::LotsDock(QWidget *parent)
    : QDockWidget(parent)
    , mView(new LotsView(this))
    , mCellDoc(0)
    , mWorldDoc(0)
{
    setObjectName(QLatin1String("LotsDock"));

    QWidget *widget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(2, 2, 2, 2);

    layout->addWidget(mView);

    setWidget(widget);
    retranslateUi();

    connect(mView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(selectionChanged()));
    connect(mView, SIGNAL(clicked(QModelIndex)),
            SLOT(itemClicked(QModelIndex)));
    connect(mView, SIGNAL(doubleClicked(QModelIndex)),
            SLOT(itemDoubleClicked(QModelIndex)));
    connect(mView, SIGNAL(trashItem(QModelIndex)),
            SLOT(trashItem(QModelIndex)));

    // Workaround since a tabbed dockwidget that is not currently visible still
    // returns true for isVisible()
    connect(this, SIGNAL(visibilityChanged(bool)),
            mView, SLOT(setVisible(bool)));
}

void LotsDock::changeEvent(QEvent *e)
{
    QDockWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        retranslateUi();
        break;
    default:
        break;
    }
}

void LotsDock::retranslateUi()
{
    setWindowTitle(tr("Lots"));
}

void LotsDock::setDocument(Document *doc)
{
    mWorldDoc = doc->asWorldDocument();
    mCellDoc = doc->asCellDocument();
    mView->setDocument(doc);
}

void LotsDock::clearDocument()
{
    mWorldDoc = 0;
    mCellDoc = 0;
    mView->setDocument(0);
}

void LotsDock::selectionChanged()
{
    if (mView->synchingSelection())
        return;

    if (mWorldDoc) {
        QModelIndexList selection = mView->selectionModel()->selectedIndexes();
        QList<WorldCellLot*> selectedLots;
        foreach (QModelIndex index, selection) {
            if (index.column() > 0) continue;
            if (WorldCellLot *lot = mView->model()->toLot(index))
                selectedLots += lot;
        }
        mWorldDoc->setSelectedLots(selectedLots);
        return;
    }

    if (!mCellDoc)
        return;

    QModelIndexList selection = mView->selectionModel()->selectedIndexes();
    if (selection.size() == mView->model()->columnCount()) {
        int level = -1;
        if (LotsModel::Level *levelPtr = mView->model()->toLevel(selection.first()))
            level = levelPtr->level;
        if (WorldCellLot *lot = mView->model()->toLot(selection.first())) {
            MapRenderer *renderer = mCellDoc->scene()->renderer();
            mCellDoc->view()->ensureRectVisible(renderer->boundingRect(lot->bounds(),
                                                                       lot->level()));
            level = lot->level();
        }
        if (level >= 0)
            mCellDoc->setCurrentLevel(level);
    }

    // Select lots in the scene only if they can be edited
    if (!SubMapTool::instance()->isCurrent())
        return;

    QList<WorldCellLot*> lots;
    foreach (QModelIndex index, selection) {
        if (index.column() > 0) continue;
        if (WorldCellLot *lot = mView->model()->toLot(index))
            lots << lot;
    }

    mView->setSynchingSelection(true);
    mCellDoc->setSelectedLots(lots);
    mView->setSynchingSelection(false);
}

void LotsDock::itemClicked(const QModelIndex &index)
{
    if (mView->model()->toLot(index)) {
        // Clicking an already-selected item should center it in the view
        if (mView->selectionModel()->isSelected(index))
            selectionChanged();
    }
}

void LotsDock::itemDoubleClicked(const QModelIndex &index)
{
    Q_UNUSED(index)
}

void LotsDock::trashItem(const QModelIndex &index)
{
    if (WorldCellLot *lot = mView->model()->toLot(index)) {
        if (mWorldDoc)
            mWorldDoc->removeCellLot(lot->cell(), lot->cell()->lots().indexOf(lot));
        if (mCellDoc)
            mCellDoc->worldDocument()->removeCellLot(lot->cell(), lot->cell()->lots().indexOf(lot));
    }
}

/////

#include <QStyledItemDelegate>
#include <QLineEdit>
#include <QSpinBox>

class LotsViewDelegate : public QStyledItemDelegate
 {
//     Q_OBJECT

 public:
     LotsViewDelegate(QObject *parent = 0)
         : QStyledItemDelegate(parent)
         , mTrashPixmap(QLatin1String(":/images/16x16/edit-delete.png"))
     {

     }

     void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
     {
         QStyledItemDelegate::paint(painter, option, index);

         if ((index.column() == 0) && index.parent().isValid() && (option.state & QStyle::State_MouseOver)) {
             painter->save();
             QRect closeRect(option.rect.x()-40+2, option.rect.y(), mTrashPixmap.width(), option.rect.height());
             painter->setClipRect(closeRect);
             painter->drawPixmap(closeRect.x(), closeRect.y(), mTrashPixmap);
             painter->restore();
         }
     }

     QRect closeButtonRect(const QRect &itemViewRect)
     {
        QRect closeRect(itemViewRect.x()-40+2, itemViewRect.y(), mTrashPixmap.width(), itemViewRect.height());
        return closeRect;
     }

private:
     QPixmap mTrashPixmap;
 };

/////

LotsView::LotsView(QWidget *parent)
    : QTreeView(parent)
    , mModel(new LotsModel(this))
    , mCell(0)
    , mCellDoc(0)
    , mWorldDoc(0)
    , mSynching(false)
    , mSynchingSelection(false)
{
    setDragEnabled(true);
    setAcceptDrops(true);
    setDragDropMode(QAbstractItemView::InternalMove);
    setUniformRowHeights(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);

    setMouseTracking(true); // Needed on Mac OS X for garbage can

    setItemDelegate(new LotsViewDelegate(this));

    setModel(mModel);

    connect(mModel, SIGNAL(synched()), SLOT(modelSynched()));
}

void LotsView::mousePressEvent(QMouseEvent *event)
{
    // Prevent drag-and-drop happening on unselected items
    QModelIndex index = indexAt(event->pos());
    if (index.isValid()) {

        // Prevent drag-and-drop starting when clicking on an unselected item.
        setDragEnabled(selectionModel()->isSelected(index));

        if ((index.column() == 0) && mModel->toLot(index)) {
            QRect itemRect = visualRect(index);
            QRect rect = ((LotsViewDelegate*)itemDelegate())->closeButtonRect(itemRect);
            if (rect.contains(event->pos())) {
                event->accept();
                emit trashItem(index);
                return;
            }
        }
    }
    QTreeView::mousePressEvent(event);
}

void LotsView::setDocument(Document *doc)
{
    if (mWorldDoc)
        mWorldDoc->disconnect(this);
    if (mCellDoc)
        mCellDoc->disconnect(this);

    mWorldDoc = doc ? doc->asWorldDocument() : 0;
    mCellDoc = doc ? doc->asCellDocument() : 0;

    mModel->setDocument(doc);

    if (mWorldDoc) {
        connect(mWorldDoc, SIGNAL(selectedCellsChanged()),
                SLOT(selectedCellsChanged()));
    }
    if (mCellDoc) {
        connect(mCellDoc, SIGNAL(selectedLotsChanged()),
                SLOT(selectedLotsChanged()));
    }
}

bool LotsView::synchingSelection() const
{
    return mSynchingSelection || mModel->synching();
}

void LotsView::selectedCellsChanged()
{
    if (mWorldDoc->selectedCellCount() == 1)
        mModel->setCell(mWorldDoc->selectedCells().first());
    else
        mModel->setCell(0);
}

void LotsView::selectedLotsChanged()
{
    if (synchingSelection())
        return;
    mSynchingSelection = true;
    clearSelection();
    foreach (WorldCellLot *lot, mCellDoc->selectedLots()) {
        selectionModel()->select(model()->index(lot),
                                 QItemSelectionModel::Select | QItemSelectionModel::Rows);
    }
    mSynchingSelection = false;
}

void LotsView::modelSynched()
{
    if (mCellDoc)
        selectedLotsChanged();
    expandAll();
}

/////

// FIXME: Are these global objects safe?
QString LotsModel::LotsModelMimeType(QLatin1String("application/x-pzworlded-lot"));

LotsModel::LotsModel(QObject *parent)
    : QAbstractItemModel(parent)
    , mCell(0)
    , mCellDoc(0)
    , mWorldDoc(0)
    , mRootItem(0)
    , mSynching(false)
{
}

LotsModel::~LotsModel()
{
    qDeleteAll(mLevels);
    delete mRootItem;
}

QModelIndex LotsModel::index(int row, int column, const QModelIndex &parent) const
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

LotsModel::Level *LotsModel::toLevel(const QModelIndex &index) const
{
    if (index.isValid())
        return toItem(index)->level;
    return 0;
}

WorldCellLot *LotsModel::toLot(const QModelIndex &index) const
{
    if (index.isValid())
        return toItem(index)->lot;
    return 0;
}

QModelIndex LotsModel::parent(const QModelIndex &index) const
{
    if (Item *item = toItem(index)) {
        if (item->parent != mRootItem) {
            Item *grandParent = item->parent->parent;
            return createIndex(grandParent->children.indexOf(item->parent), 0, item->parent);
        }
    }
    return QModelIndex();
}

int LotsModel::rowCount(const QModelIndex &parent) const
{
    if (!mRootItem)
        return 0;
    if (!parent.isValid())
        return mRootItem->children.size();
    if (Item *item = toItem(parent))
        return item->children.size();
    return 0;
}

int LotsModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

QVariant LotsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
        case 0: return tr("Column 0");
        case 1: return tr("Column 1");
        }
    }
    return QVariant();
}

QVariant LotsModel::data(const QModelIndex &index, int role) const
{
    if (Level *level = toLevel(index)) {
        if (index.column())
            return QVariant();
        switch (role) {
        case Qt::CheckStateRole: {
            if (mCellDoc) {
                bool visible = mCellDoc->isLotLevelVisible(level->level);
                return visible ? Qt::Checked : Qt::Unchecked;
            }
            return QVariant();
        }
        case Qt::DisplayRole:
            return tr("Level %1").arg(level->level);
        case Qt::EditRole:
            return QVariant(); // no edit
        case Qt::DecorationRole:
            return QVariant();
        default:
            return QVariant();
        }
    }
    if (WorldCellLot *lot = toLot(index)) {
        if (index.column())
            return QVariant();
        switch (role) {
        case Qt::CheckStateRole: {
            if (mCellDoc) {
                // Don't check SubMapItem::isVisible because it changes during CellScene::updateCurrentLevelHighlight()
                SubMapItem *sceneItem = mCellDoc->scene()->itemForLot(lot);
                bool visible = sceneItem ? sceneItem->subMap()->isVisible() : false;
                return visible ? Qt::Checked : Qt::Unchecked;
            }
            break;
        }
        case Qt::DisplayRole:
            return QFileInfo(lot->mapName()).fileName();
        case Qt::EditRole:
            return QVariant();
        case Qt::DecorationRole:
            return QVariant();
        default:
            return QVariant();
        }
    }
    return QVariant();
}

bool LotsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (Level *level = toLevel(index)) {
        switch (role) {
        case Qt::CheckStateRole: {
            if (mCellDoc) {
                Qt::CheckState c = static_cast<Qt::CheckState>(value.toInt());
                mCellDoc->setLotLevelVisible(level->level, c == Qt::Checked);
                emit dataChanged(index, index); // FIXME: connect to lotLevelVisibilityChanged signal
                return true;
            }
            break;
        }
        }
    }
    if (WorldCellLot *lot = toLot(index)) {
        switch (role) {
        case Qt::CheckStateRole: {
            if (mCellDoc) {
                Qt::CheckState c = static_cast<Qt::CheckState>(value.toInt());
                mCellDoc->scene()->setSubMapVisible(lot, c == Qt::Checked);
                emit dataChanged(index, index);
                return true;
            }
            break;
        }
        }
    }
    return QAbstractItemModel::setData(index, value, role);
}

Qt::ItemFlags LotsModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags rc = QAbstractItemModel::flags(index);
    if (index.column() == 0) {
        if (mCellDoc)
            rc |= Qt::ItemIsUserCheckable;
        if (toLevel(index))
            rc |= Qt::ItemIsDropEnabled;
        if (toLot(index))
            rc |= Qt::ItemIsDragEnabled;
    }
    return rc;
}

Qt::DropActions LotsModel::supportedDropActions() const
{
    return /*Qt::CopyAction |*/ Qt::MoveAction;
}

QStringList LotsModel::mimeTypes() const
{
    QStringList types;
    types << LotsModelMimeType;
    return types;
}

QMimeData *LotsModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QByteArray encodedData;

    QDataStream stream(&encodedData, QIODevice::WriteOnly);

    foreach (const QModelIndex &index, indexes) {
        if (WorldCellLot *lot = toLot(index)) {
            QString text = QString::number(lot->cell()->lots().indexOf(lot));
            stream << text;
        }
    }

    mimeData->setData(LotsModelMimeType, encodedData);
    return mimeData;
}

bool LotsModel::dropMimeData(const QMimeData *data,
     Qt::DropAction action, int row, int column, const QModelIndex &parent)
 {
    Q_UNUSED(row)

    if (action == Qt::IgnoreAction)
         return true;

     if (!data->hasFormat(LotsModelMimeType))
         return false;

     if (column > 0)
         return false;

     Item *parentItem = toItem(parent);
     if (!parentItem || !parentItem->level)
         return false;

     WorldDocument *worldDoc = mWorldDoc ? mWorldDoc : mCellDoc->worldDocument();

     QByteArray encodedData = data->data(LotsModelMimeType);
     QDataStream stream(&encodedData, QIODevice::ReadOnly);
     QList<WorldCellLot*> lots;
     int rows = 0;

     while (!stream.atEnd()) {
         QString text;
         stream >> text;
         lots << mCell->lots().at(text.toInt());
         ++rows;
     }

     // Note: parentItem may be destroyed by setCell()
     int level = parentItem->level->level;

     WorldCellLot *insertBefore = nullptr;
     if ((row == -1) || parentItem->children.isEmpty()) {
        // Drop on parent
     } else if (row >= parentItem->children.size()) {
         // Drop after last row
         insertBefore = parentItem->children.last()->lot;
     } else {
         WorldCellLot *prev = parentItem->children.at(row)->lot;
         int index = mCell->lots().indexOf(prev) + 1;
         if (index < mCell->lots().size())
             insertBefore = mCell->lots().at(index);
     }

     worldDoc->undoStack()->beginMacro(tr("Reorder %1 Lots").arg(lots.size()));
     for (WorldCellLot *lot : lots) {
         worldDoc->reorderCellLot(lot, insertBefore);
         insertBefore = lot;
         worldDoc->setLotLevel(lot, level);
     }
     worldDoc->undoStack()->endMacro();

     return true;
}

QModelIndex LotsModel::index(LotsModel::Level *level) const
{
    Item *item = toItem(level);
    int row = item->parent->children.indexOf(item);
    return createIndex(row, 0, item);
}

QModelIndex LotsModel::index(WorldCellLot *lot) const
{
    Item *item = toItem(lot);
    int row = item->parent->children.indexOf(item);
    return createIndex(row, 0, item);
}

LotsModel::Item *LotsModel::toItem(const QModelIndex &index) const
{
    if (index.isValid())
        return static_cast<Item*>(index.internalPointer());
    return 0;
}

LotsModel::Item *LotsModel::toItem(Level *level) const
{
    return toItem(level->level);
}

LotsModel::Item *LotsModel::toItem(int level) const
{
    if (!mRootItem)
        return 0;
    foreach (Item *item, mRootItem->children)
        if (item->level->level == level)
            return item;
    return 0;
}

LotsModel::Item *LotsModel::toItem(WorldCellLot *lot) const
{
    Level *level = mLevels[lot->level()];
    Item *parent = toItem(level);
    foreach (Item *item, parent->children)
        if (item->lot == lot)
            return item;
    return 0;
}

void LotsModel::setModelData()
{
    beginResetModel();

    qDeleteAll(mLevels);
    mLevels.clear();

    delete mRootItem;
    mRootItem = 0;

    if (!mCell) {
        endResetModel();
        return;
    }

    int maxLevel;
    if (mCellDoc) {
        maxLevel = mCellDoc->scene()->mapComposite()->maxLevel();
    } else {
        maxLevel = 0; // Could use MapInfo to get maxLevel
        foreach (WorldCellLot *lot, mCell->lots())
            maxLevel = qMax(lot->level(), maxLevel);
    }

    mRootItem = new Item();

    for (int level = 0; level <= maxLevel; level++)
        mLevels += new Level(level);

    foreach (Level *level, mLevels)
        new Item(mRootItem, 0, level);

    foreach (WorldCellLot *lot, mCell->lots()) {
        Item *parent = toItem(lot->level());
        parent->level->lots.insert(0, lot);
        new Item(parent, 0, lot);
    }

    endResetModel();
}

void LotsModel::setDocument(Document *doc)
{
    if (mWorldDoc)
        mWorldDoc->disconnect(this);
    if (mCellDoc) {
        mCellDoc->worldDocument()->disconnect(this);
        mCellDoc->disconnect(this);
    }

    mWorldDoc = doc ? doc->asWorldDocument() : nullptr;
    mCellDoc = doc ? doc->asCellDocument() : nullptr;

    WorldCell *cell = nullptr;
    WorldDocument *worldDoc = nullptr;

    if (mWorldDoc) {
        worldDoc = mWorldDoc;
        if (mWorldDoc->selectedCellCount() == 1)
            cell = mWorldDoc->selectedCells().first();
    }

    if (mCellDoc) {
        worldDoc = mCellDoc->worldDocument();
        cell = mCellDoc->cell();

        connect(mCellDoc, SIGNAL(layerGroupAdded(int)),
                SLOT(layerGroupAdded(int)));
    }

    if (worldDoc) {
        connect(worldDoc, SIGNAL(cellContentsAboutToChange(WorldCell*)),
                SLOT(cellContentsAboutToChange(WorldCell*)));
        connect(worldDoc, SIGNAL(cellContentsChanged(WorldCell*)),
                SLOT(cellContentsChanged(WorldCell*)));
        connect(worldDoc, SIGNAL(cellMapFileAboutToChange(WorldCell*)),
                SLOT(cellContentsAboutToChange(WorldCell*)));
        connect(worldDoc, SIGNAL(cellMapFileChanged(WorldCell*)),
                SLOT(cellContentsChanged(WorldCell*)));

        connect(worldDoc, SIGNAL(cellLotAdded(WorldCell*,int)),
                SLOT(cellLotAdded(WorldCell*,int)));
        connect(worldDoc, SIGNAL(cellLotAboutToBeRemoved(WorldCell*,int)),
                SLOT(cellLotAboutToBeRemoved(WorldCell*,int)));
        connect(worldDoc, &WorldDocument::cellLotReordered,
                this, &LotsModel::cellLotReordered);
        connect(worldDoc, SIGNAL(lotLevelChanged(WorldCellLot*)),
                SLOT(lotLevelChanged(WorldCellLot*)));
    }

    setCell(cell);
}

void LotsModel::setCell(WorldCell *cell)
{
    Q_ASSERT(!cell || (mWorldDoc || mCellDoc));

    mCell = cell;
    mSynching = true;
    setModelData();
    mSynching = false;

    emit synched();
}

void LotsModel::cellLotAdded(WorldCell *cell, int index)
{
    Q_UNUSED(index)
    if (cell == mCell)
        setCell(mCell); // lazy, just reset the whole list
}

void LotsModel::cellLotAboutToBeRemoved(WorldCell *cell, int index)
{
    if (cell != mCell)
        return;
    WorldCellLot *lot = cell->lots().at(index);
    if (Item *item = toItem(lot)) {
        Item *parentItem = item->parent;
        QModelIndex parent = this->index(parentItem->level);
        int row = parentItem->children.indexOf(item);
        beginRemoveRows(parent, row, row);
        delete parentItem->children.takeAt(row);
        endRemoveRows();
    }
}

void LotsModel::cellLotReordered(WorldCellLot *lot)
{
    if (lot->cell() != mCell)
        return;
    if (Item *item = toItem(lot)) {
        Item *parentItem = item->parent;
        QModelIndex parent = this->index(parentItem->level);
        int row = parentItem->children.indexOf(item);
        beginRemoveRows(parent, row, row);
        delete parentItem->children.takeAt(row);
        endRemoveRows();
    }
    cellLotAdded(mCell, mCell->indexOf(lot));
}

void LotsModel::cellContentsAboutToChange(WorldCell *cell)
{
    qDebug() << "LotsModel::cellContentsAboutToChange" << cell->pos() << "(mine is" << (mCell ? mCell->pos() : QPoint()) << ")";
    if (cell != mCell)
        return;

    beginResetModel();

    qDeleteAll(mLevels);
    mLevels.clear();

    delete mRootItem;
    mRootItem = nullptr;

    endResetModel();

    mSynching = true; // ignore layerGroupAdded
}

void LotsModel::cellContentsChanged(WorldCell *cell)
{
    if (cell != mCell)
        return;
    mSynching = false; // stop ignoring layerGroupAdded
    setCell(mCell);
}

void LotsModel::lotLevelChanged(WorldCellLot *lot)
{
    if (lot->cell() == mCell)
        setCell(mCell); // lazy, just reset the whole list
}

void LotsModel::layerGroupAdded(int level)
{
    Q_UNUSED(level)
    if (mSynching)
        return;
    if (mCellDoc)
        setCell(mCell);
}
