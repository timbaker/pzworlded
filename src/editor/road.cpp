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

#include "road.h"

Road::Road(World *world, int x1, int y1, int x2, int y2, int width, int style)
    : mWorld(world)
    , mStart(x1, y1)
    , mEnd(x2, y2)
    , mWidth(width)
    , mStyle(style)
    , mTrafficLines(0)
{
    Q_ASSERT(mWidth > 0);
    mTrafficLines = RoadTemplates::instance()->nullTrafficLines();
}

void Road::setCoords(const QPoint &start, const QPoint &end)
{
    mStart = start;
    mEnd = end;
}

void Road::setWidth(int newWidth)
{
    Q_ASSERT(newWidth > 0);
    mWidth = newWidth;
}

QRect Road::bounds() const
{
    return startBounds() | endBounds();
}

QRect Road::startBounds() const
{
    return QRect(x1() - mWidth / 2, y1() - mWidth / 2, mWidth, mWidth);
}

QRect Road::endBounds() const
{
    return QRect(x2() - mWidth / 2, y2() - mWidth / 2, mWidth, mWidth);
}

Road::RoadOrient Road::orient() const
{
    if (x1() == x2()) {
        if (y1() < y2())
            return NorthSouth;
        else
            return SouthNorth;
    } else {
        if (x1() < x2())
            return WestEast;
        else
            return EastWest;
    }
}

/////

RoadTemplates *RoadTemplates::mInstance = 0;

RoadTemplates *RoadTemplates::instance()
{
    if (!mInstance)
        mInstance = new RoadTemplates();
    return mInstance;
}

void RoadTemplates::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

RoadTemplates::RoadTemplates()
{
    mRoadTiles += QString();
#if 0
    mRoadTiles += QLatin1String("floors_exterior_street_01_16");
    mRoadTiles += QLatin1String("floors_exterior_street_01_17");
    mRoadTiles += QLatin1String("floors_exterior_street_01_18");
#endif

    /////

    TrafficLines *lines = new TrafficLines();
    lines->name = QLatin1String("None");
    mNullTrafficLines = lines;
    mTrafficLines += mNullTrafficLines;
#if 0
    lines = new TrafficLines();
    lines->name = QLatin1String("Double White (Faded)");
    lines->inner.ns = QLatin1String("street_trafficlines_01_0");
    lines->inner.we = QLatin1String("street_trafficlines_01_2");
    lines->inner.nw = QLatin1String("street_trafficlines_01_1");
    lines->inner.sw = QLatin1String("street_trafficlines_01_7");
    lines->outer.ns = QLatin1String("street_trafficlines_01_4");
    lines->outer.we = QLatin1String("street_trafficlines_01_6");
    lines->outer.ne = QLatin1String("street_trafficlines_01_3");
    lines->outer.se = QLatin1String("street_trafficlines_01_5");
    mTrafficLines += lines;

    lines = new TrafficLines();
    lines->name = QLatin1String("Double White");
    lines->inner.ns = QLatin1String("street_trafficlines_01_8");
    lines->inner.we = QLatin1String("street_trafficlines_01_10");
    lines->inner.nw = QLatin1String("street_trafficlines_01_9");
    lines->inner.sw = QLatin1String("street_trafficlines_01_15");
    lines->outer.ns = QLatin1String("street_trafficlines_01_12");
    lines->outer.we = QLatin1String("street_trafficlines_01_14");
    lines->outer.ne = QLatin1String("street_trafficlines_01_11");
    lines->outer.se = QLatin1String("street_trafficlines_01_13");
    mTrafficLines += lines;

    lines = new TrafficLines();
    lines->name = QLatin1String("Double Yellow (Faded)");
    lines->inner.ns = QLatin1String("street_trafficlines_01_16");
    lines->inner.we = QLatin1String("street_trafficlines_01_18");
    lines->inner.nw = QLatin1String("street_trafficlines_01_17");
    lines->inner.sw = QLatin1String("street_trafficlines_01_23");
    lines->outer.ns = QLatin1String("street_trafficlines_01_20");
    lines->outer.we = QLatin1String("street_trafficlines_01_22");
    lines->outer.ne = QLatin1String("street_trafficlines_01_19");
    lines->outer.se = QLatin1String("street_trafficlines_01_21");
    mTrafficLines += lines;

    lines = new TrafficLines();
    lines->name = QLatin1String("Double Yellow");
    lines->inner.ns = QLatin1String("street_trafficlines_01_24");
    lines->inner.we = QLatin1String("street_trafficlines_01_26");
    lines->inner.nw = QLatin1String("street_trafficlines_01_25");
    lines->inner.sw = QLatin1String("street_trafficlines_01_31");
    lines->outer.ns = QLatin1String("street_trafficlines_01_28");
    lines->outer.we = QLatin1String("street_trafficlines_01_30");
    lines->outer.ne = QLatin1String("street_trafficlines_01_27");
    lines->outer.se = QLatin1String("street_trafficlines_01_29");
    mTrafficLines += lines;
#endif

    parseRoadsDotTxt();
}

TrafficLines *RoadTemplates::findLines(const QString &name)
{
    foreach (TrafficLines *lines, mTrafficLines) {
        if (lines->name == name)
            return lines;
    }

    return mNullTrafficLines;
}

#include <QCoreApplication>
#include <QDebug>
#include <QMessageBox>

void RoadTemplates::parseRoadsDotTxt()
{
    QString appDirectory = QCoreApplication::applicationDirPath();
    QString filePath = appDirectory + QLatin1Char('/') + QLatin1String("Roads.txt");
    SimpleFile simpleFile;
    if (!simpleFile.read(filePath)) {
        QMessageBox::warning(0, QLatin1String("Error reading Roads.txt"),
                             QString(QLatin1String("Failed to open %1")).arg(filePath));
        return;
    }

//    simpleFile.print();

    foreach (SimpleFileBlock block, simpleFile.blocks) {
        if (block.name == QLatin1String("road"))
            handleRoad(block);
        else if (block.name == QLatin1String("lines"))
            handleLines(block);
        else {
            QMessageBox::warning(0, QLatin1String("Error reading Roads.txt"),
                                 QString(QLatin1String("Unknown block name '%1'.\nProbable syntax error in Roads.txt.\nDo not save the project file, road info is messed up!")).arg(block.name));
        }
    }
}

void RoadTemplates::handleRoad(SimpleFileBlock block)
{
    mRoadTiles += block.value("tile");
}

void RoadTemplates::handleLines(SimpleFileBlock block)
{
    TrafficLines *lines = new TrafficLines;
    lines->name = block.value("name");

    SimpleFileBlock inner = block.block("inner");
    lines->inner.ns = inner.value("ns");
    lines->inner.we = inner.value("we");
    lines->inner.nw = inner.value("nw");
    lines->inner.sw = inner.value("sw");

    SimpleFileBlock outer = block.block("outer");
    lines->outer.ns = outer.value("ns");
    lines->outer.we = outer.value("we");
    lines->outer.ne = outer.value("ne");
    lines->outer.se = outer.value("se");

    mTrafficLines += lines;
}
