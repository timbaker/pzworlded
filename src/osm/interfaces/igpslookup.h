/*
Copyright 2010  Christian Vetter veaac.fdirct@gmail.com

This file is part of MoNav.

MoNav is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

MoNav is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with MoNav.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef IGPSLOOKUP_H
#define IGPSLOOKUP_H

#include "utils/config.h"
#include "utils/coordinates.h"
#include <QtPlugin>
#include <QVector>

class IGPSLookup
{
public:

	struct Result {
		// source + target + edgeID uniquely identify an edge
		NodeID source;
		NodeID target;
		unsigned edgeID;
		// the nearest point on the edge
		UnsignedCoordinate nearestPoint;
		// the amount of way coordinates on the way before the nearest point
		unsigned previousWayCoordinates;
		// the position on the way
		double percentage;
		// the distance to the nearest point
		double distance;

	};

	virtual ~IGPSLookup() {}

	virtual QString GetName() = 0;
	virtual void SetInputDirectory( const QString& dir ) = 0;
	virtual void ShowSettings() = 0;
	virtual bool IsCompatible( int fileFormatVersion ) = 0;
	virtual bool LoadData() = 0;
	virtual bool UnloadData() = 0;
	// gets the nearest routing edge; a heading penalty can be applied if the way's orientation differs greatly from the current heading.
	virtual bool GetNearestEdge( Result* result, const UnsignedCoordinate& coordinate, double radius, bool headingPenalty = 0, double heading = 0 ) = 0;
};

Q_DECLARE_INTERFACE( IGPSLookup, "monav.IGPSLookup/1.1" )

#endif // IGPSLOOKUP_H
