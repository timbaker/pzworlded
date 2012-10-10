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

#include "undoredo.h"

#include "road.h"
#include "worldcell.h"
#include "worlddocument.h"

#include <QCoreApplication>

SetCellMainMap::SetCellMainMap(WorldDocument *doc, WorldCell *cell, const QString &mapName)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Set Cell's Map"))
    , mDocument(doc)
    , mCell(cell)
    , mMapName(mapName)
{
}

void SetCellMainMap::swap()
{
    mMapName = mDocument->undoRedo().setCellMapName(mCell, mMapName);
}

/////


ReplaceCell::ReplaceCell(WorldDocument *doc, WorldCell *cell, WorldCellContents *contents)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Replace Cell"))
    , mDocument(doc)
    , mCell(cell)
    , mContents(contents)
{
}

void ReplaceCell::swap()
{
    WorldCellContents *contents = mContents;
    mContents = mDocument->undoRedo().setCellContents(mCell, mContents);
    delete contents; // empty now
}

/////

AddRemoveCellLot::AddRemoveCellLot(WorldDocument *doc, WorldCell *cell, int index, WorldCellLot *lot)
    : mDocument(doc)
    , mCell(cell)
    , mLot(lot)
    , mIndex(index)
{
}

AddRemoveCellLot::~AddRemoveCellLot()
{
    delete mLot;
}

void AddRemoveCellLot::addLot()
{
    mDocument->undoRedo().addCellLot(mCell, mIndex, mLot);
    mLot = 0;
}

void AddRemoveCellLot::removeLot()
{
    mLot = mDocument->undoRedo().removeCellLot(mCell, mIndex);
}

/////

MoveCellLot::MoveCellLot(WorldDocument *doc, WorldCellLot *lot, const QPoint &newTilePos)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Move Lot"))
    , mDocument(doc)
    , mLot(lot)
    , mTilePos(newTilePos)
{
}

void MoveCellLot::swap()
{
    mTilePos = mDocument->undoRedo().moveCellLot(mLot, mTilePos);
}

/////

SetLotLevel::SetLotLevel(WorldDocument *doc, WorldCellLot *lot, int level)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Set Lot's Level"))
    , mDocument(doc)
    , mLot(lot)
    , mLevel(level)
{
}

void SetLotLevel::swap()
{
    mLevel = mDocument->undoRedo().setLotLevel(mLot, mLevel);
}

/////

AddRemoveCellObject::AddRemoveCellObject(WorldDocument *doc, WorldCell *cell, int index, WorldCellObject *obj)
    : mDocument(doc)
    , mCell(cell)
    , mObject(obj)
    , mIndex(index)
{
}

AddRemoveCellObject::~AddRemoveCellObject()
{
    delete mObject;
}

void AddRemoveCellObject::add()
{
    mDocument->undoRedo().addCellObject(mCell, mIndex, mObject);
    mObject = 0;
}

void AddRemoveCellObject::remove()
{
    mObject = mDocument->undoRedo().removeCellObject(mCell, mIndex);
}

/////

MoveCellObject::MoveCellObject(WorldDocument *doc, WorldCellObject *obj, const QPointF &newTilePos)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Move Object"))
    , mDocument(doc)
    , mObject(obj)
    , mTilePos(newTilePos)
{
}

void MoveCellObject::swap()
{
    mTilePos = mDocument->undoRedo().moveCellObject(mObject, mTilePos);
}

/////

ResizeCellObject::ResizeCellObject(WorldDocument *doc, WorldCellObject *obj, const QSizeF &newSize)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Resize Object"))
    , mDocument(doc)
    , mObject(obj)
    , mSize(newSize)
{
}

void ResizeCellObject::swap()
{
    mSize = mDocument->undoRedo().resizeCellObject(mObject, mSize);
}

/////

SetObjectLevel::SetObjectLevel(WorldDocument *doc, WorldCellObject *obj, int level)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Set Object's Level"))
    , mDocument(doc)
    , mObject(obj)
    , mLevel(level)
{
}

void SetObjectLevel::swap()
{
    mLevel = mDocument->undoRedo().setObjectLevel(mObject, mLevel);
}

/////

SetObjectName::SetObjectName(WorldDocument *doc, WorldCellObject *obj, const QString &name)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Set Object's Name"))
    , mDocument(doc)
    , mObject(obj)
    , mName(name)
{
}

void SetObjectName::swap()
{
    mName = mDocument->undoRedo().setCellObjectName(mObject, mName);
}

/////

SetObjectGroup::SetObjectGroup(WorldDocument *doc, WorldCellObject *obj, WorldObjectGroup *og)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Set Object's Group"))
    , mDocument(doc)
    , mObject(obj)
    , mGroup(og)
{
}

void SetObjectGroup::swap()
{
    mGroup = mDocument->undoRedo().setCellObjectGroup(mObject, mGroup);
}

/////

SetObjectType::SetObjectType(WorldDocument *doc, WorldCellObject *obj, ObjectType *type)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Set Object's Type"))
    , mDocument(doc)
    , mObject(obj)
    , mType(type)
{
}

void SetObjectType::swap()
{
    mType = mDocument->undoRedo().setCellObjectType(mObject, mType);
}

/////


ReorderCellObject::ReorderCellObject(WorldDocument *doc, WorldCellObject *obj, int newIndex)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Reorder Object"))
    , mDocument(doc)
    , mObject(obj)
    , mIndex(newIndex)
{
}

void ReorderCellObject::swap()
{
    mIndex = mDocument->undoRedo().reorderCellObject(mObject, mIndex);
}

/////


AddRemoveRoad::AddRemoveRoad(WorldDocument *doc, int index, Road *road)
    : mDocument(doc)
    , mRoad(road)
    , mIndex(index)
{
}

AddRemoveRoad::~AddRemoveRoad()
{
    delete mRoad;
}

void AddRemoveRoad::add()
{
    mDocument->undoRedo().insertRoad(mIndex, mRoad);
    mRoad = 0;
}

void AddRemoveRoad::remove()
{
    mRoad = mDocument->undoRedo().removeRoad(mIndex);
}

/////

ChangeRoadCoords::ChangeRoadCoords(WorldDocument *doc, Road *road,
                                   const QPoint &start, const QPoint &end)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Reposition Road"))
    , mDocument(doc)
    , mRoad(road)
    , mStart(start)
    , mEnd(end)
{
}

void ChangeRoadCoords::swap()
{
    QPoint start, end;
    mDocument->undoRedo().changeRoadCoords(mRoad, mStart, mEnd,
                                           start, end);
    mStart = start, mEnd = end;
}

/////

ChangeRoadWidth::ChangeRoadWidth(WorldDocument *doc, Road *road,
                                 int newWidth)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Change Road Width"))
    , mDocument(doc)
    , mRoad(road)
    , mWidth(newWidth)
{
}

bool ChangeRoadWidth::mergeWith(const QUndoCommand *other)
{
    if (other->id() != id())
        return false;
    const ChangeRoadWidth *_other = static_cast<const ChangeRoadWidth*>(other);
    if (_other->mRoad != mRoad)
        return false;
    return true;
}

void ChangeRoadWidth::swap()
{
    mWidth = mDocument->undoRedo().changeRoadWidth(mRoad, mWidth);
}

/////

ChangeRoadTileName::ChangeRoadTileName(WorldDocument *doc, Road *road,
                                       const QString &tileName)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Change Road Tile"))
    , mDocument(doc)
    , mRoad(road)
    , mTileName(tileName)
{
}

void ChangeRoadTileName::swap()
{
    mTileName = mDocument->undoRedo().changeRoadTileName(mRoad, mTileName);
}

/////

ChangeRoadLines::ChangeRoadLines(WorldDocument *doc, Road *road,
                                 TrafficLines *lines)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Change Road Lines"))
    , mDocument(doc)
    , mRoad(road)
    , mLines(lines)
{
}

void ChangeRoadLines::swap()
{
    mLines = mDocument->undoRedo().changeRoadLines(mRoad, mLines);
}

/////

AddRemoveProperty::AddRemoveProperty(WorldDocument *doc, PropertyHolder *ph, int index, Property *p)
    : mDocument(doc)
    , mPH(ph)
    , mProperty(p)
    , mIndex(index)
{
}

AddRemoveProperty::~AddRemoveProperty()
{
    delete mProperty;
}

void AddRemoveProperty::add()
{
    mDocument->undoRedo().addProperty(mPH, mIndex, mProperty);
    mProperty = 0;
}

void AddRemoveProperty::remove()
{
    mProperty = mDocument->undoRedo().removeProperty(mPH, mIndex);
}

/////

AddRemoveTemplateToFromPH::AddRemoveTemplateToFromPH(WorldDocument *doc, PropertyHolder *ph, int index, PropertyTemplate *pt)
    : mDocument(doc)
    , mPH(ph)
    , mTemplate(pt)
    , mIndex(index)
{
}

AddRemoveTemplateToFromPH::~AddRemoveTemplateToFromPH()
{
//    delete mTemplate;
}

void AddRemoveTemplateToFromPH::add()
{
    mDocument->undoRedo().addTemplate(mPH, mIndex, mTemplate);
    mTemplate = 0;
}

void AddRemoveTemplateToFromPH::remove()
{
    mTemplate = mDocument->undoRedo().removeTemplate(mPH, mIndex);
}

/////

AddRemoveTemplateToFromWorld::AddRemoveTemplateToFromWorld(WorldDocument *doc, int index, PropertyTemplate *pt)
    : mDocument(doc)
    , mTemplate(pt)
    , mIndex(index)
{
}

AddRemoveTemplateToFromWorld::~AddRemoveTemplateToFromWorld()
{
    delete mTemplate;
}

void AddRemoveTemplateToFromWorld::add()
{
    mDocument->undoRedo().addTemplateToWorld(mIndex, mTemplate);
    mTemplate = 0;
}

void AddRemoveTemplateToFromWorld::remove()
{
    mTemplate = mDocument->undoRedo().removeTemplateFromWorld(mIndex);
}

/////

ChangeTemplate::ChangeTemplate(WorldDocument *doc, PropertyTemplate *pt, const QString &name, const QString &desc)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Edit Template"))
    , mDocument(doc)
    , mTemplate(pt)
    , mName(name)
    , mDesc(desc)
{
}

void ChangeTemplate::swap()
{
    QString a = mTemplate->mName;
    QString b = mTemplate->mDescription;
    mDocument->undoRedo().changeTemplate(mTemplate, mName, mDesc);
    mName = a, mDesc = b;
}

/////

AddRemovePropertyDef::AddRemovePropertyDef(WorldDocument *doc, int index, PropertyDef *pd)
    : mDocument(doc)
    , mPropertyDef(pd)
    , mIndex(index)
{
}

AddRemovePropertyDef::~AddRemovePropertyDef()
{
    delete mPropertyDef;
}

void AddRemovePropertyDef::add()
{
    mDocument->undoRedo().addPropertyDefinition(mIndex, mPropertyDef);
    mPropertyDef = 0;
}

void AddRemovePropertyDef::remove()
{
    mPropertyDef = mDocument->undoRedo().removePropertyDefinition(mIndex);
}

/////

EditPropertyDef::EditPropertyDef(WorldDocument *doc, PropertyDef *pd, const QString &name, const QString &defValue, const QString &desc)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Edit Property Definition"))
    , mDocument(doc)
    , mPropertyDef(pd)
    , mName(name)
    , mDefault(defValue)
    , mDesc(desc)
{
}

void EditPropertyDef::swap()
{
    QString a = mPropertyDef->mName;
    QString b = mPropertyDef->mDefaultValue;
    QString c = mPropertyDef->mDescription;
    mDocument->undoRedo().changePropertyDefinition(mPropertyDef, mName, mDefault, mDesc);
    mName = a, mDefault = b, mDesc = c;
}

/////

SetPropertyValue::SetPropertyValue(WorldDocument *doc, PropertyHolder *ph, Property *p, const QString &value)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Change Property's Value"))
    , mDocument(doc)
    , mPH(ph)
    , mProperty(p)
    , mValue(value)
{
}

void SetPropertyValue::swap()
{
    mValue = mDocument->undoRedo().setPropertyValue(mPH, mProperty, mValue);
}

/////

ChangeCellSelection::ChangeCellSelection(WorldDocument *doc, const QList<WorldCell*> &selection)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Change Selection"))
    , mDocument(doc)
    , mSelection(selection)
{
}

void ChangeCellSelection::swap()
{
    mSelection = mDocument->undoRedo().setSelectedCells(mSelection);
}

/////

AddRemoveObjectGroup::AddRemoveObjectGroup(WorldDocument *doc, int index, WorldObjectGroup *og)
    : QUndoCommand()
    , mDocument(doc)
    , mGroup(og)
    , mIndex(index)
{
}

AddRemoveObjectGroup::~AddRemoveObjectGroup()
{
    delete mGroup;
}

void AddRemoveObjectGroup::add()
{
    mDocument->undoRedo().insertObjectGroup(mIndex, mGroup);
    mGroup = 0;
}

void AddRemoveObjectGroup::remove()
{
    mGroup = mDocument->undoRedo().removeObjectGroup(mIndex);
}

/////

AddRemoveObjectType::AddRemoveObjectType(WorldDocument *doc, int index, ObjectType *ot)
    : QUndoCommand()
    , mDocument(doc)
    , mType(ot)
    , mIndex(index)
{
}

AddRemoveObjectType::~AddRemoveObjectType()
{
    delete mType;
}

void AddRemoveObjectType::add()
{
    mDocument->undoRedo().insertObjectType(mIndex, mType);
    mType = 0;
}

void AddRemoveObjectType::remove()
{
    mType = mDocument->undoRedo().removeObjectType(mIndex);
}

/////

ReorderObjectGroup::ReorderObjectGroup(WorldDocument *doc, WorldObjectGroup *og, int newIndex)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Reorder Object Groups"))
    , mDocument(doc)
    , mGroup(og)
    , mIndex(newIndex)
{
}

void ReorderObjectGroup::swap()
{
    mIndex = mDocument->undoRedo().reorderObjectGroup(mGroup, mIndex);
}

/////

SetObjectGroupName::SetObjectGroupName(WorldDocument *doc, WorldObjectGroup *og, const QString &name)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Rename Object Group"))
    , mDocument(doc)
    , mGroup(og)
    , mName(name)
{
}

void SetObjectGroupName::swap()
{
    mName = mDocument->undoRedo().changeObjectGroupName(mGroup, mName);
}

/////

SetObjectGroupColor::SetObjectGroupColor(WorldDocument *doc, WorldObjectGroup *og, const QColor &color)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Change Object Group's Color"))
    , mDocument(doc)
    , mGroup(og)
    , mColor(color)
{
}

void SetObjectGroupColor::swap()
{
    mColor = mDocument->undoRedo().changeObjectGroupColor(mGroup, mColor);
}

/////

SetObjectGroupDefType::SetObjectGroupDefType(WorldDocument *doc, WorldObjectGroup *og, ObjectType *ot)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Change Object Group's Default Type"))
    , mDocument(doc)
    , mGroup(og)
    , mType(ot)
{
}

void SetObjectGroupDefType::swap()
{
    mType = mDocument->undoRedo().changeObjectGroupDefType(mGroup, mType);
}

/////

SetObjectTypeName::SetObjectTypeName(WorldDocument *doc, ObjectType *ot, const QString &name)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Rename Object Type"))
    , mDocument(doc)
    , mType(ot)
    , mName(name)
{
}

void SetObjectTypeName::swap()
{
    mName = mDocument->undoRedo().changeObjectTypeName(mType, mName);
}

/////

#include "progress.h"

void ProgressBegin::begin()
{
    Progress::instance()->begin(mMsg);
}

void ProgressBegin::end()
{
    Progress::instance()->end();
}

void ProgressEnd::begin()
{
    Progress::instance()->begin(mMsg);
}

void ProgressEnd::end()
{
    Progress::instance()->end();
}
