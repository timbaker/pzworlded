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

#ifndef WORLDCELLINGAMEMAP_H
#define WORLDCELLINGAMEMAP_H

#include <QList>

class WorldCell;

class WorldCellInGameMap;

class InGameMapPoint
{
public:
    InGameMapPoint()
        : x(0.0)
        , y(0.0)
    {

    }

    InGameMapPoint(double x, double y)
        : x(x)
        , y(y)
    {

    }

    double x;
    double y;

    bool operator==(const InGameMapPoint& rhs) const {
        return x == rhs.x && y == rhs.y;
    }

    bool operator!=(const InGameMapPoint& rhs) const {
        return x != rhs.x || y != rhs.y;
    }

    InGameMapPoint operator+(const InGameMapPoint& rhs) const {
        return { x + rhs.x, y + rhs.y };
    }

    InGameMapPoint operator-(const InGameMapPoint& rhs) const {
        return { x - rhs.x, y - rhs.y };
    }
};

class InGameMapCoordinates : public QList<InGameMapPoint>
{
public:
    InGameMapCoordinates& translate(int dx, int dy) {
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

class InGameMapGeometry
{
public:
    bool isLineString() const
    { return mType == QLatin1Literal("LineString"); }

    bool isPoint() const
    { return mType == QLatin1Literal("Point"); }

    bool isPolygon() const
    { return mType == QLatin1Literal("Polygon"); }

    QString mType;
    QList<InGameMapCoordinates> mCoordinates;
};

class InGameMapProperty
{
public:
    QString mKey;
    QString mValue;

    InGameMapProperty()
    {

    }

    InGameMapProperty(const QString& key, const QString &value)
        : mKey(key)
        , mValue(value)
    {

    }

    InGameMapProperty(const QString& key, int value)
        : mKey(key)
        , mValue(QString::number(value))
    {

    }
};

class InGameMapProperties : public QList<InGameMapProperty>
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

class InGameMapFeature
{
public:
    InGameMapFeature(WorldCellInGameMap* owner)
        : mOwner(owner)
    {

    }

    inline int index();
    inline WorldCell* cell() const;
    inline InGameMapProperties& properties() { return mProperties; }

    WorldCellInGameMap* mOwner;
    InGameMapGeometry mGeometry;
    InGameMapProperties mProperties;
};

class InGameMapFeatures : public QList<InGameMapFeature*>
{
public:
    InGameMapFeatures(WorldCellInGameMap* owner)
        : mOwner(owner)
    {

    }

    ~InGameMapFeatures() {
        qDeleteAll(*this);
    }

    WorldCellInGameMap* mOwner;
};

class WorldCellInGameMap
{
public:
    WorldCellInGameMap(WorldCell* cell)
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
    InGameMapFeatures& features() { return mFeatures; }

    WorldCell* mCell;
    InGameMapFeatures mFeatures;
};

int InGameMapFeature::index() {
    return mOwner->mFeatures.indexOf(this);
}

WorldCell* InGameMapFeature::cell() const {
    return mOwner->cell();
}

#endif // WORLDCELLINGAMEMAP_H
