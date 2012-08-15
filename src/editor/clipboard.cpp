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
