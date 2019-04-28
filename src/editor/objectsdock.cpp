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

#include "objectsdock.h"

#include "celldocument.h"
#include "cellscene.h"
#include "cellview.h"
#include "mapcomposite.h"
#include "mapmanager.h"
#include "scenetools.h"
#include "world.h"
#include "worldcell.h"
#include "worlddocument.h"

#include "maprenderer.h"
#include "tilelayer.h"

#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QEvent>
#include <QFileInfo>
#include <QHeaderView>
#include <QMimeData>
#include <QMouseEvent>
#include <QPixmap>
#include <QSet>
#include <QUndoStack>
#include <QVBoxLayout>

using namespace Tiled;

ObjectsDock::ObjectsDock(QWidget *parent)
    : QDockWidget(parent)
    , mView(new ObjectsView(this))
    , mCellDoc(0)
    , mWorldDoc(0)
{
    setObjectName(QLatin1String("ObjectsDock"));

    QWidget *widget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setMargin(2);

    layout->addWidget(mView);

    setWidget(widget);
    retranslateUi();

    connect(mView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ObjectsDock::selectionChanged);
    connect(mView, &QAbstractItemView::clicked,
            this, &ObjectsDock::itemClicked);
    connect(mView, &QAbstractItemView::doubleClicked,
            this, &ObjectsDock::itemDoubleClicked);
    connect(mView, &ObjectsView::trashItem, this, &ObjectsDock::trashItem);

    // Workaround since a tabbed dockwidget that is not currently visible still
    // returns true for isVisible()
    connect(this, &QDockWidget::visibilityChanged,
            mView, &QWidget::setVisible);
}

void ObjectsDock::changeEvent(QEvent *e)
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

void ObjectsDock::retranslateUi()
{
    setWindowTitle(tr("Objects"));
}

void ObjectsDock::setDocument(Document *doc)
{
    mWorldDoc = doc->asWorldDocument();
    mCellDoc = doc->asCellDocument();
    mView->setDocument(doc);
}

void ObjectsDock::clearDocument()
{
    mWorldDoc = 0, mCellDoc = 0;
    mView->setDocument(0);
}

void ObjectsDock::selectionChanged()
{
    if (mView->synchingSelection())
        return;

    if (mWorldDoc) {
        QModelIndexList selection = mView->selectionModel()->selectedIndexes();
        QList<WorldCellObject*> selectedObjects;
        foreach (QModelIndex index, selection) {
            if (index.column() > 0) continue;
            if (WorldCellObject *obj = mView->model()->toObject(index))
                selectedObjects += obj;
        }
        mWorldDoc->setSelectedObjects(selectedObjects);
        return;
    }

    if (!mCellDoc)
        return;

    WorldObjectGroup *objectGroup = mCellDoc->world()->nullObjectGroup();

    QModelIndexList selection = mView->selectionModel()->selectedIndexes();
    if (selection.size() == mView->model()->columnCount()) {
        QModelIndex index = selection.first();
        int level = -1;

        if (ObjectsModel::Level *levelPtr = mView->model()->toLevel(index))
            level = levelPtr->level;

        if (WorldObjectGroup *og = mView->model()->toGroup(index)) {
            objectGroup = og;
            QModelIndex parent = mView->model()->parent(index);
            if (ObjectsModel::Level *levelPtr = mView->model()->toLevel(parent))
                level = levelPtr->level;
        }

        // Center the view on the selected WorldCellObject
        if (WorldCellObject *obj = mView->model()->toObject(index)) {
            QRect rect = mCellDoc->scene()->renderer()->boundingRect(obj->bounds().toRect(),
                                                                     obj->level());
            mCellDoc->view()->ensureRectVisible(rect);
            level = obj->level();
            objectGroup = obj->group();
        }

        if (level >= 0)
            mCellDoc->setCurrentLevel(level);
    }

    mCellDoc->setCurrentObjectGroup(objectGroup);

    QList<WorldCellObject*> objects;
    foreach (QModelIndex index, selection) {
        if (index.column() > 0) continue;
        if (WorldCellObject *obj = mView->model()->toObject(index))
            objects << obj;
    }

    mView->setSynchingSelection(true);
    mCellDoc->setSelectedObjects(objects);
    mView->setSynchingSelection(false);
}

void ObjectsDock::itemClicked(const QModelIndex &index)
{
    if (mView->model()->toObject(index)) {
        // Clicking an already-selected item should center it in the view
        if (mView->selectionModel()->isSelected(index))
            selectionChanged();
    }
}

void ObjectsDock::itemDoubleClicked(const QModelIndex &index)
{
    Q_UNUSED(index)
}

void ObjectsDock::trashItem(const QModelIndex &index)
{
    if (WorldCellObject *obj = mView->model()->toObject(index)) {
        if (mWorldDoc)
            mWorldDoc->removeCellObject(obj->cell(), obj->cell()->objects().indexOf(obj));
        if (mCellDoc)
            mCellDoc->worldDocument()->removeCellObject(obj->cell(), obj->cell()->objects().indexOf(obj));
    }
}

/////

ObjectsViewDelegate::ObjectsViewDelegate(ObjectsView *view, QObject *parent)
    : QStyledItemDelegate(parent)
    , mView(view)
    , mModel(view->model())
    , mTrashPixmap(QLatin1String(":/images/16x16/edit-delete.png"))
{

}

void ObjectsViewDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyledItemDelegate::paint(painter, option, index);

    if ((index.column() == 0) && mModel->isDeletable(index) && (option.state & QStyle::State_MouseOver)) {
        painter->save();
        QRect closeRect = closeButtonRect(option.rect); //option.rect.x()-40+2, option.rect.y(), mTrashPixmap.width(), option.rect.height());
        painter->setClipRect(closeRect);
        painter->drawPixmap(closeRect.x(), closeRect.y(), mTrashPixmap);
        painter->restore();
    }
}

QWidget *ObjectsViewDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                                           const QModelIndex &index) const
{
    if (index.column() == mModel->typeColumn()) {
        QComboBox *editor = new QComboBox(parent);
        connect(editor, qOverload<int>(&QComboBox::activated),
                mView, &ObjectsView::closeComboBoxEditor);
        return editor;
    }
    return QStyledItemDelegate::createEditor(parent, option, index);
}

void ObjectsViewDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    if (index.column() == mModel->typeColumn()) {
        if (WorldCellObject *obj = mModel->toObject(index)) {
            QComboBox *comboBox = static_cast<QComboBox*>(editor);
            comboBox->clear();
            QStringList types = obj->cell()->world()->objectTypes().names().mid(1);
            comboBox->addItem(tr("<no type>"));
            comboBox->addItems(types);
            int comboIndex = obj->type()->name().isEmpty()
                    ? 0 : types.indexOf(obj->type()->name()) + 1;
            comboBox->setCurrentIndex(comboIndex);
//            comboBox->showPopup();
        }
        return;
    }
    QStyledItemDelegate::setEditorData(editor, index);
}

void ObjectsViewDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                       const QModelIndex &index) const
{
    if (index.column() == mModel->typeColumn()) {
        QComboBox *comboBox = static_cast<QComboBox*>(editor);
        int comboIndex = comboBox->currentIndex();
        switch (comboIndex) {
        case 0:
            model->setData(index, QString(), Qt::EditRole);
            break;
        default:
            model->setData(index, comboBox->currentText(), Qt::EditRole);
            break;
        }
        return;
    }
    QStyledItemDelegate::setModelData(editor, model, index);
}

void ObjectsViewDelegate::updateEditorGeometry(QWidget *editor,
                                               const QStyleOptionViewItem &option,
                                               const QModelIndex &index) const
{
    Q_UNUSED(index)

    if (index.column() == 0) {
        // Don't place the editor over the checkbox
        QStyleOptionViewItemV4 opt = option;
        initStyleOption(&opt, index);
        QStyle *style = QApplication::style();
        QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt);
        editor->setGeometry(textRect);
    } else
        editor->setGeometry(option.rect);
}

bool ObjectsViewDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

QSize ObjectsViewDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    return size;
}

QRect ObjectsViewDelegate::closeButtonRect(const QRect &itemViewRect) const
{
    QRect closeRect(itemViewRect.x()-20*3+2, itemViewRect.y(), mTrashPixmap.width(), itemViewRect.height());
    return closeRect;
}

/////

ObjectsView::ObjectsView(QWidget *parent)
    : QTreeView(parent)
    , mModel(new ObjectsModel(this))
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

    setItemDelegate(new ObjectsViewDelegate(this, this));

    setEditTriggers(QAbstractItemView::SelectedClicked
                    | QAbstractItemView::DoubleClicked);

    setModel(mModel);

    header()->resizeSection(0, 180);
#if QT_VERSION >= 0x050000
    header()->setSectionResizeMode(QHeaderView::ResizeToContents);
#else
    header()->setResizeMode(1, QHeaderView::ResizeToContents);
#endif

    for (Level& level : mExpandedLevels) {
        level.expanded = true;
    }

    connect(mModel, &QAbstractItemModel::rowsInserted,
            this, &ObjectsView::rowsInserted);
    connect(mModel, &ObjectsModel::synched, this, &ObjectsView::modelSynched);
}

void ObjectsView::mousePressEvent(QMouseEvent *event)
{
    QModelIndex index = indexAt(event->pos());
    if (index.isValid()) {
        // Prevent drag-and-drop starting when clicking on an unselected item.
        setDragEnabled(selectionModel()->isSelected(index));

        if (mModel->toObject(index)) {
            if (index.column() == 0) {
                QRect itemRect = visualRect(index);
                QRect rect = ((ObjectsViewDelegate*)itemDelegate())->closeButtonRect(itemRect);
                if (rect.contains(event->pos())) {
                    event->accept();
                    emit trashItem(index);
                    return;
                }
            }
        }
    }

    QTreeView::mousePressEvent(event);
}

void ObjectsView::setDocument(Document *doc)
{
    if (mWorldDoc)
        mWorldDoc->disconnect(this);
    if (mCellDoc)
        mCellDoc->disconnect(this);

    saveExpandedLevels();

    mWorldDoc = doc ? doc->asWorldDocument() : 0;
    mCellDoc = doc ? doc->asCellDocument() : 0;

    mModel->setDocument(doc);

    restoreExpandedLevels();

    if (mWorldDoc) {
        connect(mWorldDoc, &WorldDocument::selectedCellsChanged,
                this, &ObjectsView::selectedCellsChanged);
    }
    if (mCellDoc) {
        connect(mCellDoc, &CellDocument::selectedObjectsChanged,
                this, &ObjectsView::selectedObjectsChanged);
    }
}

bool ObjectsView::synchingSelection() const
{
    return mSynchingSelection || mModel->synching();
}

void ObjectsView::rowsInserted(const QModelIndex &parent, int start, int end)
{
    QTreeView::rowsInserted(parent, start, end);

    // When adding new ObjectGroups, they aren't expanded
    while (start <= end)
        expand(mModel->index(start++, 0, parent));
}

void ObjectsView::selectedCellsChanged()
{
    saveExpandedLevels();

    if (mWorldDoc->selectedCellCount() == 1) {
        mModel->setCell(mWorldDoc->selectedCells().first());
    } else {
        mModel->setCell(0);
    }

    restoreExpandedLevels();
}

void ObjectsView::selectedObjectsChanged()
{
    if (synchingSelection())
        return;
    mSynchingSelection = true;
    clearSelection();
    foreach (WorldCellObject *obj, mCellDoc->selectedObjects()) {
        selectionModel()->select(model()->index(obj),
                                 QItemSelectionModel::Select | QItemSelectionModel::Rows);
    }
    if (mCellDoc->selectedObjectCount() == 1)
        scrollTo(model()->index(mCellDoc->selectedObjects().first()));
    mSynchingSelection = false;
}

void ObjectsView::modelSynched()
{
    if (mCellDoc)
        selectedObjectsChanged();
    expandAll();
}

// http://qt-project.org/forums/viewthread/4531
void ObjectsView::closeComboBoxEditor()
{
    QModelIndex index = currentIndex();
    currentChanged( index, index );
}

void ObjectsView::saveExpandedLevels()
{
    int nLevels = mModel->rowCount();
    for (int i = 0; i < nLevels; i++) {
        const QModelIndex levelIndex = mModel->index(i, 0);
        ObjectsModel::Level* modelLevel = mModel->toLevel(levelIndex);
        Level& level = mExpandedLevels[modelLevel->level];
        level.expanded = isExpanded(levelIndex);

        int nGroups = mModel->rowCount(levelIndex);
        for (int j = 0; j < nGroups; j++) {
            const QModelIndex groupIndex = mModel->index(j, 0, levelIndex);
            WorldObjectGroup* group = mModel->toGroup(groupIndex);
            if (group == nullptr)
                continue; // "No Group"

            if (isExpanded(groupIndex))
                level.expandedGroups.insert(group);
            else
                level.expandedGroups.remove(group);
        }
    }
}

void ObjectsView::restoreExpandedLevels()
{
    blockSignals(true);
    mModel->blockSignals(true);
    bool animated = isAnimated();
    setAnimated(false);

    // This prevents updating the layout every time setExpanded() is called below - huge performance boost.
    scheduleDelayedItemsLayout();

    int nLevels = mModel->rowCount();
    for (int i = 0; i < nLevels; i++) {
        const QModelIndex levelIndex = mModel->index(i, 0);
        ObjectsModel::Level* modelLevel = mModel->toLevel(levelIndex);
        Level& level = mExpandedLevels[modelLevel->level];
        setExpanded(levelIndex, level.expanded);

        int nGroups = mModel->rowCount(levelIndex);
        for (int j = 0; j < nGroups; j++) {
            const QModelIndex groupIndex = mModel->index(j, 0, levelIndex);
            WorldObjectGroup* group = mModel->toGroup(groupIndex);
            if (group == nullptr)
                continue; // "No Group"

            // SLOW
            bool expanded = level.expandedGroups.contains(group);
            setExpanded(groupIndex, expanded);
        }
    }

    setAnimated(animated);
    mModel->blockSignals(false);
    blockSignals(false);
}

/////

// FIXME: Are these global objects safe?
QString ObjectsModel::ObjectsModelMimeType(QLatin1String("application/x-pzworlded-object"));

ObjectsModel::ObjectsModel(QObject *parent)
    : QAbstractItemModel(parent)
    , mCell(0)
    , mCellDoc(0)
    , mWorldDoc(0)
    , mRootItem(0)
    , mSynching(false)
    , mObjectPixmap(QLatin1String(":/images/16x16/layer-object.png"))
{
}

ObjectsModel::~ObjectsModel()
{
    qDeleteAll(mLevels);
    delete mRootItem;
}

QModelIndex ObjectsModel::index(int row, int column, const QModelIndex &parent) const
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

ObjectsModel::Level *ObjectsModel::toLevel(const QModelIndex &index) const
{
    if (index.isValid())
        return toItem(index)->level;
    return 0;
}

WorldObjectGroup *ObjectsModel::toGroup(const QModelIndex &index) const
{
    if (index.isValid())
        return toItem(index)->group;
    return 0;
}

WorldCellObject *ObjectsModel::toObject(const QModelIndex &index) const
{
    if (index.isValid())
        return toItem(index)->object;
    return 0;
}

QModelIndex ObjectsModel::parent(const QModelIndex &index) const
{
    if (Item *item = toItem(index)) {
        if (item->parent != mRootItem) {
            Item *grandParent = item->parent->parent;
            return createIndex(grandParent->children.indexOf(item->parent), 0, item->parent);
        }
    }
    return QModelIndex();
}

int ObjectsModel::rowCount(const QModelIndex &parent) const
{
    if (!mRootItem)
        return 0;
    if (!parent.isValid())
        return mRootItem->children.size();
    if (Item *item = toItem(parent))
        return item->children.size();
    return 0;
}

int ObjectsModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 2;
}

QVariant ObjectsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
        case 0: return tr("Name");
        case 1: return tr("Type");
        }
    }
    return QVariant();
}

QVariant ObjectsModel::data(const QModelIndex &index, int role) const
{
    if (Level *level = toLevel(index)) {
        if (index.column())
            return QVariant();
        switch (role) {
        case Qt::CheckStateRole: {
            if (mCellDoc) {
                bool visible = mCellDoc->isObjectLevelVisible(level->level);
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
    if (WorldObjectGroup *og = toGroup(index)) {
        if (index.column())
            return QVariant();
        switch (role) {
        case Qt::CheckStateRole: {
            if (mCellDoc) {
                int level = toItem(index)->parent->level->level;
                bool visible = mCellDoc->isObjectGroupVisible(og, level);
                return visible ? Qt::Checked : Qt::Unchecked;
            }
            return QVariant();
        }
        case Qt::DisplayRole:
            return og->name().isEmpty() ? QLatin1String("<no group>") : og->name();
        default:
            return QVariant();
        }
    }
    if (WorldCellObject *obj = toObject(index)) {
        switch (role) {
        case Qt::CheckStateRole: {
            if (index.column() == 0 && mCellDoc) {
                // Don't check ObjectItem::isVisible because it changes during CellScene::updateCurrentLevelHighlight()
                bool visible = obj->isVisible();
                return visible ? Qt::Checked : Qt::Unchecked;
            }
            break;
        }
        case Qt::DisplayRole: {
            QString value;
            if (index.column() == typeColumn()) {
                value = obj->type()->name();
                if (value.isEmpty())
                    value = tr("<no type>");
            } else {
                value = obj->name();
                if (value.isEmpty())
                    value = tr("<no name>");
            }
            return value;
        }
        case Qt::EditRole:
            return (index.column() == typeColumn()) ? obj->type()->name() : obj->name();
        case Qt::DecorationRole:
            return QVariant();
        case Qt::ForegroundRole: {
            QString value = (index.column() == typeColumn()) ? obj->type()->name() : obj->name();
            if (value.isEmpty())
                return QApplication::palette().color(QPalette::Disabled, QPalette::Text);
        }
        default:
            return QVariant();
        }
    }
    return QVariant();
}

bool ObjectsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (Level *level = toLevel(index)) {
        switch (role) {
        case Qt::CheckStateRole: {
            if (mCellDoc) {
                Qt::CheckState c = static_cast<Qt::CheckState>(value.toInt());
                mCellDoc->setObjectLevelVisible(level->level, c == Qt::Checked);
                emit dataChanged(index, index); // FIXME: connect to objectLevelVisibilityChanged signal
                return true;
            }
            break;
        }
        }
    }
    if (WorldObjectGroup *og = toGroup(index)) {
        switch (role) {
        case Qt::CheckStateRole: {
            if (mCellDoc) {
                int level = toItem(index)->parent->level->level;
                Qt::CheckState c = static_cast<Qt::CheckState>(value.toInt());
                mCellDoc->setObjectGroupVisible(og, level, c == Qt::Checked);
                emit dataChanged(index, index);
                return true;
            }
        }
        }
    }
    if (WorldCellObject *obj = toObject(index)) {
        switch (role) {
        case Qt::CheckStateRole: {
            if (mCellDoc) {
                Qt::CheckState c = static_cast<Qt::CheckState>(value.toInt());
                mCellDoc->scene()->setObjectVisible(obj, c == Qt::Checked);
                emit dataChanged(index, index);
                return true;
            }
            break;
        }
        case Qt::EditRole: {
            WorldDocument *worldDoc = mWorldDoc ? mWorldDoc : mCellDoc->worldDocument();
            if (index.column() == nameColumn())
                worldDoc->setCellObjectName(obj, value.toString());
            if (index.column() == typeColumn())
                worldDoc->setCellObjectType(obj, value.toString());
            return true;
        }
        }
    }
    return QAbstractItemModel::setData(index, value, role);
}

Qt::ItemFlags ObjectsModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags rc = QAbstractItemModel::flags(index);
    if (index.column() == 0) {
        if (mCellDoc)
            rc |= Qt::ItemIsUserCheckable;
        if (toGroup(index))
            rc |= Qt::ItemIsDropEnabled;
        if (toObject(index))
            rc |= Qt::ItemIsEditable | Qt::ItemIsDragEnabled;
    }
    if (index.column() == 1) {
        if (toObject(index))
            rc |= Qt::ItemIsEditable;
    }
    return rc;
}

Qt::DropActions ObjectsModel::supportedDropActions() const
{
    return /*Qt::CopyAction |*/ Qt::MoveAction;
}

QStringList ObjectsModel::mimeTypes() const
{
    QStringList types;
    types << ObjectsModelMimeType;
    return types;
}

QMimeData *ObjectsModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QByteArray encodedData;

    QDataStream stream(&encodedData, QIODevice::WriteOnly);

    // Keep the objects in display order.  The given list of indexes
    // appears to be in the order the user selected the items.
    QMap<int,int> sorted;
    foreach (const QModelIndex &index, indexes) {
        if (WorldCellObject *obj = toObject(index)) {
            int displayOrder = toItem(index)->flattenedRowInList();
            Q_ASSERT(!sorted.contains(displayOrder));
            int objectIndex = obj->cell()->objects().indexOf(obj);
            sorted[displayOrder] = objectIndex;
        }
    }

    // QMap::values is in ascending key order
    foreach (int objectIndex, sorted.values()) {
        QString text = QString::number(objectIndex);
        stream << text;
    }

    mimeData->setData(ObjectsModelMimeType, encodedData);
    return mimeData;
}

bool ObjectsModel::dropMimeData(const QMimeData *data,
     Qt::DropAction action, int row, int column, const QModelIndex &parent)
 {
    Q_UNUSED(row)
     if (action == Qt::IgnoreAction)
         return true;

     if (!data->hasFormat(ObjectsModelMimeType))
         return false;

     if (column > 0)
         return false;

     Item *parentItem = toItem(parent);
     if (!parentItem || !parentItem->group)
         return false;

     WorldCellObject *insertBefore = 0;
     if (row == -1 || !parentItem->children.size()) {
        // Drop on parent
     } else if (row >= parentItem->children.size()) {
         // Drop after last row
         insertBefore = parentItem->children.last()->object;
     } else {
         WorldCellObject *prevObj = parentItem->children.at(row)->object;
         int index = mCell->objects().indexOf(prevObj) + 1;
         if (index < mCell->objects().size())
             insertBefore = mCell->objects().at(index);
     }

     WorldDocument *worldDoc = mWorldDoc ? mWorldDoc : mCellDoc->worldDocument();

     QByteArray encodedData = data->data(ObjectsModelMimeType);
     QDataStream stream(&encodedData, QIODevice::ReadOnly);
     QList<WorldCellObject*> objects;
     int rows = 0;

     while (!stream.atEnd()) {
         QString text;
         stream >> text;
         objects << mCell->objects().at(text.toInt());
         ++rows;
     }

     // Note: parentItem may be destroyed by setCell()
     int level = parentItem->parent->level->level; // ouch
     int count = objects.size();
     worldDoc->undoStack()->beginMacro(tr("Reorder %1 Object%2").arg(count)
                                       .arg((count > 1) ? QLatin1String("s") : QLatin1String("")));
     foreach (WorldCellObject *obj, objects) {
         worldDoc->reorderCellObject(obj, insertBefore);
         insertBefore = obj;
         if (parentItem->group != obj->group())
             worldDoc->setCellObjectGroup(obj, parentItem->group);
         if (level != obj->level())
            worldDoc->setObjectLevel(obj, level);
     }
     worldDoc->undoStack()->endMacro();
     return true;
}

QModelIndex ObjectsModel::index(ObjectsModel::Level *level) const
{
    Item *item = toItem(level);
    int row = item->parent->children.indexOf(item);
    return createIndex(row, 0, item);
}

QModelIndex ObjectsModel::index(WorldCellObject *obj, int column) const
{
    Item *item = toItem(obj);
    int row = item->parent->children.indexOf(item);
    return createIndex(row, column, item);
}

ObjectsModel::Item *ObjectsModel::toItem(const QModelIndex &index) const
{
    if (index.isValid())
        return static_cast<Item*>(index.internalPointer());
    return 0;
}

ObjectsModel::Item *ObjectsModel::toItem(Level *level) const
{
    return toItem(level->level);
}

ObjectsModel::Item *ObjectsModel::toItem(int level) const
{
    if (!mRootItem)
        return 0;
    foreach (Item *item, mRootItem->children)
        if (item->level->level == level)
            return item;
    return 0;
}

ObjectsModel::Item *ObjectsModel::toItem(int level, WorldObjectGroup *og) const
{
    if (Item *levelItem = toItem(level)) {
        foreach (Item *item, levelItem->children)
            if (item->group == og)
                return item;
    }
    return 0;
}

ObjectsModel::Item *ObjectsModel::toItem(WorldCellObject *obj) const
{
    Item *parent = toItem(obj->level(), obj->group());
    foreach (Item *item, parent->children)
        if (item->object == obj)
            return item;
    return 0;
}

QModelIndex ObjectsModel::index(Item *item) const
{
    int row = item->parent->children.indexOf(item);
    return createIndex(row, 0, item);
}

void ObjectsModel::addItemToModel(ObjectsModel::Item *parentItem,
                                  int indexInParent,
                                  ObjectsModel::Item *item)
{
    QModelIndex parent = this->index(parentItem);
    beginInsertRows(parent, indexInParent, indexInParent);
    parentItem->children.insert(indexInParent, item);
    endInsertRows();
}

void ObjectsModel::removeItemFromModel(ObjectsModel::Item *item)
{
    if (item->children.size()) {
        QModelIndex index = this->index(item);
        beginRemoveRows(index, 0, item->children.size() - 1);
        qDeleteAll(item->children);
        item->children.clear();
        endRemoveRows();
    }

    Item *parentItem = item->parent;
    QModelIndex parent = this->index(parentItem);
    int row = parentItem->children.indexOf(item);
    beginRemoveRows(parent, row, row);
    delete parentItem->children.takeAt(row);
    endRemoveRows();
}

void ObjectsModel::setModelData()
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
        foreach (WorldCellObject *obj, mCell->objects())
            maxLevel = qMax(obj->level(), maxLevel);
    }

    mRootItem = new Item();

    for (int level = 0; level <= maxLevel; level++)
        mLevels += new Level(level);

    foreach (Level *level, mLevels) {
        Item *levelItem = new Item(mRootItem, 0, level);
        foreach (WorldObjectGroup *og, mCell->world()->objectGroups())
            new Item(levelItem, 0, og);
    }

    foreach (WorldCellObject *obj, mCell->objects()) {
        Item *parent = toItem(obj->level(), obj->group());
        new Item(parent, 0, obj);
    }

    endResetModel();
}

void ObjectsModel::setDocument(Document *doc)
{
    if (mWorldDoc)
        mWorldDoc->disconnect(this);
    if (mCellDoc) {
        mCellDoc->worldDocument()->disconnect(this);
        mCellDoc->disconnect(this);
    }

    mWorldDoc = doc ? doc->asWorldDocument() : 0;
    mCellDoc = doc ? doc->asCellDocument() : 0;

    WorldCell *cell = 0;
    WorldDocument *worldDoc = 0;

    if (mWorldDoc) {
        worldDoc = mWorldDoc;
        if (mWorldDoc->selectedCellCount() == 1)
            cell = mWorldDoc->selectedCells().first();
    }
    if (mCellDoc) {
        cell = mCellDoc->cell();
        worldDoc = mCellDoc->worldDocument();

        connect(mCellDoc, &CellDocument::layerGroupAdded,
                this, &ObjectsModel::layerGroupAdded);
    }

    if (worldDoc) {
        connect(worldDoc, &WorldDocument::objectTypeNameChanged,
                this, &ObjectsModel::objectTypeNameChanged);

        connect(worldDoc, &WorldDocument::objectGroupAdded,
                this, &ObjectsModel::objectGroupAdded);
        connect(worldDoc, &WorldDocument::objectGroupAboutToBeRemoved,
                this, &ObjectsModel::objectGroupAboutToBeRemoved);
        connect(worldDoc, &WorldDocument::objectGroupNameChanged,
                this, &ObjectsModel::objectGroupNameChanged);
        connect(worldDoc, &WorldDocument::objectGroupReordered,
                this, &ObjectsModel::objectGroupReordered);

        connect(worldDoc, &WorldDocument::cellMapFileAboutToChange,
                this, &ObjectsModel::cellContentsAboutToChange);
        connect(worldDoc, &WorldDocument::cellMapFileChanged,
                this, &ObjectsModel::cellContentsChanged);
        connect(worldDoc, &WorldDocument::cellContentsAboutToChange,
                this, &ObjectsModel::cellContentsAboutToChange);
        connect(worldDoc, &WorldDocument::cellContentsChanged,
                this, &ObjectsModel::cellContentsChanged);

        connect(worldDoc, &WorldDocument::cellObjectAdded,
                this, &ObjectsModel::cellObjectAdded);
        connect(worldDoc, &WorldDocument::cellObjectAboutToBeRemoved,
                this, &ObjectsModel::cellObjectAboutToBeRemoved);
        connect(worldDoc, &WorldDocument::cellObjectNameChanged,
                this, &ObjectsModel::cellObjectXXXXChanged);
        connect(worldDoc, &WorldDocument::cellObjectTypeChanged,
                this, &ObjectsModel::cellObjectXXXXChanged);
        connect(worldDoc, &WorldDocument::cellObjectGroupAboutToChange,
                this, &ObjectsModel::cellObjectGroupAboutToChange);
        connect(worldDoc, &WorldDocument::cellObjectGroupChanged,
                this, &ObjectsModel::cellObjectGroupChanged);
        connect(worldDoc, &WorldDocument::objectLevelAboutToChange,
                this, &ObjectsModel::objectLevelAboutToChange);
        connect(worldDoc, &WorldDocument::objectLevelChanged,
                this, &ObjectsModel::objectLevelChanged);
        connect(worldDoc, &WorldDocument::cellObjectReordered,
                this, &ObjectsModel::cellObjectReordered);
    }

    setCell(cell);
}

void ObjectsModel::setCell(WorldCell *cell)
{
    mCell = cell;
    mSynching = true;
    setModelData();
    mSynching = false;

    emit synched();
}

void ObjectsModel::objectTypeNameChanged(ObjectType *objType)
{
    foreach (WorldCellObject *obj, mCell->objects()) {
        if (obj->type() == objType)
            cellObjectXXXXChanged(obj);
    }
}

void ObjectsModel::objectGroupAdded(int index)
{
    if (!mCell)
        return;
    WorldObjectGroup *og = mCell->world()->objectGroups().at(index);
    index = mCell->world()->objectGroups().size() - index - 1;
    foreach (Item *parent, mRootItem->children) {
        Item *item = new Item(parent, index, og);
        parent->children.removeAt(index);
        addItemToModel(parent, index, item);
    }
}

void ObjectsModel::objectGroupAboutToBeRemoved(int index)
{
    if (!mCell)
        return;
    index = mCell->world()->objectGroups().size() - index - 1;
    foreach (Item *parentItem, mRootItem->children)
        removeItemFromModel(parentItem->children.at(index));
}

void ObjectsModel::objectGroupNameChanged(WorldObjectGroup *og)
{
    if (!mCell)
        return;
    int index = mCell->world()->objectGroups().indexOf(og);
    index = mCell->world()->objectGroups().size() - index - 1;
    foreach (Item *parentItem, mRootItem->children) {
        Item *item = parentItem->children.at(index);
        emit dataChanged(this->index(item), this->index(item));
    }
}

void ObjectsModel::objectGroupReordered(int index)
{
    if (!mCell)
        return;
    WorldObjectGroup *og = mCell->world()->objectGroups().at(index);
    foreach (Item *parentItem, mRootItem->children) {
        int oldRow = parentItem->rowOf(og);
        int newRow = mCell->world()->objectGroups().size() - index - 1;
        QModelIndex parent = this->index(parentItem);
        int destRow = newRow;
        if (destRow > oldRow)
            ++destRow;
        beginMoveRows(parent, oldRow, oldRow, parent, destRow);
        Item *item = parentItem->children.takeAt(oldRow);
        parentItem->children.insert(newRow, item);
        endMoveRows();
    }
}

void ObjectsModel::cellContentsAboutToChange(WorldCell *cell)
{
    if (cell != mCell)
        return;

    beginResetModel();

    qDeleteAll(mLevels);
    mLevels.clear();

    delete mRootItem;
    mRootItem = 0;

    endResetModel();

    mSynching = true; // ignore layerGroupAdded
}

void ObjectsModel::cellContentsChanged(WorldCell *cell)
{
    if (cell != mCell)
        return;
    mSynching = false; // stop ignoring layerGroupAdded
    setCell(mCell);
}

void ObjectsModel::cellObjectAdded(WorldCell *cell, int index)
{
    if (cell != mCell)
        return;

    WorldCellObject *obj = cell->objects().at(index);
    Item *parent = toItem(obj->level(), obj->group());
    WorldCellObject *prevObj = 0;
    --index;
    while (index >= 0) {
        prevObj = obj->cell()->objects().at(index);
        if (obj->level() == prevObj->level() &&
                obj->group() == prevObj->group())
            break;
        --index;
    }
    if (index >= 0) {
        Item *prevItem = parent->findChild(prevObj);
        index = parent->children.indexOf(prevItem);
    } else {
        index = parent->children.size();
    }
    Item *item = new Item(parent, index, obj);
    parent->children.removeAt(index);
    addItemToModel(parent, index, item);
}

void ObjectsModel::cellObjectAboutToBeRemoved(WorldCell *cell, int index)
{
    if (cell != mCell)
        return;
    WorldCellObject *obj = cell->objects().at(index);
    if (Item *item = toItem(obj))
        removeItemFromModel(item);
}

void ObjectsModel::cellObjectXXXXChanged(WorldCellObject *obj)
{
    if (obj->cell() != mCell)
        return;
    if (toItem(obj)) {
        QModelIndex index0 = this->index(obj);
        QModelIndex index1 = this->index(obj, 1);
        emit dataChanged(index0, index1);
    }
}

void ObjectsModel::cellObjectGroupAboutToChange(WorldCellObject *obj)
{
    if (obj->cell() != mCell)
        return;
    if (Item *item = toItem(obj))
        removeItemFromModel(item);
}

void ObjectsModel::cellObjectGroupChanged(WorldCellObject *obj)
{
    if (obj->cell() != mCell)
        return;
    int index = obj->cell()->objects().indexOf(obj);
    cellObjectAdded(obj->cell(), index);
}

void ObjectsModel::objectLevelAboutToChange(WorldCellObject *obj)
{
    if (obj->cell() != mCell)
        return;
    if (Item *item = toItem(obj))
        removeItemFromModel(item);
}

void ObjectsModel::objectLevelChanged(WorldCellObject *obj)
{
    if (obj->cell() != mCell)
        return;
    int index = obj->cell()->objects().indexOf(obj);
    cellObjectAdded(obj->cell(), index);
}

void ObjectsModel::cellObjectReordered(WorldCellObject *obj)
{
    if (obj->cell() != mCell)
        return;
    if (Item *item = toItem(obj))
        removeItemFromModel(item);
    cellObjectAdded(obj->cell(), mCell->objects().indexOf(obj));
}

void ObjectsModel::layerGroupAdded(int level)
{
    Q_UNUSED(level)
    if (mSynching)
        return;
    if (mCellDoc)
        setCell(mCell);
}

bool ObjectsModel::isEditable(const QModelIndex &index) const
{
    return toObject(index) != 0;
}

bool ObjectsModel::isDeletable(const QModelIndex &index) const
{
    return toObject(index) != 0;
}
