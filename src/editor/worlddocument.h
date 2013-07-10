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

#ifndef WORLDDOCUMENT_H
#define WORLDDOCUMENT_H

#include "document.h"

#include <QColor>
#include <QObject>
#include <QList>
#include <QPoint>
#include <QRegion>
#include <QSize>

class BMPToTMXSettings;
class GenerateLotsSettings;
class HeightMapFile;
class HeightMapRegion;
class ObjectType;
class Property;
class PropertyDef;
class PropertyEnum;
class PropertyHolder;
class PropertyTemplate;
class Road;
class TrafficLines;
class World;
class WorldBMP;
class WorldCell;
class WorldCellContents;
class WorldCellLot;
class WorldCellObject;
class WorldObjectGroup;

class QUndoStack;


/**
  * This class's methods are all called by the various QUndoCommand subclasses.
  * They were separated out of WorldDocument to avoid too many ambiguous overloads.
  * All of the signals are forwarded to WorldDocument's signals.
  */
class WorldDocumentUndoRedo : public QObject
{
    Q_OBJECT
public:
    WorldDocumentUndoRedo(WorldDocument *worldDoc);

    void addPropertyDefinition(int index, PropertyDef *pd);
    PropertyDef *removePropertyDefinition(int index);

    void changePropertyDefinition(PropertyDef *pd, const QString &name,
                                  const QString &defValue, const QString &desc,
                                  PropertyEnum *pe);
    void changeTemplate(PropertyTemplate *pt, const QString &name, const QString &desc);

    void addProperty(PropertyHolder *ph, int index, Property *p);
    Property *removeProperty(PropertyHolder *ph, int index);

    void addTemplateToWorld(int index, PropertyTemplate *pt);
    PropertyTemplate *removeTemplateFromWorld(int index);

    void addTemplate(PropertyHolder *ph, int index, PropertyTemplate *pt);
    PropertyTemplate *removeTemplate(PropertyHolder *ph, int index);

    QString setPropertyValue(PropertyHolder *ph, Property *p, const QString &value);

    void insertObjectGroup(int index, WorldObjectGroup *og);
    WorldObjectGroup *removeObjectGroup(int index);
    QString changeObjectGroupName(WorldObjectGroup *og, const QString &name);
    QColor changeObjectGroupColor(WorldObjectGroup *og, const QColor &color);
    int reorderObjectGroup(WorldObjectGroup *og, int index);
    ObjectType *changeObjectGroupDefType(WorldObjectGroup *og, ObjectType *ot);

    void insertObjectType(int index, ObjectType *ot);
    ObjectType *removeObjectType(int index);
    QString changeObjectTypeName(ObjectType *objType, const QString &name);

    QSize resizeWorld(const QSize &newSize, QVector<WorldCell*> &cells);

    QString setCellMapName(WorldCell *cell, const QString &mapName);
    WorldCellContents *setCellContents(WorldCell *cell, WorldCellContents *contents);

    void addCellLot(WorldCell *cell, int index, WorldCellLot *lot);
    WorldCellLot *removeCellLot(WorldCell *cell, int index);
    QPoint moveCellLot(WorldCellLot *lot, const QPoint &pos);
    int setLotLevel(WorldCellLot *lot, int level);

    void addCellObject(WorldCell *cell, int index, WorldCellObject *obj);
    WorldCellObject *removeCellObject(WorldCell *cell, int index);
    QPointF moveCellObject(WorldCellObject *obj, const QPointF &pos);
    QSizeF resizeCellObject(WorldCellObject *obj, const QSizeF &size);
    int setObjectLevel(WorldCellObject *obj, int level);
    QString setCellObjectName(WorldCellObject *obj, const QString &name);
    WorldObjectGroup *setCellObjectGroup(WorldCellObject *obj, WorldObjectGroup *group);
    ObjectType *setCellObjectType(WorldCellObject *obj, ObjectType *type);
    int reorderCellObject(WorldCellObject *obj, int index);

    void insertRoad(int index, Road *road);
    Road *removeRoad(int index);
    void changeRoadCoords(Road *road, const QPoint &start, const QPoint &end,
                          QPoint &oldStart, QPoint &oldEnd);
    int changeRoadWidth(Road *road, int newWidth);
    QString changeRoadTileName(Road *road, const QString &tileName);
    TrafficLines *changeRoadLines(Road *road, TrafficLines *lines);

    QList<WorldCell *> setSelectedCells(const QList<WorldCell*> &selection);

    BMPToTMXSettings changeBMPToTMXSettings(const BMPToTMXSettings &settings);
    GenerateLotsSettings changeGenerateLotsSettings(const GenerateLotsSettings &settings);

    QPoint moveBMP(WorldBMP *bmp, const QPoint &topLeft);
    void insertBMP(int index, WorldBMP *bmp);
    WorldBMP *removeBMP(int index);

    void insertPropertyEnum(int index, PropertyEnum *pe);
    PropertyEnum *removePropertyEnum(int index);
    void changePropertyEnum(PropertyEnum *pe, const QString &name, bool multi);
    QStringList setPropertyEnumChoices(PropertyEnum *pe, const QStringList &choices);

    QStringList setProfessions(const QStringList &professions);

    QString setHeightMapFileName(const QString &fileName);
    void paintHeightMap(const HeightMapRegion &region);

signals:
    void propertyDefinitionAdded(PropertyDef *pd, int index);
    void propertyDefinitionAboutToBeRemoved(int index);
    void propertyDefinitionChanged(PropertyDef *pd);

    void objectGroupAdded(int index);
    void objectGroupAboutToBeRemoved(int index);
    void objectGroupNameChanged(WorldObjectGroup *og);
    void objectGroupColorChanged(WorldObjectGroup *og);
    void objectGroupAboutToBeReordered(int index);
    void objectGroupReordered(int index);

    void objectTypeAdded(int index);
    void objectTypeAboutToBeRemoved(int index);
    void objectTypeNameChanged(ObjectType *type);

    void propertyAdded(PropertyHolder *ph, int index);
    void propertyAboutToBeRemoved(PropertyHolder *ph, int index);

    void templateAdded(int index);
    void templateAboutToBeRemoved(int index);
    void templateChanged(PropertyTemplate *pt);

    void templateAdded(PropertyHolder *ph, int index);
    void templateAboutToBeRemoved(PropertyHolder *ph, int index);

    void propertyValueChanged(PropertyHolder *ph, int index);

    void worldAboutToResize(const QSize &newSize);
    void worldResized(const QSize &oldSize);

    void cellAdded(WorldCell *cell);
    void cellAboutToBeRemoved(WorldCell *cell);

    void cellMapFileAboutToChange(WorldCell *cell);
    void cellMapFileChanged(WorldCell *cell);

    void cellContentsAboutToChange(WorldCell *cell);
    void cellContentsChanged(WorldCell *cell);

    void cellLotAdded(WorldCell *cell, int index);
    void cellLotAboutToBeRemoved(WorldCell *cell, int index);
    void cellLotMoved(WorldCellLot *lot);
    void lotLevelChanged(WorldCellLot *lot);

    void cellObjectAdded(WorldCell *cell, int index);
    void cellObjectAboutToBeRemoved(WorldCell *cell, int index);
    void cellObjectMoved(WorldCellObject *object);
    void cellObjectResized(WorldCellObject *object);
    void cellObjectNameChanged(WorldCellObject *object);
    void cellObjectGroupAboutToChange(WorldCellObject *object);
    void cellObjectGroupChanged(WorldCellObject *object);
    void cellObjectTypeChanged(WorldCellObject *object);
    void objectLevelAboutToChange(WorldCellObject *object);
    void objectLevelChanged(WorldCellObject *object);
    void cellObjectReordered(WorldCellObject *object);

    void roadAdded(int index);
    void roadAboutToBeRemoved(int index);
    void roadRemoved(Road *road);
    void roadCoordsChanged(int index);
    void roadWidthChanged(int index);
    void roadTileNameChanged(int index);
    void roadLinesChanged(int index);

    void selectedCellsChanged();

    void bmpCoordsChanged(int index);
    void bmpAdded(int index);
    void bmpAboutToBeRemoved(int index);

    void propertyEnumAdded(int index);
    void propertyEnumAboutToBeRemoved(int index);
    void propertyEnumChanged(PropertyEnum *pe);
    void propertyEnumChoicesChanged(PropertyEnum *pe);

    void professionsChanged();

    void heightMapFileNameChanged();
    void heightMapPainted(const QRegion &region);

private:
    WorldDocument *mWorldDoc;
    World *mWorld;

    friend class WorldDocument;
};

class WorldDocument : public Document
{
    Q_OBJECT

public:
    WorldDocument(World *world, const QString &fileName = QString());
    ~WorldDocument();

    void setFileName(const QString &fileName);
    const QString &fileName() const;
    bool save(const QString &filePath, QString &error);

    void setWorld(World *world);
    World *world() const { return mWorld; }

    void setSelectedCells(const QList<WorldCell*> &selectedCells, bool undoable = false);
    const QList<WorldCell*> &selectedCells() const { return mSelectedCells; }
    int selectedCellCount() const { return mSelectedCells.size(); }

    void setSelectedObjects(const QList<WorldCellObject*> &selectedObjects);
    const QList<WorldCellObject*> &selectedObjects() const { return mSelectedObjects; }
    int selectedObjectCount() const { return mSelectedObjects.size(); }

    void setSelectedLots(const QList<WorldCellLot *> &selectedLots);
    const QList<WorldCellLot*> &selectedLots() const { return mSelectedLots; }
    int selectedLotCount() const { return mSelectedLots.size(); }

    void setSelectedRoads(const QList<Road *> &selectedRoads);
    const QList<Road*> &selectedRoads() const { return mSelectedRoads; }
    int selectedRoadCount() const { return mSelectedRoads.size(); }

    void setSelectedBMPs(const QList<WorldBMP *> &selectedBMPs);
    const QList<WorldBMP*> &selectedBMPs() const { return mSelectedBMPs; }
    int selectedBMPCount() const { return mSelectedBMPs.size(); }

    void editCell(WorldCell *cell);
    void editCell(int x, int y);

    void editHeightMap(WorldCell *cell);
    HeightMapFile *hmFile() const
    { return mHMFile; }

    void resizeWorld(const QSize &newSize);

    void setCellMapName(WorldCell *cell, const QString &mapName);
    void addCellLot(WorldCell *cell, int index, WorldCellLot *lot);
    void removeCellLot(WorldCell *cell, int index);
    void moveCellLot(WorldCellLot *lot, const QPoint &pos);
    void setLotLevel(WorldCellLot *lot, int level);

    void addCellObject(WorldCell *cell, int index, WorldCellObject *obj);
    void removeCellObject(WorldCell *cell, int index);
    void moveCellObject(WorldCellObject *obj, const QPointF &pos);
    void resizeCellObject(WorldCellObject *obj, const QSizeF &size);
    void setObjectLevel(WorldCellObject *obj, int level);
    void setCellObjectName(WorldCellObject *obj, const QString &name);
    void setCellObjectGroup(WorldCellObject *obj, WorldObjectGroup *og);
    void setCellObjectType(WorldCellObject *obj, const QString &type);
    void reorderCellObject(WorldCellObject *obj, WorldCellObject *insertBefore);

    void insertRoad(int index, Road *road);
    void removeRoad(int index);
    void changeRoadCoords(Road *road, const QPoint &start, const QPoint &end);
    void changeRoadWidth(Road *road, int newWidth);
    void changeRoadTileName(Road *road, const QString &tileName);
    void changeRoadLines(Road *road, TrafficLines *lines);

    /**
      * Transfers the contents of \a cell to a different cell at
      * \a newPos.  The contents of \a cell are then set to empty.
      * This operation is undo-able.
      */
    void moveCell(WorldCell *cell, const QPoint &newPos);

    /**
      * Resets the contents of the given cell to the default empty state.
      * This operation is undo-able.
      */
    void clearCell(WorldCell *cell);

    /**
      * Removes a property definition from the world.
      * Any properties contained in templates or cells that use
      * the property definition \a pd are deleted.
      * This operation is undo-able.
      */
    bool removePropertyDefinition(PropertyDef *pd);

    /**
      * Add a brand-new property definition to the world.
      * \a pd is added to the end of the list of definitions.
      * This operation is undo-able.
      */
    void addPropertyDefinition(PropertyDef *pd);

    /**
      * Add a brand-new template to the world.
      * The new template is added to the end of the list of templates.
      * This operation is undo-able.
      */
    void addTemplate(const QString &name, const QString &desc);
    void addTemplate(PropertyTemplate *pt);

    void removeTemplate(int index);

    /**
      * Changes the name, default value, and/or description of
      * an existing property definition.
      * This operation is undo-able.
      */
    void changePropertyDefinition(PropertyDef *pd, const QString &name,
                                  const QString &defValue, const QString &desc,
                                  PropertyEnum *pe);
    void changePropertyDefinition(PropertyDef *pd, PropertyDef *other);

    /**
      * Changes the name, and/or description of an existing template.
      * This operation is undo-able.
      */
    void changeTemplate(PropertyTemplate *pt, const QString &name, const QString &desc);

    void changeTemplate(PropertyTemplate *pt, PropertyTemplate *other);

    void addObjectGroup(WorldObjectGroup *newGroup);
    bool removeObjectGroup(WorldObjectGroup *objGroup);
    void changeObjectGroupName(WorldObjectGroup *objGroup, const QString &name);
    void changeObjectGroupColor(WorldObjectGroup *objGroup, const QColor &color);
    void reorderObjectGroup(WorldObjectGroup *og, int index);
    void changeObjectGroupDefType(WorldObjectGroup *og, ObjectType *ot);
    void changeObjectGroup(WorldObjectGroup *og, WorldObjectGroup *other);

    void addObjectType(ObjectType *newType);
    bool removeObjectType(ObjectType *objType);
    void changeObjectTypeName(ObjectType *objType, const QString &name);
    void changeObjectType(ObjectType *ot, ObjectType *other);

    /**
      * Adds a new instance of the template called \a name
      * to the end of the list of templates for a holder.
      * This operation is undo-able.
      */
    void addTemplate(PropertyHolder *ph, const QString &name);
    void removeTemplate(PropertyHolder *ph, int index);

    void addProperty(PropertyHolder *ph, const QString &name);
    void addProperty(PropertyHolder *ph, const QString &name, const QString &value);
    void removeProperty(PropertyHolder *ph, int index);

    void setPropertyValue(PropertyHolder *ph, Property *p, const QString &value);

    void changeBMPToTMXSettings(const BMPToTMXSettings &settings);
    void changeGenerateLotsSettings(const GenerateLotsSettings &settings);

    void moveBMP(WorldBMP *bmp, const QPoint &topLeft);
    void insertBMP(int index, WorldBMP *bmp);
    void removeBMP(WorldBMP *bmp);

    void addPropertyEnum(const QString &name, bool multi);
    void removePropertyEnum(PropertyEnum *pe);
    void changePropertyEnum(PropertyEnum *pe, const QString &name, bool multi);
    void setPropertyEnumChoices(PropertyEnum *pe, const QStringList &choices);

    void setProfessions(const QStringList &professions);

    void setHeightMapFileName(const QString &fileName);
    void paintHeightMap(const HeightMapRegion &region, bool mergeable);

    void emitCellMapFileAboutToChange(WorldCell *cell);
    void emitCellMapFileChanged(WorldCell *cell);

    WorldDocumentUndoRedo &undoRedo() { return mUndoRedo; }

private:
    void removePropertyDefinition(PropertyHolder *ph, PropertyDef *pd);
    void removeTemplate(PropertyHolder *ph, PropertyTemplate *pt);

    void removeRoadFromSelection(Road *road);
    void removeBMPFromSelection(WorldBMP *bmp);

    bool openHeightMap();

signals:
    void propertyDefinitionAdded(PropertyDef *pd, int index);
    void propertyDefinitionAboutToBeRemoved(int index);
    void propertyDefinitionChanged(PropertyDef *pd);

    void objectGroupAdded(int index);
    void objectGroupAboutToBeRemoved(int index);
    void objectGroupNameChanged(WorldObjectGroup *og);
    void objectGroupColorChanged(WorldObjectGroup *og);
    void objectGroupAboutToBeReordered(int index);
    void objectGroupReordered(int index);

    void objectTypeAdded(int index);
    void objectTypeAboutToBeRemoved(int index);
    void objectTypeNameChanged(ObjectType *type);

    void selectedCellsChanged();
    void selectedObjectsChanged();
    void selectedLotsChanged();
    void selectedRoadsChanged();
    void selectedBMPsChanged();

    void propertyAdded(PropertyHolder *ph, int index);
    void propertyAboutToBeRemoved(PropertyHolder *ph, int index);

    void propertyValueChanged(PropertyHolder *ph, int index);

    void worldAboutToResize(const QSize &newSize);
    void worldResized(const QSize &oldSize);

    void cellAdded(WorldCell *cell);
    void cellAboutToBeRemoved(WorldCell *cell);

    void cellMapFileAboutToChange(WorldCell *cell);
    void cellMapFileChanged(WorldCell *cell);

    void cellLotAdded(WorldCell *cell, int index);
    void cellLotAboutToBeRemoved(WorldCell *cell, int index);
    void cellLotMoved(WorldCellLot *lot);
    void lotLevelChanged(WorldCellLot *lot);

    void cellObjectAdded(WorldCell* cell, int index);
    void cellObjectAboutToBeRemoved(WorldCell *cell, int index);
    void cellObjectMoved(WorldCellObject *object);
    void cellObjectResized(WorldCellObject *object);
    void cellObjectNameChanged(WorldCellObject *object);
    void cellObjectGroupAboutToChange(WorldCellObject *object);
    void cellObjectGroupChanged(WorldCellObject *object);
    void cellObjectTypeChanged(WorldCellObject *object);
    void objectLevelAboutToChange(WorldCellObject *object);
    void objectLevelChanged(WorldCellObject *object);
    void cellObjectReordered(WorldCellObject *object);

    void roadAdded(int index);
    void roadAboutToBeRemoved(int index);
    void roadRemoved(Road *road);
    void roadCoordsChanged(int index);
    void roadWidthChanged(int index);
    void roadTileNameChanged(int index);
    void roadLinesChanged(int index);

    void templateAdded(int index);
    void templateAboutToBeRemoved(int index);
    void templateChanged(PropertyTemplate *pt);

    void templateAdded(PropertyHolder *ph, int index);
    void templateAboutToBeRemoved(PropertyHolder *ph, int index);

    void cellContentsAboutToChange(WorldCell *cell);
    void cellContentsChanged(WorldCell *cell);

    void cellClipboardChanged();

    void bmpAdded(int index);
    void bmpAboutToBeRemoved(int index);
    void bmpCoordsChanged(int index);

    void propertyEnumAdded(int index);
    void propertyEnumAboutToBeRemoved(int index);
    void propertyEnumChanged(PropertyEnum *pe);
    void propertyEnumChoicesChanged(PropertyEnum *pe);

    void professionsChanged();

    void heightMapFileNameChanged();
    void heightMapPainted(const QRegion &region);

private:
    World *mWorld;
    QList<WorldCell*> mSelectedCells;
    QList<WorldCellObject*> mSelectedObjects;
    QList<WorldCellLot*> mSelectedLots;
    QList<Road*> mSelectedRoads;
    QList<WorldBMP*> mSelectedBMPs;
    QString mFileName;

    HeightMapFile *mHMFile;

    friend class WorldDocumentUndoRedo;
    WorldDocumentUndoRedo mUndoRedo;
};

#endif // WORLDDOCUMENT_H
