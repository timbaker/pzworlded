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

#include "InGameMap/ingamemapundo.h"

#include <QDebug>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QUndoStack>
#include <WorldCell.h>

WorldDocument::WorldDocument(World *world, const QString &fileName)
    : Document(WorldDocType)
    , mWorld(world)
    , mFileName(fileName)
    , mUndoRedo(this)
{
    mUndoStack = new QUndoStack(this);

    // Forward all the signals from mUndoRedo to this object's signals

    connect(&mUndoRedo, &WorldDocumentUndoRedo::propertyAdded,
            this, &WorldDocument::propertyAdded);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::propertyAboutToBeRemoved,
            this, &WorldDocument::propertyAboutToBeRemoved);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::propertyRemoved,
            this, &WorldDocument::propertyRemoved);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::propertyValueChanged,
            this, &WorldDocument::propertyValueChanged);

    connect(&mUndoRedo, &WorldDocumentUndoRedo::propertyDefinitionAdded,
            this, &WorldDocument::propertyDefinitionAdded);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::propertyDefinitionAboutToBeRemoved,
            this, &WorldDocument::propertyDefinitionAboutToBeRemoved);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::propertyDefinitionChanged,
            this, &WorldDocument::propertyDefinitionChanged);

    connect(&mUndoRedo, &WorldDocumentUndoRedo::objectTypeAdded,
            this, &WorldDocument::objectTypeAdded);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::objectTypeAboutToBeRemoved,
            this, &WorldDocument::objectTypeAboutToBeRemoved);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::objectTypeNameChanged,
            this, &WorldDocument::objectTypeNameChanged);

    connect(&mUndoRedo, &WorldDocumentUndoRedo::objectGroupAdded,
            this, &WorldDocument::objectGroupAdded);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::objectGroupAboutToBeRemoved,
            this, &WorldDocument::objectGroupAboutToBeRemoved);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::objectGroupNameChanged,
            this, &WorldDocument::objectGroupNameChanged);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::objectGroupColorChanged,
            this, &WorldDocument::objectGroupColorChanged);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::objectGroupAboutToBeReordered,
            this, &WorldDocument::objectGroupAboutToBeReordered);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::objectGroupReordered,
            this, &WorldDocument::objectGroupReordered);

    connect(&mUndoRedo, qOverload<int>(&WorldDocumentUndoRedo::templateAdded),
            this, qOverload<int>(&WorldDocument::templateAdded));
    connect(&mUndoRedo, qOverload<int>(&WorldDocumentUndoRedo::templateAboutToBeRemoved),
            this, qOverload<int>(&WorldDocument::templateAboutToBeRemoved));
    connect(&mUndoRedo, &WorldDocumentUndoRedo::templateChanged,
            this, &WorldDocument::templateChanged);

    connect(&mUndoRedo, qOverload<PropertyHolder*,int>(&WorldDocumentUndoRedo::templateAdded),
            this, qOverload<PropertyHolder*,int>(&WorldDocument::templateAdded));
    connect(&mUndoRedo, qOverload<PropertyHolder*,int>(&WorldDocumentUndoRedo::templateAboutToBeRemoved),
            this, qOverload<PropertyHolder*,int>(&WorldDocument::templateAboutToBeRemoved));
    connect(&mUndoRedo, &WorldDocumentUndoRedo::templateRemoved,
            this, &WorldDocument::templateRemoved);

    connect(&mUndoRedo, &WorldDocumentUndoRedo::worldAboutToResize,
            this, &WorldDocument::worldAboutToResize);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::worldResized,
            this, &WorldDocument::worldResized);

    connect(&mUndoRedo, &WorldDocumentUndoRedo::generateLotSettingsChanged,
            this, &WorldDocument::generateLotSettingsChanged);

    connect(&mUndoRedo, &WorldDocumentUndoRedo::cellAdded,
            this, &WorldDocument::cellAdded);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::cellAboutToBeRemoved,
            this, &WorldDocument::cellAboutToBeRemoved);

    connect(&mUndoRedo, &WorldDocumentUndoRedo::cellMapFileAboutToChange,
            this, &WorldDocument::cellMapFileAboutToChange);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::cellMapFileChanged,
            this, &WorldDocument::cellMapFileChanged);

    connect(&mUndoRedo, &WorldDocumentUndoRedo::cellContentsAboutToChange,
            this, &WorldDocument::cellContentsAboutToChange);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::cellContentsChanged,
            this, &WorldDocument::cellContentsChanged);

    connect(&mUndoRedo, &WorldDocumentUndoRedo::cellLotAdded,
            this, &WorldDocument::cellLotAdded);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::cellLotAboutToBeRemoved,
            this, &WorldDocument::cellLotAboutToBeRemoved);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::cellLotMoved,
            this, &WorldDocument::cellLotMoved);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::lotLevelChanged,
            this, &WorldDocument::lotLevelChanged);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::cellLotReordered,
            this, &WorldDocument::cellLotReordered);

    connect(&mUndoRedo, &WorldDocumentUndoRedo::cellObjectAdded,
            this, &WorldDocument::cellObjectAdded);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::cellObjectAboutToBeRemoved,
            this, &WorldDocument::cellObjectAboutToBeRemoved);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::cellObjectMoved,
            this, &WorldDocument::cellObjectMoved);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::cellObjectResized,
            this, &WorldDocument::cellObjectResized);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::cellObjectNameChanged,
            this, &WorldDocument::cellObjectNameChanged);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::cellObjectGroupAboutToChange,
            this, &WorldDocument::cellObjectGroupAboutToChange);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::cellObjectGroupChanged,
            this, &WorldDocument::cellObjectGroupChanged);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::cellObjectTypeChanged,
            this, &WorldDocument::cellObjectTypeChanged);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::objectLevelAboutToChange,
            this, &WorldDocument::objectLevelAboutToChange);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::objectLevelChanged,
            this, &WorldDocument::objectLevelChanged);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::cellObjectReordered,
            this, &WorldDocument::cellObjectReordered);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::cellObjectPointMoved,
            this, &WorldDocument::cellObjectPointMoved);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::cellObjectPointsChanged,
            this, &WorldDocument::cellObjectPointsChanged);

    connect(&mUndoRedo, &WorldDocumentUndoRedo::inGameMapFeatureAdded,
            this, &WorldDocument::inGameMapFeatureAdded);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::inGameMapFeatureAboutToBeRemoved,
            this, &WorldDocument::inGameMapFeatureAboutToBeRemoved);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::inGameMapPointMoved,
            this, &WorldDocument::inGameMapPointMoved);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::inGameMapPropertiesChanged,
            this, &WorldDocument::inGameMapPropertiesChanged);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::inGameMapGeometryChanged,
            this, &WorldDocument::inGameMapGeometryChanged);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::inGameMapHoleAdded,
            this, &WorldDocument::inGameMapHoleAdded);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::inGameMapHoleRemoved,
            this, &WorldDocument::inGameMapHoleRemoved);

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

    connect(&mUndoRedo, &WorldDocumentUndoRedo::selectedCellsChanged,
            this, &WorldDocument::selectedCellsChanged);

    connect(&mUndoRedo, &WorldDocumentUndoRedo::bmpAdded,
            this, &WorldDocument::bmpAdded);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::bmpAboutToBeRemoved,
            this, &WorldDocument::bmpAboutToBeRemoved);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::bmpCoordsChanged,
            this, &WorldDocument::bmpCoordsChanged);

    connect(&mUndoRedo, &WorldDocumentUndoRedo::propertyEnumAdded,
            this, &WorldDocument::propertyEnumAdded);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::propertyEnumAboutToBeRemoved,
            this, &WorldDocument::propertyEnumAboutToBeRemoved);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::propertyEnumChanged,
            this, &WorldDocument::propertyEnumChanged);
    connect(&mUndoRedo, &WorldDocumentUndoRedo::propertyEnumChoicesChanged,
            this, &WorldDocument::propertyEnumChoicesChanged);
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

void WorldDocument::setSelectedInGameMapFeatures(const QList<InGameMapFeature *> &selected)
{
    QList<InGameMapFeature*> selection;
    foreach (auto *feature, selected) {
        if (!selection.contains(feature))
            selection.append(feature);
        else
            qWarning("duplicate features passed to setSelectedInGameMapFeatures");
    }
    mSelectedInGameMapFeatures = selection;
    emit selectedInGameMapFeaturesChanged();
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

void WorldDocument::reorderCellLot(WorldCellLot *lot, WorldCellLot *insertBefore)
{
    if (lot == insertBefore)
        return;
    const WorldCellLotList &lots = lot->cell()->lots();
    int index = insertBefore ? lots.indexOf(insertBefore) : lots.size();
    undoStack()->push(new ReorderCellLot(this, lot, index));
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
    ObjectType *objType = nullptr;
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

void WorldDocument::moveCellObjectPoint(WorldCell *cell, int objectIndex, int pointIndex, const WorldCellObjectPoint &point)
{
    Q_ASSERT(objectIndex >= 0 && objectIndex < cell->objects().size());
    undoStack()->push(new MoveCellObjectPoint(this, cell, objectIndex, pointIndex, point));
}

void WorldDocument::setCellObjectPoints(WorldCell *cell, int objectIndex, const WorldCellObjectPoints &points)
{
    Q_ASSERT(objectIndex >= 0 && objectIndex < cell->objects().size());
    undoStack()->push(new SetCellObjectPoints(this, cell, objectIndex, points));
}

void WorldDocument::setCellObjectPolylineWidth(WorldCell *cell, int objectIndex, int width)
{
    Q_ASSERT(objectIndex >= 0 && objectIndex < cell->objects().size());
    undoStack()->push(new SetCellObjectPolylineWidth(this, cell, objectIndex, width));
}

void WorldDocument::addInGameMapFeature(WorldCell *cell, int index, InGameMapFeature *feature)
{
    Q_ASSERT(!cell->inGameMap().mFeatures.contains(feature));
    Q_ASSERT(index >= 0 && index <= cell->inGameMap().mFeatures.size());
    undoStack()->push(new AddInGameMapFeature(this, cell, index, feature));
}

void WorldDocument::removeInGameMapFeature(WorldCell *cell, int index)
{
    Q_ASSERT(index >= 0 && index < cell->inGameMap().mFeatures.size());
    undoStack()->push(new RemoveInGameMapFeature(this, cell, index));
}

void WorldDocument::moveInGameMapPoint(WorldCell *cell, int featureIndex, int coordIndex, int pointIndex, const InGameMapPoint &point)
{
    Q_ASSERT(featureIndex >= 0 && featureIndex < cell->inGameMap().mFeatures.size());
    undoStack()->push(new MoveInGameMapPoint(this, cell, featureIndex, coordIndex, pointIndex, point));
}

void WorldDocument::addInGameMapProperty(WorldCell *cell, int featureIndex, int propertyIndex, const InGameMapProperty &property)
{
    Q_ASSERT(featureIndex >= 0 && featureIndex < cell->inGameMap().mFeatures.size());
    undoStack()->push(new AddInGameMapProperty(this, cell, featureIndex, propertyIndex, property));
}

void WorldDocument::removeInGameMapProperty(WorldCell *cell, int featureIndex, int propertyIndex)
{
    Q_ASSERT(featureIndex >= 0 && featureIndex < cell->inGameMap().mFeatures.size());
    undoStack()->push(new RemoveInGameMapProperty(this, cell, featureIndex, propertyIndex));
}

void WorldDocument::setInGameMapProperty(WorldCell *cell, int featureIndex, int propertyIndex, const InGameMapProperty &property)
{
    Q_ASSERT(featureIndex >= 0 && featureIndex < cell->inGameMap().mFeatures.size());
    undoStack()->push(new SetInGameMapProperty(this, cell, featureIndex, propertyIndex, property));
}

void WorldDocument::setInGameMapProperties(WorldCell *cell, int featureIndex, const InGameMapProperties &properties)
{
    Q_ASSERT(featureIndex >= 0 && featureIndex < cell->inGameMap().mFeatures.size());
    undoStack()->push(new SetInGameMapProperties(this, cell, featureIndex, properties));
}

void WorldDocument::setInGameMapCoordinates(WorldCell *cell, int featureIndex, int coordsIndex, const InGameMapCoordinates &coords)
{
    Q_ASSERT(featureIndex >= 0 && featureIndex < cell->inGameMap().mFeatures.size());
    undoStack()->push(new SetInGameMapCoordinates(this, cell, featureIndex, coordsIndex, coords));
}

void WorldDocument::addInGameMapHole(WorldCell *cell, int featureIndex, int holeIndex, const InGameMapCoordinates &hole)
{
    Q_ASSERT(featureIndex >= 0 && featureIndex < cell->inGameMap().mFeatures.size());
    undoStack()->push(new AddInGameMapHole(this, cell, featureIndex, holeIndex, hole));
}

void WorldDocument::removeInGameMapHole(WorldCell *cell, int featureIndex, int holeIndex)
{
    Q_ASSERT(featureIndex >= 0 && featureIndex < cell->inGameMap().mFeatures.size());
    undoStack()->push(new RemoveInGameMapHole(this, cell, featureIndex, holeIndex));
}

void WorldDocument::convertToInGameMapPolygon(WorldCell *cell, int featureIndex)
{
    Q_ASSERT(featureIndex >= 0 && featureIndex < cell->inGameMap().mFeatures.size());
    undoStack()->push(new ConvertToInGameMapPolygon(this, cell, featureIndex));
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
    pt->mName = name;
    pt->mDescription = desc;
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
            QStringList values = p->mValue.split(QLatin1String(","), Qt::SkipEmptyParts);
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
            QStringList values = p->mValue.split(QLatin1String(","), Qt::SkipEmptyParts);
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

int WorldDocumentUndoRedo::reorderCellLot(WorldCellLot *lot, int index)
{
    WorldCell *cell = lot->cell();
    int oldIndex = cell->indexOf(lot);
    cell->removeLot(oldIndex);
    if (index > oldIndex) {
        index--;
    } else {
        oldIndex++;
    }
    cell->insertLot(index, lot);
    Q_ASSERT(cell->indexOf(lot) == index);
    emit cellLotReordered(lot);
    return oldIndex;
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
    if (obj->points().isEmpty() == false) {
        WorldCellObjectPoints points = obj->points();
        points.translate(pos.x() - oldPos.x(), pos.y() - oldPos.y());
        obj->setPoints(points);
    }
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
    int oldIndex = cell->indexOf(obj);
//    emit cellObjectAboutToBeReordered(oldIndex);
    cell->removeObject(oldIndex);
    if (index > oldIndex) {
        index--;
    } else {
        oldIndex++;
    }
    cell->insertObject(index, obj);
    Q_ASSERT(cell->indexOf(obj) == index);
    emit cellObjectReordered(obj);
    return oldIndex;
}

WorldCellObjectPoint WorldDocumentUndoRedo::moveCellObjectPoint(WorldCell *cell, int objectIndex, int pointIndex, const WorldCellObjectPoint &point)
{
    WorldCellObject* object = cell->objects().at(objectIndex);
    const WorldCellObjectPoints& points = object->points();
    WorldCellObjectPoint old = points[pointIndex];
    object->setPoint(pointIndex, point);
    emit cellObjectPointMoved(cell, objectIndex, pointIndex);
    return old;
}

WorldCellObjectPoints WorldDocumentUndoRedo::setCellObjectPoints(WorldCell *cell, int objectIndex, const WorldCellObjectPoints &points)
{
    WorldCellObject* object = cell->objects().at(objectIndex);
    WorldCellObjectPoints old = object->points();
    object->setPoints(points);
    emit cellObjectPointsChanged(cell, objectIndex);
    return old;
}

int WorldDocumentUndoRedo::setCellObjectPolylineWidth(WorldCell *cell, int objectIndex, int width)
{
    WorldCellObject* object = cell->objects().at(objectIndex);
    int old = object->polylineWidth();
    object->setPolylineWidth(width);
    emit cellObjectPointsChanged(cell, objectIndex);
    return old;
}

void WorldDocumentUndoRedo::addInGameMapFeature(WorldCell *cell, int index, InGameMapFeature *feature)
{
    cell->inGameMap().mFeatures.insert(index, feature);
    emit inGameMapFeatureAdded(cell, index);
}

InGameMapFeature *WorldDocumentUndoRedo::removeInGameMapFeature(WorldCell *cell, int index)
{
    InGameMapFeature* feature = cell->inGameMap().features().at(index);
    mWorldDoc->mSelectedInGameMapFeatures.removeAll(feature); // FIXME: no signal?

    emit inGameMapFeatureAboutToBeRemoved(cell, index);
    return cell->inGameMap().features().takeAt(index);
}

InGameMapPoint WorldDocumentUndoRedo::moveInGameMapPoint(WorldCell *cell, int featureIndex, int coordIndex, int pointIndex, const InGameMapPoint &point)
{
    InGameMapFeature* feature = cell->inGameMap().mFeatures[featureIndex];
    InGameMapCoordinates& coords = feature->mGeometry.mCoordinates[coordIndex];
    InGameMapPoint old = coords[pointIndex];
    coords[pointIndex] = point;
    emit inGameMapPointMoved(cell, featureIndex, coordIndex, pointIndex);
    return old;
}

void WorldDocumentUndoRedo::addInGameMapProperty(WorldCell *cell, int featureIndex, int propertyIndex, const InGameMapProperty &property)
{
    InGameMapFeature* feature = cell->inGameMap().mFeatures[featureIndex];
    feature->properties().insert(propertyIndex, property);
    emit inGameMapPropertiesChanged(cell, featureIndex);
}

InGameMapProperty WorldDocumentUndoRedo::removeInGameMapProperty(WorldCell *cell, int featureIndex, int propertyIndex)
{
    InGameMapFeature* feature = cell->inGameMap().mFeatures[featureIndex];
    InGameMapProperty old = feature->properties().takeAt(propertyIndex);
    emit inGameMapPropertiesChanged(cell, featureIndex);
    return old;
}

InGameMapProperty WorldDocumentUndoRedo::setInGameMapProperty(WorldCell *cell, int featureIndex, int propertyIndex, const InGameMapProperty &property)
{
    InGameMapFeature* feature = cell->inGameMap().mFeatures[featureIndex];
    InGameMapProperty old = feature->properties().at(propertyIndex);
    feature->properties().replace(propertyIndex, property);
    emit inGameMapPropertiesChanged(cell, featureIndex);
    return old;
}

InGameMapProperties WorldDocumentUndoRedo::setInGameMapProperties(WorldCell *cell, int featureIndex, const InGameMapProperties &properties)
{
    InGameMapFeature* feature = cell->inGameMap().mFeatures[featureIndex];
    InGameMapProperties old = feature->properties();
    feature->properties() = properties;
    emit inGameMapPropertiesChanged(cell, featureIndex);
    return old;
}

InGameMapCoordinates WorldDocumentUndoRedo::setInGameMapCoordinates(WorldCell *cell, int featureIndex, int coordsIndex, const InGameMapCoordinates &coords)
{
    InGameMapFeature* feature = cell->inGameMap().mFeatures[featureIndex];
    InGameMapCoordinates old = feature->mGeometry.mCoordinates[coordsIndex];
    feature->mGeometry.mCoordinates[coordsIndex] = coords;
    emit inGameMapGeometryChanged(cell, featureIndex);
    return old;
}

void WorldDocumentUndoRedo::addInGameMapHole(WorldCell *cell, int featureIndex, int holeIndex, const InGameMapCoordinates &hole)
{
    InGameMapFeature* feature = cell->inGameMap().mFeatures[featureIndex];
    feature->mGeometry.mCoordinates.insert(holeIndex, hole);
    emit inGameMapHoleAdded(cell, featureIndex, holeIndex);
    emit inGameMapGeometryChanged(cell, featureIndex);
}

InGameMapCoordinates WorldDocumentUndoRedo::removeInGameMapHole(WorldCell *cell, int featureIndex, int holeIndex)
{
    InGameMapFeature* feature = cell->inGameMap().mFeatures[featureIndex];
    InGameMapCoordinates old = feature->mGeometry.mCoordinates.takeAt(holeIndex);
    emit inGameMapHoleRemoved(cell, featureIndex, holeIndex);
    emit inGameMapGeometryChanged(cell, featureIndex);
    return old;
}

void WorldDocumentUndoRedo::convertToInGameMapPolygon(WorldCell *cell, int featureIndex)
{
    InGameMapFeature* feature = cell->inGameMap().mFeatures[featureIndex];
    feature->mGeometry.mType = QStringLiteral("Polygon");
    emit inGameMapGeometryChanged(cell, featureIndex);
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
