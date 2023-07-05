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

#ifndef INGAMEMAP_FEATURE_GENERATOR_H
#define INGAMEMAP_FEATURE_GENERATOR_H

#include <QImage>
#include <QObject>
#include <QPainter>
#include <QSet>

class MapComposite;
class MapInfo;
class WorldCell;
class WorldDocument;

namespace Tiled {
class ObjectGroup;
}


class InGameMapFeatureGenerator : public QObject
{
    Q_OBJECT
public:
    enum GenerateMode {
        GenerateAll,
        GenerateSelected
    };
    enum FeatureType {
        FeatureBuilding,
        FeatureTree,
        FeatureWater
    };
    struct GenerateCellFailure
    {
        WorldCell* cell;
        QString error;

        GenerateCellFailure(WorldCell* cell, const QString& error)
            : cell(cell)
            , error(error)
        {
        }
    };

    explicit InGameMapFeatureGenerator(QObject *parent = nullptr);

    bool generateWorld(WorldDocument *worldDoc, GenerateMode mode, FeatureType type);

    QString errorString() const { return mError; }

private:
    bool shouldGenerateCell(WorldCell *cell);
    bool generateCell(WorldCell *cell);
    bool doBuildings(WorldCell *cell, MapInfo *mapInfo);
    bool processObjectGroups(WorldCell *cell, MapComposite *mapComposite);
    bool processObjectGroup(WorldCell *cell, Tiled::ObjectGroup *objectGroup, int levelOffset, const QPoint &offset);
    bool processObjectGroup(WorldCell *cell, MapInfo *mapInfo, Tiled::ObjectGroup *objectGroup, int levelOffset, const QPoint &offset, QRect &bounds, QVector<QRect> &rects);
    bool traceBuildingOutline(WorldCell *cell, MapInfo *mapInfo, QRect &bounds, QVector<QRect> &rects);
    bool isInvalidBuildingPolygon(const QPolygon &poly);
    bool doWater(WorldCell* cell, MapInfo* mapInfo);
    bool doTrees(WorldCell* cell, MapInfo *mapInfo);

private:
    WorldDocument *mWorldDoc;
    QString mError;
    FeatureType mFeatureType;
    QList<GenerateCellFailure> mFailures;
};

#endif // INGAMEMAP_FEATURE_GENERATOR_H
