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

#ifndef PATHWORLDDOCUMENT_H
#define PATHWORLDDOCUMENT_H

#include "document.h"

#include <QList>
#include <QRectF>

class PathView;
class World;
class WorldLookup;
class WorldScript;

namespace WorldPath {
    class Path;
};

class PathDocument : public Document
{
public:
    PathDocument(World *world, const QString &fileName = QString());
    ~PathDocument();

    void setFileName(const QString &fileName);
    const QString &fileName() const;

    bool save(const QString &filePath, QString &error);

    World *world() const
    { return mWorld; }

    PathView *view() const
    { return (PathView*)Document::view(); }

    QList<WorldScript*> lookupScripts(const QRectF &bounds);
    QList<WorldPath::Path*> lookupPaths(const QRectF &bounds);

private:
    World *mWorld;
    QString mFileName;
    WorldLookup *mLookup;
};

#endif // PATHWORLDDOCUMENT_H
