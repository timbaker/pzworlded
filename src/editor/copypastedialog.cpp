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
        ObjectType_Type,
        PropertyType,
        PropertyDefType,
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
    typedef CopyPasteDialog::ObjectGroupItem ObjectGroupItem;
    typedef CopyPasteDialog::ObjectTypeItem ObjectTypeItem;
    typedef CopyPasteDialog::PropertyItem PropertyItem;
    typedef CopyPasteDialog::PropertyDefItem PropertyDefItem;
    typedef CopyPasteDialog::TemplateItem TemplateItem;

    CellItem *asCellItem();
    LevelItem *asLevelItem();
    LotItem *asLotItem();
    MapTypeItem *asMapItem();
    ObjectItem *asObjectItem();
    ObjectGroupItem *asObjectGroupItem();
    ObjectTypeItem *asObjectTypeItem();
    PropertyItem *asPropertyItem();
    PropertyDefItem *asPropertyDefItem();
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


class CopyPasteDialog::ObjectTypeItem : public CopyPasteDialog::Item
{
public:
    ObjectTypeItem(ObjectType *ot)
        : Item(ObjectType_Type)
        , mType(ot)
    {
    }

    ObjectType *mType;
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

class CopyPasteDialog::PropertyDefItem : public CopyPasteDialog::Item
{
public:
    PropertyDefItem(PropertyDef *pd)
        : Item(PropertyDefType)
        , mPropertyDef(pd)
    {
    }

    PropertyDef *mPropertyDef;
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

CopyPasteDialog::Item::ObjectTypeItem *CopyPasteDialog::Item::asObjectTypeItem()
{
    return (mType == ObjectType_Type) ? static_cast<ObjectTypeItem*>(this) : 0;
}

CopyPasteDialog::Item::PropertyItem *CopyPasteDialog::Item::asPropertyItem()
{
    return (mType == PropertyType) ? static_cast<PropertyItem*>(this) : 0;
}

CopyPasteDialog::Item::PropertyDefItem *CopyPasteDialog::Item::asPropertyDefItem()
{
    return (mType == PropertyDefType) ? static_cast<PropertyDefItem*>(this) : 0;
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
    for (int c = FirstWorldCat; c < MaxWorldCat; c++)
        delete mWorldRootItem[c];
    for (int c = FirstCellCat; c < MaxCellCat; c++)
        delete mCellRootItem[c];
}

World *CopyPasteDialog::toWorld() const
{
    World *world = new World(mWorldDoc->world()->width(),
                             mWorldDoc->world()->height());

    if (ui->worldCat->item(PropertyDefs)->checkState() == Qt::Checked) {
        Item *root = mWorldRootItem[PropertyDefs];
        foreach (Item *item, root->children()) {
            PropertyDefItem *pdItem = item->asPropertyDefItem();
            if (pdItem->mChecked) {
                PropertyDef *pd = new PropertyDef();
                *pd = *pdItem->mPropertyDef;
                world->addPropertyDefinition(world->propertyDefinitions().size(), pd);
            }
        }
    }
    if (ui->worldCat->item(Templates)->checkState() == Qt::Checked) {
        Item *root = mWorldRootItem[Templates];
        foreach (Item *item, root->children()) {
            TemplateItem *ptItem = item->asTemplateItem();
            if (PropertyTemplate *pt = cloneTemplate(world, ptItem->mTemplate))
                world->addPropertyTemplate(world->propertyTemplates().size(), pt);
        }
    }
    /* Types must come before Groups. */
    if (ui->worldCat->item(ObjectTypes)->checkState() == Qt::Checked) {
        Item *root = mWorldRootItem[ObjectTypes];
        foreach (Item *item, root->children()) {
            ObjectTypeItem *otItem = item->asObjectTypeItem();
            if (otItem->mChecked) {
                ObjectType *ot = new ObjectType(otItem->mType->name());
                world->insertObjectType(world->objectTypes().size(), ot);
            }
        }
    }
    if (ui->worldCat->item(ObjectGroups)->checkState() == Qt::Checked) {
        Item *root = mWorldRootItem[ObjectGroups];
        foreach (Item *item, root->children()) {
            ObjectGroupItem *ogItem = item->asObjectGroupItem();
            if (ogItem->mChecked) {
                WorldObjectGroup *og = new WorldObjectGroup(world,
                                                            ogItem->mGroup);
                world->insertObjectGroup(0, og);
            }
        }
    }

    /////

    if (ui->cellCat->item(Map)->checkState() == Qt::Checked) {
        Item *root = mCellRootItem[Map];
        foreach (Item *item0, root->children()) {
            CellItem *cellItem = item0->asCellItem();
            if (!cellItem->mChecked)
                continue;
            WorldCell *cell = world->cellAt(cellItem->mCell->x(),
                                             cellItem->mCell->y());
            MapTypeItem *mapItem = cellItem->children(Item::MapType).first()->asMapItem();
            if (mapItem->mChecked)
                cell->setMapFilePath(mapItem->mName);
        }
    }

    if (ui->cellCat->item(Properties)->checkState() == Qt::Checked) {
        Item *root = mCellRootItem[Properties];
        foreach (Item *item0, root->children()) {
            CellItem *cellItem = item0->asCellItem();
            if (!cellItem->mChecked)
                continue;
            WorldCell *cell = world->cellAt(cellItem->mCell->x(),
                                             cellItem->mCell->y());
            foreach (Item *item1, cellItem->children()) {
                PropertyItem *pItem = item1->asPropertyItem();
                if (!pItem->mChecked)
                    continue;
                if (Property *p = cloneProperty(world, pItem->mProperty))
                    cell->addProperty(cell->properties().size(), p);
            }
        }
    }

    if (ui->cellCat->item(CellTemplates)->checkState() == Qt::Checked) {
        Item *root = mCellRootItem[CellTemplates];
        foreach (Item *item0, root->children()) {
            CellItem *cellItem = item0->asCellItem();
            if (!cellItem->mChecked)
                continue;
            WorldCell *cell = world->cellAt(cellItem->mCell->x(),
                                             cellItem->mCell->y());
            foreach (Item *item1, cellItem->children()) {
                TemplateItem *ptItem = item1->asTemplateItem();
                if (!ptItem->mChecked)
                    continue;
                if (PropertyTemplate *pt = cloneTemplate(world, ptItem->mTemplate))
                    cell->addTemplate(cell->templates().size(), pt);
            }
        }
    }

    if (ui->cellCat->item(Lots)->checkState() == Qt::Checked) {
        Item *root = mCellRootItem[Lots];
        foreach (Item *item0, root->children()) {
            CellItem *cellItem = item0->asCellItem();
            if (!cellItem->mChecked)
                continue;

            QList<LevelItem*> levelItems;
            foreach (LevelItem *lvItem, cellItem->mLevels.values())
                levelItems.insert(0, lvItem);
            foreach (LevelItem *lvItem, levelItems) {
                if (!lvItem->mChecked)
                    continue;

                WorldCell *cell = world->cellAt(cellItem->mCell->x(),
                                                cellItem->mCell->y());
                foreach (LotItem *lotItem, lvItem->mLots) {
                    if (!lotItem->mChecked)
                        continue;
                    WorldCellLot *lot = new WorldCellLot(cell,
                                                         lotItem->mLot);
                    cell->insertLot(0, lot);
                }
            }
        }
    }

    if (ui->cellCat->item(Objects)->checkState() == Qt::Checked) {
        Item *root = mCellRootItem[Objects];
        foreach (Item *item0, root->children()) {
            CellItem *cellItem = item0->asCellItem();
            if (!cellItem->mChecked)
                continue;

            QList<LevelItem*> levelItems;
            foreach (LevelItem *lvItem, cellItem->mLevels.values())
                levelItems.insert(0, lvItem);
            foreach (LevelItem *lvItem, levelItems) {
                if (!lvItem->mChecked)
                    continue;

                WorldCell *cell = world->cellAt(cellItem->mCell->x(),
                                                 cellItem->mCell->y());
                foreach (ObjectGroupItem *ogItem, lvItem->mObjectGroups) {
                    if (!ogItem->mChecked)
                        continue;
                    foreach (Item *item3, ogItem->children()) {
                        ObjectItem *objItem = item3->asObjectItem();
                        if (!objItem->mChecked)
                            continue;
                        WorldCellObject *obj = new WorldCellObject(cell,
                                                                   objItem->mObject);
                        cell->insertObject(0, obj);
                    }
                }
            }
        }
    }

    return world;
}

void CopyPasteDialog::setup()
{
    QHeaderView *header = ui->worldTree->header();
    header->setStretchLastSection(false);
    header->setResizeMode(0, QHeaderView::ResizeToContents);
    header->setResizeMode(1, QHeaderView::ResizeToContents);

    header = ui->cellTree->header();
    header->setStretchLastSection(false);
    header->setResizeMode(0, QHeaderView::ResizeToContents);
    header->setResizeMode(1, QHeaderView::ResizeToContents);

    ///// WORLD /////

    connect(ui->worldCat, SIGNAL(currentRowChanged(int)),
            SLOT(worldSelectionChanged(int)));
    connect(ui->worldTree, SIGNAL(itemChanged(QTreeWidgetItem*,int)),
            SLOT(worldItemChanged(QTreeWidgetItem*,int)));
    connect(ui->worldCheckAll, SIGNAL(clicked()), SLOT(worldCheckAll()));
    connect(ui->worldCheckNone, SIGNAL(clicked()), SLOT(worldCheckNone()));

    mWorldRootItem[PropertyDefs] = new Item();
    mWorldRootItem[Templates] = new Item();
    mWorldRootItem[ObjectTypes] = new Item();
    mWorldRootItem[ObjectGroups] = new Item();

    Item *root = mWorldRootItem[PropertyDefs];
    foreach (PropertyDef *pd, mWorld->propertyDefinitions().sorted())
        root->insertChild(-1, new PropertyDefItem(pd));

    root = mWorldRootItem[Templates];
    foreach (PropertyTemplate *pt, mWorld->propertyTemplates().sorted())
        root->insertChild(-1, new TemplateItem(pt));

    root = mWorldRootItem[ObjectTypes];
    foreach (ObjectType *ot, mWorld->objectTypes()) {
        if (ot->isNull()) continue;
        root->insertChild(-1, new ObjectTypeItem(ot));
    }

    root = mWorldRootItem[ObjectGroups];
    foreach (WorldObjectGroup *og, mWorld->objectGroups()) {
        if (og->isNull()) continue;
        root->insertChild(0, new ObjectGroupItem(og));
    }

    ///// CELLS /////

    connect(ui->cellCat, SIGNAL(currentRowChanged(int)),
            SLOT(cellCategoryChanged(int)));
    connect(ui->cellTree, SIGNAL(itemChanged(QTreeWidgetItem*,int)),
            SLOT(cellItemChanged(QTreeWidgetItem*,int)));
    connect(ui->cellCheckAll, SIGNAL(clicked()), SLOT(cellCheckAll()));
    connect(ui->cellCheckNone, SIGNAL(clicked()), SLOT(cellCheckNone()));

    if (mCellDoc) {
        mCells += mCellDoc->cell();
    } else {
        mCells = mWorldDoc->selectedCells();
        if (mCells.isEmpty()) {
            foreach (WorldCell *cell, mWorld->cells()) {
                if (!cell->isEmpty())
                    mCells += cell;
            }
        }
    }

    mCellRootItem[CellTemplates] = new Item();
    mCellRootItem[Lots] = new Item();
    mCellRootItem[Map] = new Item();
    mCellRootItem[Objects] = new Item();
    mCellRootItem[Properties] = new Item();

    foreach (WorldCell *cell, mCells) {

        CellItem *cellItem = new CellItem(cell);
        mCellRootItem[Map]->insertChild(-1, cellItem);
        cellItem->insertChild(-1, new MapTypeItem(cell->mapFilePath()));

        cellItem = new CellItem(cell);
        mCellRootItem[Properties]->insertChild(-1, cellItem);
        foreach (Property *p, cell->properties().sorted())
            cellItem->insertChild(-1, new PropertyItem(p));

        cellItem = new CellItem(cell);
        mCellRootItem[CellTemplates]->insertChild(-1, cellItem);
        foreach (PropertyTemplate *pt, cell->templates().sorted())
            cellItem->insertChild(-1, new TemplateItem(pt));

        cellItem = new CellItem(cell);
        mCellRootItem[Objects]->insertChild(-1, cellItem);
        foreach (WorldCellObject *obj, cell->objects()) {
            LevelItem *levelItem = cellItem->itemForLevel(obj->level());
            foreach (WorldObjectGroup *og, mWorld->objectGroups())
                (void) levelItem->itemForGroup(og);
            ObjectGroupItem *ogItem = levelItem->itemForGroup(obj->group());
            ogItem->insertChild(0, new ObjectItem(obj));
        }

        cellItem = new CellItem(cell);
        mCellRootItem[Lots]->insertChild(-1, cellItem);
        foreach (WorldCellLot *lot, cell->lots())
            cellItem->itemForLevel(lot->level())->insertChild(0, new LotItem(lot));
    }
}

PropertyTemplate *CopyPasteDialog::cloneTemplate(World *world, PropertyTemplate *ptIn) const
{
    if (ui->worldCat->item(Templates)->checkState() != Qt::Checked)
        return 0;
    Item *root = mWorldRootItem[Templates];
    foreach (Item *item, root->children()) {
        if ((item->asTemplateItem()->mTemplate == ptIn) && !item->mChecked)
            return 0;
    }

    PropertyTemplate *pt = new PropertyTemplate();
    pt->mName = ptIn->mName;
    pt->mDescription = ptIn->mDescription;
    foreach (PropertyTemplate *pt1, ptIn->templates())
        if (PropertyTemplate *pt2 = cloneTemplate(world, pt1))
            pt->addTemplate(pt->templates().size(), pt1);
    foreach (Property *p, ptIn->properties()) {
        if (Property *p1 = cloneProperty(world, p))
            pt->addProperty(pt->properties().size(), p1);
    }
    return pt;
}

Property *CopyPasteDialog::cloneProperty(World *world, Property *pIn) const
{
    if (PropertyDef *pd = world->propertyDefinitions().findPropertyDef(pIn->mDefinition->mName)) {
        Property *p = new Property(pd, pIn->mValue);
        p->mNote = pIn->mNote;
        return p;
    }
    return 0;
}

///// WORLD /////

void CopyPasteDialog::showPropertyDefs()
{
    QTreeWidget *v = ui->worldTree;
    v->setColumnHidden(1, false);
    v->clear();
    Item *root = mWorldRootItem[PropertyDefs];
    root->setViewItem(0);
    foreach (Item *item, root->children()) {
        PropertyDefItem *pdItem = item->asPropertyDefItem();
        addToTree(v, 0, -1, item, pdItem->mPropertyDef->mName,
                  pdItem->mPropertyDef->mDefaultValue);
    }
}

void CopyPasteDialog::showTemplates()
{
    QTreeWidget *v = ui->worldTree;
    v->clear();
    Item *root = mWorldRootItem[Templates];
    root->setViewItem(0);
    foreach (Item *item, root->children()) {
        TemplateItem *ptItem = item->asTemplateItem();
        addToTree(v, 0, -1, item, ptItem->mTemplate->mName);
    }
}

void CopyPasteDialog::showObjectTypes()
{
    QTreeWidget *v = ui->worldTree;
    v->clear();
    Item *root = mWorldRootItem[ObjectTypes];
    root->setViewItem(0);
    foreach (Item *item, root->children()) {
        ObjectTypeItem *otItem = item->asObjectTypeItem();
        addToTree(v, 0, -1, item, otItem->mType->name());
    }
}

void CopyPasteDialog::showObjectGroups()
{
    QTreeWidget *v = ui->worldTree;
    v->clear();
    Item *root = mWorldRootItem[ObjectGroups];
    root->setViewItem(0);
    foreach (Item *item, root->children()) {
        ObjectGroupItem *ogItem = item->asObjectGroupItem();
        addToTree(v, 0, -1, item, ogItem->mGroup->name());
    }
}

void CopyPasteDialog::worldSelectionChanged(int index)
{
    ui->worldTree->setColumnHidden(1, true);

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

void CopyPasteDialog::worldItemChanged(QTreeWidgetItem *viewItem, int column)
{
    Q_UNUSED(column)
    Item *item = viewItem->data(0, Qt::UserRole).value<Item*>();
    item->mChecked = viewItem->checkState(0) == Qt::Checked;
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
    Item *root = mCellRootItem[Properties];
    root->setViewItem(0);
    foreach (Item *item, root->children()) {
        CellItem *cellItem = item->asCellItem();
        WorldCell *cell = cellItem->mCell;
        if (cell->properties().isEmpty())
            continue;
        addToTree(v, 0, 0, cellItem, tr("Cell %1,%2").arg(cell->x()).arg(cell->y()));

        foreach (Item *item, cellItem->children(Item::PropertyType)) {
            PropertyItem *propertyItem = item->asPropertyItem();
            addToTree(v, cellItem, -1, propertyItem,
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
    Item *root = mCellRootItem[CellTemplates];
    root->setViewItem(0);
    foreach (Item *item, root->children()) {
        CellItem *cellItem = item->asCellItem();
        WorldCell *cell = cellItem->mCell;
        if (cell->templates().isEmpty())
            continue;
        addToTree(v, 0, 0, cellItem, tr("Cell %1,%2").arg(cell->x()).arg(cell->y()));

        foreach (Item *item, cellItem->children(Item::TemplateType)) {
            TemplateItem *templateItem = item->asTemplateItem();
            addToTree(v, cellItem, -1, templateItem, templateItem->mTemplate->mName);
        }
    }
    v->expandAll();
}

void CopyPasteDialog::showCellLots()
{
    QTreeWidget *v = ui->cellTree;
    v->clear();
    Item *root = mCellRootItem[Lots];
    root->setViewItem(0);
    foreach (Item *item, root->children()) {
        CellItem *cellItem = item->asCellItem();
        WorldCell *cell = cellItem->mCell;
        if (cell->lots().isEmpty())
            continue;
        addToTree(v, 0, 0, cellItem, tr("Cell %1,%2").arg(cell->x()).arg(cell->y()));

        foreach (LevelItem *levelItem, cellItem->mLevels.values()) {
            if (levelItem->mLots.size() == 0)
                continue;
            addToTree(v, cellItem, 0, levelItem, tr("Level %1").arg(levelItem->mZ));

            foreach (LotItem *lotItem, levelItem->mLots)
                addToTree(v, levelItem, -1, lotItem,
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
    Item *root = mCellRootItem[Objects];
    root->setViewItem(0);
    foreach (Item *item, root->children()) {
        CellItem *cellItem = item->asCellItem();
        WorldCell *cell = cellItem->mCell;
        if (cell->objects().isEmpty())
            continue;
        addToTree(v, 0, 0, cellItem, tr("Cell %1,%2").arg(cell->x()).arg(cell->y()));

        foreach (LevelItem *levelItem, cellItem->mLevels.values()) {
            int anyObjects = 0;
            foreach (ObjectGroupItem *ogItem, levelItem->mObjectGroups)
                if (anyObjects += ogItem->mObjects.size())
                    break;
            if (!anyObjects)
                continue;

            addToTree(v, cellItem, 0, levelItem, tr("Level %1").arg(levelItem->mZ));

            foreach (ObjectGroupItem *ogItem, levelItem->mObjectGroups) {
                if (!ogItem->mObjects.size())
                    continue;
                QString text = ogItem->mGroup->name();
                if (text.isEmpty())
                    text = tr("<no group>");
                addToTree(v, levelItem, -1, ogItem, text);

                foreach (ObjectItem *objItem, ogItem->mObjects) {
                    QString name = objItem->mObject->name();
                    QString type = objItem->mObject->type()->name();
                    if (name.isEmpty())
                        name = tr("<no name>");
                    if (type.isEmpty())
                        type = tr("<no type>");
                    addToTree(v, ogItem, -1, objItem, name, type);
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
    Item *root = mCellRootItem[Map];
    root->setViewItem(0);
    foreach (Item *item, root->children()) {
        CellItem *cellItem = item->asCellItem();
        WorldCell *cell = cellItem->mCell;
        if (cell->mapFilePath().isEmpty())
            continue;

        addToTree(v, 0, 0, cellItem, tr("Cell %1,%2").arg(cell->x()).arg(cell->y()));

        MapTypeItem *mapItem = cellItem->children(Item::MapType).first()->asMapItem();
        addToTree(v, cellItem, 0, mapItem, QFileInfo(mapItem->mName).fileName());
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
    Q_UNUSED(column)
    Item *item = viewItem->data(0, Qt::UserRole).value<Item*>();
    item->mChecked = viewItem->checkState(0) == Qt::Checked;
}

void CopyPasteDialog::cellCheckAll()
{
    QTreeWidget *view = ui->cellTree;
    for (int i = 0; i < view->topLevelItemCount(); i++)
        view->topLevelItem(i)->setCheckState(0, Qt::Checked);
}

void CopyPasteDialog::cellCheckNone()
{
    QTreeWidget *view = ui->cellTree;
    for (int i = 0; i < view->topLevelItemCount(); i++)
        view->topLevelItem(i)->setCheckState(0, Qt::Unchecked);
}

void CopyPasteDialog::addToTree(QTreeWidget *v, Item *parent, int index,
                                Item *item, const QString &text, const QString &text2)
{
    item->mViewItem = new QTreeWidgetItem(QStringList() << text << text2);
    if (!parent) {
        if (index == -1)
            index = v->topLevelItemCount();
        v->insertTopLevelItem(index, item->mViewItem);
    } else {
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
