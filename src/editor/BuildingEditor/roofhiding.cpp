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

#include "roofhiding.h"

using namespace BuildingEditor;

bool RoofHiding::isEmptyOutside(const QString &roomName)
{
    int pos = roomName.indexOf(QLatin1Char('#'));
    if (pos != -1)
        return roomName.left(pos).compare(QLatin1String("emptyoutside"), Qt::CaseInsensitive) == 0;
    return roomName.compare(QLatin1String("emptyoutside"), Qt::CaseInsensitive) == 0;
}
