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
    QString mapName = mCell->mapFilePath();

    mDocument->undoRedo().setCellMapName(mCell, mMapName);

    mMapName = mapName;
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
    delete mTemplate;
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
