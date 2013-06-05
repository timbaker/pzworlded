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

#ifndef HEIGHTMAPDOCUMENT_H
#define HEIGHTMAPDOCUMENT_H

#include "document.h"

class HeightMapFile;
class WorldCell;

class HeightMapDocument : public Document
{
public:
    HeightMapDocument(WorldDocument *worldDoc, WorldCell *cell);
    ~HeightMapDocument();

    void setFileName(const QString &fileName);
    const QString &fileName() const;
    bool save(const QString &filePath, QString &error);

    bool isModified() const { return false; }

    WorldDocument *worldDocument() const
    { return mWorldDoc; }

    WorldCell *cell() const
    { return mCell; }

    HeightMapFile *hmFile() const;

private:
    WorldDocument *mWorldDoc;
    WorldCell *mCell;
};

#endif // HEIGHTMAPDOCUMENT_H
