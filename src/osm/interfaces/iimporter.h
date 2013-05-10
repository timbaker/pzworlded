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

#ifndef IIMPORTER_H
#define IIMPORTER_H

#include <vector>
#include <QString>
#include <QtPlugin>
#include "utils/config.h"
#include "utils/coordinates.h"

class QWidget;
class QSettings;

class IImporter
{
public:

	struct BoundingBox {
		UnsignedCoordinate min;
		UnsignedCoordinate max;
	};

	struct RoutingEdge {
		NodeID source;
		NodeID target;
		unsigned short edgeIDAtSource; // uniquely identifies this edge among all outgoing edges adjacent to the source, at most degree( source ) - 1
		unsigned short edgeIDAtTarget; // uniquely identifies this edge among all incoming edges adjacent to the target, at most degree( target ) - 1
		double distance; // travel time metric -> seconds
		bool bidirectional : 1; // can the edge be traversed in both directions? edgeIDAtSource / edgeIDAtTarget are also unique for incoming / outgoing edges in this case.
		bool branchingPossible : 1; // is there more than one subsequent edge to traverse ( turing around and traversing this edge in the opposite direction does not count )
		unsigned nameID; // id of the way's name
		unsigned pathID; // id of the way's path's description
		unsigned short type; // id of the way's type's name
		unsigned short pathLength; // length of the way's path's description
	};

	struct RoutingNode {
		UnsignedCoordinate coordinate;
	};

	struct Place {
		QString name;
		int population;
		UnsignedCoordinate coordinate;
		enum Type
		{
			City = 0, Town = 1, Hamlet = 2, Suburb = 3, Village = 4, None = 5
		} type;
		bool operator<( const Place& right ) const {
			if ( name != right.name )
				return name < right.name;
			if ( type != right.type )
				return type < right.type;
			return population < right.population;
		}
	};

	struct Address {
		unsigned name;
		unsigned pathID; // start of the address' description
		unsigned pathLength; // length of the address' description
		unsigned nearPlace; // id of the nearest place
		bool operator<( const Address& right ) const {
			if ( nearPlace != right.nearPlace )
				return nearPlace < right.nearPlace;
			return name < right.name;
		}
	};

	virtual QString GetName() = 0;
	virtual void SetOutputDirectory( const QString& dir ) = 0;
	virtual bool LoadSettings( QSettings* settings ) = 0;
	virtual bool SaveSettings( QSettings* settings ) = 0;
	virtual bool Preprocess( QString filename ) = 0;
	// IRouter is allowed to remap node ids and must set the resulting id map
	virtual bool SetIDMap( const std::vector< NodeID >& idMap ) = 0;
	// IGPSLookup has to use the router's id map
	virtual bool GetIDMap( std::vector< NodeID >* idMap ) = 0;
	// IRouter is allowed to remap edge ids and must set the resulting id map
	virtual bool SetEdgeIDMap( const std::vector< NodeID >& idMap ) = 0;
	// IGPSLookup has to use the router's edge id map
	virtual bool GetEdgeIDMap( std::vector< NodeID >* idMap ) = 0;
	// get all routing edges
	virtual bool GetRoutingEdges( std::vector< RoutingEdge >* data ) = 0;
	// get all routing edges' path descriptions
	virtual bool GetRoutingEdgePaths( std::vector< RoutingNode >* data ) = 0;
	// get all routing nodes
	virtual bool GetRoutingNodes( std::vector< RoutingNode >* data ) = 0;
	// get all way names
	virtual bool GetRoutingWayNames( std::vector< QString >* data ) = 0;
	// get all way names
	virtual bool GetRoutingWayTypes( std::vector< QString >* data ) = 0;
	// get all turning penalties. Each node has a table inDegree[node] * outDegree[node] of penalties.
	// the tables are serialized in the penalties vector
	// access a penalty for turning from edgeID "a" into edgeID "b" with penalties[a * outDegree[node] + b + beginTable]
	// a negativ penalty forbids a turn
	virtual bool GetRoutingPenalties( std::vector< char >* inDegree, std::vector< char >* outDegree, std::vector< double >* penalties ) = 0;
	// get address data
	virtual bool GetAddressData( std::vector< Place >* dataPlaces, std::vector< Address >* dataAddresses, std::vector< UnsignedCoordinate >* dataWayBuffer, std::vector< QString >* addressNames ) = 0;
	// get the bounding bos of all routing coordinates ( routing nodes + way descriptions )
	virtual bool GetBoundingBox( BoundingBox* box ) = 0;
	// delete preprocessed IImporter files
	virtual void DeleteTemporaryFiles() = 0;
	virtual ~IImporter() {}
};

Q_DECLARE_INTERFACE( IImporter, "monav.IImporter/1.0" )

#endif // IIMPORTER_H
