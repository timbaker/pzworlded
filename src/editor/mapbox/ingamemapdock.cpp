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

#include "ingamemapdock.h"

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

#include "ingamemappropertiesform.h"
#include "ingamemapscene.h"

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

InGameMapDock::InGameMapDock(QWidget *parent)
    : QDockWidget(parent)
    , mView(new InGameMapView(this))
    , mCellDoc(nullptr)
    , mWorldDoc(nullptr)
{
    setObjectName(QLatin1String("InGameMapDock"));

    QWidget *widget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setMargin(2);

    layout->addWidget(mView);

    mPropertiesForm = new InGameMapPropertiesForm(widget);
    layout->addWidget(mPropertiesForm);
    layout->setStretch(0, 2);

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

void InGameMapDock::changeEvent(QEvent *e)
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

void InGameMapDock::retranslateUi()
{
    setWindowTitle(tr("InGameMap"));
}

void InGameMapDock::setDocument(Document *doc)
{
    if (mCellDoc)
        mCellDoc->disconnect(this);

    mWorldDoc = doc->asWorldDocument();
    mCellDoc = doc->asCellDocument();
    mView->setDocument(doc);
    mPropertiesForm->setDocument(doc);

    if (mCellDoc) {
        connect(mCellDoc, &CellDocument::selectedInGameMapFeaturesChanged,
                this, &InGameMapDock::selectedFeaturesChanged);
    }
}

void InGameMapDock::clearDocument()
{
    if (mCellDoc)
        mCellDoc->disconnect(this);

    mWorldDoc = nullptr;
    mCellDoc = nullptr;
    mView->setDocument(nullptr);
    mPropertiesForm->clearDocument();
}

void InGameMapDock::selectionChanged()
{
    if (mView->synchingSelection())
        return;

    if (mWorldDoc) {
        QModelIndexList selection = mView->selectionModel()->selectedIndexes();
        QList<InGameMapFeature*> selectedFeatures;
        for (auto& index : selection) {
            if (index.column() > 0)
                continue;
            if (auto *feature = mView->model()->toFeature(index))
                selectedFeatures += feature;
        }
        mView->setSynchingSelection(true);
        mWorldDoc->setSelectedInGameMapFeatures(selectedFeatures);
        mView->setSynchingSelection(false);
        return;
    }

    if (!mCellDoc)
        return;

    QModelIndexList selection = mView->selectionModel()->selectedIndexes();
    if (selection.size() == mView->model()->columnCount()) {
        if (InGameMapFeature* feature = mView->model()->toFeature(selection.first())) {
            if (!feature->mGeometry.mCoordinates.isEmpty() && !feature->mGeometry.mCoordinates[0].isEmpty()) {
                InGameMapPoint& point = feature->mGeometry.mCoordinates[0][0];
                QPointF scenePos = mCellDoc->scene()->renderer()->tileToPixelCoords(point.x, point.y);
                mCellDoc->view()->centerOn(scenePos.x(), scenePos.y());
    //            InGameMapFeatureItem* item = mCellDoc->scene()->itemForInGameMapFeature(feature);
    //            mCellDoc->view()->ensureVisible(item);
            }
        }
    }

    // Select features in the scene only if they can be edited
    if (!CreateInGameMapPointTool::instance().isCurrent() &&
            !CreateInGameMapPolygonTool::instance().isCurrent() &&
            !CreateInGameMapPolylineTool::instance().isCurrent() &&
            !EditInGameMapFeatureTool::instance().isCurrent())
        return;

    QList<InGameMapFeature*> features;
    for (QModelIndex& index : selection) {
        if (index.column() > 0)
            continue;
        if (InGameMapFeature* feature = mView->model()->toFeature(index)) {
            features << feature;
        }
    }

    mView->setSynchingSelection(true);
    mCellDoc->setSelectedInGameMapFeatures(features);
    mView->setSynchingSelection(false);
}

void InGameMapDock::itemClicked(const QModelIndex &index)
{
    if (mView->model()->toFeature(index)) {
        // Clicking an already-selected item should center it in the view
        if (mView->selectionModel()->isSelected(index))
            selectionChanged();
    }
}

void InGameMapDock::itemDoubleClicked(const QModelIndex &index)
{
    Q_UNUSED(index)
}

void InGameMapDock::trashItem(const QModelIndex &index)
{
    if (InGameMapFeature* feature = mView->model()->toFeature(index)) {
        if (mWorldDoc)
            mWorldDoc->removeInGameMapFeature(feature->cell(), feature->index());
        if (mCellDoc)
            mCellDoc->worldDocument()->removeInGameMapFeature(feature->cell(), feature->index());
    }
}

void InGameMapDock::selectedFeaturesChanged()
{
}

/////

#include <QStyledItemDelegate>
#include <QLineEdit>
#include <QSpinBox>

class InGameMapViewDelegate : public QStyledItemDelegate
 {
//     Q_OBJECT

 public:
     InGameMapViewDelegate(QObject *parent = nullptr)
         : QStyledItemDelegate(parent)
         , mTrashPixmap(QLatin1String(":/images/16x16/edit-delete.png"))
     {

     }

     void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
     {
         QStyledItemDelegate::paint(painter, option, index);

         if ((index.column() == 0) /*&& index.parent().isValid()*/ && (option.state & QStyle::State_MouseOver)) {
             painter->save();
             QRect closeRect = closeButtonRect(option.rect);
             painter->setClipRect(closeRect);
             painter->drawPixmap(closeRect.x(), closeRect.y(), mTrashPixmap);
             painter->restore();
         }
     }

     QRect closeButtonRect(const QRect &itemViewRect) const
     {
        QRect closeRect(itemViewRect.x()-20+2, itemViewRect.y(), mTrashPixmap.width(), itemViewRect.height());
        return closeRect;
     }

private:
     QPixmap mTrashPixmap;
 };

/////

InGameMapView::InGameMapView(QWidget *parent)
    : QTreeView(parent)
    , mModel(new InGameMapModel(this))
    , mCell(nullptr)
    , mCellDoc(nullptr)
    , mWorldDoc(nullptr)
    , mSynching(false)
    , mSynchingSelection(false)
{
    setDragEnabled(true);
    setAcceptDrops(true);
    setDragDropMode(QAbstractItemView::InternalMove);
    setUniformRowHeights(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);

    setMouseTracking(true); // Needed on Mac OS X for garbage can

    header()->hide();

    setItemDelegate(new InGameMapViewDelegate(this));

    setModel(mModel);

    connect(mModel, SIGNAL(synched()), SLOT(modelSynched()));
}

void InGameMapView::mousePressEvent(QMouseEvent *event)
{
    // Prevent drag-and-drop happening on unselected items
    QModelIndex index = indexAt(event->pos());
    if (index.isValid()) {

        // Prevent drag-and-drop starting when clicking on an unselected item.
        setDragEnabled(selectionModel()->isSelected(index));

        if ((index.column() == 0) && mModel->toFeature(index)) {
            QRect itemRect = visualRect(index);
            QRect rect = static_cast<InGameMapViewDelegate*>(itemDelegate())->closeButtonRect(itemRect);
            if (rect.contains(event->pos())) {
                event->accept();
                emit trashItem(index);
                return;
            }
        }
    }
    QTreeView::mousePressEvent(event);
}

void InGameMapView::setDocument(Document *doc)
{
    if (mWorldDoc)
        mWorldDoc->disconnect(this);
    if (mCellDoc)
        mCellDoc->disconnect(this);

    mWorldDoc = doc ? doc->asWorldDocument() : nullptr;
    mCellDoc = doc ? doc->asCellDocument() : nullptr;

    mModel->setDocument(doc);

    if (mWorldDoc) {
        connect(mWorldDoc, &WorldDocument::selectedCellsChanged,
                this, &InGameMapView::selectedCellsChanged);
        connect(mWorldDoc, &WorldDocument::selectedInGameMapFeaturesChanged,
                this, &InGameMapView::selectedFeaturesChanged);
    }
    if (mCellDoc) {
        connect(mCellDoc, &CellDocument::selectedInGameMapFeaturesChanged,
                this, &InGameMapView::selectedFeaturesChanged);
    }
}

bool InGameMapView::synchingSelection() const
{
    return mSynchingSelection || mModel->synching();
}

void InGameMapView::selectedCellsChanged()
{
    mWorldDoc->setSelectedInGameMapFeatures(QList<InGameMapFeature*>());

    if (mWorldDoc->selectedCellCount() == 1)
        mModel->setCell(mWorldDoc->selectedCells().first());
    else
        mModel->setCell(nullptr);
}

void InGameMapView::selectedFeaturesChanged()
{
    if (synchingSelection())
        return;
    mSynchingSelection = true;
    clearSelection();
    auto selected = mWorldDoc ? mWorldDoc->selectedInGameMapFeatures() : mCellDoc->selectedInGameMapFeatures();
    for (InGameMapFeature* feature : selected) {
        selectionModel()->select(model()->index(feature),
                                 QItemSelectionModel::Select | QItemSelectionModel::Rows);
    }
    mSynchingSelection = false;
}

void InGameMapView::modelSynched()
{
    if (mWorldDoc)
        selectedFeaturesChanged();
    if (mCellDoc)
        selectedFeaturesChanged();
    expandAll();
}

/////

// FIXME: Are these global objects safe?
QString InGameMapModel::InGameMapModelMimeType(QLatin1String("application/x-pzworlded-lot"));

InGameMapModel::InGameMapModel(QObject *parent)
    : QAbstractItemModel(parent)
    , mCell(nullptr)
    , mCellDoc(nullptr)
    , mWorldDoc(nullptr)
    , mRootItem(nullptr)
    , mSynching(false)
{
}

InGameMapModel::~InGameMapModel()
{
    delete mRootItem;
}

QModelIndex InGameMapModel::index(int row, int column, const QModelIndex &parent) const
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

InGameMapFeature *InGameMapModel::toFeature(const QModelIndex &index) const
{
    if (index.isValid())
        return toItem(index)->feature;
    return nullptr;
}

QModelIndex InGameMapModel::parent(const QModelIndex &index) const
{
    if (Item *item = toItem(index)) {
        if (item->parent != mRootItem) {
            Item *grandParent = item->parent->parent;
            return createIndex(grandParent->children.indexOf(item->parent), 0, item->parent);
        }
    }
    return QModelIndex();
}

int InGameMapModel::rowCount(const QModelIndex &parent) const
{
    if (!mRootItem)
        return 0;
    if (!parent.isValid())
        return mRootItem->children.size();
    if (Item *item = toItem(parent))
        return item->children.size();
    return 0;
}

int InGameMapModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

QVariant InGameMapModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
        case 0: return tr("Column 0");
        case 1: return tr("Column 1");
        }
    }
    return QVariant();
}

QVariant InGameMapModel::data(const QModelIndex &index, int role) const
{
    if (InGameMapFeature* feature = toFeature(index)) {
        if (index.column())
            return QVariant();
        switch (role) {
        case Qt::CheckStateRole: {
            if (mCellDoc) {
                // Don't check SubMapItem::isVisible because it changes during CellScene::updateCurrentLevelHighlight()
                InGameMapFeatureItem *sceneItem = mCellDoc->scene()->itemForInGameMapFeature(feature);
                bool visible = sceneItem ? sceneItem->isVisible() : false /*sceneItem ? sceneItem->subMap()->isVisible() : false*/;
                return visible ? Qt::Checked : Qt::Unchecked;
            }
            break;
        }
        case Qt::DisplayRole: {
            QString propertyString;
            for (auto& property : feature->properties()) {
                propertyString += QStringLiteral(" %1=%2").arg(property.mKey).arg(property.mValue);
            }
            return feature->mGeometry.mType + propertyString;
        }
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

bool InGameMapModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (InGameMapFeature * feature = toFeature(index)) {
        switch (role) {
        case Qt::CheckStateRole: {
            if (mCellDoc) {
                Qt::CheckState c = static_cast<Qt::CheckState>(value.toInt());
                mCellDoc->scene()->setInGameMapFeatureVisible(feature, c == Qt::Checked);
                emit dataChanged(index, index);
                return true;
            }
            break;
        }
        }
    }
    return QAbstractItemModel::setData(index, value, role);
}

Qt::ItemFlags InGameMapModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags rc = QAbstractItemModel::flags(index);
    if (index.column() == 0) {
        if (mCellDoc)
            rc |= Qt::ItemIsUserCheckable;
//        if (toFeature(index))
//            rc |= Qt::ItemIsDragEnabled;
    }
    return rc;
}

Qt::DropActions InGameMapModel::supportedDropActions() const
{
    return /*Qt::CopyAction |*/ Qt::MoveAction;
}

QStringList InGameMapModel::mimeTypes() const
{
    QStringList types;
    types << InGameMapModelMimeType;
    return types;
}

QMimeData *InGameMapModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QByteArray encodedData;

    QDataStream stream(&encodedData, QIODevice::WriteOnly);

    for (const QModelIndex& index : indexes) {
        if (InGameMapFeature* feature = toFeature(index)) {
            QString text = QString::number(feature->index());
            stream << text;
        }
    }

    mimeData->setData(InGameMapModelMimeType, encodedData);
    return mimeData;
}

bool InGameMapModel::dropMimeData(const QMimeData* data,
     Qt::DropAction action, int row, int column, const QModelIndex& parent)
 {
    Q_UNUSED(row)

    if (action == Qt::IgnoreAction)
         return true;

     if (!data->hasFormat(InGameMapModelMimeType))
         return false;

     if (column > 0)
         return false;

     Item *parentItem = toItem(parent);
     if (parentItem == nullptr || parentItem != mRootItem)
         return false;

     WorldDocument* worldDoc = mWorldDoc ? mWorldDoc : mCellDoc->worldDocument();

     QByteArray encodedData = data->data(InGameMapModelMimeType);
     QDataStream stream(&encodedData, QIODevice::ReadOnly);
     QList<InGameMapFeature*> features;
     int rows = 0;

     while (!stream.atEnd()) {
         QString text;
         stream >> text;
         features << mCell->inGameMap().mFeatures.at(text.toInt());
         ++rows;
     }
#if 0
     // Note: parentItem may be destroyed by setCell()
     int count = features.size();
     if (count > 1)
         worldDoc->undoStack()->beginMacro(tr("Change %1 Lots' Level").arg(count));
     foreach (InGameMapFeature* feature, features)
         worldDoc->setLotLevel(lot, level);
     if (count > 1)
         worldDoc->undoStack()->endMacro();
#endif
     return true;
}

QModelIndex InGameMapModel::index(InGameMapFeature* feature) const
{
    if (Item *item = toItem(feature)) {
        int row = item->parent->children.indexOf(item);
        return createIndex(row, 0, item);
    }
    Q_ASSERT(false);
    return QModelIndex();
}

InGameMapModel::Item *InGameMapModel::toItem(const QModelIndex &index) const
{
    if (index.isValid())
        return static_cast<Item*>(index.internalPointer());
    return nullptr;
}

InGameMapModel::Item *InGameMapModel::toItem(InGameMapFeature* feature) const
{
    Item *parent = mRootItem;
    foreach (Item *item, parent->children)
        if (item->feature == feature)
            return item;
    return nullptr;
}

void InGameMapModel::setModelData()
{
    beginResetModel();

    delete mRootItem;
    mRootItem = nullptr;

    if (!mCell) {
        endResetModel();
        return;
    }

    mRootItem = new Item();

    for (InGameMapFeature* feature : mCell->inGameMap().mFeatures) {
        Item *parent = mRootItem;
        new Item(parent, parent->children.size(), feature);
    }

    endResetModel();
}

void InGameMapModel::setDocument(Document *doc)
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
    }

    if (worldDoc) {
        connect(worldDoc, &WorldDocument::cellContentsAboutToChange,
                this, &InGameMapModel::cellContentsAboutToChange);
        connect(worldDoc, &WorldDocument::cellContentsChanged,
                this, &InGameMapModel::cellContentsChanged);

        connect(worldDoc, &WorldDocument::inGameMapFeatureAdded,
                this, &InGameMapModel::featureAdded);
        connect(worldDoc, &WorldDocument::inGameMapFeatureAboutToBeRemoved,
                this, &InGameMapModel::featureAboutToBeRemoved);
        connect(worldDoc, &WorldDocument::inGameMapPropertiesChanged,
                this, &InGameMapModel::propertiesChanged);
    }

    setCell(cell);
}

void InGameMapModel::setCell(WorldCell *cell)
{
    Q_ASSERT(!cell || (mWorldDoc || mCellDoc));

    mCell = cell;
    mSynching = true;
    setModelData();
    mSynching = false;

    emit synched();
}

void InGameMapModel::featureAdded(WorldCell *cell, int index)
{
    Q_UNUSED(index)
    if (cell == mCell)
        setCell(mCell); // lazy, just reset the whole list
}

void InGameMapModel::featureAboutToBeRemoved(WorldCell *cell, int index)
{
    if (cell != mCell)
        return;
    InGameMapFeature* feature = cell->inGameMap().mFeatures.at(index);
    if (Item *item = toItem(feature)) {
        Item *parentItem = item->parent;
        QModelIndex parent = QModelIndex(); // root
        int row = parentItem->children.indexOf(item);
        beginRemoveRows(parent, row, row);
        delete parentItem->children.takeAt(row);
        endRemoveRows();
    }
}

void InGameMapModel::propertiesChanged(WorldCell *cell, int featureIndex)
{
    Q_UNUSED(featureIndex)
    if (cell == mCell)
        setCell(mCell); // lazy, just reset the whole list
}

void InGameMapModel::cellContentsAboutToChange(WorldCell *cell)
{
    qDebug() << "InGameMapModel::cellContentsAboutToChange" << cell->pos() << "(mine is" << (mCell ? mCell->pos() : QPoint()) << ")";
    if (cell != mCell)
        return;

    beginResetModel();

    delete mRootItem;
    mRootItem = nullptr;

    endResetModel();

    mSynching = true; // ignore layerGroupAdded
}

void InGameMapModel::cellContentsChanged(WorldCell *cell)
{
    if (cell != mCell)
        return;
    mSynching = false; // stop ignoring layerGroupAdded
    setCell(mCell);
}
