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

    bool isClockwise() const
    {
        float sum = 0.0f;
        for (int i = 0; i < size(); i++) {
            auto& p1 = at(i);
            auto& p2 = at((i + 1) % size());
            sum += (p2.x - p1.x) * (p2.y + p1.y);
        }
        return sum > 0.0f;
    }

};

class MapBoxGeometry
{
public:
    bool isLineString() const
    { return mType == QLatin1Literal("LineString"); }

    bool isPoint() const
    { return mType == QLatin1Literal("Point"); }

    bool isPolygon() const
    { return mType == QLatin1Literal("Polygon"); }

    QString mType;
    QList<MapBoxCoordinates> mCoordinates;
};

class MapBoxProperty
{
public:
    QString mKey;
    QString mValue;

    MapBoxProperty()
    {

    }

    MapBoxProperty(const QString& key, const QString &value)
        : mKey(key)
        , mValue(value)
    {

    }

    MapBoxProperty(const QString& key, int value)
        : mKey(key)
        , mValue(QString::number(value))
    {

    }
};

class MapBoxProperties : public QList<MapBoxProperty>
{
public:
    void set(const QString& key, const QString& value) {
        for (auto& property : *this) {
            if (property.mKey == key) {
                property.mValue = value;
                return;
            }
        }
        push_back({key, value});
    }

    void set(const QString& key, int value) {
        for (auto& property : *this) {
            if (property.mKey == key) {
                property.mValue = value;
                return;
            }
        }
        push_back({key, value});
    }

    bool containsKey(const QString& key) const {
        for (auto& property : *this) {
            if (property.mKey == key)
                return true;
        }
        return false;
    }

    bool contains(const QString& key, const QString &value) const {
        for (auto& property : *this) {
            if ((property.mKey == key) && (property.mValue == value))
                return true;
        }
        return false;
    }

    int getInt(const QString& key, int defaultVal) const {
        for (auto& property : *this) {
            if (property.mKey == key) {
                bool ok;
                int result = property.mValue.toInt(&ok);
                return ok ? result : defaultVal;
            }
        }
        return defaultVal;
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

    void clear()
    {
        qDeleteAll(mFeatures);
        mFeatures.clear();
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
