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

#ifndef IROUTER_H
#define IROUTER_H

#include "utils/config.h"
#include "utils/coordinates.h"
#include "interfaces/igpslookup.h"
#include <QtPlugin>
#include <QVector>

class IRouter
{
public:

	struct Node {
		Node(){}
		Node( UnsignedCoordinate coord )
		{
			coordinate = coord;
		}

		UnsignedCoordinate coordinate;
	};

	struct Edge {
		Edge(){}
		Edge( unsigned name_, bool branchingPossible_, unsigned char type_, unsigned short length_, unsigned seconds_ )
		{
			name = name_;
			branchingPossible = branchingPossible_;
			type = type_;
			length = length_;
			seconds = seconds_;
		}
		unsigned name : 30; // name ID of the edge
		bool branchingPossible : 1; // is there more than one subsequent edge to traverse ( turing around and traversing this edge in the opposite direction does not count )
		unsigned char type; // type ID of the edge
		unsigned short length; // the amount of path nodes - 1 == amount of edges
		unsigned seconds;
	};

	virtual ~IRouter() {}

	virtual QString GetName() = 0;
	virtual void SetInputDirectory( const QString& dir ) = 0;
	virtual void ShowSettings() = 0;
	virtual bool IsCompatible( int fileFormatVersion ) = 0;
	virtual bool LoadData() = 0;
	virtual bool UnloadData() = 0;
	// computes the route between source and target and returns the distance in second
	virtual bool GetRoute( double* distance, QVector< Node>* pathNodes, QVector< Edge >* pathEdges, const IGPSLookup::Result& source, const IGPSLookup::Result& target ) = 0;
	// translate a name ID into the corresponding string
	virtual bool GetName( QString* result, unsigned name ) = 0;
	// translate a list of name IDs into the corresponding strings
	virtual bool GetNames( QVector< QString >* result, QVector< unsigned > names ) = 0;
	// translate a type ID into the corresponding description
	virtual bool GetType( QString* result, unsigned type ) = 0;
	// translate a list of type IDs into the corresponding descriptions
	virtual bool GetTypes( QVector< QString >* result, QVector< unsigned > types ) = 0;
};

Q_DECLARE_INTERFACE( IRouter, "monav.IRouter/1.1" )

#endif // IROUTER_H
