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

#ifndef IENTITYREADER_H
#define IENTITYREADER_H

#include "utils/coordinates.h"
#include <QStringList>
#include <vector>

class IEntityReader {

public:

	struct Tag {
		unsigned key;
		QString value;
	};

	struct Node {
		unsigned id;
		GPSCoordinate coordinate;
		std::vector< Tag > tags;
	};

	struct Way {
		unsigned id;
		std::vector< unsigned > nodes;
		std::vector< Tag > tags;
	};

	struct RelationMember {
		unsigned ref;
		enum Type {
			Way, Node, Relation
		} type;
		QString role;
	};

	struct Relation {
		unsigned id;
		std::vector< RelationMember > members;
		std::vector< Tag > tags;
	};

	enum EntityType {
		EntityNone, EntityNode, EntityWay, EntityRelation
	};

	virtual bool open( QString filename ) = 0;
	virtual void setNodeTags( QStringList tags ) = 0; // sets the set of tags to extract
	virtual void setWayTags( QStringList tags ) = 0; // sets the set of tags to extract
	virtual void setRelationTags( QStringList tags ) = 0; // sets the set of tags to extract
	virtual EntityType getEntitiy( Node* node, Way* way, Relation* relation ) = 0; // get the next entity. EntityNone signifies the end of the data stream
	virtual ~IEntityReader(){}
};

#endif // IENTITYREADER_H
