/*
 * buildingcell.h
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

#ifndef BUILDINGCELL_H
#define BUILDINGCELL_H

#include "maprotation.h"

#include <QString>

namespace BuildingEditor {

// Equivalent of Tiled::Cell
class BuildingCell
{
public:
    BuildingCell()
        : mRotation(Tiled::MapRotation::NotRotated)
    {

    }

    BuildingCell(const QString &tileName, Tiled::MapRotation rotation)
        : mTileName(tileName)
        , mRotation(rotation)
    {

    }

    bool isEmpty() const
    {
        return mTileName.isEmpty();
    }

    const QString& tileName() const
    {
        return mTileName;
    }

    Tiled::MapRotation rotation() const
    {
        return mRotation;
    }

    QString mTileName;
    Tiled::MapRotation mRotation;
};

inline bool operator==(const BuildingCell& lhs, const BuildingCell& rhs)
{
    return lhs.mTileName == rhs.tileName() && lhs.mRotation == rhs.rotation();
}

inline bool operator!=(const BuildingCell& lhs, const BuildingCell& rhs)
{
    return !(lhs == rhs);
}

// namespace BuildingEditor
}

#endif // BUILDINGCELL_H
