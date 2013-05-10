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

#ifndef INTERSECTION_H
#define INTERSECTION_H

#include "utils/coordinates.h"

struct DoublePoint {
	DoublePoint(){
		x = y = 0;
	}
	DoublePoint( double xVal, double yVal ) {
		x = xVal;
		y = yVal;
	}
	double x;
	double y;
};

static inline bool pointInPolygon( int numPoints, const DoublePoint* points, DoublePoint testPoint ) {
	bool inside = false;
	for ( int i = 0, j = numPoints - 1; i < numPoints; j = i++ ) {
		if ( ((  points[i].y > testPoint.y ) != ( points[j].y > testPoint.y ) ) && ( testPoint.x < ( points[j].x - points[i].x ) * ( testPoint.y - points[i].y ) / ( points[j].y - points[i].y ) + points[i].x ) )
			inside = !inside;
	}
	return inside;
}

static inline bool clipHelper( double directedProjection, double directedDistance, double* tMinimum, double* tMaximum ) {
	if ( directedProjection == 0 ) {
		if ( directedDistance < 0 )
			return false;
	} else {
		double amount = directedDistance / directedProjection;
		if ( directedProjection < 0 ) {
			if ( amount > *tMaximum )
				return false;
			else if ( amount > *tMinimum )
				*tMinimum = amount;
		} else {
			if ( amount < *tMinimum )
				return false;
			else if ( amount < *tMaximum )
				*tMaximum = amount;
		}
	}
	return true;
}

static inline bool clipEdge( ProjectedCoordinate* start, ProjectedCoordinate* end, ProjectedCoordinate min, ProjectedCoordinate max ) {
	const double xDiff = ( double ) end->x - start->x;
	const double yDiff = ( double ) end->y - start->y;
	double tMinimum = 0, tMaximum = 1;

	if ( clipHelper( -xDiff, ( double ) start->x - min.x, &tMinimum, &tMaximum ) ) {
		if ( clipHelper( xDiff, ( double ) max.x - start->x, &tMinimum, &tMaximum ) ) {
			if ( clipHelper( -yDiff, ( double ) start->y - min.y, &tMinimum, &tMaximum ) ) {
				if ( clipHelper( yDiff, ( double ) max.y - start->y, &tMinimum, &tMaximum ) ) {
					if ( tMaximum < 1 ) {
						end->x = start->x + tMaximum * xDiff;
						end->y = start->y + tMaximum * yDiff;
					}
					if ( tMinimum > 0 ) {
						start->x += tMinimum * xDiff;
						start->y += tMinimum * yDiff;
					}
					if ( start->x < min.x )
						start->x = min.x;
					if ( start->y < min.y )
						start->y = min.y;
					if ( start->x > max.x )
						start->x = max.x;
					if ( start->y > max.y )
						start->y = max.y;

					if ( end->x < min.x )
						end->x = min.x;
					if ( end->y < min.y )
						end->y = min.y;
					if ( end->x > max.x )
						end->x = max.x;
					if ( end->y > max.y )
						end->y = max.y;

					return true;
				}
			}
		}
	}
	return false;

}

#endif // INTERSECTION_H
