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

#include "worlddocument.h"

#include "celldocument.h"
#include "documentmanager.h"
#include "luawriter.h"
#include "undoredo.h"
#include "world.h"
#include "worldscene.h"
#include "worldview.h"
#include "worldwriter.h"

#include <QDebug>
#include <QFileInfo>
#include <QUndoStack>

WorldDocument::WorldDocument(World *world, const QString &fileName)
    : Document(WorldDocType)
    , mWorld(world)
    , mFileName(fileName)
    , mUndoRedo(this)
{
    mUndoStack = new QUndoStack(this);

    // Forward all the signals from mUndoRedo to this object's signals

    connect(&mUndoRedo, SIGNAL(propertyAdded(PropertyHolder*,int)),
            SIGNAL(propertyAdded(PropertyHolder*,int)));
    connect(&mUndoRedo, SIGNAL(propertyAboutToBeRemoved(PropertyHolder*,int)),
            SIGNAL(propertyAboutToBeRemoved(PropertyHolder*,int)));
    connect(&mUndoRedo, SIGNAL(propertyValueChanged(PropertyHolder*,int)),
            SIGNAL(propertyValueChanged(PropertyHolder*,int)));

    connect(&mUndoRedo, SIGNAL(propertyDefinitionAdded(PropertyDef*,int)),
            SIGNAL(propertyDefinitionAdded(PropertyDef*,int)));
    connect(&mUndoRedo, SIGNAL(propertyDefinitionAboutToBeRemoved(int)),
            SIGNAL(propertyDefinitionAboutToBeRemoved(int)));
    connect(&mUndoRedo, SIGNAL(propertyDefinitionChanged(PropertyDef*)),
            SIGNAL(propertyDefinitionChanged(PropertyDef*)));

    connect(&mUndoRedo, SIGNAL(objectTypeAdded(int)),
            SIGNAL(objectTypeAdded(int)));
    connect(&mUndoRedo, SIGNAL(objectTypeAboutToBeRemoved(int)),
            SIGNAL(objectTypeAboutToBeRemoved(int)));
    connect(&mUndoRedo, SIGNAL(objectTypeNameChanged(ObjectType*)),
            SIGNAL(objectTypeNameChanged(ObjectType*)));

    connect(&mUndoRedo, SIGNAL(objectGroupAdded(int)),
            SIGNAL(objectGroupAdded(int)));
    connect(&mUndoRedo, SIGNAL(objectGroupAboutToBeRemoved(int)),
            SIGNAL(objectGroupAboutToBeRemoved(int)));
    connect(&mUndoRedo, SIGNAL(objectGroupNameChanged(WorldObjectGroup*)),
            SIGNAL(objectGroupNameChanged(WorldObjectGroup*)));
    connect(&mUndoRedo, SIGNAL(objectGroupColorChanged(WorldObjectGroup*)),
            SIGNAL(objectGroupColorChanged(WorldObjectGroup*)));
    connect(&mUndoRedo, SIGNAL(objectGroupAboutToBeReordered(int)),
            SIGNAL(objectGroupAboutToBeReordered(int)));
    connect(&mUndoRedo, SIGNAL(objectGroupReordered(int)),
            SIGNAL(objectGroupReordered(int)));

    connect(&mUndoRedo, SIGNAL(templateAdded(int)),
            SIGNAL(templateAdded(int)));
    connect(&mUndoRedo, SIGNAL(templateAboutToBeRemoved(int)),
            SIGNAL(templateAboutToBeRemoved(int)));
    connect(&mUndoRedo, SIGNAL(templateChanged(PropertyTemplate*)),
            SIGNAL(templateChanged(PropertyTemplate*)));

    connect(&mUndoRedo, SIGNAL(templateAdded(PropertyHolder*,int)),
            SIGNAL(templateAdded(PropertyHolder*,int)));
    connect(&mUndoRedo, SIGNAL(templateAboutToBeRemoved(PropertyHolder*,int)),
            SIGNAL(templateAboutToBeRemoved(PropertyHolder*,int)));

    connect(&mUndoRedo, SIGNAL(cellMapFileAboutToChange(WorldCell*)),
            SIGNAL(cellMapFileAboutToChange(WorldCell*)));
    connect(&mUndoRedo, SIGNAL(cellMapFileChanged(WorldCell*)),
            SIGNAL(cellMapFileChanged(WorldCell*)));

    connect(&mUndoRedo, SIGNAL(cellContentsAboutToChange(WorldCell*)),
            SIGNAL(cellContentsAboutToChange(WorldCell*)));
    connect(&mUndoRedo, SIGNAL(cellContentsChanged(WorldCell*)),
            SIGNAL(cellContentsChanged(WorldCell*)));

    connect(&mUndoRedo, SIGNAL(cellLotAdded(WorldCell*,int)),
            SIGNAL(cellLotAdded(WorldCell*,int)));
    connect(&mUndoRedo, SIGNAL(cellLotAboutToBeRemoved(WorldCell*,int)),
            SIGNAL(cellLotAboutToBeRemoved(WorldCell*,int)));
    connect(&mUndoRedo, SIGNAL(cellLotMoved(WorldCellLot*)),
            SIGNAL(cellLotMoved(WorldCellLot*)));
    connect(&mUndoRedo, SIGNAL(lotLevelChanged(WorldCellLot*)),
            SIGNAL(lotLevelChanged(WorldCellLot*)));

    connect(&mUndoRedo, SIGNAL(cellObjectAdded(WorldCell*,int)),
            SIGNAL(cellObjectAdded(WorldCell*,int)));
    connect(&mUndoRedo, SIGNAL(cellObjectAboutToBeRemoved(WorldCell*,int)),
            SIGNAL(cellObjectAboutToBeRemoved(WorldCell*,int)));
    connect(&mUndoRedo, SIGNAL(cellObjectMoved(WorldCellObject*)),
            SIGNAL(cellObjectMoved(WorldCellObject*)));
    connect(&mUndoRedo, SIGNAL(cellObjectResized(WorldCellObject*)),
            SIGNAL(cellObjectResized(WorldCellObject*)));
    connect(&mUndoRedo, SIGNAL(cellObjectNameChanged(WorldCellObject*)),
            SIGNAL(cellObjectNameChanged(WorldCellObject*)));
    connect(&mUndoRedo, SIGNAL(cellObjectGroupAboutToChange(WorldCellObject*)),
            SIGNAL(cellObjectGroupAboutToChange(WorldCellObject*)));
    connect(&mUndoRedo, SIGNAL(cellObjectGroupChanged(WorldCellObject*)),
            SIGNAL(cellObjectGroupChanged(WorldCellObject*)));
    connect(&mUndoRedo, SIGNAL(cellObjectTypeChanged(WorldCellObject*)),
            SIGNAL(cellObjectTypeChanged(WorldCellObject*)));
    connect(&mUndoRedo, SIGNAL(objectLevelAboutToChange(WorldCellObject*)),
            SIGNAL(objectLevelAboutToChange(WorldCellObject*)));
    connect(&mUndoRedo, SIGNAL(objectLevelChanged(WorldCellObject*)),
            SIGNAL(objectLevelChanged(WorldCellObject*)));
    connect(&mUndoRedo, SIGNAL(cellObjectReordered(WorldCellObject*)),
            SIGNAL(cellObjectReordered(WorldCellObject*)));

    connect(&mUndoRedo, SIGNAL(selectedCellsChanged()),
            SIGNAL(selectedCellsChanged()));
}

WorldDocument::~WorldDocument()
{
    qDeleteAll(mCellClipboard);
    delete mWorld;
}

void WorldDocument::setFileName(const QString &fileName)
{
    mFileName = fileName;
}

const QString &WorldDocument::fileName() const
{
    return mFileName;
}

bool WorldDocument::save(const QString &filePath, QString &error)
{
    WorldWriter writer;
    if (!writer.writeWorld(mWorld, filePath)) {
        error = writer.errorString();
        return false;
    }

    {
        QFileInfo fi(filePath);
        QString luaPath = fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName() + QLatin1String(".lua");
        LuaWriter writer;
        if (!writer.writeWorld(mWorld, luaPath)) {
            error += writer.errorString();
        }
    }

    undoStack()->setClean();
    setFileName(filePath);

    return true;
}

void WorldDocument::setWorld(World *world)
{
    mWorld = world;
}

void WorldDocument::setSelectedCells(const QList<WorldCell*> &selectedCells, bool undoable)
{
    QList<WorldCell*> selection;
    foreach (WorldCell *cell, selectedCells) {
        if (!selection.contains(cell))
            selection.append(cell);
        else
            qWarning("duplicate cells passed to setSelectedCells");
    }
    if (undoable)
        undoStack()->push(new ChangeCellSelection(this, selection));
    else {
        mSelectedCells = selection;
        emit selectedCellsChanged();
    }
}

void WorldDocument::setSelectedObjects(const QList<WorldCellObject *> &selectedObjects)
{
    QList<WorldCellObject*> selection;
    foreach (WorldCellObject *obj, selectedObjects) {
        if (!selection.contains(obj))
            selection.append(obj);
        else
            qWarning("duplicate objects passed to setSelectedObjects");
    }
    mSelectedObjects = selection;
    emit selectedObjectsChanged();
}

void WorldDocument::setSelectedLots(const QList<WorldCellLot *> &selectedLots)
{
    QList<WorldCellLot*> selection;
    foreach (WorldCellLot *lot, selectedLots) {
        if (!selection.contains(lot))
            selection.append(lot);
        else
            qWarning("duplicate lots passed to setSelectedLots");
    }
    mSelectedLots = selection;
    emit selectedLotsChanged();
}

void WorldDocument::editCell(WorldCell *cell)
{
    editCell(cell->x(), cell->y());
}

void WorldDocument::editCell(int x, int y)
{
    DocumentManager *docman = DocumentManager::instance();
    WorldCell *cell = world()->cellAt(x, y);
    if (CellDocument *cellDoc = docman->findDocument(cell)) {
        docman->setCurrentDocument(cellDoc);
        return;
    }
    CellDocument *cellDoc = new CellDocument(this, cell);
    docman->addDocument(cellDoc);
}

void WorldDocument::setCellMapName(WorldCell *cell, const QString &mapName)
{
    Q_ASSERT(cell && cell->world() == world());
    undoStack()->push(new SetCellMainMap(this, cell, mapName));
}

void WorldDocument::addCellLot(WorldCell *cell, int index, WorldCellLot *lot)
{
    undoStack()->push(new AddCellLot(this, cell, index, lot));
}

void WorldDocument::removeCellLot(WorldCell *cell, int index)
{
    undoStack()->push(new RemoveCellLot(this, cell, index));
}

void WorldDocument::moveCellLot(WorldCellLot *lot, const QPoint &pos)
{
    undoStack()->push(new MoveCellLot(this, lot, pos));
}

void WorldDocument::setLotLevel(WorldCellLot *lot, int level)
{
    if (lot->level() == level)
        return;
    undoStack()->push(new SetLotLevel(this, lot, level));
}

void WorldDocument::addCellObject(WorldCell *cell, int index, WorldCellObject *obj)
{
    Q_ASSERT(!cell->objects().contains(obj));
    Q_ASSERT(index >= 0 && index <= cell->objects().size());
    undoStack()->push(new AddCellObject(this, cell, index, obj));
}

void WorldDocument::removeCellObject(WorldCell *cell, int index)
{
    Q_ASSERT(index >= 0 && index < cell->objects().size());
    undoStack()->push(new RemoveCellObject(this, cell, index));
}

void WorldDocument::moveCellObject(WorldCellObject *obj, const QPointF &pos)
{
    undoStack()->push(new MoveCellObject(this, obj, pos));
}

void WorldDocument::resizeCellObject(WorldCellObject *obj, const QSizeF &size)
{
    undoStack()->push(new ResizeCellObject(this, obj, size));
}

void WorldDocument::setObjectLevel(WorldCellObject *obj, int level)
{
    if (obj->level() == level)
        return;
    undoStack()->push(new SetObjectLevel(this, obj, level));
}

void WorldDocument::setCellObjectName(WorldCellObject *obj, const QString &name)
{
    if (name == obj->name()) return;
    undoStack()->push(new SetObjectName(this, obj, name));
}

void WorldDocument::setCellObjectGroup(WorldCellObject *obj, WorldObjectGroup *og)
{
    Q_ASSERT(og);
    if (og == obj->group())
        return;
    undoStack()->push(new SetObjectGroup(this, obj, og));
}

void WorldDocument::setCellObjectType(WorldCellObject *obj, const QString &type)
{
    ObjectType *objType = 0;
    foreach (ObjectType *ot, mWorld->objectTypes()) {
        if (ot->name() == type) {
            objType = ot;
            break;
        }
    }
    Q_ASSERT(objType);
    if (objType == obj->type())
        return;
    undoStack()->push(new SetObjectType(this, obj, objType));
}

void WorldDocument::reorderCellObject(WorldCellObject *obj, WorldCellObject *insertBefore)
{
    if (obj == insertBefore)
        return;
    const WorldCellObjectList &objects = obj->cell()->objects();
    int index = insertBefore ? objects.indexOf(insertBefore) : objects.size();
    undoStack()->push(new ReorderCellObject(this, obj, index));
}

void WorldDocument::moveCell(WorldCell *cell, const QPoint &newPos)
{
    WorldCell *dest = mWorld->cellAt(newPos);
    WorldCellContents *srcContents = new WorldCellContents(cell, false);
    undoStack()->beginMacro(tr("Move Cell"));
    undoStack()->push(new ReplaceCell(this, dest, srcContents));
    WorldCellContents *emptyContents = new WorldCellContents(cell);
    undoStack()->push(new ReplaceCell(this, cell, emptyContents));
    undoStack()->endMacro();
}

void WorldDocument::clearCell(WorldCell *cell)
{
    WorldCellContents *emptyContents = new WorldCellContents(cell);
    undoStack()->push(new ReplaceCell(this, cell, emptyContents));
}

bool WorldDocument::removePropertyDefinition(PropertyDef *pd)
{
    undoStack()->beginMacro(tr("Remove Property Definition (%1)").arg(pd->mName));

    // Remove all properties using this definition from templates and cells
    foreach (PropertyTemplate *pt, mWorld->propertyTemplates()) {
        removePropertyDefinition(pt, pd);
    }
    foreach (WorldCell *cell, mWorld->cells())
        removePropertyDefinition(cell, pd);

    int index = mWorld->propertyDefinitions().indexOf(pd);
    undoStack()->push(new RemovePropertyDef(this, index, pd));

    undoStack()->endMacro();

    return true;
}

void WorldDocument::addPropertyDefinition(PropertyDef *pd)
{
    int index = mWorld->propertyDefinitions().size();
    undoStack()->push(new AddPropertyDef(this, index, pd));
}

void WorldDocument::addTemplate(const QString &name, const QString &desc)
{
    int index = mWorld->propertyTemplates().size();
    PropertyTemplate *pt = new PropertyTemplate;
    pt->mName = name, pt->mDescription = desc;
    undoStack()->push(new AddTemplateToWorld(this, index, pt));
}

void WorldDocument::removeTemplate(int index)
{
    PropertyTemplate *remove = mWorld->propertyTemplates().at(index);

    undoStack()->beginMacro(tr("Remove Template (%1)").arg(remove->mName));

    // Remove the template from other templates and cells
    foreach (PropertyTemplate *pt, mWorld->propertyTemplates())
        removeTemplate(pt, remove);
    foreach (WorldCell *cell, mWorld->cells())
        removeTemplate(cell, remove);

    undoStack()->push(new RemoveTemplateFromWorld(this, index, remove));

    undoStack()->endMacro();
}

void WorldDocument::changePropertyDefinition(PropertyDef *pd, const QString &name, const QString &defValue, const QString &desc)
{
    undoStack()->push(new EditPropertyDef(this, pd, name, defValue, desc));
}

void WorldDocument::changeTemplate(PropertyTemplate *pt, const QString &name, const QString &desc)
{
    undoStack()->push(new ChangeTemplate(this, pt, name, desc));
}

void WorldDocument::addObjectGroup(WorldObjectGroup *newGroup)
{
    int index = mWorld->objectGroups().size();
    undoStack()->push(new AddObjectGroup(this, index, newGroup));
}

bool WorldDocument::removeObjectGroup(WorldObjectGroup *og)
{
    int index = mWorld->objectGroups().indexOf(og);

    undoStack()->beginMacro(tr("Remove Object Group (%1)").arg(og->name()));

    // Reset the object group on any objects using this group
    foreach (WorldCell *cell, mWorld->cells()) {
        foreach (WorldCellObject *obj, cell->objects()) {
            if (obj->group() == og)
                undoStack()->push(new SetObjectGroup(this, obj, mWorld->nullObjectGroup()));
        }
    }

    undoStack()->push(new RemoveObjectGroup(this, index));
    undoStack()->endMacro();

    return true;
}

void WorldDocument::changeObjectGroupName(WorldObjectGroup *objGroup, const QString &name)
{
    Q_ASSERT(!name.isEmpty());
    Q_ASSERT(!mWorld->objectGroups().contains(name));
    undoStack()->push(new SetObjectGroupName(this, objGroup, name));
}

void WorldDocument::changeObjectGroupColor(WorldObjectGroup *objGroup, const QColor &color)
{
    undoStack()->push(new SetObjectGroupColor(this, objGroup, color));
}

void WorldDocument::reorderObjectGroup(WorldObjectGroup *og, int index)
{
    int oldIndex = mWorld->objectGroups().indexOf(og);
    Q_ASSERT(index != oldIndex);
    Q_ASSERT(index >= 0 && index < mWorld->objectGroups().size());
    undoStack()->beginMacro(tr("Reorder Object Groups"));
    undoStack()->push(new ReorderObjectGroup(this, og, index));
    undoStack()->endMacro();
}

void WorldDocument::changeObjectGroupDefType(WorldObjectGroup *og, ObjectType *ot)
{
    if (og->type() == ot)
        return;
    undoStack()->push(new SetObjectGroupDefType(this, og, ot));
}

void WorldDocument::addObjectType(ObjectType *newType)
{
    int index = mWorld->objectTypes().size();
    undoStack()->push(new AddObjectType(this, index, newType));
}

bool WorldDocument::removeObjectType(ObjectType *ot)
{
    int index = mWorld->objectTypes().indexOf(ot);

    undoStack()->beginMacro(tr("Remove Object Type (%1)").arg(ot->name()));

    // Reset the object type on any objects using this type
    foreach (WorldCell *cell, mWorld->cells()) {
        foreach (WorldCellObject *obj, cell->objects()) {
            if (obj->type() == ot)
                undoStack()->push(new SetObjectType(this, obj,
                                                    mWorld->nullObjectType()));
        }
    }

    // Reset the default type for object groups using this type
    foreach (WorldObjectGroup *og, mWorld->objectGroups()) {
        if (og->type() == ot)
            undoStack()->push(new SetObjectGroupDefType(this, og,
                                                        mWorld->nullObjectType()));
    }

    undoStack()->push(new RemoveObjectType(this, index));
    undoStack()->endMacro();

    return true;
}

void WorldDocument::changeObjectTypeName(ObjectType *objType, const QString &name)
{
    Q_ASSERT(!name.isEmpty());
    Q_ASSERT(!mWorld->objectTypes().contains(name));
    undoStack()->push(new SetObjectTypeName(this, objType, name));
}

void WorldDocument::addTemplate(PropertyHolder *ph, const QString &name)
{
    PropertyTemplate *pt = mWorld->propertyTemplates().find(name);
    int index = ph->templates().size();
    undoStack()->push(new AddTemplateToPH(this, ph, index, pt));
}

void WorldDocument::removeTemplate(PropertyHolder *ph, int index)
{
    PropertyTemplate *pt = ph->templates().at(index);
    undoStack()->push(new RemoveTemplateFromPH(this, ph, index, pt));
}

void WorldDocument::addProperty(PropertyHolder *ph, const QString &name)
{
    PropertyDef *pd = mWorld->propertyDefinitions().findPropertyDef(name);
    Property *p = new Property(pd, pd->mDefaultValue);
    int index = ph->properties().size();
    undoStack()->push(new AddPropertyToPH(this, ph, index, p));
}

void WorldDocument::removeProperty(PropertyHolder *ph, int index)
{
    Property *p = ph->properties().at(index);
    undoStack()->push(new RemovePropertyFromPH(this, ph, index, p));
}

void WorldDocument::setPropertyValue(PropertyHolder *ph, Property *p, const QString &value)
{
    undoStack()->push(new SetPropertyValue(this, ph, p, value));
}

void WorldDocument::copyCellsToClipboard(const QList<WorldCell *> &cells)
{
    clearCellClipboard();
    foreach (WorldCell *cell, cells) {
        WorldCellContents *contents = new WorldCellContents(cell, false);
        mCellClipboard += contents;
    }
    emit cellClipboardChanged();
}

void WorldDocument::clearCellClipboard()
{
    qDeleteAll(mCellClipboard);
    mCellClipboard.clear();
}

void WorldDocument::removePropertyDefinition(PropertyHolder *ph, PropertyDef *pd)
{
    int index = 0;
    foreach (Property *p, ph->properties()) {
        if (p->mDefinition == pd) {
            undoStack()->push(new RemovePropertyFromPH(this, ph, index, p));
            break;
        }
        ++index;
    }
}

void WorldDocument::removeTemplate(PropertyHolder *ph, PropertyTemplate *pt)
{
    int index = ph->templates().indexOf(pt);
    if (index != -1)
        undoStack()->push(new RemoveTemplateFromPH(this, ph, index, pt));
}

void WorldDocument::removeTemplateFromCellClipboard(int index)
{
    PropertyTemplate *pt = mWorld->propertyTemplates().at(index);
    foreach (WorldCellContents *contents, mCellClipboard)
        contents->removeTemplate(pt);
}

void WorldDocument::removePropertyDefFromCellClipboard(int index)
{
    PropertyDef *pd = mWorld->propertyDefinitions().at(index);
    foreach (WorldCellContents *contents, mCellClipboard)
        contents->removePropertyDef(pd);
}

/////

WorldDocumentUndoRedo::WorldDocumentUndoRedo(WorldDocument *worldDoc)
    : mWorldDoc(worldDoc)
    , mWorld(worldDoc->world())
{
}

void WorldDocumentUndoRedo::addPropertyDefinition(int index, PropertyDef *pd)
{
    mWorld->addPropertyDefinition(index, pd);
    emit propertyDefinitionAdded(pd, index);
}

PropertyDef *WorldDocumentUndoRedo::removePropertyDefinition(int index)
{
    emit propertyDefinitionAboutToBeRemoved(index);
    mWorldDoc->removePropertyDefFromCellClipboard(index);
    return mWorld->removePropertyDefinition(index);
}

void WorldDocumentUndoRedo::changePropertyDefinition(PropertyDef *pd, const QString &name, const QString &defValue, const QString &desc)
{
    pd->mName = name;
    pd->mDefaultValue = defValue;
    pd->mDescription = desc;
    emit propertyDefinitionChanged(pd);
}


void WorldDocumentUndoRedo::changeTemplate(PropertyTemplate *pt, const QString &name, const QString &desc)
{
    pt->mName = name;
    pt->mDescription = desc;
    emit templateChanged(pt);
}

void WorldDocumentUndoRedo::addProperty(PropertyHolder *ph, int index, Property *p)
{
    ph->addProperty(index, p);
    emit propertyAdded(ph, index);
}

Property *WorldDocumentUndoRedo::removeProperty(PropertyHolder *ph, int index)
{
    emit propertyAboutToBeRemoved(ph, index);
    return ph->removeProperty(index);
}

void WorldDocumentUndoRedo::addTemplateToWorld(int index, PropertyTemplate *pt)
{
    mWorld->addPropertyTemplate(index, pt);
    emit templateAdded(index);
}

PropertyTemplate *WorldDocumentUndoRedo::removeTemplateFromWorld(int index)
{
    emit templateAboutToBeRemoved(index);
    mWorldDoc->removeTemplateFromCellClipboard(index);
    return mWorld->removeTemplate(index);
}

void WorldDocumentUndoRedo::addTemplate(PropertyHolder *ph, int index, PropertyTemplate *pt)
{
    ph->addTemplate(index, pt);
    emit templateAdded(ph, index);
}

PropertyTemplate *WorldDocumentUndoRedo::removeTemplate(PropertyHolder *ph, int index)
{
    emit templateAboutToBeRemoved(ph, index);
    return ph->removeTemplate(index);
}

QString WorldDocumentUndoRedo::setPropertyValue(PropertyHolder *ph, Property *p, const QString &value)
{
    QString oldValue = p->mValue;
    p->mValue = value;
    emit propertyValueChanged(ph, ph->properties().indexOf(p));
    return oldValue;
}

void WorldDocumentUndoRedo::insertObjectGroup(int index, WorldObjectGroup *og)
{
    mWorld->insertObjectGroup(index, og);
    emit objectGroupAdded(index);
}

WorldObjectGroup *WorldDocumentUndoRedo::removeObjectGroup(int index)
{
    emit objectGroupAboutToBeRemoved(index);
    return mWorld->removeObjectGroup(index);
}

QString WorldDocumentUndoRedo::changeObjectGroupName(WorldObjectGroup *og, const QString &name)
{
    Q_ASSERT(!name.isEmpty());
    Q_ASSERT(!mWorld->objectGroups().contains(name));
    QString oldName = og->name();
    og->setName(name);
    emit objectGroupNameChanged(og);
    return oldName;
}

QColor WorldDocumentUndoRedo::changeObjectGroupColor(WorldObjectGroup *og, const QColor &color)
{
    QColor oldColor = og->color();
    og->setColor(color);
    emit objectGroupColorChanged(og);
    return oldColor;
}

int WorldDocumentUndoRedo::reorderObjectGroup(WorldObjectGroup *og, int index)
{
    int oldIndex = mWorld->objectGroups().indexOf(og);
    emit objectGroupAboutToBeReordered(oldIndex);
    mWorld->removeObjectGroup(oldIndex);
    mWorld->insertObjectGroup(index, og);
    Q_ASSERT(mWorld->objectGroups().indexOf(og) == index);
    emit objectGroupReordered(index);
    return oldIndex;
}

ObjectType *WorldDocumentUndoRedo::changeObjectGroupDefType(WorldObjectGroup *og, ObjectType *ot)
{
    Q_ASSERT(ot);
    ObjectType *oldType = og->type();
    og->setType(ot);
    //emit objectGroupDefTypeChanged(og);
    return oldType;
}

void WorldDocumentUndoRedo::insertObjectType(int index, ObjectType *ot)
{
    mWorld->insertObjectType(index, ot);
    emit objectTypeAdded(index);
}

ObjectType *WorldDocumentUndoRedo::removeObjectType(int index)
{
    emit objectTypeAboutToBeRemoved(index);
    return mWorld->removeObjectType(index);
}

QString WorldDocumentUndoRedo::changeObjectTypeName(ObjectType *objType, const QString &name)
{
    Q_ASSERT(!name.isEmpty());
    Q_ASSERT(!mWorld->objectTypes().contains(name));
    QString oldName = objType->name();
    objType->setName(name);
    emit objectTypeNameChanged(objType);
    return oldName;
}

QString WorldDocumentUndoRedo::setCellMapName(WorldCell *cell, const QString &mapName)
{
    QString oldName = cell->mapFilePath();
    emit cellMapFileAboutToChange(cell);
    cell->setMapFilePath(mapName);
    emit cellMapFileChanged(cell);
    return oldName;
}

WorldCellContents *WorldDocumentUndoRedo::setCellContents(WorldCell *cell, WorldCellContents *contents)
{
    qDebug() << "WorldDocumentUndoRedo::setCellContents" << cell->pos();
    emit cellContentsAboutToChange(cell);
    WorldCellContents *oldContents = new WorldCellContents(cell, true);
    contents->setCellContents(cell, true);
    emit cellContentsChanged(cell);
    return oldContents;
}

void WorldDocumentUndoRedo::addCellLot(WorldCell *cell, int index, WorldCellLot *lot)
{
    cell->insertLot(index, lot);
    emit cellLotAdded(cell, index);
}

WorldCellLot *WorldDocumentUndoRedo::removeCellLot(WorldCell *cell, int index)
{
    WorldCellLot *lot = cell->lots().at(index);
    mWorldDoc->mSelectedLots.removeAll(lot); // FIXME: no signal?

    emit cellLotAboutToBeRemoved(cell, index);
    return cell->removeLot(index);
}

QPoint WorldDocumentUndoRedo::moveCellLot(WorldCellLot *lot, const QPoint &pos)
{
    QPoint oldPos = lot->pos();
    lot->setPos(pos);
    emit cellLotMoved(lot);
    return oldPos;
}

int WorldDocumentUndoRedo::setLotLevel(WorldCellLot *lot, int level)
{
    int oldLevel = lot->level();
    lot->setLevel(level);

    // Update the lot's position so it stays in the same visual location
    QPoint offset(3, 3);
    int delta = level - oldLevel;
    lot->setPos(lot->pos() + offset * delta);

    emit lotLevelChanged(lot);
    return oldLevel;
}

void WorldDocumentUndoRedo::addCellObject(WorldCell *cell, int index, WorldCellObject *obj)
{
    cell->insertObject(index, obj);
    emit cellObjectAdded(cell, index);
}

WorldCellObject *WorldDocumentUndoRedo::removeCellObject(WorldCell *cell, int index)
{
    WorldCellObject *obj = cell->objects().at(index);
    mWorldDoc->mSelectedObjects.removeAll(obj); // FIXME: no signal?

    emit cellObjectAboutToBeRemoved(cell, index);
    return cell->removeObject(index);
}

QPointF WorldDocumentUndoRedo::moveCellObject(WorldCellObject *obj, const QPointF &pos)
{
    QPointF oldPos = obj->pos();
    obj->setPos(pos);
    emit cellObjectMoved(obj);
    return oldPos;
}

QSizeF WorldDocumentUndoRedo::resizeCellObject(WorldCellObject *obj, const QSizeF &size)
{
    QSizeF oldSize = obj->size();
    obj->setWidth(size.width());
    obj->setHeight(size.height());
    emit cellObjectResized(obj);
    return oldSize;
}

int WorldDocumentUndoRedo::setObjectLevel(WorldCellObject *obj, int level)
{
    int oldLevel = obj->level();
    emit objectLevelAboutToChange(obj);
    obj->setLevel(level);

    // Update the object's position so it stays in the same visual location
    QPoint offset(3, 3);
    int delta = level - oldLevel;
    obj->setPos(obj->pos() + offset * delta);

    emit objectLevelChanged(obj);
    return oldLevel;
}

QString WorldDocumentUndoRedo::setCellObjectName(WorldCellObject *obj, const QString &name)
{
    QString oldName = obj->name();
    obj->setName(name);
    emit cellObjectNameChanged(obj);
    return oldName;
}

WorldObjectGroup *WorldDocumentUndoRedo::setCellObjectGroup(WorldCellObject *obj, WorldObjectGroup *og)
{
    WorldObjectGroup *oldGroup = obj->group();
    emit cellObjectGroupAboutToChange(obj);
    obj->setGroup(og);
    emit cellObjectGroupChanged(obj);
    return oldGroup;
}

ObjectType *WorldDocumentUndoRedo::setCellObjectType(WorldCellObject *obj, ObjectType *type)
{
    ObjectType *oldType = obj->type();
    obj->setType(type);
    emit cellObjectTypeChanged(obj);
    return oldType;
}

int WorldDocumentUndoRedo::reorderCellObject(WorldCellObject *obj, int index)
{
    WorldCell *cell = obj->cell();
    int oldIndex = cell->objects().indexOf(obj);
//    emit cellObjectAboutToBeReordered(oldIndex);
    cell->removeObject(oldIndex);
    if (index > oldIndex)
        index--;
    cell->insertObject(index, obj);
    Q_ASSERT(cell->objects().indexOf(obj) == index);
    emit cellObjectReordered(obj);
    return oldIndex;
}

QList<WorldCell *> WorldDocumentUndoRedo::setSelectedCells(const QList<WorldCell *> &selection)
{
    QList<WorldCell *> oldSelection = mWorldDoc->selectedCells();
    mWorldDoc->mSelectedCells = selection;
    emit selectedCellsChanged();
    return oldSelection;
}
