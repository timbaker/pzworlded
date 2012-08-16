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

#include "clipboard.h"

#include "world.h"
#include "worldcell.h"
#include "worlddocument.h"

#include <QUndoStack>

Clipboard *Clipboard::mInstance = 0;

Clipboard *Clipboard::instance()
{
    if (!mInstance)
        mInstance = new Clipboard;
    return mInstance;
}

void Clipboard::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

bool Clipboard::isEmpty() const
{
    if (!mWorld)
        return true;
    if (mCellContents.size())
        return false;
    if (mWorld->propertyDefinitions().size())
        return false;
    if (mWorld->propertyTemplates().size())
        return false;
    if (mWorld->objectGroups().size())
        return false;
    if (mWorld->objectTypes().size())
        return false;
    return true;
}

void Clipboard::setWorld(World *world)
{
    qDeleteAll(mCellContents);
    mCellContents.clear();
    delete mWorld;

    mWorld = world;

    if (mWorld) {
        for (int y = 0; y < world->height(); y++) {
            for (int x = 0; x < world->width(); x++) {
                WorldCell *cell = mWorld->cellAt(x, y);
                if (!cell->isEmpty())
                    mCellContents += new WorldCellContents(cell, false);
            }
        }
    }

    emit clipboardChanged();
}

const QList<WorldCellContents *> &Clipboard::cellsInClipboard() const
{
    return mCellContents;
}

const int Clipboard::cellsInClipboardCount() const
{
    return mCellContents.size();
}

/**
  * This can be called multiple times by PasteCellsTool. It mustn't make
  * changes more than once.
  */
void Clipboard::pasteEverythingButCells(WorldDocument *worldDoc)
{
    worldDoc->undoStack()->beginMacro(tr("Paste"));
    World *world = worldDoc->world();
    World *worldClip = mWorld;
    foreach (PropertyDef *pdClip, worldClip->propertyDefinitions()) {
        PropertyDef *pd = world->propertyDefinitions().findPropertyDef(pdClip->mName);
        if (pd) {
            if (*pd != *pdClip)
                worldDoc->changePropertyDefinition(pd, pdClip);
        } else {
            pd = new PropertyDef(pdClip);
            worldDoc->addPropertyDefinition(pd);
        }
    }
    foreach (PropertyTemplate *ptClip, worldClip->propertyTemplates()) {
        PropertyTemplate *pt = world->propertyTemplates().find(ptClip->mName);
        if (pt) {
            if (*pt != *ptClip)
                worldDoc->changeTemplate(pt, ptClip);
        } else {
            pt = new PropertyTemplate(world, ptClip);
            worldDoc->addTemplate(pt);
        }
    }
    foreach (ObjectType *otClip, worldClip->objectTypes()) {
        ObjectType *ot = world->objectTypes().find(otClip->name());
        if (ot) {
            if (*ot != *otClip)
                worldDoc->changeObjectType(ot, otClip);
        } else {
            ot = new ObjectType(world, otClip);
            worldDoc->addObjectType(ot);
        }
    }
    foreach (WorldObjectGroup *ogClip, worldClip->objectGroups()) {
        WorldObjectGroup *og = world->objectGroups().find(ogClip->name());
        if (og) {
            if (*og != *ogClip)
                worldDoc->changeObjectGroup(og, ogClip);
        } else {
            og = new WorldObjectGroup(world, ogClip);
            worldDoc->addObjectGroup(og);
        }
    }
    worldDoc->undoStack()->endMacro();
}

Clipboard::Clipboard(QObject *parent)
    : QObject(parent)
    , mWorld(0)
{
}

Clipboard::~Clipboard()
{
    qDeleteAll(mCellContents);
    delete mWorld;
}
