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

#include "mapboxdock.h"

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

#include "mapboxpropertiesform.h"
#include "mapboxscene.h"

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

MapboxDock::MapboxDock(QWidget *parent)
    : QDockWidget(parent)
    , mView(new MapboxView(this))
    , mCellDoc(nullptr)
    , mWorldDoc(nullptr)
{
    setObjectName(QLatin1String("MapboxDock"));

    QWidget *widget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setMargin(2);

    layout->addWidget(mView);

    mPropertiesForm = new MapboxPropertiesForm(widget);
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

void MapboxDock::changeEvent(QEvent *e)
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

void MapboxDock::retranslateUi()
{
    setWindowTitle(tr("Mapbox"));
}

void MapboxDock::setDocument(Document *doc)
{
    if (mCellDoc)
        mCellDoc->disconnect(this);

    mWorldDoc = doc->asWorldDocument();
    mCellDoc = doc->asCellDocument();
    mView->setDocument(doc);
    mPropertiesForm->setDocument(doc);

    if (mCellDoc) {
        connect(mCellDoc, &CellDocument::selectedMapboxFeaturesChanged,
                this, &MapboxDock::selectedFeaturesChanged);
    }
}

void MapboxDock::clearDocument()
{
    if (mCellDoc)
        mCellDoc->disconnect(this);

    mWorldDoc = nullptr;
    mCellDoc = nullptr;
    mView->setDocument(nullptr);
    mPropertiesForm->clearDocument();
}

void MapboxDock::selectionChanged()
{
    if (mView->synchingSelection())
        return;

    if (mWorldDoc) {
        QModelIndexList selection = mView->selectionModel()->selectedIndexes();
        QList<MapBoxFeature*> selectedFeatures;
        for (auto& index : selection) {
            if (index.column() > 0)
                continue;
            if (auto *feature = mView->model()->toFeature(index))
                selectedFeatures += feature;
        }
        mWorldDoc->setSelectedMapboxFeatures(selectedFeatures);
        return;
    }

    if (!mCellDoc)
        return;

    QModelIndexList selection = mView->selectionModel()->selectedIndexes();
    if (selection.size() == mView->model()->columnCount()) {
        if (MapBoxFeature* feature = mView->model()->toFeature(selection.first())) {
            if (!feature->mGeometry.mCoordinates.isEmpty() && !feature->mGeometry.mCoordinates[0].isEmpty()) {
                MapBoxPoint& point = feature->mGeometry.mCoordinates[0][0];
                QPointF scenePos = mCellDoc->scene()->renderer()->tileToPixelCoords(point.x, point.y);
                mCellDoc->view()->centerOn(scenePos.x(), scenePos.y());
    //            MapboxFeatureItem* item = mCellDoc->scene()->itemForMapboxFeature(feature);
    //            mCellDoc->view()->ensureVisible(item);
            }
        }
    }

    // Select features in the scene only if they can be edited
    if (!CreateMapboxPointTool::instance().isCurrent() &&
            !CreateMapboxPolygonTool::instance().isCurrent() &&
            !CreateMapboxPolylineTool::instance().isCurrent() &&
            !EditMapboxFeatureTool::instance().isCurrent())
        return;

    QList<MapBoxFeature*> features;
    for (QModelIndex& index : selection) {
        if (index.column() > 0)
            continue;
        if (MapBoxFeature* feature = mView->model()->toFeature(index)) {
            features << feature;
        }
    }

    mView->setSynchingSelection(true);
    mCellDoc->setSelectedMapboxFeatures(features);
    mView->setSynchingSelection(false);
}

void MapboxDock::itemClicked(const QModelIndex &index)
{
    if (mView->model()->toFeature(index)) {
        // Clicking an already-selected item should center it in the view
        if (mView->selectionModel()->isSelected(index))
            selectionChanged();
    }
}

void MapboxDock::itemDoubleClicked(const QModelIndex &index)
{
    Q_UNUSED(index)
}

void MapboxDock::trashItem(const QModelIndex &index)
{
    if (MapBoxFeature* feature = mView->model()->toFeature(index)) {
        if (mWorldDoc)
            mWorldDoc->removeMapboxFeature(feature->cell(), feature->index());
        if (mCellDoc)
            mCellDoc->worldDocument()->removeMapboxFeature(feature->cell(), feature->index());
    }
}

void MapboxDock::selectedFeaturesChanged()
{
}

/////

#include <QStyledItemDelegate>
#include <QLineEdit>
#include <QSpinBox>

class MapboxViewDelegate : public QStyledItemDelegate
 {
//     Q_OBJECT

 public:
     MapboxViewDelegate(QObject *parent = nullptr)
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

MapboxView::MapboxView(QWidget *parent)
    : QTreeView(parent)
    , mModel(new MapboxModel(this))
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

    setItemDelegate(new MapboxViewDelegate(this));

    setModel(mModel);

    connect(mModel, SIGNAL(synched()), SLOT(modelSynched()));
}

void MapboxView::mousePressEvent(QMouseEvent *event)
{
    // Prevent drag-and-drop happening on unselected items
    QModelIndex index = indexAt(event->pos());
    if (index.isValid()) {

        // Prevent drag-and-drop starting when clicking on an unselected item.
        setDragEnabled(selectionModel()->isSelected(index));

        if ((index.column() == 0) && mModel->toFeature(index)) {
            QRect itemRect = visualRect(index);
            QRect rect = static_cast<MapboxViewDelegate*>(itemDelegate())->closeButtonRect(itemRect);
            if (rect.contains(event->pos())) {
                event->accept();
                emit trashItem(index);
                return;
            }
        }
    }
    QTreeView::mousePressEvent(event);
}

void MapboxView::setDocument(Document *doc)
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
                this, &MapboxView::selectedCellsChanged);
        connect(mWorldDoc, &WorldDocument::selectedMapboxFeaturesChanged,
                this, &MapboxView::selectedFeaturesChanged);
    }
    if (mCellDoc) {
        connect(mCellDoc, &CellDocument::selectedMapboxFeaturesChanged,
                this, &MapboxView::selectedFeaturesChanged);
    }
}

bool MapboxView::synchingSelection() const
{
    return mSynchingSelection || mModel->synching();
}

void MapboxView::selectedCellsChanged()
{
    if (mWorldDoc->selectedCellCount() == 1)
        mModel->setCell(mWorldDoc->selectedCells().first());
    else
        mModel->setCell(nullptr);
}

void MapboxView::selectedFeaturesChanged()
{
    if (synchingSelection())
        return;
    mSynchingSelection = true;
    clearSelection();
    auto selected = mWorldDoc ? mWorldDoc->selectedMapboxFeatures() : mCellDoc->selectedMapboxFeatures();
    for (MapBoxFeature* feature : selected) {
        selectionModel()->select(model()->index(feature),
                                 QItemSelectionModel::Select | QItemSelectionModel::Rows);
    }
    mSynchingSelection = false;
}

void MapboxView::modelSynched()
{
    if (mWorldDoc)
        selectedFeaturesChanged();
    if (mCellDoc)
        selectedFeaturesChanged();
    expandAll();
}

/////

// FIXME: Are these global objects safe?
QString MapboxModel::MapboxModelMimeType(QLatin1String("application/x-pzworlded-lot"));

MapboxModel::MapboxModel(QObject *parent)
    : QAbstractItemModel(parent)
    , mCell(nullptr)
    , mCellDoc(nullptr)
    , mWorldDoc(nullptr)
    , mRootItem(nullptr)
    , mSynching(false)
{
}

MapboxModel::~MapboxModel()
{
    delete mRootItem;
}

QModelIndex MapboxModel::index(int row, int column, const QModelIndex &parent) const
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

MapBoxFeature *MapboxModel::toFeature(const QModelIndex &index) const
{
    if (index.isValid())
        return toItem(index)->feature;
    return nullptr;
}

QModelIndex MapboxModel::parent(const QModelIndex &index) const
{
    if (Item *item = toItem(index)) {
        if (item->parent != mRootItem) {
            Item *grandParent = item->parent->parent;
            return createIndex(grandParent->children.indexOf(item->parent), 0, item->parent);
        }
    }
    return QModelIndex();
}

int MapboxModel::rowCount(const QModelIndex &parent) const
{
    if (!mRootItem)
        return 0;
    if (!parent.isValid())
        return mRootItem->children.size();
    if (Item *item = toItem(parent))
        return item->children.size();
    return 0;
}

int MapboxModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

QVariant MapboxModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
        case 0: return tr("Column 0");
        case 1: return tr("Column 1");
        }
    }
    return QVariant();
}

QVariant MapboxModel::data(const QModelIndex &index, int role) const
{
    if (MapBoxFeature* feature = toFeature(index)) {
        if (index.column())
            return QVariant();
        switch (role) {
        case Qt::CheckStateRole: {
            if (mCellDoc) {
                // Don't check SubMapItem::isVisible because it changes during CellScene::updateCurrentLevelHighlight()
                MapboxFeatureItem *sceneItem = mCellDoc->scene()->itemForMapboxFeature(feature);
                bool visible = true/*sceneItem ? sceneItem->subMap()->isVisible() : false*/;
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

bool MapboxModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (MapBoxFeature * feature = toFeature(index)) {
        switch (role) {
        case Qt::CheckStateRole: {
            if (mCellDoc) {
                Qt::CheckState c = static_cast<Qt::CheckState>(value.toInt());
                mCellDoc->scene()->setMapboxFeatureVisible(feature, c == Qt::Checked);
                emit dataChanged(index, index);
                return true;
            }
            break;
        }
        }
    }
    return QAbstractItemModel::setData(index, value, role);
}

Qt::ItemFlags MapboxModel::flags(const QModelIndex &index) const
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

Qt::DropActions MapboxModel::supportedDropActions() const
{
    return /*Qt::CopyAction |*/ Qt::MoveAction;
}

QStringList MapboxModel::mimeTypes() const
{
    QStringList types;
    types << MapboxModelMimeType;
    return types;
}

QMimeData *MapboxModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QByteArray encodedData;

    QDataStream stream(&encodedData, QIODevice::WriteOnly);

    for (const QModelIndex& index : indexes) {
        if (MapBoxFeature* feature = toFeature(index)) {
            QString text = QString::number(feature->index());
            stream << text;
        }
    }

    mimeData->setData(MapboxModelMimeType, encodedData);
    return mimeData;
}

bool MapboxModel::dropMimeData(const QMimeData* data,
     Qt::DropAction action, int row, int column, const QModelIndex& parent)
 {
    Q_UNUSED(row)

    if (action == Qt::IgnoreAction)
         return true;

     if (!data->hasFormat(MapboxModelMimeType))
         return false;

     if (column > 0)
         return false;

     Item *parentItem = toItem(parent);
     if (parentItem == nullptr || parentItem != mRootItem)
         return false;

     WorldDocument* worldDoc = mWorldDoc ? mWorldDoc : mCellDoc->worldDocument();

     QByteArray encodedData = data->data(MapboxModelMimeType);
     QDataStream stream(&encodedData, QIODevice::ReadOnly);
     QList<MapBoxFeature*> features;
     int rows = 0;

     while (!stream.atEnd()) {
         QString text;
         stream >> text;
         features << mCell->mapBox().mFeatures.at(text.toInt());
         ++rows;
     }
#if 0
     // Note: parentItem may be destroyed by setCell()
     int count = features.size();
     if (count > 1)
         worldDoc->undoStack()->beginMacro(tr("Change %1 Lots' Level").arg(count));
     foreach (MapBoxFeature* feature, features)
         worldDoc->setLotLevel(lot, level);
     if (count > 1)
         worldDoc->undoStack()->endMacro();
#endif
     return true;
}

QModelIndex MapboxModel::index(MapBoxFeature* feature) const
{
    Item *item = toItem(feature);
    int row = item->parent->children.indexOf(item);
    return createIndex(row, 0, item);
}

MapboxModel::Item *MapboxModel::toItem(const QModelIndex &index) const
{
    if (index.isValid())
        return static_cast<Item*>(index.internalPointer());
    return nullptr;
}

MapboxModel::Item *MapboxModel::toItem(MapBoxFeature* feature) const
{
    Item *parent = mRootItem;
    foreach (Item *item, parent->children)
        if (item->feature == feature)
            return item;
    return nullptr;
}

void MapboxModel::setModelData()
{
    beginResetModel();

    delete mRootItem;
    mRootItem = nullptr;

    if (!mCell) {
        endResetModel();
        return;
    }

    mRootItem = new Item();

    for (MapBoxFeature* feature : mCell->mapBox().mFeatures) {
        Item *parent = mRootItem;
        new Item(parent, parent->children.size(), feature);
    }

    endResetModel();
}

void MapboxModel::setDocument(Document *doc)
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
                this, &MapboxModel::cellContentsAboutToChange);
        connect(worldDoc, &WorldDocument::cellContentsChanged,
                this, &MapboxModel::cellContentsChanged);

        connect(worldDoc, &WorldDocument::mapboxFeatureAdded,
                this, &MapboxModel::featureAdded);
        connect(worldDoc, &WorldDocument::mapboxFeatureAboutToBeRemoved,
                this, &MapboxModel::featureAboutToBeRemoved);
        connect(worldDoc, &WorldDocument::mapboxPropertiesChanged,
                this, &MapboxModel::propertiesChanged);
    }

    setCell(cell);
}

void MapboxModel::setCell(WorldCell *cell)
{
    Q_ASSERT(!cell || (mWorldDoc || mCellDoc));

    mCell = cell;
    mSynching = true;
    setModelData();
    mSynching = false;

    emit synched();
}

void MapboxModel::featureAdded(WorldCell *cell, int index)
{
    Q_UNUSED(index)
    if (cell == mCell)
        setCell(mCell); // lazy, just reset the whole list
}

void MapboxModel::featureAboutToBeRemoved(WorldCell *cell, int index)
{
    if (cell != mCell)
        return;
    MapBoxFeature* feature = cell->mapBox().mFeatures.at(index);
    if (Item *item = toItem(feature)) {
        Item *parentItem = item->parent;
        QModelIndex parent = QModelIndex(); // root
        int row = parentItem->children.indexOf(item);
        beginRemoveRows(parent, row, row);
        delete parentItem->children.takeAt(row);
        endRemoveRows();
    }
}

void MapboxModel::propertiesChanged(WorldCell *cell, int featureIndex)
{
    Q_UNUSED(featureIndex)
    if (cell == mCell)
        setCell(mCell); // lazy, just reset the whole list
}

void MapboxModel::cellContentsAboutToChange(WorldCell *cell)
{
    qDebug() << "MapboxModel::cellContentsAboutToChange" << cell->pos() << "(mine is" << (mCell ? mCell->pos() : QPoint()) << ")";
    if (cell != mCell)
        return;

    beginResetModel();

    delete mRootItem;
    mRootItem = nullptr;

    endResetModel();

    mSynching = true; // ignore layerGroupAdded
}

void MapboxModel::cellContentsChanged(WorldCell *cell)
{
    if (cell != mCell)
        return;
    mSynching = false; // stop ignoring layerGroupAdded
    setCell(mCell);
}
