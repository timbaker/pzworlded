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

#ifndef COORDINATES_H_INCLUDED
#define COORDINATES_H_INCLUDED

#include <limits>
#include <cmath>
#include <cassert>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
	this class represents a GPS position
	latitude ranges in [-90,+90]
 */
class GPSCoordinate {
public:

	/** initialize with invalid status */
	GPSCoordinate()
	{
		latitude = longitude = std::numeric_limits< double >::max();
	}
	/** initalize with given (lat,lon) pair */
	GPSCoordinate( double lat, double lon )
	{
		latitude = lat;
		longitude = lon;
	}

	/**
		computes the distance on the earth's surface

		uses vincenty's inverse formula
		it computes the distance on the WGS84 ellipsoid, which should be exact enough for many purposes
		the computation is quite slow as it includes many triogometric functions

		can fail for extrem cases ( e.g., pole to pole )
	 */
	double Distance( const GPSCoordinate &right ) const
	{
		assert( fabs( latitude ) < 90 && fabs( right.latitude ) < 90 );
		//assert ( nicht antipodal, nicht an den Polen )

		// convert inputs in degrees to radians:
		static const double DEG_TO_RAD = 0.017453292519943295769236907684886;
		double lat1 = latitude * DEG_TO_RAD;
		double lon1 = longitude * DEG_TO_RAD;
		double lat2 = right.latitude * DEG_TO_RAD;
		double lon2 = right.longitude * DEG_TO_RAD;

		static const double a = 6378137;
		static const double b = 6356752.31424518;
		static const double f = ( a - b ) / a;

		const double U1 = atan(( 1 - f ) * tan( lat1 ) );
		const double U2 = atan(( 1 - f ) * tan( lat2 ) );
		const double cosU1 = cos( U1 );
		const double cosU2 = cos( U2 );
		const double sinU1 = sin( U1 );
		const double sinU2 = sin( U2 );
		const double L = fabs( lon2 - lon1 );

		double lambda = L;
		double lambdaOld;
		unsigned iterLimit = 50;
		while ( true ) {
			const double cosLambda = cos( lambda );
			const double sinLambda = sin( lambda );
			const double leftSinSigma = cosU2 * sinLambda;
			const double rightSinSigma = cosU1 * sinU2 - sinU1 * cosU2 * cosLambda;
			const double sinSigma = sqrt( leftSinSigma * leftSinSigma + rightSinSigma * rightSinSigma );
			if ( sinSigma == 0 ) // Fast identisch
				return 0;
			const double cosSigma = sinU1 * sinU2 + cosU1 * cosU2 * cosLambda;
			const double sigma = atan2( sinSigma, cosSigma );
			const double sinAlpha = cosU1 * cosU2 * sinLambda / sinSigma;
			const double cosSquareAlpha = 1 - sinAlpha * sinAlpha;
			double cos2sigmam = cosSigma - 2 * sinU1 * sinU2 / cosSquareAlpha;
			if ( cos2sigmam != cos2sigmam ) // NAN: Parellel zum Äquator
				cos2sigmam = 0;
			const double C = f / 16 * cosSquareAlpha * ( 4 + f * ( 4 - 3 * cosSquareAlpha ) );
			lambdaOld = lambda;
			lambda = L + ( 1 - C ) * f * sinAlpha * ( sigma + C * sinSigma * ( cos2sigmam + C * cosSigma * ( -1 + 2 * cos2sigmam * cos2sigmam ) ) );
			if ( fabs( lambda - lambdaOld ) < 1e-12 || --iterLimit == 0 ) {
				const double u2 = cosSquareAlpha * ( a * a - b * b ) / ( b * b );
				const double A = 1 + u2 / 16384 * ( 4096 + u2 * ( -768 + u2 * ( 320 - 175 * u2 ) ) );
				const double B = u2 / 1024 * ( 256 + u2 * ( -128 + u2 * ( 74 - 47 * u2 ) ) );
				const double deltasigma = B * sinSigma * ( cos2sigmam + B / 4 * ( cosSigma * ( -1 + 2 * cos2sigmam * cos2sigmam ) - B / 6 * cos2sigmam * ( -3 + 4 * sinSigma * sinSigma ) * ( -3 + 4 * cos2sigmam * cos2sigmam ) ) );
				return b * A * ( sigma - deltasigma );
			}
		}

		//should never be reached
		return 0;
	}

	/**
		approximates the distance on the earth's surface

		uses a sphere approximation and the law of haversines
		usually about 6% worse than vincenty's approximation
	 */
	double ApproximateDistance( const GPSCoordinate &right ) const
	{
		static const double DEG_TO_RAD = 0.017453292519943295769236907684886;
		///Earth's quatratic mean radius for WGS-84
		static const double EARTH_RADIUS_IN_METERS = 6372797.560856;
		double latitudeArc  = ( latitude - right.latitude ) * DEG_TO_RAD;
		double longitudeArc = ( longitude - right.longitude ) * DEG_TO_RAD;
		double latitudeH = sin( latitudeArc * 0.5 );
		latitudeH *= latitudeH;
		double lontitudeH = sin( longitudeArc * 0.5 );
		lontitudeH *= lontitudeH;
		double tmp = cos( latitude * DEG_TO_RAD ) * cos( right.latitude * DEG_TO_RAD );
		double distanceArc =  2.0 * asin( sqrt( latitudeH + tmp * lontitudeH ) );
		return EARTH_RADIUS_IN_METERS * distanceArc;
	}

	bool operator==( const GPSCoordinate& right ) const
	{
		return latitude == right.latitude && longitude == right.longitude;
	}

	bool operator!=( const GPSCoordinate& right ) const
	{
		return !( *this == right );
	}

	bool operator<( const GPSCoordinate& right ) const
	{
		if ( latitude != right.latitude )
			return latitude < right.latitude;
		return longitude < right.longitude;
	}

	/**
		checks whether this coordinate is in the invalid state
		does not check for invalid coordinates, e.g. latitude in [-90,90]
	 */
	bool IsValid() const
	{
		return latitude != std::numeric_limits< double >::max() && longitude != std::numeric_limits< double >::max();
	}

	double latitude, longitude;
};

/**
	This class represents a coordinate in the mercator projection

	x and y values range from 0 to 1
 */
class ProjectedCoordinate {
public:

	/** contructs a coordinate in the invalid state */
	ProjectedCoordinate()
	{
		x = y = std::numeric_limits< double >::max();
	}

	ProjectedCoordinate( double xVal, double yVal )
	{
		x = xVal;
		y = yVal;
	}

	ProjectedCoordinate( double xVal, double yVal, int zoom ) {
		x = xVal / ( 1u << zoom );
		y = yVal / ( 1u << zoom );
	}

	/** constructs a coordinate from GPS position */
	explicit ProjectedCoordinate( const GPSCoordinate& gps )
	{
		if ( !gps.IsValid() ) {
			x = y = std::numeric_limits< double >::max();
			return;
		}
		x = ( gps.longitude + 180.0 ) / 360.0;
		y = ( 1.0 - log( tan( gps.latitude * M_PI / 180.0 ) + 1.0 / cos( gps.latitude * M_PI / 180.0 ) ) / M_PI ) / 2.0;
	}

	/** converts into a GPS position */
	GPSCoordinate ToGPSCoordinate() const
	{
		GPSCoordinate gps;
		if ( !IsValid() )
			return gps;

		gps.longitude = x * 360.0 - 180;
		const double n = M_PI - 2.0 * M_PI * y;
		gps.latitude = 180.0 / M_PI * atan( 0.5 * ( exp( n ) - exp( -n ) ) );
		return gps;
	}

	bool operator==( const ProjectedCoordinate& right ) const
	{
		return x == right.x && y == right.y;
	}

	bool operator!=( const ProjectedCoordinate& right ) const
	{
		return !( *this == right );
	}

	bool operator<( const ProjectedCoordinate& right ) const
	{
		if ( x != right.x )
			return x < right.x;
		return y < right.y;
	}

	/**
		checks whether this coordinate is in the invalid state
		does not check for invalid coordinates, e.g. x in [0,1]
	 */
	bool IsValid() const
	{
		return x != std::numeric_limits< double >::max() && y != std::numeric_limits< double >::max();
	}

	double x, y;
};

/**
	This class represents a quantized coordinate in mercator projection

	it uses 31 bits to represent the range of x and y respectively
	this means we have a worst-case resolution of ~2cm

	try not to convert coordinates to much as each time we might loose about 2cm precision
 */
class UnsignedCoordinate {
public:
	
	/** constructs a coordinate in the invalid state */
	UnsignedCoordinate()
	{
		x = y = std::numeric_limits< unsigned >::max();
	}

	UnsignedCoordinate( unsigned xVal, unsigned yVal )
	{
		x = xVal;
		y = yVal;
	}

	/** converts from floating point coordinate */
	explicit UnsignedCoordinate( ProjectedCoordinate tile )
	{
		if ( !tile.IsValid() ) {
			x = y = std::numeric_limits< unsigned >::max();
			return;
		}
		x = ( unsigned ) floor( tile.x * ( 1u << 30 ) );
		y = ( unsigned ) floor( tile.y * ( 1u << 30 ) );
	}

	/** converts from GPS coordinate */
	explicit UnsignedCoordinate( GPSCoordinate gps )
	{
		*this = UnsignedCoordinate( ProjectedCoordinate( gps ) );
	}

	/** converts to GPS coordinate */
	GPSCoordinate ToGPSCoordinate() const
	{
		return ToProjectedCoordinate().ToGPSCoordinate();
	}

	/** converts to floating point coordinate */
	ProjectedCoordinate ToProjectedCoordinate() const
	{
		ProjectedCoordinate tile;
		if ( !IsValid() )
			return tile;
		tile.x = x;
		tile.y = y;
		tile.x /= ( 1u << 30 );
		tile.y /= ( 1u << 30 );
		return tile;
	}

	/** get tile index at specific zoom level */
	unsigned GetTileX( int zoom ) const {
		if ( zoom == 0 )
			return 0;
		return x >> ( 30 - zoom );
	}
	unsigned GetTileY( int zoom ) const {
		if ( zoom == 0 )
			return 0;
		return y >> ( 30 - zoom );
	}

	/**
		get coordinate within the tile at a specific zoom level
		\param precision specifies the amount of precision bits needed
	 */
	unsigned GetTileSubX( int zoom, int precision ) const {
		assert( zoom + precision < 31 );
		assert( zoom + precision > 0 );
		const unsigned subX = ( x << zoom ) >> zoom;
		return subX >> ( 30 - precision - zoom );
	}
	unsigned GetTileSubY( int zoom, int precision ) const {
		assert( zoom + precision < 31 );
		assert( zoom + precision > 0 );
		const unsigned subY = ( y << zoom ) >> zoom;
		return subY >> ( 30 - precision - zoom );
	}

	bool operator==( const UnsignedCoordinate& right ) const
	{
		return x == right.x && y == right.y;
	}

	bool operator!=( const UnsignedCoordinate& right ) const
	{
		return !( *this == right );
	}

	bool operator<( const UnsignedCoordinate& right ) const
	{
		if ( x != right.x )
			return x < right.x;
		return y < right.y;
	}

	/**
		checks whether this coordinate is in the invalid state
		does not check for invalid coordinates, e.g. x in [0,2^31]
	 */
	bool IsValid() const
	{
		return x != std::numeric_limits< unsigned >::max() && y != std::numeric_limits< unsigned >::max();
	}

	unsigned x, y;
};

#endif // COORDINATES_H_INCLUDED
