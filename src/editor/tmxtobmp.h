/*
 * Copyright 2014, Tim Baker <treectrl@users.sf.net>
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

#ifndef TMXTOBMP_H
#define TMXTOBMP_H

#include "singleton.h"

#include <QImage>
#include <QObject>
#include <QPainter>
#include <QSet>

class BMPToTMXImages;
class MapComposite;
class MapInfo;
class WorldBMP;
class WorldCell;
class WorldDocument;

namespace Tiled {
class ObjectGroup;
}

class TMXToBMP : public QObject, public Singleton<TMXToBMP>
{
    Q_OBJECT
public:
    enum GenerateMode {
        GenerateAll,
        GenerateSelected
    };

    explicit TMXToBMP(QObject *parent = 0);

    bool generateWorld(WorldDocument *worldDoc, GenerateMode mode);

    QString errorString() const { return mError; }

private:
    bool shouldGenerateCell(WorldCell *cell, int &bmpIndex);
    bool generateCell(WorldCell *cell);
    bool doBuildings(WorldCell *cell, MapInfo *mapInfo);
    bool processObjectGroups(WorldCell *cell, MapComposite *mapComposite);
    bool processObjectGroup(WorldCell *cell, Tiled::ObjectGroup *objectGroup, int levelOffset, const QPoint &offset);

private:
    WorldDocument *mWorldDoc;
    QList<BMPToTMXImages*> mImages;
    QSet<BMPToTMXImages*> mModifiedImages;
    QImage mImageBldg;
    bool mDoMain;
    bool mDoVeg;
    bool mDoBldg;
    QPainter mBldgPainter;
    QColor mBldgColor;
    QString mError;
};

#endif // TMXTOBMP_H
