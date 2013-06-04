/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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

#include "heightmapdocument.h"

#include "heightmapfile.h"
#include "world.h"
#include "worlddocument.h"

#include <QUndoStack>

HeightMapDocument::HeightMapDocument(WorldDocument *worldDoc, WorldCell *cell) :
    Document(HeightMapDocType),
    mWorldDoc(worldDoc),
    mCell(cell),
    mFile(new HeightMapFile)
{
    mUndoStack = new QUndoStack(this);

    mFile->open(fileName());
}

HeightMapDocument::~HeightMapDocument()
{
    delete mFile;
}

void HeightMapDocument::setFileName(const QString &fileName)
{
    worldDocument()->world()->setHeightMapFileName(fileName);
}

const QString &HeightMapDocument::fileName() const
{
    return worldDocument()->world()->hmFileName();
}

bool HeightMapDocument::save(const QString &filePath, QString &error)
{
    return false;
}

