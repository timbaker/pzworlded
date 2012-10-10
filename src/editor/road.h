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

#ifndef ROAD_H
#define ROAD_H

#include <QPoint>
#include <QRect>
#include <QList>
#include <QString>

class World;

struct TrafficLines
{
    QString name;
    struct {
        QString ns;
        QString we;
    } inner;
    struct {
        QString ns;
        QString we;
    } outer;
};

class Road
{
public:
    Road(World *world, int x1, int y1, int x2, int y2, int width, int style);

    World *world() const
    { return mWorld; }

    void setCoords(const QPoint &start, const QPoint &end);
    void setCoords(int x1, int y1, int x2, int y2);

    void setWidth(int newWidth);
    int width() const { return mWidth; }

    QPoint start() const { return mStart; }
    QPoint end() const { return mEnd; }

    int x1() const { return mStart.x(); }
    int y1() const { return mStart.y(); }
    int x2() const { return mEnd.x(); }
    int y2() const { return mEnd.y(); }

    QRect bounds() const;
    QRect startBounds() const;
    QRect endBounds() const;

    enum RoadOrient
    {
        NorthSouth,
        SouthNorth,
        WestEast,
        EastWest
    };

    RoadOrient orient() const;

    bool isVertical() const
    { return mStart.x() == mEnd.x(); }

    void setTileName(const QString &tileName)
    { mTileName = tileName; }

    QString tileName() const
    { return mTileName; }

    void setTrafficLines(TrafficLines *lines)
    { mTrafficLines = lines; }

    TrafficLines *trafficLines() const
    { return mTrafficLines; }

private:
    World *mWorld;
    QPoint mStart;
    QPoint mEnd;
    int mWidth;
    int mStyle;
    QString mTileName;
    TrafficLines *mTrafficLines;
};

typedef QList<Road*> RoadList;

/////

#include <QVector>

class RoadTemplates
{
public:
    static RoadTemplates *instance();
    static void deleteInstance();

    RoadTemplates()
    {
        TrafficLines *lines = new TrafficLines();
        lines->name = QLatin1String("None");
        mNullTrafficLines = lines;
        mTrafficLines += mNullTrafficLines;

        lines = new TrafficLines();
        lines->name = QLatin1String("Double Yellow");
        lines->inner.ns = QLatin1String("street_trafficlines_01_16");
        lines->inner.we = QLatin1String("street_trafficlines_01_18");
        lines->outer.ns = QLatin1String("street_trafficlines_01_20");
        lines->outer.we = QLatin1String("street_trafficlines_01_22");
        mTrafficLines += lines;
    }

    QVector<QString> roadTiles()
    {
        QVector<QString> roads;
        roads += QLatin1String("floors_exterior_street_01_16");
        roads += QLatin1String("floors_exterior_street_01_17");
        roads += QLatin1String("floors_exterior_street_01_18");
        return roads;
    }

    const QVector<TrafficLines*> &trafficLines() const
    { return mTrafficLines; }

    TrafficLines *nullTrafficLines() const
    { return mNullTrafficLines; }

    TrafficLines *findLines(const QString &name);

private:
    Q_DISABLE_COPY(RoadTemplates)
    static RoadTemplates *mInstance;

    QVector<TrafficLines*> mTrafficLines;
    TrafficLines *mNullTrafficLines;
};

#endif // ROAD_H
