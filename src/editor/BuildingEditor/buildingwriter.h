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

#ifndef BUILDINGWRITER_H
#define BUILDINGWRITER_H

#include <QString>

class QIODevice;

namespace BuildingEditor {

class BuildingWriterPrivate;

class Building;

class BuildingWriter
{
public:
    BuildingWriter();
    ~BuildingWriter();

    bool write(Building *building, const QString &filePath);

    QString errorString() const;

private:
    void write(Building *building, QIODevice *device, const QString &absDirPath);

    BuildingWriterPrivate *d;
};

} // namespace BuildingEditor

#endif // BUILDINGWRITER_H
