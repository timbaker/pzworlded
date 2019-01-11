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

#ifndef WORLDCELLMAPBOX_H
#define WORLDCELLMAPBOX_H

#include <QList>

class WorldCell;

class WorldCellMapBox;

class MapBoxPoint
{
public:
    MapBoxPoint()
        : x(0.0)
        , y(0.0)
    {

    }

    MapBoxPoint(double x, double y)
        : x(x)
        , y(y)
    {

    }

    double x;
    double y;

    bool operator==(const MapBoxPoint& rhs) const {
        return x == rhs.x && y == rhs.y;
    }

    bool operator!=(const MapBoxPoint& rhs) const {
        return x != rhs.x || y != rhs.y;
    }

    MapBoxPoint operator+(const MapBoxPoint& rhs) const {
        return { x + rhs.x, y + rhs.y };
    }

    MapBoxPoint operator-(const MapBoxPoint& rhs) const {
        return { x - rhs.x, y - rhs.y };
    }
};

class MapBoxCoordinates : public QList<MapBoxPoint>
{
public:
    MapBoxCoordinates& translate(int dx, int dy) {
        for (auto& pt : *this) {
            pt.x += dx;
            pt.y += dy;
        }
        return *this;
    }
};

class MapBoxGeometry
{
public:
    QString mType;
    QList<MapBoxCoordinates> mCoordinates;
};

class MapBoxProperty
{
public:
    QString mKey;
    QString mValue;
};

class MapBoxProperties : public QList<MapBoxProperty>
{
public:
    bool containsKey(const QString& key) const {
        for (auto& property : *this) {
            if (property.mKey == key)
                return true;
        }
        return false;
    }
};

class MapBoxFeature
{
public:
    MapBoxFeature(WorldCellMapBox* owner)
        : mOwner(owner)
    {

    }

    inline int index();
    inline WorldCell* cell() const;
    inline MapBoxProperties& properties() { return mProperties; }

    WorldCellMapBox* mOwner;
    MapBoxGeometry mGeometry;
    MapBoxProperties mProperties;
};

class MapBoxFeatures : public QList<MapBoxFeature*>
{
public:
    MapBoxFeatures(WorldCellMapBox* owner)
        : mOwner(owner)
    {

    }

    ~MapBoxFeatures() {
        qDeleteAll(*this);
    }

    WorldCellMapBox* mOwner;
};

class WorldCellMapBox
{
public:
    WorldCellMapBox(WorldCell* cell)
        : mCell(cell)
        , mFeatures(this)
    {

    }

    WorldCell* cell() const { return mCell; }
    MapBoxFeatures& features() { return mFeatures; }

    WorldCell* mCell;
    MapBoxFeatures mFeatures;
};

int MapBoxFeature::index() {
    return mOwner->mFeatures.indexOf(this);
}

WorldCell* MapBoxFeature::cell() const {
    return mOwner->cell();
}

#endif // WORLDCELLMAPBOX_H
