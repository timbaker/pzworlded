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
#include "worldcell.h"
#include "worlddocument.h"

#include "maprenderer.h"
#include "tilelayer.h"

#include <QApplication>
#include <QDebug>
#include <QEvent>
#include <QFileInfo>
#include <QHeaderView>
#include <QMimeData>
#include <QMouseEvent>
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

    connect(mView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(selectionChanged()));
    connect(mView, SIGNAL(clicked(QModelIndex)),
            SLOT(itemClicked(QModelIndex)));
    connect(mView, SIGNAL(doubleClicked(QModelIndex)),
            SLOT(itemDoubleClicked(QModelIndex)));
    connect(mView, SIGNAL(trashItem(QModelIndex)), SLOT(trashItem(QModelIndex)));

    // Workaround since a tabbed dockwidget that is not currently visible still
    // returns true for isVisible()
    connect(this, SIGNAL(visibilityChanged(bool)),
            mView, SLOT(setVisible(bool)));
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

    QModelIndexList selection = mView->selectionModel()->selectedIndexes();
    if (selection.size() == mView->model()->columnCount()) {
        int level = -1;
        if (ObjectsModel::Level *levelPtr = mView->model()->toLevel(selection.first()))
            level = levelPtr->level;

        // Center the view on the selected WorldCellObject
        if (WorldCellObject *obj = mView->model()->toObject(selection.first())) {
#if 1
            mCellDoc->view()->ensureVisible(mCellDoc->scene()->itemForObject(obj));
#else
            QPointF scenePos = mCellDoc->scene()->renderer()->tileToPixelCoords(obj->x() + obj->width() / 2.0,
                                                                                obj->y() + obj->height() / 2.0,
                                                                                obj->level());
            mCellDoc->view()->centerOn(scenePos);
#endif
            level = obj->level();
        }

        // Set the CellDocument's current layer
        if (level >= 0)
            mCellDoc->setCurrentLevel(level);
    }

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

#include "world.h"

#include <QComboBox>
#include <QPixmap>

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

    if ((index.column() == 0) && index.parent().isValid() && (option.state & QStyle::State_MouseOver)) {
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
        connect(editor, SIGNAL(activated(int)),
                mView, SLOT(closeComboBoxEditor()));
        return editor;
    }
    QWidget *widget = QStyledItemDelegate::createEditor(parent, option, index);
#if 0
    if (QLineEdit *editor = qobject_cast<QLineEdit*>(widget)) {
        if (index.column() == 2)
            editor->setValidator(new QIntValidator(0, 30, editor));
    }
#endif
    return widget;
}

void ObjectsViewDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    if (index.column() == mModel->typeColumn()) {
        if (WorldCellObject *obj = mModel->toObject(index)) {
            QComboBox *comboBox = static_cast<QComboBox*>(editor);
            comboBox->clear();
            QStringList types = obj->cell()->world()->objectTypes().names().mid(1);
            comboBox->addItem(tr("<unspecified>"));
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
#if 0
    if (size.height() < 22)
        size.setHeight(22);
#endif
    return size;
}

QRect ObjectsViewDelegate::closeButtonRect(const QRect &itemViewRect) const
{
    QRect closeRect(itemViewRect.x()-40+2, itemViewRect.y(), mTrashPixmap.width(), itemViewRect.height());
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

    setItemDelegate(new ObjectsViewDelegate(this, this));

    setEditTriggers(QAbstractItemView::SelectedClicked
                    | QAbstractItemView::DoubleClicked);

    setModel(mModel);

    header()->resizeSection(0, 180);
    header()->setResizeMode(1, QHeaderView::ResizeToContents);

    connect(mModel, SIGNAL(synched()), SLOT(modelSynched()));
}

void ObjectsView::mousePressEvent(QMouseEvent *event)
{
    QModelIndex index = indexAt(event->pos());
    if (index.isValid()) {
        // Prevent drag-and-drop starting when clicking on an unselected item.
        setDragEnabled(selectionModel()->isSelected(index));

        if (mModel->toObject(index)) {
#if 0
            if (index.column() == mModel->typeColumn()) {
                event->accept();
                setCurrentIndex(index);
                edit(index);
                return;
            }
#endif
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

    mWorldDoc = doc ? doc->asWorldDocument() : 0;
    mCellDoc = doc ? doc->asCellDocument() : 0;

    mModel->setDocument(doc);

    if (mWorldDoc) {
        connect(mWorldDoc, SIGNAL(selectedCellsChanged()),
                SLOT(selectedCellsChanged()));
    }
    if (mCellDoc) {
        connect(mCellDoc, SIGNAL(selectedObjectsChanged()),
                SLOT(selectedObjectsChanged()));
    }
}

bool ObjectsView::synchingSelection() const
{
    return mSynchingSelection || mModel->synching();
}

void ObjectsView::selectedCellsChanged()
{
    if (mWorldDoc->selectedCellCount() == 1)
        mModel->setCell(mWorldDoc->selectedCells().first());
    else
        mModel->setCell(0);
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
#if 0
    QComboBox *editor = static_cast<QComboBox*>(sender());
    editor->disconnect(this);
    qDebug() << "closeComboBoxEditor";
#endif
    QModelIndex index = currentIndex();
    currentChanged( index, index );
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
    connect(MapManager::instance(), SIGNAL(mapMagicallyGotMoreLayers(Tiled::Map*)),
            SLOT(mapMagicallyGotMoreLayers(Tiled::Map*)));
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
    if (WorldCellObject *obj = toObject(index)) {
        switch (role) {
        case Qt::CheckStateRole: {
            if (index.column() == 0 && mCellDoc) {
#if 1
                // Don't check ObjectItem::isVisible because it changes during CellScene::updateCurrentLevelHighlight()
                bool visible = obj->isVisible();
#else
                ObjectItem *sceneItem = mCellDoc->scene()->itemForObject(obj);
                bool visible = sceneItem ? sceneItem->isVisible() : false;
#endif
                return visible ? Qt::Checked : Qt::Unchecked;
            }
            break;
        }
        case Qt::DisplayRole: {
            QString value = (index.column() == typeColumn()) ? obj->type()->name() : obj->name();
            if (value.isEmpty())
                value = tr("<unspecified>");
            return value;
        }
        case Qt::EditRole:
            return (index.column() == typeColumn()) ? obj->type()->name() : obj->name();
        case Qt::DecorationRole:
#if 0
            if (index.column() == nameColumn())
                return mObjectPixmap;
#endif
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
        if (toLevel(index))
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

    foreach (const QModelIndex &index, indexes) {
        if (WorldCellObject *obj = toObject(index)) {
            QString text = QString::number(obj->cell()->objects().indexOf(obj));
            stream << text;
        }
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

#if 1
     Item *parentItem = toItem(parent);
     if (!parentItem || !parentItem->level)
         return false;

     WorldDocument *worldDoc = mWorldDoc ? mWorldDoc : mCellDoc->worldDocument();
#else
     int beginRow;
     if (row != -1)
         beginRow = row;
     else if (parent.isValid())
         beginRow = parent.row();
     else
         beginRow = rowCount(QModelIndex());
#endif

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
     int level = parentItem->level->level;
     int count = objects.size();
     if (count > 1)
         worldDoc->undoStack()->beginMacro(tr("Change %1 Objects' Level").arg(count));
     foreach (WorldCellObject *obj, objects)
         worldDoc->setObjectLevel(obj, level);
     if (count > 1)
         worldDoc->undoStack()->endMacro();

     return true;
}

QModelIndex ObjectsModel::index(ObjectsModel::Level *level) const
{
    Item *item = toItem(level);
    int row = item->parent->children.indexOf(item);
    return createIndex(row, 0, item);
}

QModelIndex ObjectsModel::index(WorldCellObject *lot, int column) const
{
    Item *item = toItem(lot);
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

ObjectsModel::Item *ObjectsModel::toItem(WorldCellObject *obj) const
{
    Level *level = mLevels[obj->level()];
    Item *parent = toItem(level);
    foreach (Item *item, parent->children)
        if (item->object == obj)
            return item;
    return 0;
}

void ObjectsModel::setModelData()
{
    qDeleteAll(mLevels);
    mLevels.clear();

    delete mRootItem;
    mRootItem = 0;

    if (!mCell) {
        reset();
        return;
    }

    int maxLevel;
    if (mCellDoc) {
        maxLevel = mCellDoc->scene()->map()->maxLevel();
    } else {
        maxLevel = 0; // Could use MapInfo to get maxLevel
        foreach (WorldCellObject *obj, mCell->objects())
            maxLevel = qMax(obj->level(), maxLevel);
    }

    mRootItem = new Item();

    for (int level = 0; level <= maxLevel; level++)
        mLevels += new Level(level);

    foreach (Level *level, mLevels)
        new Item(mRootItem, 0, level);

    foreach (WorldCellObject *obj, mCell->objects()) {
        Item *parent = toItem(obj->level());
        parent->level->objects.insert(0, obj);
        new Item(parent, 0, obj);
    }

    reset();
}

void ObjectsModel::setDocument(Document *doc)
{
    if (mWorldDoc)
        mWorldDoc->disconnect(this);
    if (mCellDoc)
        mCellDoc->disconnect(this);

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
    }

    if (worldDoc) {
        connect(worldDoc, SIGNAL(objectTypeNameChanged(ObjectType*)),
                SLOT(objectTypeNameChanged(ObjectType*)));

        connect(worldDoc, SIGNAL(cellMapFileAboutToChange(WorldCell*)),
                SLOT(cellContentsAboutToChange(WorldCell*)));
        connect(worldDoc, SIGNAL(cellMapFileChanged(WorldCell*)),
                SLOT(cellContentsChanged(WorldCell*)));
        connect(worldDoc, SIGNAL(cellContentsAboutToChange(WorldCell*)),
                SLOT(cellContentsAboutToChange(WorldCell*)));
        connect(worldDoc, SIGNAL(cellContentsChanged(WorldCell*)),
                SLOT(cellContentsChanged(WorldCell*)));

        connect(worldDoc, SIGNAL(cellObjectAdded(WorldCell*,int)),
                SLOT(cellObjectAdded(WorldCell*,int)));
        connect(worldDoc, SIGNAL(cellObjectAboutToBeRemoved(WorldCell*,int)),
                SLOT(cellObjectAboutToBeRemoved(WorldCell*,int)));
        connect(worldDoc, SIGNAL(cellObjectNameChanged(WorldCellObject*)),
                SLOT(cellObjectXXXXChanged(WorldCellObject*)));
        connect(worldDoc, SIGNAL(cellObjectTypeChanged(WorldCellObject*)),
                SLOT(cellObjectXXXXChanged(WorldCellObject*)));
        connect(worldDoc, SIGNAL(objectLevelChanged(WorldCellObject*)),
                SLOT(objectLevelChanged(WorldCellObject*)));
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

void ObjectsModel::cellContentsAboutToChange(WorldCell *cell)
{
    if (cell != mCell)
        return;

    qDeleteAll(mLevels);
    mLevels.clear();

    delete mRootItem;
    mRootItem = 0;

    reset();

    mSynching = true; // ignore mapMagicallyGotMoreLayers
}

void ObjectsModel::cellContentsChanged(WorldCell *cell)
{
    if (cell != mCell)
        return;
    mSynching = false; // stop ignoring mapMagicallyGotMoreLayers
    setCell(mCell);
}

void ObjectsModel::cellObjectAdded(WorldCell *cell, int index)
{
    Q_UNUSED(index)
    if (cell == mCell)
        setCell(mCell); // lazy, just reset the whole list
}

void ObjectsModel::cellObjectAboutToBeRemoved(WorldCell *cell, int index)
{
    if (cell != mCell)
        return;
    WorldCellObject *obj = cell->objects().at(index);
    if (Item *item = toItem(obj)) {
        Item *parentItem = item->parent;
        QModelIndex parent = this->index(parentItem->level);
        int row = parentItem->children.indexOf(item);
        beginRemoveRows(parent, row, row);
        delete parentItem->children.takeAt(row);
        endRemoveRows();
    }
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

void ObjectsModel::objectLevelChanged(WorldCellObject *obj)
{
    if (obj->cell() == mCell)
        setCell(mCell); // lazy, just reset the whole list
}

void ObjectsModel::mapMagicallyGotMoreLayers(Map *map)
{
    if (mSynching)
        return;
    if (mCellDoc && mCellDoc->scene()->mapComposite()->map() == map)
        setCell(mCell);
}
