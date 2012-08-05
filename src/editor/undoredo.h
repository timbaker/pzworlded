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

#ifndef UNDOREDO_H
#define UNDOREDO_H

#include <QColor>
#include <QCoreApplication>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QUndoCommand>

class ObjectType;
class Property;
class PropertyDef;
class PropertyHolder;
class PropertyTemplate;
class WorldCell;
class WorldCellContents;
class WorldCellLot;
class WorldCellObject;
class WorldDocument;
class WorldObjectGroup;

class SetCellMainMap : public QUndoCommand
{
public:
    SetCellMainMap(WorldDocument *doc, WorldCell *cell, const QString &mapName);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    WorldDocument *mDocument;
    WorldCell *mCell;
    QString mMapName;
};

/////

class ReplaceCell : public QUndoCommand
{
public:
    ReplaceCell(WorldDocument *doc, WorldCell *cell, WorldCellContents *contents);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    WorldDocument *mDocument;
    WorldCell *mCell;
    WorldCellContents *mContents;
};

/////

// Base class for AddCellLot/RemoveCellLot
class AddRemoveCellLot : public QUndoCommand
{
public:
    AddRemoveCellLot(WorldDocument *doc, WorldCell *cell, int index, WorldCellLot *lot);
    ~AddRemoveCellLot();

protected:
    void addLot();
    void removeLot();

    WorldDocument *mDocument;
    WorldCell *mCell;
    WorldCellLot *mLot;
    int mIndex;
};

class AddCellLot : public AddRemoveCellLot
{
public:
    AddCellLot(WorldDocument *doc, WorldCell *cell, int index, WorldCellLot *lot)
        : AddRemoveCellLot(doc, cell, index, lot)
    {
        setText(QCoreApplication::translate("Undo Commands", "Add Lot to Cell"));
    }

    void undo() { removeLot(); }
    void redo() { addLot(); }
};

class RemoveCellLot : public AddRemoveCellLot
{
public:
    RemoveCellLot(WorldDocument *doc, WorldCell *cell, int index)
        : AddRemoveCellLot(doc, cell, index, 0)
    {
        setText(QCoreApplication::translate("Undo Commands", "Remove Lot from Cell"));
    }

    void undo() { addLot(); }
    void redo() { removeLot(); }
};

/////

class MoveCellLot : public QUndoCommand
{
public:
    MoveCellLot(WorldDocument *doc, WorldCellLot *lot, const QPoint &newTilePos);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    WorldDocument *mDocument;
    WorldCellLot *mLot;
    QPoint mTilePos;
};

/////

class SetLotLevel : public QUndoCommand
{
public:
    SetLotLevel(WorldDocument *doc, WorldCellLot *lot, int level);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    WorldDocument *mDocument;
    WorldCellLot *mLot;
    int mLevel;
};

/////

// Base class for AddCellObject/RemoveCellObject
class AddRemoveCellObject : public QUndoCommand
{
public:
    AddRemoveCellObject(WorldDocument *doc, WorldCell *cell, int index, WorldCellObject *obj);
    ~AddRemoveCellObject();

protected:
    void add();
    void remove();

    WorldDocument *mDocument;
    WorldCell *mCell;
    WorldCellObject *mObject;
    int mIndex;
};

class AddCellObject : public AddRemoveCellObject
{
public:
    AddCellObject(WorldDocument *doc, WorldCell *cell, int index, WorldCellObject *obj)
        : AddRemoveCellObject(doc, cell, index, obj)
    {
        setText(QCoreApplication::translate("Undo Commands", "Add Object to Cell"));
    }

    void undo() { remove(); }
    void redo() { add(); }
};

class RemoveCellObject : public AddRemoveCellObject
{
public:
    RemoveCellObject(WorldDocument *doc, WorldCell *cell, int index)
        : AddRemoveCellObject(doc, cell, index, 0)
    {
        setText(QCoreApplication::translate("Undo Commands", "Remove Object from Cell"));
    }

    void undo() { add(); }
    void redo() { remove(); }
};

/////

class MoveCellObject : public QUndoCommand
{
public:
    MoveCellObject(WorldDocument *doc, WorldCellObject *obj, const QPointF &newTilePos);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    WorldDocument *mDocument;
    WorldCellObject *mObject;
    QPointF mTilePos;
};

/////

class ResizeCellObject : public QUndoCommand
{
public:
    ResizeCellObject(WorldDocument *doc, WorldCellObject *obj, const QSizeF &newSize);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    WorldDocument *mDocument;
    WorldCellObject *mObject;
    QSizeF mSize;
};

/////

class SetObjectLevel : public QUndoCommand
{
public:
    SetObjectLevel(WorldDocument *doc, WorldCellObject *obj, int level);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    WorldDocument *mDocument;
    WorldCellObject *mObject;
    int mLevel;
};

/////

class SetObjectName : public QUndoCommand
{
public:
    SetObjectName(WorldDocument *doc, WorldCellObject *obj, const QString &name);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    WorldDocument *mDocument;
    WorldCellObject *mObject;
    QString mName;
};

/////

class SetObjectGroup : public QUndoCommand
{
public:
    SetObjectGroup(WorldDocument *doc, WorldCellObject *obj, WorldObjectGroup *og);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    WorldDocument *mDocument;
    WorldCellObject *mObject;
    WorldObjectGroup *mGroup;
};

/////

class SetObjectType : public QUndoCommand
{
public:
    SetObjectType(WorldDocument *doc, WorldCellObject *obj, ObjectType *type);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    WorldDocument *mDocument;
    WorldCellObject *mObject;
    ObjectType *mType;
};

/////

// Base class for AddCellProperty/RemoveCellProperty
class AddRemoveProperty : public QUndoCommand
{
public:
    AddRemoveProperty(WorldDocument *doc, PropertyHolder *ph, int index, Property *p);
    ~AddRemoveProperty();

protected:
    void add();
    void remove();

    WorldDocument *mDocument;
    PropertyHolder *mPH;
    Property *mProperty;
    int mIndex;
};

class AddPropertyToPH : public AddRemoveProperty
{
public:
    AddPropertyToPH(WorldDocument *doc, PropertyHolder *ph, int index, Property *p)
        : AddRemoveProperty(doc, ph, index, p)
    {
        setText(QCoreApplication::translate("Undo Commands", "Add Property"));
    }

    void undo() { remove(); }
    void redo() { add(); }
};

class RemovePropertyFromPH : public AddRemoveProperty
{
public:
    RemovePropertyFromPH(WorldDocument *doc, PropertyHolder *ph, int index, Property *p)
        : AddRemoveProperty(doc, ph, index, p)
    {
        setText(QCoreApplication::translate("Undo Commands", "Remove Property"));
    }

    void undo() { add(); }
    void redo() { remove(); }
};

/////

// Base class for AddTemplateToCell/RemoveTemplateFromCell
class AddRemoveTemplateToFromPH : public QUndoCommand
{
public:
    AddRemoveTemplateToFromPH(WorldDocument *doc, PropertyHolder *ph, int index, PropertyTemplate *pt);
    ~AddRemoveTemplateToFromPH();

protected:
    void add();
    void remove();

    WorldDocument *mDocument;
    PropertyHolder *mPH;
    PropertyTemplate *mTemplate;
    int mIndex;
};

class AddTemplateToPH : public AddRemoveTemplateToFromPH
{
public:
    AddTemplateToPH(WorldDocument *doc, PropertyHolder *ph, int index, PropertyTemplate *pt)
        : AddRemoveTemplateToFromPH(doc, ph, index, pt)
    {
        setText(QCoreApplication::translate("Undo Commands", "Add Template"));
    }

    void undo() { remove(); }
    void redo() { add(); }
};

class RemoveTemplateFromPH : public AddRemoveTemplateToFromPH
{
public:
    RemoveTemplateFromPH(WorldDocument *doc, PropertyHolder *ph, int index, PropertyTemplate *pt)
        : AddRemoveTemplateToFromPH(doc, ph, index, pt)
    {
        setText(QCoreApplication::translate("Undo Commands", "Remove Template"));
    }

    void undo() { add(); }
    void redo() { remove(); }
};

/////

// Base class for AddTemplateToWorld/RemoveTemplateFromWorld
class AddRemoveTemplateToFromWorld : public QUndoCommand
{
public:
    AddRemoveTemplateToFromWorld(WorldDocument *doc, int index, PropertyTemplate *pt);
    ~AddRemoveTemplateToFromWorld();

protected:
    void add();
    void remove();

    WorldDocument *mDocument;
    PropertyTemplate *mTemplate;
    int mIndex;
};

class AddTemplateToWorld : public AddRemoveTemplateToFromWorld
{
public:
    AddTemplateToWorld(WorldDocument *doc, int index, PropertyTemplate *pt)
        : AddRemoveTemplateToFromWorld(doc, index, pt)
    {
        setText(QCoreApplication::translate("Undo Commands", "Add Template"));
    }

    void undo() { remove(); }
    void redo() { add(); }
};

class RemoveTemplateFromWorld : public AddRemoveTemplateToFromWorld
{
public:
    RemoveTemplateFromWorld(WorldDocument *doc, int index, PropertyTemplate *pt)
        : AddRemoveTemplateToFromWorld(doc, index, pt)
    {
        setText(QCoreApplication::translate("Undo Commands", "Remove Template"));
    }

    void undo() { add(); }
    void redo() { remove(); }
};

/////

class ChangeTemplate : public QUndoCommand
{
public:
    ChangeTemplate(WorldDocument *doc, PropertyTemplate *pt, const QString &name, const QString &desc);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    WorldDocument *mDocument;
    PropertyTemplate *mTemplate;
    QString mName;
    QString mDesc;
};

/////

// Base class for AddPropertyDef/RemovePropertyDef
class AddRemovePropertyDef : public QUndoCommand
{
public:
    AddRemovePropertyDef(WorldDocument *doc, int index, PropertyDef *pd);
    ~AddRemovePropertyDef();

protected:
    void add();
    void remove();

    WorldDocument *mDocument;
    PropertyDef *mPropertyDef;
    int mIndex;
};

class AddPropertyDef : public AddRemovePropertyDef
{
public:
    AddPropertyDef(WorldDocument *doc, int index, PropertyDef *pd)
        : AddRemovePropertyDef(doc, index, pd)
    {
        setText(QCoreApplication::translate("Undo Commands", "Add Property Definition"));
    }

    void undo() { remove(); }
    void redo() { add(); }
};

class RemovePropertyDef : public AddRemovePropertyDef
{
public:
    RemovePropertyDef(WorldDocument *doc, int index, PropertyDef *pd)
        : AddRemovePropertyDef(doc, index, pd)
    {
        setText(QCoreApplication::translate("Undo Commands", "Remove Property Definition"));
    }

    void undo() { add(); }
    void redo() { remove(); }
};

/////

class EditPropertyDef : public QUndoCommand
{
public:
    EditPropertyDef(WorldDocument *doc, PropertyDef *pd, const QString &name, const QString &defValue, const QString &desc);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    WorldDocument *mDocument;
    PropertyDef *mPropertyDef;
    QString mName;
    QString mDefault;
    QString mDesc;
};

/////

class SetPropertyValue : public QUndoCommand
{
public:
    SetPropertyValue(WorldDocument *doc, PropertyHolder *ph, Property *p, const QString &value);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    WorldDocument *mDocument;
    PropertyHolder *mPH;
    Property *mProperty;
    QString mValue;
};

/////

class ChangeCellSelection : public QUndoCommand
{
public:
    ChangeCellSelection(WorldDocument *doc, const QList<WorldCell*> &selection);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    WorldDocument *mDocument;
    QList<WorldCell*> mSelection;
};

/////

// Base class for AddObjectGroup/RemoveObjectGroup
class AddRemoveObjectGroup : public QUndoCommand
{
public:
    AddRemoveObjectGroup(WorldDocument *doc, int index, WorldObjectGroup *og);
    ~AddRemoveObjectGroup();

protected:
    void add();
    void remove();

    WorldDocument *mDocument;
    WorldObjectGroup *mGroup;
    int mIndex;
};

class AddObjectGroup : public AddRemoveObjectGroup
{
public:
    AddObjectGroup(WorldDocument *doc, int index, WorldObjectGroup *og)
        : AddRemoveObjectGroup(doc, index, og)
    {
        setText(QCoreApplication::translate("Undo Commands", "Add Object Group"));
    }

    void undo() { remove(); }
    void redo() { add(); }
};

class RemoveObjectGroup : public AddRemoveObjectGroup
{
public:
    RemoveObjectGroup(WorldDocument *doc, int index)
        : AddRemoveObjectGroup(doc, index, 0)
    {
        setText(QCoreApplication::translate("Undo Commands", "Remove Object Group"));
    }

    void undo() { add(); }
    void redo() { remove(); }
};

/////

class SetObjectGroupName : public QUndoCommand
{
public:
    SetObjectGroupName(WorldDocument *doc, WorldObjectGroup *og, const QString &name);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    WorldDocument *mDocument;
    WorldObjectGroup *mGroup;
    QString mName;
};

/////

class SetObjectGroupColor : public QUndoCommand
{
public:
    SetObjectGroupColor(WorldDocument *doc, WorldObjectGroup *og, const QColor &color);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    WorldDocument *mDocument;
    WorldObjectGroup *mGroup;
    QColor mColor;
};

/////

// Base class for AddObjectType/RemoveObjectType
class AddRemoveObjectType : public QUndoCommand
{
public:
    AddRemoveObjectType(WorldDocument *doc, int index, ObjectType *ot);
    ~AddRemoveObjectType();

protected:
    void add();
    void remove();

    WorldDocument *mDocument;
    ObjectType *mType;
    int mIndex;
};

class AddObjectType : public AddRemoveObjectType
{
public:
    AddObjectType(WorldDocument *doc, int index, ObjectType *ot)
        : AddRemoveObjectType(doc, index, ot)
    {
        setText(QCoreApplication::translate("Undo Commands", "Add Object Type"));
    }

    void undo() { remove(); }
    void redo() { add(); }
};

class RemoveObjectType : public AddRemoveObjectType
{
public:
    RemoveObjectType(WorldDocument *doc, int index)
        : AddRemoveObjectType(doc, index, 0)
    {
        setText(QCoreApplication::translate("Undo Commands", "Remove Object Type"));
    }

    void undo() { add(); }
    void redo() { remove(); }
};

/////

class SetObjectTypeName : public QUndoCommand
{
public:
    SetObjectTypeName(WorldDocument *doc, ObjectType *ot, const QString &name);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    WorldDocument *mDocument;
    ObjectType *mType;
    QString mName;
};

/////

class ProgressBegin : public QUndoCommand
{
public:
    ProgressBegin(const QString &msg)
        : mMsg(msg)
    {}

    void undo() { end(); }
    void redo() { begin(); }

private:
    void begin();
    void end();

    QString mMsg;
};

class ProgressEnd : public QUndoCommand
{
public:
    ProgressEnd(const QString &msg)
        : mMsg(msg)
    {}

    void undo() { begin(); }
    void redo() { end(); }

private:
    void begin();
    void end();

    QString mMsg;
};

#endif // UNDOREDO_H
