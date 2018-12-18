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

#include "bmptotmx.h"
#include "celldocument.h"
#include "documentmanager.h"
#include "luawriter.h"
#include "mainwindow.h"
#include "undoredo.h"
#include "world.h"
#include "worldscene.h"
#include "worldview.h"
#include "worldwriter.h"

#include "mapbox/mapboxundo.h"

#include <QDebug>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
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
    connect(&mUndoRedo, SIGNAL(propertyRemoved(PropertyHolder*,int)),
            SIGNAL(propertyRemoved(PropertyHolder*,int)));
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
    connect(&mUndoRedo, SIGNAL(templateRemoved(PropertyHolder*,int)),
            SIGNAL(templateRemoved(PropertyHolder*,int)));

    connect(&mUndoRedo, SIGNAL(worldAboutToResize(QSize)),
            SIGNAL(worldAboutToResize(QSize)));
    connect(&mUndoRedo, SIGNAL(worldResized(QSize)),
            SIGNAL(worldResized(QSize)));

    connect(&mUndoRedo, SIGNAL(generateLotSettingsChanged()),
            SIGNAL(generateLotSettingsChanged()));

    connect(&mUndoRedo, SIGNAL(cellAdded(WorldCell*)),
            SIGNAL(cellAdded(WorldCell*)));
    connect(&mUndoRedo, SIGNAL(cellAboutToBeRemoved(WorldCell*)),
            SIGNAL(cellAboutToBeRemoved(WorldCell*)));

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

    connect(&mUndoRedo, SIGNAL(mapboxFeatureAdded(WorldCell*,int)),
            SIGNAL(mapboxFeatureAdded(WorldCell*,int)));
    connect(&mUndoRedo, SIGNAL(mapboxFeatureAboutToBeRemoved(WorldCell*,int)),
            SIGNAL(mapboxFeatureAboutToBeRemoved(WorldCell*,int)));
    connect(&mUndoRedo, SIGNAL(mapboxPointMoved(WorldCell*,int,int)),
            SIGNAL(mapboxPointMoved(WorldCell*,int,int)));
    connect(&mUndoRedo, &WorldDocumentUndoRedo::mapboxPropertiesChanged,
            this, &WorldDocument::mapboxPropertiesChanged);

    connect(&mUndoRedo, SIGNAL(roadAdded(int)),
            SIGNAL(roadAdded(int)));
    connect(&mUndoRedo, SIGNAL(roadAboutToBeRemoved(int)),
            SIGNAL(roadAboutToBeRemoved(int)));
    connect(&mUndoRedo, SIGNAL(roadRemoved(Road*)),
            SIGNAL(roadRemoved(Road*)));
    connect(&mUndoRedo, SIGNAL(roadCoordsChanged(int)),
            SIGNAL(roadCoordsChanged(int)));
    connect(&mUndoRedo, SIGNAL(roadWidthChanged(int)),
            SIGNAL(roadWidthChanged(int)));
    connect(&mUndoRedo, SIGNAL(roadTileNameChanged(int)),
            SIGNAL(roadTileNameChanged(int)));
    connect(&mUndoRedo, SIGNAL(roadLinesChanged(int)),
            SIGNAL(roadLinesChanged(int)));

    connect(&mUndoRedo, SIGNAL(selectedCellsChanged()),
            SIGNAL(selectedCellsChanged()));

    connect(&mUndoRedo, SIGNAL(bmpAdded(int)),
            SIGNAL(bmpAdded(int)));
    connect(&mUndoRedo, SIGNAL(bmpAboutToBeRemoved(int)),
            SIGNAL(bmpAboutToBeRemoved(int)));
    connect(&mUndoRedo, SIGNAL(bmpCoordsChanged(int)),
            SIGNAL(bmpCoordsChanged(int)));

    connect(&mUndoRedo, SIGNAL(propertyEnumAdded(int)),
            SIGNAL(propertyEnumAdded(int)));
    connect(&mUndoRedo, SIGNAL(propertyEnumAboutToBeRemoved(int)),
            SIGNAL(propertyEnumAboutToBeRemoved(int)));
    connect(&mUndoRedo, SIGNAL(propertyEnumChanged(PropertyEnum*)),
            SIGNAL(propertyEnumChanged(PropertyEnum*)));
    connect(&mUndoRedo, SIGNAL(propertyEnumChoicesChanged(PropertyEnum*)),
            SIGNAL(propertyEnumChoicesChanged(PropertyEnum*)));
}

WorldDocument::~WorldDocument()
{
    delete mUndoStack; // before mWorld is deleted
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

    QString luaFileName = world()->getLuaSettings().spawnPointsFile;
    if (!luaFileName.isEmpty()) {
        LuaWriter writer;
        if (!writer.writeSpawnPoints(world(), luaFileName)) {
            QMessageBox::warning(MainWindow::instance(), tr("Error saving spawnpoints"),
                                 tr("An error occurred saving the LUA spawnpoints file.\n%1\n\n%2")
                                 .arg(writer.errorString())
                                 .arg(QDir::toNativeSeparators(luaFileName)));
        }
    }

    luaFileName = world()->getLuaSettings().worldObjectsFile;
    if (!luaFileName.isEmpty()) {
        LuaWriter writer;
        if (!writer.writeWorldObjects(world(), luaFileName)) {
            QMessageBox::warning(MainWindow::instance(), tr("Error saving objects"),
                                 tr("An error occurred saving the LUA objects file.\n%1\n\n%2")
                                 .arg(writer.errorString())
                                 .arg(QDir::toNativeSeparators(luaFileName)));
        }
    }

#if 0
    {
        QFileInfo fi(filePath);
        QString luaPath = fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName() + QLatin1String(".lua");
        LuaWriter writer;
        if (!writer.writeWorld(mWorld, luaPath)) {
            error += writer.errorString();
        }
    }
#endif

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

void WorldDocument::setSelectedRoads(const QList<Road *> &selectedRoads)
{
    QList<Road*> selection;
    foreach (Road *road, selectedRoads) {
        if (!selection.contains(road))
            selection.append(road);
        else
            qWarning("duplicate roads passed to setSelectedRoads");
    }
    mSelectedRoads = selection;
    emit selectedRoadsChanged();
}

void WorldDocument::setSelectedBMPs(const QList<WorldBMP *> &selectedBMPs)
{
    QList<WorldBMP*> selection;
    foreach (WorldBMP *bmp, selectedBMPs) {
        if (!selection.contains(bmp))
            selection.append(bmp);
        else
            qWarning("duplicate BMPs passed to setSelectedBMPs");
    }
    mSelectedBMPs = selection;
    emit selectedBMPsChanged();
}

void WorldDocument::removeRoadFromSelection(Road *road)
{
    if (mSelectedRoads.contains(road)) {
        mSelectedRoads.removeAll(road);
        emit selectedRoadsChanged();
    }
}

void WorldDocument::removeBMPFromSelection(WorldBMP *bmp)
{
    if (mSelectedBMPs.contains(bmp)) {
        mSelectedBMPs.removeAll(bmp);
        emit selectedBMPsChanged();
    }
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

void WorldDocument::resizeWorld(const QSize &newSize)
{
    mUndoStack->push(new ResizeWorld(this, newSize));
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

void WorldDocument::addMapboxFeature(WorldCell *cell, int index, MapBoxFeature *feature)
{
    Q_ASSERT(!cell->mapBox().mFeatures.contains(feature));
    Q_ASSERT(index >= 0 && index <= cell->mapBox().mFeatures.size());
    undoStack()->push(new AddMapboxFeature(this, cell, index, feature));
}

void WorldDocument::removeMapboxFeature(WorldCell *cell, int index)
{
    Q_ASSERT(index >= 0 && index < cell->mapBox().mFeatures.size());
    undoStack()->push(new RemoveMapboxFeature(this, cell, index));
}

void WorldDocument::moveMapboxPoint(WorldCell *cell, int featureIndex, int pointIndex, const MapBoxPoint &point)
{
    Q_ASSERT(featureIndex >= 0 && featureIndex < cell->mapBox().mFeatures.size());
    undoStack()->push(new MoveMapboxPoint(this, cell, featureIndex, pointIndex, point));
}

void WorldDocument::addMapboxProperty(WorldCell *cell, int featureIndex, int propertyIndex, const MapBoxProperty &property)
{
    Q_ASSERT(featureIndex >= 0 && featureIndex < cell->mapBox().mFeatures.size());
    undoStack()->push(new AddMapboxProperty(this, cell, featureIndex, propertyIndex, property));
}

void WorldDocument::removeMapboxProperty(WorldCell *cell, int featureIndex, int propertyIndex)
{
    Q_ASSERT(featureIndex >= 0 && featureIndex < cell->mapBox().mFeatures.size());
    undoStack()->push(new RemoveMapboxProperty(this, cell, featureIndex, propertyIndex));
}

void WorldDocument::setMapboxProperty(WorldCell *cell, int featureIndex, int propertyIndex, const MapBoxProperty &property)
{
    Q_ASSERT(featureIndex >= 0 && featureIndex < cell->mapBox().mFeatures.size());
    undoStack()->push(new SetMapboxProperty(this, cell, featureIndex, propertyIndex, property));
}

void WorldDocument::insertRoad(int index, Road *road)
{
    Q_ASSERT(!world()->roads().contains(road));
    Q_ASSERT(index >= 0 && index <= world()->roads().size());
    undoStack()->push(new AddRoad(this, index, road));
}

void WorldDocument::removeRoad(int index)
{
    Q_ASSERT(index >= 0 && index < world()->roads().size());
    undoStack()->push(new RemoveRoad(this, index));
}

void WorldDocument::changeRoadCoords(Road *road,
                                     const QPoint &start, const QPoint &end)
{
    undoStack()->push(new ChangeRoadCoords(this, road, start, end));
}

void WorldDocument::changeRoadWidth(Road *road, int newWidth)
{
    undoStack()->push(new ChangeRoadWidth(this, road, newWidth));
}

void WorldDocument::changeRoadTileName(Road *road, const QString &tileName)
{
    undoStack()->push(new ChangeRoadTileName(this, road, tileName));
}

void WorldDocument::changeRoadLines(Road *road, TrafficLines *lines)
{
    undoStack()->push(new ChangeRoadLines(this, road, lines));
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
    foreach (PropertyTemplate *pt, mWorld->propertyTemplates())
        removePropertyDefinition(pt, pd);
    foreach (WorldCell *cell, mWorld->cells()) {
        removePropertyDefinition(cell, pd);
        foreach (WorldCellObject *obj, cell->objects())
            removePropertyDefinition(obj, pd);
    }

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

void WorldDocument::addTemplate(PropertyTemplate *pt)
{
    int index = mWorld->propertyTemplates().size();
    undoStack()->push(new AddTemplateToWorld(this, index, pt));
}

void WorldDocument::removeTemplate(int index)
{
    PropertyTemplate *remove = mWorld->propertyTemplates().at(index);

    undoStack()->beginMacro(tr("Remove Template (%1)").arg(remove->mName));

    // Remove the template from other templates, cells and objects
    foreach (PropertyTemplate *pt, mWorld->propertyTemplates())
        removeTemplate(pt, remove);
    foreach (WorldCell *cell, mWorld->cells()) {
        removeTemplate(cell, remove);
        foreach (WorldCellObject *obj, cell->objects())
            removeTemplate(obj, remove);
    }

    undoStack()->push(new RemoveTemplateFromWorld(this, index, remove));

    undoStack()->endMacro();
}

void WorldDocument::changePropertyDefinition(PropertyDef *pd, const QString &name,
                                             const QString &defValue, const QString &desc,
                                             PropertyEnum *pe)
{
    undoStack()->beginMacro(tr("Edit Property Definition (%1)").arg(pd->mName));

    if (pe != pd->mEnum) {
        // Remove unknown values from every affected property.
        QStringList choices;
        if (pe) choices = pe->values();
        foreach (PropertyTemplate *pt, mWorld->propertyTemplates())
            syncPropertyEnumChoices(pt, pd, choices);
        foreach (WorldCell *cell, mWorld->cells()) {
            syncPropertyEnumChoices(cell, pd, choices);
            foreach (WorldCellObject *obj, cell->objects())
                syncPropertyEnumChoices(obj, pd, choices);
        }
    }

    undoStack()->push(new EditPropertyDef(this, pd, name, defValue, desc, pe));

    undoStack()->endMacro();
}

void WorldDocument::changePropertyDefinition(PropertyDef *pd, PropertyDef *other)
{
    undoStack()->push(new EditPropertyDef(this, pd, other->mName,
                                          other->mDefaultValue,
                                          other->mDescription,
                                          other->mEnum));
}

void WorldDocument::changeTemplate(PropertyTemplate *pt, const QString &name, const QString &desc)
{
    undoStack()->push(new ChangeTemplate(this, pt, name, desc));
}

void WorldDocument::changeTemplate(PropertyTemplate *pt, PropertyTemplate *other)
{
    for (int i = 0; i < pt->templates().size(); i++)
        removeTemplate(pt, i);
    foreach (PropertyTemplate *ptOther, other->templates())
        addTemplate(pt, ptOther->mName);

    for (int i = 0; i < pt->properties().size(); i++)
        removeProperty(pt, i);
    foreach (Property *pOther, other->properties())
        addProperty(pt, pOther->mDefinition->mName, pOther->mValue); // FIXME: copy mNote

    changeTemplate(pt, other->mName, other->mDescription);
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
    Q_UNUSED(oldIndex)
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

void WorldDocument::changeObjectGroup(WorldObjectGroup *og, WorldObjectGroup *other)
{
    if (og->name() != other->name())
        changeObjectGroupName(og, other->name());
    if (og->color() != other->color())
        changeObjectGroupColor(og, other->color());
    if (og->type()->name() != other->type()->name()) {
        ObjectType *ot = mWorld->objectTypes().find(other->type()->name());
        Q_ASSERT(ot);
        changeObjectGroupDefType(og, ot);
    }
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

void WorldDocument::changeObjectType(ObjectType *ot, ObjectType *other)
{
    if (ot->name() != other->name())
        changeObjectTypeName(ot, other->name());
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
    addProperty(ph, name, pd->mDefaultValue);
}

void WorldDocument::addProperty(PropertyHolder *ph, const QString &name, const QString &value)
{
    PropertyDef *pd = mWorld->propertyDefinitions().findPropertyDef(name);
    Property *p = new Property(pd, value);
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

void WorldDocument::changeBMPToTMXSettings(const BMPToTMXSettings &settings)
{
    undoStack()->push(new ChangeBMPToTMXSettings(this, settings));
}

void WorldDocument::changeTMXToBMPSettings(const TMXToBMPSettings &settings)
{
    undoStack()->push(new ChangeTMXToBMPSettings(this, settings));
}

void WorldDocument::changeGenerateLotsSettings(const GenerateLotsSettings &settings)
{
    undoStack()->push(new ChangeGenerateLotsSettings(this, settings));
}

void WorldDocument::changeLuaSettings(const LuaSettings &settings)
{
    undoStack()->push(new ChangeLuaSettings(this, settings));
}

void WorldDocument::moveBMP(WorldBMP *bmp, const QPoint &topLeft)
{
    undoStack()->push(new MoveBMP(this, bmp, topLeft));
}

void WorldDocument::insertBMP(int index, WorldBMP *bmp)
{
    undoStack()->push(new AddBMP(this, index, bmp));
}

void WorldDocument::removeBMP(WorldBMP *bmp)
{
    int index = mWorld->bmps().indexOf(bmp);
    Q_ASSERT(index >= 0);
    undoStack()->push(new RemoveBMP(this, index));
}

void WorldDocument::addPropertyEnum(const QString &name, const QStringList &choices, bool multi)
{
    PropertyEnum *pe = new PropertyEnum(name, choices, multi);
    undoStack()->push(new AddPropertyEnum(this, mWorld->propertyEnums().size(), pe));
}

void WorldDocument::addPropertyEnum(PropertyEnum *pe)
{
    undoStack()->push(new AddPropertyEnum(this, mWorld->propertyEnums().size(), pe));
}

void WorldDocument::removePropertyEnum(PropertyEnum *pe)
{
    undoStack()->beginMacro(tr("Remove Property Enum (%1)").arg(pe->name()));

    // Remove the enum from all property definitions using it
    foreach (PropertyDef *pd, mWorld->propertyDefinitions()) {
        if (pd->mEnum == pe) {
            changePropertyDefinition(pd, pd->mName, pd->mDefaultValue,
                                     pd->mDescription, 0);
        }
    }

    int index = mWorld->propertyEnums().indexOf(pe);
    undoStack()->push(new RemovePropertyEnum(this, index));

    undoStack()->endMacro();
}

void WorldDocument::changePropertyEnum(PropertyEnum *pe, const QString &name, bool multi)
{
    undoStack()->push(new ChangePropertyEnum(this, pe, name, multi));
}

void WorldDocument::changePropertyEnum(PropertyEnum *pe, PropertyEnum *other)
{
    changePropertyEnum(pe, other->name(), other->isMulti());
    for (int i = 0; i < pe->values().size(); i++)
        removePropertyEnumChoice(pe, i);
    for (int i = 0; i < other->values().size(); i++)
        insertPropertyEnumChoice(pe, i, other->values()[i]);
}

void WorldDocument::insertPropertyEnumChoice(PropertyEnum *pe, int index, const QString &name)
{
    undoStack()->push(new AddPropertyEnumChoice(this, pe, index, name));
}

void WorldDocument::removePropertyEnumChoice(PropertyEnum *pe, int index)
{
    QString choice = pe->values()[index];
    undoStack()->beginMacro(tr("Remove Property Enum Choice (%1 [%2])").arg(pe->name()).arg(choice));

    // Update the values of every affected property.
    foreach (PropertyTemplate *pt, mWorld->propertyTemplates())
        removePropertyEnumChoice(pt, pe, choice);
    foreach (WorldCell *cell, mWorld->cells()) {
        removePropertyEnumChoice(cell, pe, choice);
        foreach (WorldCellObject *obj, cell->objects())
            removePropertyEnumChoice(obj, pe, choice);
    }

    undoStack()->push(new RemovePropertyEnumChoice(this, pe, index));

    undoStack()->endMacro();
}

void WorldDocument::renamePropertyEnumChoice(PropertyEnum *pe, int index, const QString &name)
{
    QString oldName = pe->values()[index];
    undoStack()->beginMacro(tr("Rename Property Enum Choice (%1 [%2])").arg(pe->name()).arg(oldName));

    // Update the values of every affected property.
    foreach (PropertyTemplate *pt, mWorld->propertyTemplates())
        renamePropertyEnumChoice(pt, pe, oldName, name);
    foreach (WorldCell *cell, mWorld->cells()) {
        renamePropertyEnumChoice(cell, pe, oldName, name);
        foreach (WorldCellObject *obj, cell->objects())
            renamePropertyEnumChoice(obj, pe, oldName, name);
    }

    undoStack()->push(new RenamePropertyEnumChoice(this, pe, index, name));

    undoStack()->endMacro();
}

void WorldDocument::emitCellMapFileAboutToChange(WorldCell *cell)
{
    emit cellMapFileAboutToChange(cell);
}

void WorldDocument::emitCellMapFileChanged(WorldCell *cell)
{
    emit cellMapFileChanged(cell);
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

void WorldDocument::removePropertyEnumChoice(PropertyHolder *ph, PropertyEnum *pe,
                                             const QString &name)
{
    foreach (Property *p, ph->properties()) {
        if (p->mDefinition->mEnum == pe) {
            QStringList values = p->mValue.split(QLatin1String(","), QString::SkipEmptyParts);
            values.removeAll(name);
            QString value = values.join(QLatin1String(","));
            if (value != p->mValue)
               setPropertyValue(ph, p, value);
        }
    }
}

void WorldDocument::renamePropertyEnumChoice(PropertyHolder *ph, PropertyEnum *pe,
                                             const QString &oldName, const QString &newName)
{
    foreach (Property *p, ph->properties()) {
        if (p->mDefinition->mEnum == pe) {
            QString value = p->mValue;
            value.replace(oldName, newName);
            if (value != p->mValue)
               setPropertyValue(ph, p, value);
        }
    }
}

void WorldDocument::syncPropertyEnumChoices(PropertyHolder *ph, PropertyDef *pd, const QStringList &choices)
{
    foreach (Property *p, ph->properties()) {
        if (p->mDefinition == pd) {
            QStringList values = p->mValue.split(QLatin1String(","), QString::SkipEmptyParts);
            for (int i = 0; i < values.size(); i++) {
                if (!choices.contains(values[i]))
                    values.removeAt(i--);
            }
            QString value = values.join(QLatin1String(","));
            if (value != p->mValue)
               setPropertyValue(ph, p, value);
        }
    }
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
    return mWorld->removePropertyDefinition(index);
}

void WorldDocumentUndoRedo::changePropertyDefinition(PropertyDef *pd, const QString &name,
                                                     const QString &defValue, const QString &desc,
                                                     PropertyEnum *pe)
{
    pd->mName = name;
    pd->mDefaultValue = defValue;
    pd->mDescription = desc;
    pd->mEnum = pe;
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
    Property *p = ph->removeProperty(index);
    emit propertyRemoved(ph, index);
    return p;
}

void WorldDocumentUndoRedo::addTemplateToWorld(int index, PropertyTemplate *pt)
{
    mWorld->addPropertyTemplate(index, pt);
    emit templateAdded(index);
}

PropertyTemplate *WorldDocumentUndoRedo::removeTemplateFromWorld(int index)
{
    emit templateAboutToBeRemoved(index);
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
    PropertyTemplate *old = ph->removeTemplate(index);
    emit templateRemoved(ph, index);
    return old;
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

QSize WorldDocumentUndoRedo::resizeWorld(const QSize &newSize, QVector<WorldCell *> &cells)
{
    DocumentManager *docMgr = DocumentManager::instance();
    QList<WorldCell*> selection = mWorldDoc->selectedCells();
    QRect newBounds(QPoint(0, 0), newSize);
    for (int x = 0; x < mWorld->width(); x++) {
        for (int y = 0; y < mWorld->height(); y++) {
            if (!newBounds.contains(x, y)) {
                WorldCell *cell = mWorld->cellAt(x, y);
                emit cellAboutToBeRemoved(cell);
                // Deselect any cells that are getting chopped off.
                selection.removeAll(cell);
                // Close any cell documents for cells that are getting chopped off.
                if (CellDocument *doc = docMgr->findDocument(cell))
                    docMgr->closeDocument(doc);
            }
        }
    }
    if (selection != mWorldDoc->selectedCells())
        setSelectedCells(selection);

    emit worldAboutToResize(newSize);
    QSize oldSize = mWorld->size();
    mWorld->swapCells(cells);
    mWorld->setSize(newSize);
    emit worldResized(oldSize);

    for (int x = 0; x < newSize.width(); x++) {
        for (int y = 0; y < newSize.height(); y++) {
            if (!QRect(QPoint(), oldSize).contains(x, y)) {
                emit cellAdded(mWorld->cellAt(x, y));
            }
        }
    }

    return oldSize;
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

void WorldDocumentUndoRedo::addMapboxFeature(WorldCell *cell, int index, MapBoxFeature *feature)
{
    cell->mapBox().mFeatures.insert(index, feature);
    emit mapboxFeatureAdded(cell, index);
}

MapBoxFeature *WorldDocumentUndoRedo::removeMapboxFeature(WorldCell *cell, int index)
{
    emit mapboxFeatureAboutToBeRemoved(cell, index);
    return cell->mapBox().mFeatures.takeAt(index);
}

MapBoxPoint WorldDocumentUndoRedo::moveMapboxPoint(WorldCell *cell, int featureIndex, int pointIndex, const MapBoxPoint &point)
{
    MapBoxFeature* feature = cell->mapBox().mFeatures[featureIndex];
    MapBoxCoordinates& coords = feature->mGeometry.mCoordinates[0];
    MapBoxPoint old = coords[pointIndex];
    coords[pointIndex] = point;
    emit mapboxPointMoved(cell, featureIndex, pointIndex);
    return old;
}

void WorldDocumentUndoRedo::addMapboxProperty(WorldCell *cell, int featureIndex, int propertyIndex, const MapBoxProperty &property)
{
    MapBoxFeature* feature = cell->mapBox().mFeatures[featureIndex];
    feature->properties().insert(propertyIndex, property);
    emit mapboxPropertiesChanged(cell, featureIndex);
}

MapBoxProperty WorldDocumentUndoRedo::removeMapboxProperty(WorldCell *cell, int featureIndex, int propertyIndex)
{
    MapBoxFeature* feature = cell->mapBox().mFeatures[featureIndex];
    MapBoxProperty old = feature->properties().takeAt(propertyIndex);
    emit mapboxPropertiesChanged(cell, featureIndex);
    return old;
}

MapBoxProperty WorldDocumentUndoRedo::setMapboxProperty(WorldCell *cell, int featureIndex, int propertyIndex, const MapBoxProperty &property)
{
    MapBoxFeature* feature = cell->mapBox().mFeatures[featureIndex];
    MapBoxProperty old = feature->properties().at(propertyIndex);
    feature->properties().replace(propertyIndex, property);
    emit mapboxPropertiesChanged(cell, featureIndex);
    return old;
}

void WorldDocumentUndoRedo::insertRoad(int index, Road *road)
{
    mWorld->insertRoad(index, road);
    emit roadAdded(index);
}

Road *WorldDocumentUndoRedo::removeRoad(int index)
{
    Road *road = mWorld->roads().at(index);
    Q_ASSERT(road);

    // Must make sure to remove the road from the selection so WorldScene
    // can update its mSelectedRoadItems
    mWorldDoc->removeRoadFromSelection(road);

    emit roadAboutToBeRemoved(index);
    road = mWorld->removeRoad(index);
    emit roadRemoved(road);
    return road;
}

void WorldDocumentUndoRedo::changeRoadCoords(Road *road, const
                                             QPoint &start, const QPoint &end,
                                             QPoint &oldStart, QPoint &oldEnd)
{
    oldStart = road->start();
    oldEnd = road->end();
    road->setCoords(start, end);
    emit roadCoordsChanged(mWorld->roads().indexOf(road));
}

int WorldDocumentUndoRedo::changeRoadWidth(Road *road, int newWidth)
{
    int oldWidth = road->width();
    road->setWidth(newWidth);
    emit roadWidthChanged(mWorld->roads().indexOf(road));
    return oldWidth;
}

QString WorldDocumentUndoRedo::changeRoadTileName(Road *road, const QString &tileName)
{
    QString old = road->tileName();
    road->setTileName(tileName);
    emit roadTileNameChanged(mWorld->roads().indexOf(road));
    return old;
}

TrafficLines *WorldDocumentUndoRedo::changeRoadLines(Road *road, TrafficLines *lines)
{
    TrafficLines *old = road->trafficLines();
    road->setTrafficLines(lines);
    emit roadLinesChanged(mWorld->roads().indexOf(road));
    return old;
}

QList<WorldCell *> WorldDocumentUndoRedo::setSelectedCells(const QList<WorldCell *> &selection)
{
    QList<WorldCell *> oldSelection = mWorldDoc->selectedCells();
    mWorldDoc->mSelectedCells = selection;
    emit selectedCellsChanged();
    return oldSelection;
}

BMPToTMXSettings WorldDocumentUndoRedo::changeBMPToTMXSettings(const BMPToTMXSettings &settings)
{
    BMPToTMXSettings old = mWorld->getBMPToTMXSettings();
    mWorld->setBMPToTMXSettings(settings);
    return old;
}

TMXToBMPSettings WorldDocumentUndoRedo::changeTMXToBMPSettings(const TMXToBMPSettings &settings)
{
    TMXToBMPSettings old = mWorld->getTMXToBMPSettings();
    mWorld->setTMXToBMPSettings(settings);
    return old;
}

GenerateLotsSettings WorldDocumentUndoRedo::changeGenerateLotsSettings(const GenerateLotsSettings &settings)
{
    GenerateLotsSettings old = mWorld->getGenerateLotsSettings();
    mWorld->setGenerateLotsSettings(settings);
    emit generateLotSettingsChanged();
    return old;
}

LuaSettings WorldDocumentUndoRedo::changeLuaSettings(const LuaSettings &settings)
{
    LuaSettings old = mWorld->getLuaSettings();
    mWorld->setLuaSettings(settings);
    return old;
}

QPoint WorldDocumentUndoRedo::moveBMP(WorldBMP *bmp, const QPoint &topLeft)
{
    QPoint old = bmp->pos();
    bmp->setPos(topLeft);
    emit bmpCoordsChanged(mWorld->bmps().indexOf(bmp));
    return old;
}

void WorldDocumentUndoRedo::insertBMP(int index, WorldBMP *bmp)
{
    Q_ASSERT(!mWorld->bmps().contains(bmp));
    mWorld->insertBmp(index, bmp);
    emit bmpAdded(index);
}

WorldBMP *WorldDocumentUndoRedo::removeBMP(int index)
{
    WorldBMP *bmp = mWorld->bmps().at(index);

    mWorldDoc->removeBMPFromSelection(bmp);

    emit bmpAboutToBeRemoved(index);
    bmp = mWorld->removeBmp(index);
    // emit bmpRemoved(bmp)
    return bmp;
}

void WorldDocumentUndoRedo::insertPropertyEnum(int index, PropertyEnum *pe)
{
    mWorld->insertPropertyEnum(index, pe);
    emit propertyEnumAdded(index);
}

PropertyEnum *WorldDocumentUndoRedo::removePropertyEnum(int index)
{
    emit propertyEnumAboutToBeRemoved(index);
    return mWorld->removePropertyEnum(index);
}

void WorldDocumentUndoRedo::changePropertyEnum(PropertyEnum *pe, const QString &name, bool multi)
{
    pe->setName(name);
    pe->setMulti(multi);
    emit propertyEnumChanged(pe);
}

void WorldDocumentUndoRedo::insertPropertyEnumChoice(PropertyEnum *pe, int index, const QString &choice)
{
    QStringList choices = pe->values();
    choices.insert(index, choice);
    pe->setValues(choices);
    emit propertyEnumChoicesChanged(pe);
}

QString WorldDocumentUndoRedo::removePropertyEnumChoice(PropertyEnum *pe, int index)
{
    QStringList choices = pe->values();
    QString old = choices.takeAt(index);
    pe->setValues(choices);
    emit propertyEnumChoicesChanged(pe);
    return old;
}

QString WorldDocumentUndoRedo::renamePropertyEnumChoice(PropertyEnum *pe, int index, const QString &name)
{
    QStringList choices = pe->values();
    QString old = choices.at(index);
    choices[index] = name;
    pe->setValues(choices);
    emit propertyEnumChoicesChanged(pe);
    return old;
}
