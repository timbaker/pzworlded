/*
 * Copyright 2021, Tim Baker <treectrl@users.sf.net>
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

#ifndef INGAMEMAPWRITERBINARY_H
#define INGAMEMAPWRITERBINARY_H

#include <QString>

class World;
class InGameMapWriterBinaryPrivate;

class QIODevice;

class InGameMapWriterBinary
{
public:
    InGameMapWriterBinary();
    ~InGameMapWriterBinary();

    bool writeWorld(World *world, const QString &filePath, bool b256);
    void writeWorld(World *world, QIODevice *device, const QString &absDirPath, bool b256);

    QString errorString() const;

private:
    InGameMapWriterBinaryPrivate *d;
};

#endif // INGAMEMAPWRITERBINARY_H
