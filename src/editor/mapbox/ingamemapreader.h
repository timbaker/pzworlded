/*
 * Copyright 2018, Tim Baker <treectrl@users.sf.net>
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

#ifndef INGAMEMAPREADER_H
#define INGAMEMAPREADER_H

#include <QString>

class World;

class QIODevice;

class InGameMapReaderPrivate;

class InGameMapReader
{
public:
    InGameMapReader();
    ~InGameMapReader();

    World *readWorld(QIODevice *device, const QString &path, World *world);
    World *readWorld(const QString &fileName, World* world);

    QString errorString() const;

private:
    friend class InGameMapReaderPrivate;
    InGameMapReaderPrivate *d;
};

#endif // INGAMEMAPREADER_H
