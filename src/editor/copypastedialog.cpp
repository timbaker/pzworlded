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

#include "copypastedialog.h"
#include "ui_copypastedialog.h"

#include "celldocument.h"
#include "world.h"
#include "worldcell.h"
#include "worlddocument.h"

#include <QFileInfo>

/////

class CopyPasteDialog::Item
{
public:
    enum Type
    {
        Unknown,
        Root,
        Cell,
        Level,
        Lot,
        MapType,
        Object,
        ObjectGroup,
        PropertyType,
        TemplateType
    };

    Item()
        : mParent(0)
        , mType(Root)
        , mChecked(true)
    {
    }

    Item(Type type)
        : mParent(0)
        , mType(type)
        , mChecked(true)
    {
    }

    virtual void insertChild(int index, Item *child)
    {
        if (index == -1)
            index = mChildren.size();
        mChildren.insert(index, child);
    }

    ~Item()
    {
        qDeleteAll(mChildren);
    }

    void setViewItem(QTreeWidgetItem *item)
    {
        Q_ASSERT(!item || !mViewItem);
        mViewItem = item;
        if (!mViewItem) {
            foreach (Item *child, mChildren)
                child->setViewItem(0);
        }
    }

    QList<Item*> children(Type type = Unknown)
    {
        if (type == Unknown)
            return mChildren;
        QList<Item*> result;
        foreach (Item *child, mChildren)
            if (child->mType == type)
                result += child;
        return result;
    }

    typedef CopyPasteDialog::CellItem CellItem;
    typedef CopyPasteDialog::LevelItem LevelItem;
    typedef CopyPasteDialog::LotItem LotItem;
    typedef CopyPasteDialog::MapTypeItem MapTypeItem;
    typedef CopyPasteDialog::ObjectGroupItem ObjectGroupItem;
    typedef CopyPasteDialog::ObjectItem ObjectItem;
    typedef CopyPasteDialog::PropertyItem PropertyItem;
    typedef CopyPasteDialog::TemplateItem TemplateItem;

    CellItem *asCellItem();
    LevelItem *asLevelItem();
    LotItem *asLotItem();
    MapTypeItem *asMapItem();
    ObjectItem *asObjectItem();
    ObjectGroupItem *asObjectGroupItem();
    PropertyItem *asPropertyItem();
    TemplateItem *asTemplateItem();

    Item *mParent;
    QList<Item*> mChildren;
    Type mType;
    bool mChecked;
    QTreeWidgetItem *mViewItem;
};

#include <QMetaType>
Q_DECLARE_METATYPE(CopyPasteDialog::Item*)

class CopyPasteDialog::CellItem : public CopyPasteDialog::Item
{
public:
    CellItem(WorldCell *cell)
        : Item(Cell)
        , mCell(cell)
    {
    }

    virtual void insertChild(int index, Item *child);

    LevelItem *itemForLevel(int level);

    WorldCell *mCell;
    QMap<int,LevelItem*> mLevels;
};

class CopyPasteDialog::LevelItem : public CopyPasteDialog::Item
{
public:
    LevelItem(int z)
        : Item(Level)
        , mZ(z)
    {
    }

    virtual void insertChild(int index, Item *child);

    ObjectGroupItem *itemForGroup(WorldObjectGroup *og);

    int mZ;
    QList<LotItem*> mLots;
    QList<ObjectGroupItem*> mObjectGroups;
    QMap<WorldObjectGroup*,ObjectGroupItem*> mGroupToItem;
};

class CopyPasteDialog::LotItem : public CopyPasteDialog::Item
{
public:
    LotItem(WorldCellLot *lot)
        : Item(Lot)
        , mLot(lot)
    {

    }

    WorldCellLot *mLot;
};

class CopyPasteDialog::MapTypeItem : public CopyPasteDialog::Item
{
public:
    MapTypeItem(const QString &mapName)
        : Item(MapType)
        , mName(mapName)
    {
    }

    QString mName;
};

class CopyPasteDialog::ObjectItem : public CopyPasteDialog::Item
{
public:
    ObjectItem(WorldCellObject *obj)
        : Item(Object)
        , mObject(obj)
    {

    }

    WorldCellObject *mObject;
};

class CopyPasteDialog::ObjectGroupItem : public CopyPasteDialog::Item
{
public:
    ObjectGroupItem(WorldObjectGroup *og)
        : Item(ObjectGroup)
        , mGroup(og)
    {
    }

    virtual void insertChild(int index, Item *child)
    {
        Item::insertChild(index, child);
        if (ObjectItem *item = child->asObjectItem())
            mObjects.insert(0, item);
    }

    WorldObjectGroup *mGroup;
    QList<ObjectItem*> mObjects;
};

class CopyPasteDialog::PropertyItem : public CopyPasteDialog::Item
{
public:
    PropertyItem(Property *p)
        : Item(PropertyType)
        , mProperty(p)
    {
    }

    Property *mProperty;
};

class CopyPasteDialog::TemplateItem : public CopyPasteDialog::Item
{
public:
    TemplateItem(PropertyTemplate *pt)
        : Item(TemplateType)
        , mTemplate(pt)
    {
    }

    PropertyTemplate *mTemplate;
};

//

CopyPasteDialog::CellItem *CopyPasteDialog::Item::asCellItem()
{
    return (mType == Cell) ? static_cast<CellItem*>(this) : 0;
}

CopyPasteDialog::LevelItem *CopyPasteDialog::Item::asLevelItem()
{
    return (mType == Level) ? static_cast<LevelItem*>(this) : 0;
}

CopyPasteDialog::LotItem *CopyPasteDialog::Item::asLotItem()
{
    return (mType == Lot) ? static_cast<LotItem*>(this) : 0;
}

CopyPasteDialog::Item::MapTypeItem *CopyPasteDialog::Item::asMapItem()
{
    return (mType == MapType) ? static_cast<MapTypeItem*>(this) : 0;
}

CopyPasteDialog::ObjectItem *CopyPasteDialog::Item::asObjectItem()
{
    return (mType == Object) ? static_cast<ObjectItem*>(this) : 0;
}

CopyPasteDialog::Item::ObjectGroupItem *CopyPasteDialog::Item::asObjectGroupItem()
{
    return (mType == ObjectGroup) ? static_cast<ObjectGroupItem*>(this) : 0;
}

CopyPasteDialog::Item::PropertyItem *CopyPasteDialog::Item::asPropertyItem()
{
    return (mType == PropertyType) ? static_cast<PropertyItem*>(this) : 0;
}

CopyPasteDialog::Item::TemplateItem *CopyPasteDialog::Item::asTemplateItem()
{
    return (mType == TemplateType) ? static_cast<TemplateItem*>(this) : 0;
}

//

void CopyPasteDialog::CellItem::insertChild(int index, Item *child)
{
    Item::insertChild(index, child);

    if (LevelItem *levelItem = child->asLevelItem())
        mLevels[levelItem->mZ] = levelItem;
}

CopyPasteDialog::LevelItem *CopyPasteDialog::CellItem::itemForLevel(int level)
{
    if (!mLevels.contains(level))
        insertChild(-1, new LevelItem(level));
    return mLevels[level];
}

//

void CopyPasteDialog::LevelItem::insertChild(int index, Item *child)
{
    Item::insertChild(index, child);
    if (LotItem *lotItem = child->asLotItem())
        mLots.insert(0, lotItem);
    if (ObjectGroupItem *ogItem = child->asObjectGroupItem()) {
        mObjectGroups.insert(0, ogItem);
        mGroupToItem[ogItem->mGroup] = ogItem;
    }
}

CopyPasteDialog::Item::ObjectGroupItem *CopyPasteDialog::LevelItem::itemForGroup(WorldObjectGroup *og)
{
    if (!mGroupToItem.contains(og))
        insertChild(0, new ObjectGroupItem(og));
    return mGroupToItem[og];
}

/////

CopyPasteDialog::CopyPasteDialog(WorldDocument *worldDoc, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CopyPasteDialog)
    , mWorldDoc(worldDoc)
    , mCellDoc(0)
    , mWorld(worldDoc->world())
{
    ui->setupUi(this);

    QHeaderView *header = ui->cellTree->header();
    header->setStretchLastSection(false);
    header->setResizeMode(0, QHeaderView::ResizeToContents);
    header->setResizeMode(1, QHeaderView::ResizeToContents);

    setup();
}

CopyPasteDialog::CopyPasteDialog(CellDocument *cellDoc, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CopyPasteDialog)
    , mWorldDoc(cellDoc->worldDocument())
    , mCellDoc(cellDoc)
    , mWorld(mWorldDoc->world())
{
    ui->setupUi(this);

    setup();
}

CopyPasteDialog::~CopyPasteDialog()
{
    for (int c = FirstCellCat; c < MaxCellCat; c++)
        delete mRootItem[c];
}

void CopyPasteDialog::setup()
{
    ///// WORLD /////

    connect(ui->worldCat, SIGNAL(currentRowChanged(int)),
            SLOT(worldSelectionChanged(int)));
    connect(ui->worldTree, SIGNAL(itemChanged(QTreeWidgetItem*,int)),
            SLOT(worldItemChanged(QTreeWidgetItem*,int)));
    connect(ui->worldCheckAll, SIGNAL(clicked()), SLOT(worldCheckAll()));
    connect(ui->worldCheckNone, SIGNAL(clicked()), SLOT(worldCheckNone()));

    foreach (PropertyDef *pd, mWorld->propertyDefinitions())
        mCheckedPropertyDefs[pd] = true;

    ///// CELLS /////

    connect(ui->cellCat, SIGNAL(currentRowChanged(int)),
            SLOT(cellCategoryChanged(int)));
    connect(ui->cellTree, SIGNAL(itemChanged(QTreeWidgetItem*,int)),
            SLOT(cellItemChanged(QTreeWidgetItem*,int)));

    mCells = mWorldDoc->selectedCells();
    if (mCells.isEmpty()) {
        foreach (WorldCell *cell, mWorld->cells()) {
            if (!cell->isEmpty())
                mCells += cell;
        }
    }

    mRootItem[CellTemplates] = new Item();
    mRootItem[Lots] = new Item();
    mRootItem[Map] = new Item();
    mRootItem[Objects] = new Item();
    mRootItem[Properties] = new Item();

    foreach (WorldCell *cell, mCells) {

        CellItem *cellItem = new CellItem(cell);
        mRootItem[Map]->insertChild(-1, cellItem);
        cellItem->insertChild(-1, new MapTypeItem(cell->mapFilePath()));

        cellItem = new CellItem(cell);
        mRootItem[Properties]->insertChild(-1, cellItem);
        foreach (Property *p, cell->properties().sorted())
            cellItem->insertChild(-1, new PropertyItem(p));

        cellItem = new CellItem(cell);
        mRootItem[CellTemplates]->insertChild(-1, cellItem);
        foreach (PropertyTemplate *pt, cell->templates().sorted())
            cellItem->insertChild(-1, new TemplateItem(pt));

        cellItem = new CellItem(cell);
        mRootItem[Objects]->insertChild(-1, cellItem);
        foreach (WorldCellObject *obj, cell->objects()) {
            LevelItem *levelItem = cellItem->itemForLevel(obj->level());
            foreach (WorldObjectGroup *og, mWorld->objectGroups())
                (void) levelItem->itemForGroup(og);
            ObjectGroupItem *ogItem = levelItem->itemForGroup(obj->group());
            ogItem->insertChild(0, new ObjectItem(obj));
        }

        cellItem = new CellItem(cell);
        mRootItem[Lots]->insertChild(-1, cellItem);
        foreach (WorldCellLot *lot, cell->lots())
            cellItem->itemForLevel(lot->level())->insertChild(0, new LotItem(lot));
    }
}

///// WORLD /////

void CopyPasteDialog::showPropertyDefs()
{
    ui->worldTree->clear();
    foreach (PropertyDef *pd, mWorld->propertyDefinitions().sorted()) {
        QTreeWidgetItem *item = new QTreeWidgetItem(QStringList() << pd->mName);
        item->setCheckState(0, mCheckedPropertyDefs[pd]
                            ? Qt::Checked : Qt::Unchecked);
        mItemToPropertyDef[item] = pd;
        ui->worldTree->addTopLevelItem(item);
    }
}

void CopyPasteDialog::showTemplates()
{
    ui->worldTree->clear();
    foreach (PropertyTemplate *pt, mWorld->propertyTemplates().sorted()) {
        QTreeWidgetItem *item = new QTreeWidgetItem(QStringList() << pt->mName);
        item->setCheckState(0, Qt::Checked);
        ui->worldTree->addTopLevelItem(item);
    }
}

void CopyPasteDialog::showObjectTypes()
{
    ui->worldTree->clear();
    foreach (ObjectType *ot, mWorld->objectTypes()) {
        if (ot->isNull())
            continue;
        QTreeWidgetItem *item = new QTreeWidgetItem(QStringList() << ot->name());
        item->setCheckState(0, Qt::Checked);
        ui->worldTree->addTopLevelItem(item);
    }
}

void CopyPasteDialog::showObjectGroups()
{
    ui->worldTree->clear();
    foreach (WorldObjectGroup *og, mWorld->objectGroups()) {
        if (og->isNull())
            continue;
        QTreeWidgetItem *item = new QTreeWidgetItem(QStringList() << og->name());
        item->setCheckState(0, Qt::Checked);
        ui->worldTree->insertTopLevelItem(0, item);
    }
}

void CopyPasteDialog::worldSelectionChanged(int index)
{
    mWorldCat = static_cast<enum WorldCat>(index);
    switch (mWorldCat) {
    case PropertyDefs:
        showPropertyDefs();
        break;
    case Templates:
        showTemplates();
        break;
    case ObjectTypes:
        showObjectTypes();
        break;
    case ObjectGroups:
        showObjectGroups();
        break;
    }
}

void CopyPasteDialog::worldItemChanged(QTreeWidgetItem *item, int column)
{
    switch (mWorldCat) {
    case PropertyDefs:
        mCheckedPropertyDefs[mItemToPropertyDef[item]] = item->checkState(column);
        break;
    case Templates:
        break;
    case ObjectTypes:
        break;
    case ObjectGroups:
        break;
    }
}

void CopyPasteDialog::worldCheckAll()
{
    QTreeWidget *view = ui->worldTree;
    switch (mWorldCat) {
    case PropertyDefs:
    case Templates:
    case ObjectTypes:
    case ObjectGroups:
        for (int i = 0; i < view->topLevelItemCount(); i++)
            view->topLevelItem(i)->setCheckState(0, Qt::Checked);
        break;
    }
}

void CopyPasteDialog::worldCheckNone()
{
    QTreeWidget *view = ui->worldTree;
    switch (mWorldCat) {
    case PropertyDefs:
    case Templates:
    case ObjectTypes:
    case ObjectGroups:
        for (int i = 0; i < view->topLevelItemCount(); i++)
            view->topLevelItem(i)->setCheckState(0, Qt::Unchecked);
        break;
    }
}

///// CELLS /////

void CopyPasteDialog::showCellProperties()
{
    QTreeWidget *v = ui->cellTree;
    v->setColumnHidden(1, false);
    v->clear();
    Item *root = mRootItem[Properties];
    root->setViewItem(0);
    foreach (Item *item, root->children()) {
        CellItem *cellItem = item->asCellItem();
        WorldCell *cell = cellItem->mCell;
        if (cell->properties().isEmpty())
            continue;
        addToTree(0, 0, cellItem, tr("Cell %1,%2").arg(cell->x()).arg(cell->y()));

        foreach (Item *item, cellItem->children(Item::PropertyType)) {
            PropertyItem *propertyItem = item->asPropertyItem();
            addToTree(cellItem, -1, propertyItem,
                      propertyItem->mProperty->mDefinition->mName,
                      propertyItem->mProperty->mValue);
        }
    }
    v->expandAll();
}

void CopyPasteDialog::showCellTemplates()
{
    QTreeWidget *v = ui->cellTree;
    v->clear();
    Item *root = mRootItem[CellTemplates];
    root->setViewItem(0);
    foreach (Item *item, root->children()) {
        CellItem *cellItem = item->asCellItem();
        WorldCell *cell = cellItem->mCell;
        if (cell->templates().isEmpty())
            continue;
        addToTree(0, 0, cellItem, tr("Cell %1,%2").arg(cell->x()).arg(cell->y()));

        foreach (Item *item, cellItem->children(Item::TemplateType)) {
            TemplateItem *templateItem = item->asTemplateItem();
            addToTree(cellItem, -1, templateItem, templateItem->mTemplate->mName);
        }
    }
    v->expandAll();
}

void CopyPasteDialog::showCellLots()
{
    QTreeWidget *v = ui->cellTree;
    v->clear();
    Item *root = mRootItem[Lots];
    root->setViewItem(0);
    foreach (Item *item, root->children()) {
        CellItem *cellItem = item->asCellItem();
        WorldCell *cell = cellItem->mCell;
        if (cell->lots().isEmpty())
            continue;
        addToTree(0, 0, cellItem, tr("Cell %1,%2").arg(cell->x()).arg(cell->y()));

        foreach (LevelItem *levelItem, cellItem->mLevels.values()) {
            if (levelItem->mLots.size() == 0)
                continue;
            addToTree(cellItem, 0, levelItem, tr("Level %1").arg(levelItem->mZ));

            foreach (LotItem *lotItem, levelItem->mLots)
                addToTree(levelItem, -1, lotItem,
                          QFileInfo(lotItem->mLot->mapName()).fileName());
        }

    }
    v->expandAll();
}

void CopyPasteDialog::showCellObjects()
{
    QTreeWidget *v = ui->cellTree;
    v->setColumnHidden(1, false);
    v->clear();
    Item *root = mRootItem[Objects];
    root->setViewItem(0);
    foreach (Item *item, root->children()) {
        CellItem *cellItem = item->asCellItem();
        WorldCell *cell = cellItem->mCell;
        if (cell->objects().isEmpty())
            continue;
        addToTree(0, 0, cellItem, tr("Cell %1,%2").arg(cell->x()).arg(cell->y()));

        foreach (LevelItem *levelItem, cellItem->mLevels.values()) {
            int anyObjects = 0;
            foreach (ObjectGroupItem *ogItem, levelItem->mObjectGroups)
                if (anyObjects += ogItem->mObjects.size())
                    break;
            if (!anyObjects)
                continue;

            addToTree(cellItem, 0, levelItem, tr("Level %1").arg(levelItem->mZ));

            foreach (ObjectGroupItem *ogItem, levelItem->mObjectGroups) {
                if (!ogItem->mObjects.size())
                    continue;
                QString text = ogItem->mGroup->name();
                if (text.isEmpty())
                    text = tr("<no group>");
                addToTree(levelItem, -1, ogItem, text);

                foreach (ObjectItem *objItem, ogItem->mObjects) {
                    QString name = objItem->mObject->name();
                    QString type = objItem->mObject->type()->name();
                    if (name.isEmpty())
                        name = tr("<no name>");
                    if (type.isEmpty())
                        type = tr("<no type>");
                    addToTree(ogItem, -1, objItem, name, type);
                }
            }
        }
    }
    v->expandAll();
}

void CopyPasteDialog::showCellMap()
{
    QTreeWidget *v = ui->cellTree;
    v->clear();
    Item *root = mRootItem[Map];
    root->setViewItem(0);
    foreach (Item *item, root->children()) {
        CellItem *cellItem = item->asCellItem();
        WorldCell *cell = cellItem->mCell;
        if (cell->mapFilePath().isEmpty())
            continue;

        addToTree(0, 0, cellItem, tr("Cell %1,%2").arg(cell->x()).arg(cell->y()));

        MapTypeItem *mapItem = cellItem->children(Item::MapType).first()->asMapItem();
        addToTree(cellItem, 0, mapItem, QFileInfo(mapItem->mName).fileName());
    }
    v->expandAll();
}

void CopyPasteDialog::cellCategoryChanged(int index)
{
    ui->cellTree->setColumnHidden(1, true);

    mCellCat = static_cast<enum CellCat>(index);
    switch (mCellCat) {
    case Properties:
        showCellProperties();
        break;
    case CellTemplates:
        showCellTemplates();
        break;
    case Lots:
        showCellLots();
        break;
    case Objects:
        showCellObjects();
        break;
    case Map:
        showCellMap();
        break;
    }
}

void CopyPasteDialog::cellItemChanged(QTreeWidgetItem *viewItem, int column)
{
    Item *item = viewItem->data(0, Qt::UserRole).value<Item*>();
    item->mChecked = viewItem->checkState(0) == Qt::Checked;
}

void CopyPasteDialog::cellCheckAll()
{
}

void CopyPasteDialog::cellCheckNone()
{
}

void CopyPasteDialog::addToTree(Item *parent, int index, Item *item,
                                const QString &text, const QString &text2)
{
    QTreeWidget *v = ui->cellTree;
    item->mViewItem = new QTreeWidgetItem(QStringList() << text << text2);
    if (!parent)
        v->insertTopLevelItem(index, item->mViewItem);
    else {
        if (index == -1)
            index = parent->mViewItem->childCount();
        parent->mViewItem->insertChild(index, item->mViewItem);
    }
    // Must set UserData before setting the check-state.
    // Must preserve check-state because setData will set it to false.
    bool checked = item->mChecked;
    item->mViewItem->setData(0, Qt::UserRole, QVariant::fromValue(item));
    item->mChecked = checked;
    item->mViewItem->setCheckState(0, item->mChecked ? Qt::Checked : Qt::Unchecked);
}
