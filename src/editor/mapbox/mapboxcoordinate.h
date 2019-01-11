/*
 * Copyright 2019, Tim Baker <treectrl@users.sf.net>
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

#ifndef MAPBOXCOORDINATE_H
#define MAPBOXCOORDINATE_H

#define _USE_MATH_DEFINES // Windows M_PI
#include <math.h>

#include <QMapbox>

class GameCoordinate {
public:
    // https://gis.stackexchange.com/questions/2951/algorithm-for-offsetting-a-latitude-longitude-by-some-amount-of-meters
    // If your displacements aren't too great (less than a few kilometers) and you're not right at the poles,
    // use the quick and dirty estimate that 111,111 meters (111.111 km) in the y direction is 1 degree (of latitude)
    // and 111,111 * cos(latitude) meters in the x direction is 1 degree (of longitude).
    static constexpr double LATITUDE = 0.0; // north/south
    static constexpr double LONGITUDE = 0.0; // west/east

    static double degreesToRadians(double degrees) {
        return degrees * M_PI / 180.0;
    }

    static double toLat(double worldX, double worldY) {
        return LATITUDE + -worldY / 111111.0;
    }

    static double toLong(double worldX, double worldY) {
        return LONGITUDE + worldX / (111111.0 * std::cos(degreesToRadians(toLat(worldX, worldY))));
    }

    bool operator==(const GameCoordinate& rhs) const {
        return x == rhs.x && y == rhs.y;
    }

    static GameCoordinate fromSquare(int x, int y) {
        return GameCoordinate(x, y);
    }

    static GameCoordinate fromLatLng(const QMapbox::Coordinate& latLng) {
        double x = (latLng.second - LONGITUDE) * (111111.0 * std::cos(degreesToRadians(latLng.first)));
        double y = -(latLng.first - LATITUDE) * 111111.0;
        return { (int)std::round(x), (int)std::round(y) };
    }

    QMapbox::Coordinate toLatLng() {
        return { toLat(x, y), toLong(x, y) };
    }

    int x, y; // IsoGridSquare coordinates

private:
    GameCoordinate(int x, int y) :
        x(x), y(y)
    {}

};

#endif // MAPBOXCOORDINATE_H
