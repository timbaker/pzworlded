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

#include "osmimporter.h"
#ifndef NOGUI
#include "oisettingsdialog.h"
#endif
#include "xmlreader.h"
#ifndef NOPBF
#include "pbfreader.h"
#endif
#include "utils/qthelpers.h"
#include "utils/formattedoutput.h"
#include <algorithm>
#include <QtDebug>
#include <QSettings>
#include <limits>

#define qls(x) QLatin1String(x)


OSMImporter::OSMImporter()
{
    Q_INIT_RESOURCE(speedProfiles);

    m_kmhStrings.push_back( qls("kmh") );
	m_kmhStrings.push_back( qls(" kmh") );
	m_kmhStrings.push_back( qls("km/h") );
	m_kmhStrings.push_back( qls(" km/h") );
	m_kmhStrings.push_back( qls("kph") );
	m_kmhStrings.push_back( qls(" kph") );

	m_mphStrings.push_back( qls("mph") );
	m_mphStrings.push_back( qls(" mph") );

	m_settings.speedProfile = qls(":/speed profiles/motorcar.spp");
	m_settings.languageSettings << qls("name");
}

void OSMImporter::setRequiredTags( IEntityReader *reader )
{
	QStringList list;
	// Place = 0, Population = 1, Highway = 2
	list.push_back( qls("place") );
	list.push_back( qls("population") );
	list.push_back( qls("barrier") );
	for ( int i = 0; i < m_settings.languageSettings.size(); i++ )
		list.push_back( m_settings.languageSettings[i] );
	for ( int i = 0; i < m_profile.accessList.size(); i++ )
		list.push_back( m_profile.accessList[i] );
	for ( int i = 0; i < m_profile.nodeModificators.size(); i++ ) {
		int index = list.indexOf( m_profile.nodeModificators[i].key );
		if ( index == -1 ) {
			index = list.size();
			list.push_back( m_profile.nodeModificators[i].key );
		}
		m_nodeModificatorIDs.push_back( index );
	}
	reader->setNodeTags( list );

	list.clear();
	//Oneway = 0, Junction = 1, Highway = 2, Ref = 3, PlaceName = 4, Place = 5, MaxSpeed = 6,
	list.push_back( qls("oneway") );
	list.push_back( qls("junction") );
	list.push_back( qls("highway") );
	list.push_back( qls("ref") );
	list.push_back( qls("place_name") );
	list.push_back( qls("place") );
	list.push_back( qls("maxspeed") );
	for ( int i = 0; i < m_settings.languageSettings.size(); i++ )
		list.push_back( m_settings.languageSettings[i] );
	for ( int i = 0; i < m_profile.accessList.size(); i++ )
		list.push_back( m_profile.accessList[i] );
	for ( int i = 0; i < m_profile.wayModificators.size(); i++ ) {
		int index = list.indexOf( m_profile.wayModificators[i].key );
		if ( index == -1 ) {
			index = list.size();
			list.push_back( m_profile.wayModificators[i].key );
		}
		m_wayModificatorIDs.push_back( index );
	}
	reader->setWayTags( list );

	list.clear();
	list.push_back( qls("type") );
	list.push_back( qls("restriction") );
	list.push_back( qls("except") );
	reader->setRelationTags( list );
}

OSMImporter::~OSMImporter()
{
    Q_CLEANUP_RESOURCE(speedProfiles);
}

QString OSMImporter::GetName()
{
	return qls("OpenStreetMap Importer");
}

void OSMImporter::SetOutputDirectory( const QString& dir )
{
	m_outputDirectory = dir;
}

bool OSMImporter::LoadSettings( QSettings* settings )
{
	if ( settings == NULL )
		return false;
	settings->beginGroup( qls("OSM Importer") );
	m_settings.languageSettings = settings->value( qls("languages"), QStringList( qls("name") ) ).toStringList();
	m_settings.speedProfile = settings->value( qls("speedProfile"), qls(":/speed profiles/motorcar.spp") ).toString();
	settings->endGroup();
	return true;
}

bool OSMImporter::SaveSettings( QSettings* settings )
{
	if ( settings == NULL )
		return false;
	settings->beginGroup( qls("OSM Importer") );
	settings->setValue( qls("languages"), m_settings.languageSettings );
	settings->setValue( qls("speedProfile"), m_settings.speedProfile );
	settings->endGroup();
	return true;
}

void OSMImporter::clear()
{
	std::vector< unsigned >().swap( m_usedNodes );
	std::vector< unsigned >().swap( m_outlineNodes );
	std::vector< unsigned >().swap( m_routingNodes );
	std::vector< NodePenalty >().swap( m_penaltyNodes );
	std::vector< unsigned >().swap( m_noAccessNodes );
	m_wayNames.clear();
	m_wayRefs.clear();
	std::vector< int >().swap( m_nodeModificatorIDs );
	std::vector< int >().swap( m_wayModificatorIDs );
	std::vector< char >().swap( m_inDegree );
	std::vector< char >().swap( m_outDegree );
	std::vector< EdgeInfo >().swap( m_edgeInfo );
}

bool OSMImporter::Preprocess( QString inputFilename )
{
	if ( !m_profile.load( m_settings.speedProfile ) ) {
		qCritical() << qls("Failed to load speed profile:") << m_settings.speedProfile;
		return false;
	}

	if ( m_profile.highways.size() == 0 ) {
        qCritical( "no highway types specified" );
		return false;
	}

	clear();

	QString filename = fileInDirectory( m_outputDirectory, qls("OSM Importer") );
	FileStream typeData( filename + qls("_way_types") );
	if ( !typeData.open( QIODevice::WriteOnly ) )
		return false;

	for ( int type = 0; type < m_profile.highways.size(); type++ )
		typeData << m_profile.highways[type].value;
	typeData << QString( qls("roundabout") );

	m_statistics = Statistics();

	Timer time;

	if ( !read( inputFilename, filename ) )
		return false;
	qDebug() << qls("OSM Importer: finished import pass 1:") << time.restart() << qls("ms");

	if ( m_usedNodes.size() == 0 ) {
        qCritical( "OSM Importer: no routing nodes found in the data set" );
		return false;
	}

	std::sort( m_usedNodes.begin(), m_usedNodes.end() );
	for ( unsigned i = 0; i < m_usedNodes.size(); ) {
		unsigned currentNode = m_usedNodes[i];
		int count = 1;
		for ( i++; i < m_usedNodes.size() && currentNode == m_usedNodes[i]; i++ )
			count++;

		if ( count > 1 )
			m_routingNodes.push_back( currentNode );
	}
	m_usedNodes.resize( std::unique( m_usedNodes.begin(), m_usedNodes.end() ) - m_usedNodes.begin() );
	std::sort( m_outlineNodes.begin(), m_outlineNodes.end() );
	m_outlineNodes.resize( std::unique( m_outlineNodes.begin(), m_outlineNodes.end() ) - m_outlineNodes.begin() );
	std::sort( m_penaltyNodes.begin(), m_penaltyNodes.end() );
	std::sort( m_noAccessNodes.begin(), m_noAccessNodes.end() );
	std::sort( m_routingNodes.begin(), m_routingNodes.end() );
	m_routingNodes.resize( std::unique( m_routingNodes.begin(), m_routingNodes.end() ) - m_routingNodes.begin() );

	if ( !preprocessData( filename ) )
		return false;
	qDebug() << qls("OSM Importer: finished import pass 2:") << time.restart() << qls("ms");
	printStats();
	clear();
	return true;
}

void OSMImporter::printStats()
{
	qDebug() << qls("OSM Importer: === Statistics ===");
	qDebug() << qls("OSM Importer: nodes:") << m_statistics.numberOfNodes;
	qDebug() << qls("OSM Importer: ways:") << m_statistics.numberOfWays;
	qDebug() << qls("OSM Importer: relations") << m_statistics.numberOfRelations;
	qDebug() << qls("OSM Importer: used nodes:") << m_statistics.numberOfUsedNodes;
	qDebug() << qls("OSM Importer: segments") << m_statistics.numberOfSegments;
	qDebug() << qls("OSM Importer: routing edges:") << m_statistics.numberOfEdges;
	qDebug() << qls("OSM Importer: routing nodes:") << m_routingNodes.size();
	qDebug() << qls("OSM Importer: nodes with penalty:") << m_penaltyNodes.size();
	qDebug() << qls("OSM Importer: nodes with no access:") << m_noAccessNodes.size();
	qDebug() << qls("OSM Importer: maxspeed:") << m_statistics.numberOfMaxspeed;
	qDebug() << qls("OSM Importer: zero speed ways:") << m_statistics.numberOfZeroSpeed;
	qDebug() << qls("OSM Importer: default city speed:") << m_statistics.numberOfDefaultCitySpeed;
	qDebug() << qls("OSM Importer: distinct way names:") << m_wayNames.size();
	qDebug() << qls("OSM Importer: distinct way refs:") << m_wayRefs.size();
	qDebug() << qls("OSM Importer: restrictions:") << m_statistics.numberOfRestrictions;
	qDebug() << qls("OSM Importer: restrictions applied:") << m_statistics.numberOfRestrictionsApplied;
	qDebug() << qls("OSM Importer: turning penalties:") << m_statistics.numberOfTurningPenalties;
	qDebug() << qls("OSM Importer: max turning penalty:") << m_statistics.maxTurningPenalty;
	qDebug() << qls("OSM Importer: average turning left penalty:") << m_statistics.averageLeftPenalty;
	qDebug() << qls("OSM Importer: average turning right penalty:") << m_statistics.averageRightPenalty;
	qDebug() << qls("OSM Importer: average turning straight penalty:") << m_statistics.averageStraightPenalty;
	qDebug() << qls("OSM Importer: places:") << m_statistics.numberOfPlaces;
	qDebug() << qls("OSM Importer: places outlines:") << m_statistics.numberOfOutlines;
	qDebug() << qls("OSM Importer: places outline nodes:") << m_outlineNodes.size();
}

bool OSMImporter::read( const QString& inputFilename, const QString& filename ) {
	FileStream edgeData( filename + qls("_edges") );
	FileStream onewayEdgeData( filename + qls("_oneway_edges") );
	FileStream placeData( filename + qls("_places") );
	FileStream boundingBoxData( filename + qls("_bounding_box") );
	FileStream nodeData( filename + qls("_all_nodes") );
	FileStream cityOutlineData( filename + qls("_city_outlines") );
	FileStream wayNameData( filename + qls("_way_names") );
	FileStream wayRefData( filename + qls("_way_refs") );
	FileStream restrictionData( filename + qls("_restrictions") );

	if ( !edgeData.open( QIODevice::WriteOnly ) )
		return false;
	if ( !onewayEdgeData.open( QIODevice::WriteOnly ) )
		return false;
	if ( !placeData.open( QIODevice::WriteOnly ) )
		return false;
	if ( !boundingBoxData.open( QIODevice::WriteOnly ) )
		return false;
	if ( !nodeData.open( QIODevice::WriteOnly ) )
		return false;
	if ( !cityOutlineData.open( QIODevice::WriteOnly ) )
		return false;
	if ( !wayNameData.open( QIODevice::WriteOnly ) )
		return false;
	if ( !wayRefData.open( QIODevice::WriteOnly ) )
		return false;
	if ( !restrictionData.open( QIODevice::WriteOnly ) )
		return false;

	m_wayNames[QString()] = 0;
	wayNameData << QString();
	m_wayRefs[QString()] = 0;
	wayRefData << QString();

	IEntityReader* reader = NULL;
	if ( inputFilename.endsWith( qls("osm.bz2") ) || inputFilename.endsWith( qls(".osm") ) )
		reader = new XMLReader();
#ifndef NOPBF
	else if ( inputFilename.endsWith( qls(".pbf") ) )
		reader = new PBFReader();
#endif

	if ( reader == NULL ) {
		qCritical() << qls("file format not supporter");
		return false;
	}

	if ( !reader->open( inputFilename ) ) {
		qCritical() << qls("error opening file");
		return false;
	}

	try {
		GPSCoordinate min( std::numeric_limits< double >::max(), std::numeric_limits< double >::max() );
		GPSCoordinate max( -std::numeric_limits< double >::max(), -std::numeric_limits< double >::max() );

		setRequiredTags( reader );

		IEntityReader::Way inputWay;
		IEntityReader::Node inputNode;
		IEntityReader::Relation inputRelation;
		Node node;
		Way way;
		Relation relation;
		while ( true ) {
			IEntityReader::EntityType type = reader->getEntitiy( &inputNode, &inputWay, &inputRelation );

			if ( type == IEntityReader::EntityNone )
				break;

			if ( type == IEntityReader::EntityNode ) {
				m_statistics.numberOfNodes++;
				readNode( &node, inputNode );

				min.latitude = std::min( min.latitude, inputNode.coordinate.latitude );
				min.longitude = std::min( min.longitude, inputNode.coordinate.longitude );
				max.latitude = std::max( max.latitude, inputNode.coordinate.latitude );
				max.longitude = std::max( max.longitude, inputNode.coordinate.longitude );

				if ( node.penalty != 0 )
					m_penaltyNodes.push_back( NodePenalty( inputNode.id, node.penalty ) );

				if ( !node.access )
					m_noAccessNodes.push_back( inputNode.id );

				UnsignedCoordinate coordinate( inputNode.coordinate );
				nodeData << unsigned( inputNode.id ) << coordinate.x << coordinate.y;

				if ( node.type != Place::None && !node.name.isEmpty() ) {
					placeData << inputNode.coordinate.latitude << inputNode.coordinate.longitude << unsigned( node.type ) << node.population << node.name;
					m_statistics.numberOfPlaces++;
				}

				continue;
			}

			if ( type == IEntityReader::EntityWay ) {
				m_statistics.numberOfWays++;
				readWay( &way, inputWay );

				if ( way.usefull && way.access && inputWay.nodes.size() > 1 ) {
					for ( unsigned node = 0; node < inputWay.nodes.size(); ++node )
						m_usedNodes.push_back( inputWay.nodes[node] );

					m_routingNodes.push_back( inputWay.nodes.front() ); // first and last node are always considered routing nodes
					m_routingNodes.push_back( inputWay.nodes.back() );  // as ways are never merged

					QString name = way.name.simplified();

					if ( !m_wayNames.contains( name ) ) {
						wayNameData << name;
						int id = m_wayNames.size();
						m_wayNames[name] = id;
					}

					QString ref = name; // fallback to name
					if ( !way.ref.isEmpty() )
						ref = way.ref.simplified();

					if ( !m_wayRefs.contains( ref ) ) {
						wayRefData << ref;
						int id = m_wayRefs.size();
						m_wayRefs[ref] = id;
					}

					if ( m_profile.ignoreOneway )
						way.direction = Way::Bidirectional;
					if ( m_profile.ignoreMaxspeed )
						way.maximumSpeed = -1;

					if ( way.direction == Way::Opposite )
						std::reverse( inputWay.nodes.begin(), inputWay.nodes.end() );

					// seperate oneway edges from bidirectional ones. neccessary to determine consistent edgeIDAtSource / edgeIDAtTarget
					if ( ( way.direction == Way::Oneway || way.direction == Way::Opposite ) ) {
						onewayEdgeData << inputWay.id;
						onewayEdgeData << m_wayNames[name];
						onewayEdgeData << m_wayRefs[ref];
						onewayEdgeData << way.type;
						onewayEdgeData << way.roundabout;
						onewayEdgeData << way.maximumSpeed;
						onewayEdgeData << way.addFixed << way.addPercentage;
						onewayEdgeData << bool( false ); // bidirectional?
						onewayEdgeData << unsigned( inputWay.nodes.size() );
						for ( unsigned node = 0; node < inputWay.nodes.size(); ++node )
							onewayEdgeData << inputWay.nodes[node];
					} else {
						edgeData << inputWay.id;
						edgeData << m_wayNames[name];
						edgeData << m_wayRefs[ref];
						edgeData << way.type;
						edgeData << way.roundabout;
						edgeData << way.maximumSpeed;
						edgeData << way.addFixed << way.addPercentage;
						edgeData << bool( true ); // bidirectional?
						edgeData << unsigned( inputWay.nodes.size() );
						for ( unsigned node = 0; node < inputWay.nodes.size(); ++node )
							edgeData << inputWay.nodes[node];
					}

				}

				bool closedWay = inputWay.nodes.size() != 0 && inputWay.nodes.front() == inputWay.nodes.back();

				if ( closedWay && way.placeType != Place::None && !way.placeName.isEmpty() ) {
					QString name = way.placeName.simplified();

					cityOutlineData << unsigned( way.placeType ) << unsigned( inputWay.nodes.size() - 1 );
					cityOutlineData << name;
					for ( unsigned i = 1; i < inputWay.nodes.size(); ++i ) {
						m_outlineNodes.push_back( inputWay.nodes[i] );
						cityOutlineData << inputWay.nodes[i];
					}
					m_statistics.numberOfOutlines++;
				}

				continue;
			}

			if ( type == IEntityReader::EntityRelation ) {
				m_statistics.numberOfRelations++;
				readRelation( &relation, inputRelation );

				if ( relation.type != Relation::TypeRestriction )
					continue;

				if ( !relation.restriction.access )
					continue;
				if ( relation.restriction.type == Restriction::None )
					continue;
				if ( relation.restriction.from == std::numeric_limits< unsigned >::max() )
					continue;
				if ( relation.restriction.to == std::numeric_limits< unsigned >::max() )
					continue;
				if ( relation.restriction.via == std::numeric_limits< unsigned >::max() )
					continue;

				m_statistics.numberOfRestrictions++;

				restrictionData << ( relation.restriction.type == Restriction::No );
				restrictionData << relation.restriction.from;
				restrictionData << relation.restriction.to;
				restrictionData << relation.restriction.via;

				continue;
			}
		}

		boundingBoxData << min.latitude << min.longitude << max.latitude << max.longitude;

	} catch ( const std::exception& e ) {
        qCritical( "OSM Importer: caught execption: %s", e.what() );
		return false;
	}

	delete reader;
	return true;
}

bool OSMImporter::preprocessData( const QString& filename ) {
	std::vector< UnsignedCoordinate > nodeCoordinates( m_usedNodes.size() );
	std::vector< UnsignedCoordinate > outlineCoordinates( m_outlineNodes.size() );

	FileStream allNodesData( filename + qls("_all_nodes") );

	if ( !allNodesData.open( QIODevice::ReadOnly ) )
		return false;

	FileStream routingCoordinatesData( filename + qls("_routing_coordinates") );

	if ( !routingCoordinatesData.open( QIODevice::WriteOnly ) )
		return false;

	Timer time;

	while ( true ) {
		unsigned node;
		UnsignedCoordinate coordinate;
		allNodesData >> node >> coordinate.x >> coordinate.y;
		if ( allNodesData.status() == QDataStream::ReadPastEnd )
			break;
		std::vector< NodeID >::const_iterator element = std::lower_bound( m_usedNodes.begin(), m_usedNodes.end(), node );
		if ( element != m_usedNodes.end() && *element == node )
			nodeCoordinates[element - m_usedNodes.begin()] = coordinate;
		element = std::lower_bound( m_outlineNodes.begin(), m_outlineNodes.end(), node );
		if ( element != m_outlineNodes.end() && *element == node )
			outlineCoordinates[element - m_outlineNodes.begin()] = coordinate;
	}

	qDebug() << qls("OSM Importer: filtered node coordinates:") << time.restart() << qls("ms");

	m_statistics.numberOfUsedNodes = m_usedNodes.size();
	std::vector< NodeLocation > nodeLocation( m_usedNodes.size() );

	if ( !computeInCityFlags( filename, &nodeLocation, nodeCoordinates, outlineCoordinates ) )
		return false;
	std::vector< UnsignedCoordinate >().swap( outlineCoordinates );

	m_inDegree.resize( m_routingNodes.size(), 0 );
	m_outDegree.resize( m_routingNodes.size(), 0 );
	if ( !remapEdges( filename, nodeCoordinates, nodeLocation ) )
		return false;

	for ( unsigned i = 0; i < m_routingNodes.size(); i++ ) {
		unsigned mapped = std::lower_bound( m_usedNodes.begin(), m_usedNodes.end(), m_routingNodes[i] ) - m_usedNodes.begin();
		routingCoordinatesData << nodeCoordinates[mapped].x << nodeCoordinates[mapped].y;
	}

	qDebug() << qls("OSM Importer: wrote routing node coordinates:") << time.restart() << qls("ms");

	std::vector< unsigned >().swap( m_usedNodes );
	std::vector< UnsignedCoordinate >().swap( nodeCoordinates );

	if ( !computeTurningPenalties( filename ) )
		return false;

	return true;
}

bool OSMImporter::computeInCityFlags( QString filename, std::vector< NodeLocation >* nodeLocation, const std::vector< UnsignedCoordinate >& nodeCoordinates, const std::vector< UnsignedCoordinate >& outlineCoordinates )
{
	FileStream cityOutlinesData( filename + qls("_city_outlines") );
	FileStream placesData( filename + qls("_places") );

	if ( !cityOutlinesData.open( QIODevice::ReadOnly ) )
		return false;
	if ( !placesData.open( QIODevice::ReadOnly ) )
		return false;

	Timer time;

	std::vector< Outline > cityOutlines;
	while ( true ) {
		Outline outline;
		unsigned type, numberOfPathNodes;
		cityOutlinesData >> type >> numberOfPathNodes >> outline.name;
		if ( cityOutlinesData.status() == QDataStream::ReadPastEnd )
			break;

		bool valid = true;
		for ( int i = 0; i < ( int ) numberOfPathNodes; ++i ) {
			unsigned node;
			cityOutlinesData >> node;
			NodeID mappedNode = std::lower_bound( m_outlineNodes.begin(), m_outlineNodes.end(), node ) - m_outlineNodes.begin();
			if ( !outlineCoordinates[mappedNode].IsValid() ) {
                qDebug( "OSM Importer: inconsistent OSM data: missing outline node coordinate %d", ( int ) mappedNode );
				valid = false;
			}
			DoublePoint point( outlineCoordinates[mappedNode].x, outlineCoordinates[mappedNode].y );
			outline.way.push_back( point );
		}
		if ( valid )
			cityOutlines.push_back( outline );
	}
	std::sort( cityOutlines.begin(), cityOutlines.end() );

	qDebug() << qls("OSM Importer: read city outlines:") << time.restart() << qls("ms");

	std::vector< Location > places;
	while ( true ) {
		Location place;
		unsigned type;
		int population;
		placesData >> place.coordinate.latitude >> place.coordinate.longitude >> type >> population >> place.name;

		if ( placesData.status() == QDataStream::ReadPastEnd )
			break;

		place.type = ( Place::Type ) type;
		places.push_back( place );
	}

	qDebug() << qls("OSM Importer: read places:") << time.restart() << qls("ms");

	typedef GPSTree::InputPoint InputPoint;
	std::vector< InputPoint > kdPoints;
	kdPoints.reserve( m_usedNodes.size() );
	for ( std::vector< UnsignedCoordinate >::const_iterator node = nodeCoordinates.begin(), endNode = nodeCoordinates.end(); node != endNode; ++node ) {
		InputPoint point;
		point.data = node - nodeCoordinates.begin();
		point.coordinates[0] = node->x;
		point.coordinates[1] = node->y;
		kdPoints.push_back( point );
		nodeLocation->at( point.data ).isInPlace = false;
		nodeLocation->at( point.data ).distance = std::numeric_limits< double >::max();
	}
	GPSTree kdTree( kdPoints );

	qDebug() << qls("OSM Importer: build kd-tree:") << time.restart() << qls("ms");

	for ( std::vector< Location >::const_iterator place = places.begin(), endPlace = places.end(); place != endPlace; ++place ) {
		InputPoint point;
		UnsignedCoordinate placeCoordinate( place->coordinate );
		point.coordinates[0] = placeCoordinate.x;
		point.coordinates[1] = placeCoordinate.y;
		std::vector< InputPoint > result;

		const Outline* placeOutline = NULL;
		double radius = 0;
		Outline searchOutline;
		searchOutline.name = place->name;
		for ( std::vector< Outline >::const_iterator outline = std::lower_bound( cityOutlines.begin(), cityOutlines.end(), searchOutline ), outlineEnd = std::upper_bound( cityOutlines.begin(), cityOutlines.end(), searchOutline ); outline != outlineEnd; ++outline ) {
			UnsignedCoordinate cityCoordinate = UnsignedCoordinate( place->coordinate );
			DoublePoint cityPoint( cityCoordinate.x, cityCoordinate.y );
			if ( pointInPolygon( outline->way.size(), &outline->way[0], cityPoint ) ) {
				placeOutline = &( *outline );
				for ( std::vector< DoublePoint >::const_iterator way = outline->way.begin(), wayEnd = outline->way.end(); way != wayEnd; ++way ) {
					UnsignedCoordinate coordinate( way->x, way->y );
					double distance = coordinate.ToGPSCoordinate().ApproximateDistance( place->coordinate );
					radius = std::max( radius, distance );
				}
				break;
			}
		}

		if ( placeOutline != NULL ) {
			kdTree.NearNeighbors( &result, point, radius );
			for ( std::vector< InputPoint >::const_iterator i = result.begin(), e = result.end(); i != e; ++i ) {
				UnsignedCoordinate coordinate( i->coordinates[0], i->coordinates[1] );
				DoublePoint nodePoint;
				nodePoint.x = coordinate.x;
				nodePoint.y = coordinate.y;
				if ( !pointInPolygon( placeOutline->way.size(), &placeOutline->way[0], nodePoint ) )
					continue;
				nodeLocation->at( i->data ).isInPlace = true;
				nodeLocation->at( i->data ).place = place - places.begin();
				nodeLocation->at( i->data ).distance = 0;
			}
		} else {
			switch ( place->type ) {
			case Place::None:
				continue;
			case Place::Suburb:
				continue;
			case Place::Hamlet:
				kdTree.NearNeighbors( &result, point, 300 );
				break;
			case Place::Village:
				kdTree.NearNeighbors( &result, point, 1000 );
				break;
			case Place::Town:
				kdTree.NearNeighbors( &result, point, 5000 );
				break;
			case Place::City:
				kdTree.NearNeighbors( &result, point, 10000 );
				break;
			}

			for ( std::vector< InputPoint >::const_iterator i = result.begin(), e = result.end(); i != e; ++i ) {
				UnsignedCoordinate coordinate( i->coordinates[0], i->coordinates[1] );
				double distance =  coordinate.ToGPSCoordinate().ApproximateDistance( place->coordinate );
				if ( distance >= nodeLocation->at( i->data ).distance )
					continue;
				nodeLocation->at( i->data ).isInPlace = true;
				nodeLocation->at( i->data ).place = place - places.begin();
				nodeLocation->at( i->data ).distance = distance;
			}
		}
	}

	qDebug() << qls("OSM Importer: assigned 'in-city' flags:") << time.restart() << qls("ms");

	return true;
}

bool OSMImporter::remapEdges( QString filename, const std::vector< UnsignedCoordinate >& nodeCoordinates, const std::vector< NodeLocation >& nodeLocation )
{
	FileStream edgeData( filename + qls("_edges") );
	FileStream onewayEdgeData( filename + qls("_oneway_edges") );

	if ( !edgeData.open( QIODevice::ReadOnly ) )
		return false;
	if ( !onewayEdgeData.open( QIODevice::ReadOnly ) )
			return false;

	FileStream mappedEdgesData( filename + qls("_mapped_edges") );
	FileStream edgeAddressData( filename + qls("_address") );
	FileStream edgePathsData( filename + qls("_paths") );

	if ( !mappedEdgesData.open( QIODevice::WriteOnly ) )
		return false;
	if ( !edgeAddressData.open( QIODevice::WriteOnly ) )
		return false;
	if ( !edgePathsData.open( QIODevice::WriteOnly ) )
		return false;

	unsigned oldRoutingNodes = m_routingNodes.size();

	Timer time;

	unsigned pathID = 0;
	unsigned addressID = 0;

	// bidirectional && oneway
	for ( int onewayType = 0; onewayType < 2; onewayType++ ) {
		while ( true ) {
			double speed;
			unsigned id, numberOfPathNodes, type, nameID, refID;
			int addFixed, addPercentage;
			bool bidirectional, roundabout;
			std::vector< unsigned > way;

			if ( onewayType == 0 ) {
				edgeData >> id >> nameID >> refID >> type >> roundabout >> speed >> addFixed >> addPercentage >> bidirectional >> numberOfPathNodes;
				if ( edgeData.status() == QDataStream::ReadPastEnd )
					break;
				assert( bidirectional );
			} else {
				onewayEdgeData >> id >> nameID >> refID >> type >> roundabout >> speed >> addFixed >> addPercentage >> bidirectional >> numberOfPathNodes;
				if ( onewayEdgeData.status() == QDataStream::ReadPastEnd )
					break;
				assert( !bidirectional );
			}

			assert( ( int ) type < m_profile.highways.size() );
			if ( speed <= 0 )
				speed = std::numeric_limits< double >::max();

			bool valid = true;

			for ( int i = 0; i < ( int ) numberOfPathNodes; ++i ) {
				unsigned node;
				if ( onewayType == 0 )
					edgeData >> node;
				else
					onewayEdgeData >> node;
				if ( !valid )
					continue;

				NodeID mappedNode = std::lower_bound( m_usedNodes.begin(), m_usedNodes.end(), node ) - m_usedNodes.begin();
				if ( !nodeCoordinates[mappedNode].IsValid() ) {
					qDebug() << qls("OSM Importer: inconsistent OSM data: skipping way with missing node coordinate");
					valid = false;
				}
				way.push_back( mappedNode );
			}
			if ( !valid )
				continue;

			for ( unsigned pathNode = 0; pathNode + 1 < way.size(); ) {
				unsigned source = std::lower_bound( m_routingNodes.begin(), m_routingNodes.begin() + oldRoutingNodes, m_usedNodes[way[pathNode]] ) - m_routingNodes.begin();
				if ( std::binary_search( m_noAccessNodes.begin(), m_noAccessNodes.end(), m_usedNodes[way[pathNode]] ) ) {
					source = m_routingNodes.size();
					m_routingNodes.push_back( m_usedNodes[way[pathNode]] );
					m_inDegree.push_back( 0 );
					m_outDegree.push_back( 0 );
				}
				assert( source < m_routingNodes.size() );
				NodeID target = 0;
				double seconds = 0;

				EdgeInfo sourceInfo;
				EdgeInfo targetInfo;
				targetInfo.backward = sourceInfo.forward = true;
				targetInfo.forward = sourceInfo.backward = bidirectional;
				targetInfo.crossing = sourceInfo.crossing = false;
				targetInfo.oldID = sourceInfo.oldID = id;
				targetInfo.type = sourceInfo.type = type;

				unsigned nextRoutingNode = pathNode + 1;
				double lastAngle = 0;
				while ( true ) {
					m_statistics.numberOfSegments++;

					NodeID from = way[nextRoutingNode - 1];
					NodeID to = way[nextRoutingNode];
					GPSCoordinate fromCoordinate = nodeCoordinates[from].ToGPSCoordinate();
					GPSCoordinate toCoordinate = nodeCoordinates[to].ToGPSCoordinate();

					double distance = fromCoordinate.Distance( toCoordinate );

					double segmentSpeed = speed;
					if ( m_profile.defaultCitySpeed && ( nodeLocation[from].isInPlace || nodeLocation[to].isInPlace ) ) {
						if ( segmentSpeed == std::numeric_limits< double >::max() ) {
							segmentSpeed = m_profile.highways[type].defaultCitySpeed;
							m_statistics.numberOfDefaultCitySpeed++;
						}
					}

					segmentSpeed = std::min( ( double ) m_profile.highways[type].maxSpeed, segmentSpeed );

					segmentSpeed *= m_profile.highways[type].averageSpeed / 100.0;
					segmentSpeed /= 1.0 + addPercentage / 100.0;

					double toAngle;
					if ( nextRoutingNode == pathNode + 1 ) {
						sourceInfo.angle = atan2( ( double ) nodeCoordinates[to].y - nodeCoordinates[from].y, ( double ) nodeCoordinates[to].x - nodeCoordinates[from].x );;
						sourceInfo.speed = segmentSpeed;
						sourceInfo.length = distance;
						lastAngle = sourceInfo.angle;
						toAngle = sourceInfo.angle + M_PI;
					} else {
						toAngle = atan2( ( double ) nodeCoordinates[from].y - nodeCoordinates[to].y, ( double ) nodeCoordinates[from].x - nodeCoordinates[to].x );
						double halfAngle = ( lastAngle - toAngle ) / 2.0;
						double radius = sin( fabs( halfAngle ) ) / cos( fabs( halfAngle ) ) * distance / 2.0;
						double maxSpeed = sqrt( m_profile.tangentialAcceleration * radius ) * 3.6;
						if ( radius < 1000 && radius > 2.5 && maxSpeed < segmentSpeed ) // NAN and inf not possible
							segmentSpeed = maxSpeed; // turn radius and maximum tangential acceleration limit turning speed
						lastAngle = toAngle + M_PI;
					}

					seconds += distance * 3.6 / segmentSpeed;
					seconds += addFixed;

					unsigned nodePenalty = std::lower_bound( m_penaltyNodes.begin(), m_penaltyNodes.end(), m_usedNodes[from] ) - m_penaltyNodes.begin();
					if ( nodePenalty < m_penaltyNodes.size() && m_penaltyNodes[nodePenalty].id == m_usedNodes[from] )
						seconds += m_penaltyNodes[nodePenalty].seconds;
					nodePenalty = std::lower_bound( m_penaltyNodes.begin(), m_penaltyNodes.end(), m_usedNodes[to] ) - m_penaltyNodes.begin();
					if ( nodePenalty < m_penaltyNodes.size() && m_penaltyNodes[nodePenalty].id == m_usedNodes[to] )
						seconds += m_penaltyNodes[nodePenalty].seconds;

					bool splitPath = false;

					if ( std::binary_search( m_noAccessNodes.begin(), m_noAccessNodes.end(), m_usedNodes[to] ) ) {
						target = m_routingNodes.size();
						m_routingNodes.push_back( m_usedNodes[to] );
						m_inDegree.push_back( 0 );
						m_outDegree.push_back( 0 );
						splitPath = true;
					} else {
						target = std::lower_bound( m_routingNodes.begin(), m_routingNodes.begin() + oldRoutingNodes, m_usedNodes[to] ) - m_routingNodes.begin();
						if ( target < m_routingNodes.size() && m_routingNodes[target] == m_usedNodes[to] )
							splitPath = true;
					}

					if ( splitPath ) {
						targetInfo.angle = toAngle;
						if ( targetInfo.angle > M_PI )
							targetInfo.angle -= 2 * M_PI;
						targetInfo.speed = segmentSpeed;
						targetInfo.length = distance;
						break;
					}

					edgePathsData << nodeCoordinates[to].x << nodeCoordinates[to].y;

					nextRoutingNode++;
				}

				char edgeIDAtSource = m_outDegree[source]; // == inDegree[source] for bidirectional edges
				char edgeIDAtTarget = m_inDegree[target]; // == outDegree[target] for bidirectional edges

				m_outDegree[source]++;
				m_inDegree[target]++;
				if ( m_outDegree[source] == std::numeric_limits< char >::max() ) {
					qCritical() << qls("OSM Importer: node degree too large, node:") << m_usedNodes[source];
					return false;
				}
				if ( m_inDegree[target] == std::numeric_limits< char >::max() ) {
					qCritical() << qls("OSM Importer: node degree too large, node:") << m_usedNodes[target];
					return false;
				}
				if ( onewayType == 0 ) {
					m_outDegree[target]++;
					m_inDegree[source]++;
					if ( m_inDegree[source] == std::numeric_limits< char >::max() ) {
						qCritical() << qls("OSM Importer: node degree too large, node:") << m_usedNodes[source];
						return false;
					}
					if ( m_outDegree[target] == std::numeric_limits< char >::max() ) {
						qCritical() << qls("OSM Importer: node degree too large, node:") << m_usedNodes[target];
						return false;
					}
				}

				sourceInfo.id = edgeIDAtSource;
				targetInfo.id = edgeIDAtTarget;
				sourceInfo.node = source;
				targetInfo.node = target;
				m_edgeInfo.push_back( sourceInfo );
				m_edgeInfo.push_back( targetInfo );

				std::vector< unsigned > wayPlaces;
				for ( unsigned i = pathNode; i < nextRoutingNode; i++ ) {
					if ( nodeLocation[way[i]].isInPlace )
						wayPlaces.push_back( nodeLocation[way[i]].place );
				}
				std::sort( wayPlaces.begin(), wayPlaces.end() );
				wayPlaces.resize( std::unique( wayPlaces.begin(), wayPlaces.end() ) - wayPlaces.begin() );
				for ( unsigned i = 0; i < wayPlaces.size(); i++ )
					edgeAddressData << wayPlaces[i];

				mappedEdgesData << source << target << bidirectional << seconds;
				mappedEdgesData << nameID << refID;
				if ( roundabout )
					mappedEdgesData << unsigned( m_profile.highways.size() );
				else
					mappedEdgesData << type;
				mappedEdgesData << pathID << nextRoutingNode - pathNode - 1;
				mappedEdgesData << addressID << unsigned( wayPlaces.size() );
				mappedEdgesData << qint8( edgeIDAtSource ) << qint8( edgeIDAtTarget );

				pathID += nextRoutingNode - pathNode - 1;
				addressID += wayPlaces.size();
				pathNode = nextRoutingNode;

				m_statistics.numberOfEdges++;
			}
		}

		if ( onewayType == 0 )
			qDebug() << qls("OSM Importer: remapped edges") << time.restart() << qls("ms");
		else
			qDebug() << qls("OSM Importer: remapped oneway edges") << time.restart() << qls("ms");
	}

	return true;
}

bool OSMImporter::computeTurningPenalties( QString filename )
{
	FileStream restrictionData( filename + qls("_restrictions") );

	if ( !restrictionData.open( QIODevice::ReadOnly ) )
		return false;

	FileStream penaltyData( filename + qls("_penalties") );

	if ( !penaltyData.open( QIODevice::WriteOnly ) )
		return false;

	Timer time;

	std::vector< RestrictionInfo > restrictions;
	while ( true ) {
		RestrictionInfo restriction;
		restrictionData >> restriction.exclude;
		restrictionData >> restriction.from;
		restrictionData >> restriction.to;
		restrictionData >> restriction.via;

		if ( restrictionData.status() == QDataStream::ReadPastEnd )
			break;

		restrictions.push_back( restriction );
	}

	qDebug() << qls("OSM Importer: read restrictions:") << time.restart() << qls("ms");

	std::sort( restrictions.begin(), restrictions.end() );
	std::sort( m_edgeInfo.begin(), m_edgeInfo.end() );
	double leftSum = 0;
	double rightSum = 0;
	double straightSum = 0;
	unsigned left = 0;
	unsigned right = 0;
	unsigned straight = 0;
	unsigned edge = 0;
	unsigned restriction = 0;
	std::vector< double > table;
	std::vector< int > histogram( m_profile.highways.size(), 0 );
	for ( unsigned node = 0; node < m_routingNodes.size(); node++ ) {
		penaltyData << ( int ) m_inDegree[node] << ( int ) m_outDegree[node];

		while ( edge < m_edgeInfo.size() && m_edgeInfo[edge].node < node )
			edge++;
		while ( restriction < restrictions.size() && restrictions[restriction].via < m_routingNodes[node] )
			restriction++;

		table.clear();
		table.resize( ( int ) m_inDegree[node] * m_outDegree[node], 0 );

		for ( unsigned i = restriction; i < restrictions.size() && restrictions[i].via == m_routingNodes[node]; i++ ) {
			unsigned from = std::numeric_limits< unsigned >::max();
			unsigned to = std::numeric_limits< unsigned >::max();
			//qDebug() << restrictions[i].from << restrictions[i].to;
			for ( unsigned j = edge; j < m_edgeInfo.size() && m_edgeInfo[j].node == node; j++ ) {
				//qDebug() << m_edgeInfo[j].oldID;
										  if ( m_edgeInfo[j].oldID == restrictions[i].from && m_edgeInfo[j].backward )
					from = m_edgeInfo[j].id;
										  if ( m_edgeInfo[j].oldID == restrictions[i].to && m_edgeInfo[j].forward )
					to = m_edgeInfo[j].id;
				if ( from != std::numeric_limits< unsigned >::max() && to != std::numeric_limits< unsigned >::max() ) {
					table[from * m_outDegree[node] + to] = -1; // infinity == not allowed
					m_statistics.numberOfRestrictionsApplied++;
					break;
				}
			}
		}

		histogram.assign( histogram.size(), 0 );
		for ( unsigned i = edge; i < m_edgeInfo.size() && m_edgeInfo[i].node == node; i++ ) {
			if ( m_edgeInfo[i].backward )
				histogram[m_edgeInfo[i].type]++;
		}

		//penalties
		for ( unsigned i = edge; i < m_edgeInfo.size() && m_edgeInfo[i].node == node; i++ ) {
			const EdgeInfo& from = m_edgeInfo[i];
			if ( !from.backward )
				continue;
			for ( unsigned j = edge; j < m_edgeInfo.size() && m_edgeInfo[j].node == node; j++ ) {
				const EdgeInfo& to = m_edgeInfo[j];
				if ( !to.forward )
					continue;

				unsigned tablePosition = ( int ) from.id * m_outDegree[node] + to.id;

				if ( table[tablePosition] < 0 )
					continue;

				if ( from.speed == 0 || to.speed == 0 )
					continue;
				if ( m_profile.decceleration == 0 || m_profile.acceleration == 0 )
					continue;

				double angle = fmod( ( from.angle - to.angle ) / M_PI * 180.0 + 360.0, 360.0 ) - 180.0;
				double radius = 1000;
				if ( fabs( angle ) < 189 ) {
					 // turn radius witht he assumption that the resolution is at least 5m
					radius = sin( fabs( angle / 180.0 * M_PI ) / 2 ) / cos( fabs( angle / 180.0 * M_PI ) / 2 ) * std::min( 5.0, std::min( from.length, to.length ) ) / 2.0;
				}
				double maxVelocity = std::min( from.speed, to.speed );
				if ( radius < 1000 ) // NAN and inf not possible
					maxVelocity = std::min( maxVelocity, sqrt( m_profile.tangentialAcceleration * radius ) * 3.6 ); // turn radius and maximum tangential acceleration limit turning speed

				if ( m_profile.highways[to.type].pedestrian && fabs( angle ) < 180 - 45 )
					maxVelocity = std::min( maxVelocity, ( double ) m_profile.pedestrian );

				{
					int otherDirections = 0;
					int startType = from.type;
					bool equal = false;
					bool skip = true;

					if ( angle < 0 && angle > -180 + 45 ) {
						if ( m_profile.highways[from.type].otherLeftPenalty )
							skip = false;
						else if ( m_profile.highways[from.type].otherLeftEqual )
							equal = true;
					} else if ( angle > 0 && angle < 180 - 45 ) {
						if ( m_profile.highways[from.type].otherRightPenalty )
							skip = false;
						else if ( m_profile.highways[from.type].otherRightEqual )
							equal = true;
					} else {
						if ( m_profile.highways[from.type].otherStraightPenalty )
							skip = false;
						else if ( m_profile.highways[from.type].otherStraightEqual )
							equal = true;
					}

					if ( !skip ) {
						if ( !equal )
							startType++;
						else
							otherDirections--; // exclude your own origin

						if ( to.type >= startType && to.backward )
							otherDirections--; // exclude your target

						for ( unsigned type = 0; type < histogram.size(); type++ ) {
							if ( m_profile.highways[type].priority > m_profile.highways[from.type].priority )
								otherDirections += histogram[type];
						}

						if ( otherDirections >= 1 )
							maxVelocity = std::min( maxVelocity, ( double ) m_profile.otherCars );
					}
				}

				// the time it takes to deccelerate vs the travel time assumed on the edge
				double decceleratingPenalty = ( from.speed - maxVelocity ) * ( from.speed - maxVelocity ) / ( 2 * from.speed * m_profile.decceleration * 3.6 );
				// the time it takes to accelerate vs the travel time assumed on the edge
				double acceleratingPenalty = ( to.speed - maxVelocity ) * ( to.speed - maxVelocity ) / ( 2 * to.speed * m_profile.acceleration * 3.6 );

				table[tablePosition] = decceleratingPenalty + acceleratingPenalty;
				if ( angle < 0 && angle > -180 + 45 )
					table[tablePosition] += m_profile.highways[to.type].leftPenalty;
				if ( angle > 0 && angle < 180 - 45 )
					table[tablePosition] += m_profile.highways[to.type].rightPenalty;
				//if ( tables[position + from.id + m_inDegree[node] * to.id] > m_statistics.maxTurningPenalty ) {
				//	qDebug() << angle << radius << from.speed << to.speed << maxVelocity;
				//	qDebug() << from.length << to.length;
				//}
				m_statistics.maxTurningPenalty = std::max( m_statistics.maxTurningPenalty, table[tablePosition] );
				if ( angle < 0 && angle > -180 + 45 ) {
					leftSum += table[tablePosition];
					left++;
				} else if ( angle > 0 && angle < 180 - 45 ) {
					rightSum += table[tablePosition];
					right++;
				} else {
					straightSum += table[tablePosition];
					straight++;
				}
			}
		}

		for ( unsigned i = 0; i < table.size(); i++ )
			penaltyData << table[i];
	}


	m_statistics.numberOfTurningPenalties = table.size();
	m_statistics.averageLeftPenalty = leftSum / left;
	m_statistics.averageRightPenalty = rightSum / right;
	m_statistics.averageStraightPenalty = straightSum / straight;
	qDebug() << qls("OSM Importer: computed turning penalties:") << time.restart() << qls("ms");

	return true;
}

void OSMImporter::readWay( OSMImporter::Way* way, const IEntityReader::Way& inputWay ) {
	way->direction = Way::NotSure;
	way->maximumSpeed = -1;
	way->type = -1;
	way->roundabout = false;
	way->name.clear();
	way->namePriority = m_settings.languageSettings.size();
	way->ref.clear();
	way->placeType = Place::None;
	way->placeName.clear();
	way->usefull = false;
	way->access = true;
	way->accessPriority = m_profile.accessList.size();
	way->addFixed = 0;
	way->addPercentage = 0;

	for ( unsigned tag = 0; tag < inputWay.tags.size(); tag++ ) {
		int key = inputWay.tags[tag].key;
		QString value = inputWay.tags[tag].value;

		if ( key < WayTags::MaxTag ) {
			switch ( WayTags::Key( key ) ) {
			case WayTags::Oneway:
				{
					if ( value== qls("no") || value == qls("false") || value == qls("0") )
						way->direction = Way::Bidirectional;
					else if ( value == qls("yes") || value == qls("true") || value == qls("1") )
						way->direction = Way::Oneway;
					else if ( value == qls("-1") )
						way->direction = Way::Opposite;
					break;
				}
			case WayTags::Junction:
				{
					if ( value == qls("roundabout") ) {
						if ( way->direction == Way::NotSure ) {
							way->direction = Way::Oneway;
							way->roundabout = true;
						}
					}
					break;
				}
			case WayTags::Highway:
				{
					if ( value == qls("motorway") ) {
						if ( way->direction == Way::NotSure )
							way->direction = Way::Oneway;
					} else if ( value ==qls("motorway_link") ) {
						if ( way->direction == Way::NotSure )
							way->direction = Way::Oneway;
					}

					for ( int type = 0; type < m_profile.highways.size(); type++ ) {
						if ( value == m_profile.highways[type].value ) {
							way->type = type;
							way->usefull = true;
						}
					}
					break;
				}
			case WayTags::Ref:
				{
					way->ref = value;
					break;
				}
			case WayTags::PlaceName:
				{
					way->placeName = value;
					break;
				}
			case WayTags::Place:
				{
					way->placeType = parsePlaceType( value );
					break;
				}
			case WayTags::MaxSpeed:
				{
					int index = -1;
					double factor = 1.609344;
					for ( unsigned i = 0; index == -1 && i < m_mphStrings.size(); i++ )
						index = value.lastIndexOf( m_mphStrings[i] );

					if ( index == -1 )
						factor = 1;

					for ( unsigned i = 0; index == -1 && i < m_kmhStrings.size(); i++ )
						index = value.lastIndexOf( m_kmhStrings[i] );

					if( index == -1 )
						index = value.size();
					bool ok = true;
					double maxspeed = value.left( index ).toDouble( &ok );
					if ( ok ) {
						way->maximumSpeed = maxspeed * factor;
						//qDebug() << value << index << value.left( index ) << way->maximumSpeed;
						m_statistics.numberOfMaxspeed++;
					}
					break;
				}
			case WayTags::MaxTag:
				assert( false );
			}

			continue;
		}

		key -= WayTags::MaxTag;
		if ( key < m_settings.languageSettings.size() ) {
			if ( key < way->namePriority ) {
				way->namePriority = key;
				way->name = value;
			}

			continue;
		}

		key -= m_settings.languageSettings.size();
		if ( key < m_profile.accessList.size() ) {
				if ( key < way->accessPriority ) {
					if ( value == qls("private") || value == qls("no") || value == qls("agricultural") || value == qls("forestry") || value == qls("delivery") ) {
						way->access = false;
						way->accessPriority = key;
					} else if ( value == qls("yes") || value == qls("designated") || value == qls("official") || value == qls("permissive") ) {
						way->access = true;
						way->accessPriority = key;
					}
				}

			continue;
		}
	}

	// rescan tags to apply modificators
	for ( unsigned tag = 0; tag < inputWay.tags.size(); tag++ ) {
		int key = inputWay.tags[tag].key;
		QString value = inputWay.tags[tag].value;

		for ( unsigned modificator = 0; modificator < m_wayModificatorIDs.size(); modificator++ ) {
			if ( m_wayModificatorIDs[modificator] != key )
				continue;

			const MoNav::WayModificator& mod = m_profile.wayModificators[modificator];
			if ( mod.checkValue && mod.value != value )
				continue;

			switch ( mod.type ) {
			case MoNav::WayModifyFixed:
				way->addFixed += mod.modificatorValue.toInt();
				break;
			case MoNav::WayModifyPercentage:
				way->addPercentage = std::min( way->addPercentage, mod.modificatorValue.toInt() );
				break;
			case MoNav::WayAccess:
				way->access = mod.modificatorValue.toBool();
				break;
			case MoNav::WayOneway:
				if ( mod.modificatorValue.toBool() )
					way->direction = Way::Oneway;
				else
					way->direction = Way::Bidirectional;
				break;
			}
		}
	}
}

void OSMImporter::readNode( OSMImporter::Node* node, const IEntityReader::Node& inputNode ) {
	node->name.clear();
	node->namePriority = m_settings.languageSettings.size();
	node->type = Place::None;
	node->population = -1;
	node->penalty = 0;
	node->access = true;
	node->accessPriority = m_profile.accessList.size();

	for ( unsigned tag = 0; tag < inputNode.tags.size(); tag++ ) {
		int key = inputNode.tags[tag].key;
		QString value = inputNode.tags[tag].value;

		if ( key < NodeTags::MaxTag ) {
			switch ( NodeTags::Key( key ) ) {
			case NodeTags::Place:
				{
					node->type = parsePlaceType( value );
					break;
				}
			case NodeTags::Population:
				{
					bool ok;
					int population = value.toInt( &ok );
					if ( ok )
						node->population = population;
					break;
				}
			case NodeTags::Barrier:
				{
					if ( node->accessPriority == m_profile.accessList.size() )
						node->access = false;
					break;
				}
			case NodeTags::MaxTag:
				assert( false );
			}

			continue;
		}

		key -= NodeTags::MaxTag;
		if ( key < m_settings.languageSettings.size() ) {
			if ( key < node->namePriority ) {
				node->namePriority = key;
				node->name = value;
			}

			continue;
		}

		key -= m_settings.languageSettings.size();
		if ( key < m_profile.accessList.size() ) {
				if ( key < node->accessPriority ) {
					if ( value == qls("private") || value == qls("no") || value == qls("agricultural") || value == qls("forestry") || value == qls("delivery") ) {
						node->access = false;
						node->accessPriority = key;
					} else if ( value == qls("yes") || value == qls("designated") || value == qls("official") || value == qls("permissive") ) {
						node->access = true;
						node->accessPriority = key;
					}
				}

			continue;
		}
	}

	// rescan tags to apply modificators
	for ( unsigned tag = 0; tag < inputNode.tags.size(); tag++ ) {
		int key = inputNode.tags[tag].key;
		QString value = inputNode.tags[tag].value;

		for ( unsigned modificator = 0; modificator < m_nodeModificatorIDs.size(); modificator++ ) {
			if ( m_nodeModificatorIDs[modificator] != key )
				continue;

			const MoNav::NodeModificator& mod = m_profile.nodeModificators[modificator];
			if ( mod.checkValue && mod.value != value )
				continue;

			switch ( mod.type ) {
			case MoNav::NodeModifyFixed:
				node->penalty += mod.modificatorValue.toInt();
				break;
			case MoNav::NodeAccess:
				node->access = mod.modificatorValue.toBool();
				break;
			}
		}
	}
}

void OSMImporter::readRelation( Relation* relation, const IEntityReader::Relation& inputRelation )
{
	relation->type = Relation::TypeNone;
	relation->restriction.access = true;
	relation->restriction.from = std::numeric_limits< unsigned >::max();
	relation->restriction.to = std::numeric_limits< unsigned >::max();;
	relation->restriction.via = std::numeric_limits< unsigned >::max();
	relation->restriction.type = Restriction::None;

	for ( unsigned tag = 0; tag < inputRelation.tags.size(); tag++ ) {
		int key = inputRelation.tags[tag].key;
		QString value = inputRelation.tags[tag].value;

		if ( key < RelationTags::MaxTag ) {
			switch ( RelationTags::Key( key ) ) {
			case RelationTags::Type:
				{
					if ( value == qls("restriction") )
						relation->type = Relation::TypeRestriction;
					break;
				}
			case RelationTags::Except:
				{
                    QStringList accessTypes = value.split( QLatin1Char(';') );
					foreach( QString access, m_profile.accessList ) {
						if ( accessTypes.contains( access ) )
							relation->restriction.access = false;
					}
					break;
				}
			case RelationTags::Restriction:
				{
					if ( value.startsWith( qls("no") ) )
						relation->restriction.type = Restriction::No;
					else if ( value.startsWith( qls("only") ) )
						relation->restriction.type = Restriction::Only;
					break;
				}
			case RelationTags::MaxTag:
				assert( false );
			}

			continue;
		}

	}

	for ( unsigned i = 0; i < inputRelation.members.size(); i++ ) {
		const IEntityReader::RelationMember& member = inputRelation.members[i];
		if ( member.type == IEntityReader::RelationMember::Way && member.role == qls("from") )
			relation->restriction.from = member.ref;
		else if ( member.type == IEntityReader::RelationMember::Way && member.role == qls("to") )
			relation->restriction.to = member.ref;
		else if ( member.type == IEntityReader::RelationMember::Node && member.role == qls("via") )
			relation->restriction.via = member.ref;
	}
}

OSMImporter::Place::Type OSMImporter::parsePlaceType( const QString& type )
{
	if ( type == qls("city") )
		return Place::City;
	if ( type == qls("town") )
		return Place::Town;
	if ( type == qls("village") )
		return Place::Village;
	if ( type == qls("hamlet") )
		return Place::Hamlet;
	if ( type == qls("suburb") )
		return Place::Suburb;
	return Place::None;
}

bool OSMImporter::SetIDMap( const std::vector< NodeID >& idMap )
{
	FileStream idMapData( fileInDirectory( m_outputDirectory, qls("OSM Importer") ) + qls("_id_map") );

	if ( !idMapData.open( QIODevice::WriteOnly ) )
		return false;

	idMapData << unsigned( idMap.size() );
	for ( NodeID i = 0; i < ( NodeID ) idMap.size(); i++ )
		idMapData << idMap[i];

	return true;
}

bool OSMImporter::GetIDMap( std::vector< NodeID >* idMap )
{
	FileStream idMapData( fileInDirectory( m_outputDirectory, qls("OSM Importer") ) + qls("_id_map") );

	if ( !idMapData.open( QIODevice::ReadOnly ) )
		return false;

	unsigned numNodes;

	idMapData >> numNodes;
	idMap->resize( numNodes );

	for ( NodeID i = 0; i < ( NodeID ) numNodes; i++ ) {
		unsigned temp;
		idMapData >> temp;
		( *idMap )[i] = temp;
	}

	if ( idMapData.status() == QDataStream::ReadPastEnd )
		return false;

	return true;
}

bool OSMImporter::SetEdgeIDMap( const std::vector< NodeID >& idMap )
{
	FileStream idMapData( fileInDirectory( m_outputDirectory, qls("OSM Importer") ) + qls("_edge_id_map") );

	if ( !idMapData.open( QIODevice::WriteOnly ) )
		return false;

	idMapData << unsigned( idMap.size() );
	for ( NodeID i = 0; i < ( NodeID ) idMap.size(); i++ )
		idMapData << idMap[i];

	return true;
}

bool OSMImporter::GetEdgeIDMap( std::vector< NodeID >* idMap )
{
	FileStream idMapData( fileInDirectory( m_outputDirectory, qls("OSM Importer") ) + qls("_edge_id_map") );

	if ( !idMapData.open( QIODevice::ReadOnly ) )
		return false;

	unsigned numEdges;

	idMapData >> numEdges;
	idMap->resize( numEdges );

	for ( NodeID i = 0; i < ( NodeID ) numEdges; i++ ) {
		unsigned temp;
		idMapData >> temp;
		( *idMap )[i] = temp;
	}

	if ( idMapData.status() == QDataStream::ReadPastEnd )
		return false;

	return true;
}

bool OSMImporter::GetRoutingEdges( std::vector< RoutingEdge >* data )
{
	FileStream mappedEdgesData( fileInDirectory( m_outputDirectory, qls("OSM Importer") ) + qls("_mapped_edges") );

	if ( !mappedEdgesData.open( QIODevice::ReadOnly ) )
		return false;

	std::vector< int > nodeOutDegree;
	while ( true ) {
		unsigned source, target, nameID, refID, type;
		unsigned pathID, pathLength;
		unsigned addressID, addressLength;
		bool bidirectional;
		double seconds;
		qint8 edgeIDAtSource, edgeIDAtTarget;

		mappedEdgesData >> source >> target >> bidirectional >> seconds;
		mappedEdgesData >> nameID >> refID >> type;
		mappedEdgesData >> pathID >> pathLength;
		mappedEdgesData >> addressID >> addressLength;
		mappedEdgesData >> edgeIDAtSource >> edgeIDAtTarget;

		if ( mappedEdgesData.status() == QDataStream::ReadPastEnd )
			break;

		RoutingEdge edge;
		edge.source = source;
		edge.target = target;
		edge.bidirectional = bidirectional;
		edge.distance = seconds;
		edge.nameID = refID;
		edge.type = type;
		edge.pathID = pathID;
		edge.pathLength = pathLength;
		edge.edgeIDAtSource = edgeIDAtSource;
		edge.edgeIDAtTarget = edgeIDAtTarget;

		data->push_back( edge );

		if ( source >= nodeOutDegree.size() )
			nodeOutDegree.resize( source + 1, 0 );
		if ( target >= nodeOutDegree.size() )
			nodeOutDegree.resize( target + 1, 0 );
		nodeOutDegree[source]++;
		if ( bidirectional )
			nodeOutDegree[target]++;
	}

	for ( unsigned edge = 0; edge < data->size(); edge++ ) {
		int branches = nodeOutDegree[data->at( edge ).target];
		branches -= data->at( edge ).bidirectional ? 1 : 0;
		( *data )[edge].branchingPossible = branches > 1;
	}

	return true;
}

bool OSMImporter::GetRoutingNodes( std::vector< RoutingNode >* data )
{
	FileStream routingCoordinatesData( fileInDirectory( m_outputDirectory, qls("OSM Importer") ) + qls("_routing_coordinates") );

	if ( !routingCoordinatesData.open( QIODevice::ReadOnly ) )
		return false;

	while ( true ) {
		UnsignedCoordinate coordinate;
		routingCoordinatesData >> coordinate.x >> coordinate.y;
		if ( routingCoordinatesData.status() == QDataStream::ReadPastEnd )
			break;
		RoutingNode node;
		node.coordinate = coordinate;
		data->push_back( node );
	}

	return true;
}

bool OSMImporter::GetRoutingEdgePaths( std::vector< RoutingNode >* data )
{
	FileStream edgePathsData( fileInDirectory( m_outputDirectory, qls("OSM Importer") ) + qls("_paths") );

	if ( !edgePathsData.open( QIODevice::ReadOnly ) )
		return false;

	while ( true ) {
		UnsignedCoordinate coordinate;
		edgePathsData >> coordinate.x >> coordinate.y;
		if ( edgePathsData.status() == QDataStream::ReadPastEnd )
			break;
		RoutingNode node;
		node.coordinate = coordinate;
		data->push_back( node );
	}
	return true;
}

bool OSMImporter::GetRoutingWayNames( std::vector< QString >* data )
{
	FileStream wayRefsData( fileInDirectory( m_outputDirectory, qls("OSM Importer") ) + qls("_way_refs") );

	if ( !wayRefsData.open( QIODevice::ReadOnly ) )
		return false;

	while ( true ) {
		QString ref;
		wayRefsData >> ref;
		if ( wayRefsData.status() == QDataStream::ReadPastEnd )
			break;
		data->push_back( ref );
	}
	return true;
}

bool OSMImporter::GetRoutingWayTypes( std::vector< QString >* data )
{
	FileStream wayTypesData( fileInDirectory( m_outputDirectory, qls("OSM Importer") ) + qls("_way_types") );

	if ( !wayTypesData.open( QIODevice::ReadOnly ) )
		return false;

	while ( true ) {
		QString type;
		wayTypesData >> type;
		if ( wayTypesData.status() == QDataStream::ReadPastEnd )
			break;
		data->push_back( type );
	}
	return true;
}

bool OSMImporter::GetRoutingPenalties( std::vector< char >* inDegree, std::vector< char >* outDegree, std::vector< double >* penalties )
{
	QString filename = fileInDirectory( m_outputDirectory, qls("OSM Importer") );

	FileStream penaltyData( filename + qls("_penalties") );

	if ( !penaltyData.open( QIODevice::ReadOnly ) )
		return false;

	while ( true ) {
		int in, out;
		penaltyData >> in >> out;
		if ( penaltyData.status() == QDataStream::ReadPastEnd )
			break;
		unsigned oldPosition = penalties->size();
		for ( int i = 0; i < in; i++ ) {
			for ( int j = 0; j < out; j++ ) {
				double penalty;
				penaltyData >> penalty;
				penalties->push_back( penalty );
			}
		}
		if ( penaltyData.status() == QDataStream::ReadPastEnd ) {
			penalties->resize( oldPosition );
			qCritical() << qls("OSM Importer: Corrupt Penalty Data");
			return false;
		}
		inDegree->push_back( in );
		outDegree->push_back( out );
	}

	return true;
}

bool OSMImporter::GetAddressData( std::vector< Place >* dataPlaces, std::vector< Address >* dataAddresses, std::vector< UnsignedCoordinate >* dataWayBuffer, std::vector< QString >* addressNames )
{
	QString filename = fileInDirectory( m_outputDirectory, qls("OSM Importer") );

	FileStream mappedEdgesData( filename + qls("_mapped_edges") );
	FileStream routingCoordinatesData( filename + qls("_routing_coordinates") );
	FileStream placesData( filename + qls("_places") );
	FileStream edgeAddressData( filename + qls("_address") );
	FileStream edgePathsData( filename + qls("_paths") );

	if ( !mappedEdgesData.open( QIODevice::ReadOnly ) )
		return false;
	if ( !routingCoordinatesData.open( QIODevice::ReadOnly ) )
		return false;
	if ( !placesData.open( QIODevice::ReadOnly ) )
		return false;
	if ( !edgeAddressData.open( QIODevice::ReadOnly ) )
		return false;
	if ( !edgePathsData.open( QIODevice::ReadOnly ) )
		return false;

	std::vector< UnsignedCoordinate > coordinates;
	while ( true ) {
		UnsignedCoordinate node;
		routingCoordinatesData >> node.x >> node.y;
		if ( routingCoordinatesData.status() == QDataStream::ReadPastEnd )
			break;
		coordinates.push_back( node );
	}

	while ( true ) {
		GPSCoordinate gps;
		unsigned type;
		QString name;
		unsigned population;
		placesData >> gps.latitude >> gps.longitude >> type >> population >> name;

		if ( placesData.status() == QDataStream::ReadPastEnd )
			break;

		Place place;
		place.name = name;
		place.population = population;
		place.coordinate = UnsignedCoordinate( gps );
		place.type = ( Place::Type ) type;
		dataPlaces->push_back( place );
	}

	std::vector< unsigned > edgeAddress;
	while ( true ) {
		unsigned place;
		edgeAddressData >> place;
		if ( edgeAddressData.status() == QDataStream::ReadPastEnd )
			break;
		edgeAddress.push_back( place );
	}

	std::vector< UnsignedCoordinate > edgePaths;
	while ( true ) {
		UnsignedCoordinate coordinate;
		edgePathsData >> coordinate.x >> coordinate.y;
		if ( edgePathsData.status() == QDataStream::ReadPastEnd )
			break;
		edgePaths.push_back( coordinate );
	}

	long long numberOfEdges = 0;
	long long numberOfAddressPlaces = 0;

	while ( true ) {
		unsigned source, target, nameID, refID, type;
		unsigned pathID, pathLength;
		unsigned addressID, addressLength;
		bool bidirectional;
		double seconds;
		qint8 edgeIDAtSource, edgeIDAtTarget;

		mappedEdgesData >> source >> target >> bidirectional >> seconds;
		mappedEdgesData >> nameID >> refID >> type;
		mappedEdgesData >> pathID >> pathLength;
		mappedEdgesData >> addressID >> addressLength;
		mappedEdgesData >> edgeIDAtSource >> edgeIDAtTarget;
		if ( mappedEdgesData.status() == QDataStream::ReadPastEnd )
			break;

		if ( nameID == 0 || addressLength == 0 )
			continue;

		Address newAddress;
		newAddress.name = nameID;
		newAddress.pathID = dataWayBuffer->size();

		dataWayBuffer->push_back( coordinates[source] );
		for ( unsigned i = 0; i < pathLength; i++ )
			dataWayBuffer->push_back( edgePaths[i + pathID] );
		dataWayBuffer->push_back( coordinates[target] );

		newAddress.pathLength = pathLength + 2;
		numberOfEdges++;

		for ( unsigned i = 0; i < addressLength; i++ ) {
			newAddress.nearPlace = edgeAddress[i + addressID];
			dataAddresses->push_back( newAddress );

			numberOfAddressPlaces++;
		}

	}

	FileStream wayNamesData( fileInDirectory( m_outputDirectory, qls("OSM Importer") ) + qls("_way_names") );

	if ( !wayNamesData.open( QIODevice::ReadOnly ) )
		return false;

	while ( true ) {
		QString name;
		wayNamesData >> name;
		if ( wayNamesData.status() == QDataStream::ReadPastEnd )
			break;
		addressNames->push_back( name );
	}

	qDebug() << qls("OSM Importer: edges:") << numberOfEdges;
	qDebug() << qls("OSM Importer: address entries:") << numberOfAddressPlaces;
	qDebug() << qls("OSM Importer: address entries per way:") << ( double ) numberOfAddressPlaces / numberOfEdges;
	qDebug() << qls("OSM Importer: coordinates:") << dataWayBuffer->size();
	return true;
}

bool OSMImporter::GetBoundingBox( BoundingBox* box )
{
	FileStream boundingBoxData( fileInDirectory( m_outputDirectory, qls("OSM Importer") ) + qls("_bounding_box") );

	if ( !boundingBoxData.open( QIODevice::ReadOnly ) )
		return false;

	GPSCoordinate minGPS, maxGPS;

	boundingBoxData >> minGPS.latitude >> minGPS.longitude >> maxGPS.latitude >> maxGPS.longitude;

	if ( boundingBoxData.status() == QDataStream::ReadPastEnd ) {
		qCritical() << qls("error reading bounding box");
		return false;
	}

	box->min = UnsignedCoordinate( minGPS );
	box->max = UnsignedCoordinate( maxGPS );
	if ( box->min.x > box->max.x )
		std::swap( box->min.x, box->max.x );
	if ( box->min.y > box->max.y )
		std::swap( box->min.y, box->max.y );

	return true;
}

void OSMImporter::DeleteTemporaryFiles()
{
	QString filename = fileInDirectory( m_outputDirectory, qls("OSM Importer") );
	QFile::remove( filename + qls("_address") );
	QFile::remove( filename + qls("_all_nodes") );
	QFile::remove( filename + qls("_bounding_box") );
	QFile::remove( filename + qls("_city_outlines") );
	QFile::remove( filename + qls("_edge_id_map") );
	QFile::remove( filename + qls("_edges") );
	QFile::remove( filename + qls("_id_map") );
	QFile::remove( filename + qls("_mapped_edges") );
	QFile::remove( filename + qls("_oneway_edges") );
	QFile::remove( filename + qls("_paths") );
	QFile::remove( filename + qls("_penalties") );
	QFile::remove( filename + qls("_places") );
	QFile::remove( filename + qls("_restrictions") );
	QFile::remove( filename + qls("_routing_coordinates") );
	QFile::remove( filename + qls("_way_names") );
	QFile::remove( filename + qls("_way_refs") );
	QFile::remove( filename + qls("_way_types") );
}

#ifndef NOGUI
	// IGUISettings
bool OSMImporter::GetSettingsWindow( QWidget** window )
{
	*window = new OISettingsDialog();
	return true;
}

bool OSMImporter::FillSettingsWindow( QWidget* window )
{
	OISettingsDialog* settings = qobject_cast< OISettingsDialog* >( window );
	if ( settings == NULL )
		return false;

	return settings->readSettings( m_settings );
}

bool OSMImporter::ReadSettingsWindow( QWidget* window )
{
	OISettingsDialog* settings = qobject_cast< OISettingsDialog* >( window );
	if ( settings == NULL )
		return false;

	return settings->fillSettings( &m_settings );
}
#endif

// IConsoleSettings
QString OSMImporter::GetModuleName()
{
	return GetName();
}

bool OSMImporter::GetSettingsList( QVector< Setting >* settings )
{
	settings->push_back( Setting( qls(""), qls("profile"), qls("build in speed profile"), qls("speed profile name") ) );
	settings->push_back( Setting( qls(""), qls("profile-file"), qls("read speed profile from file"), qls("speed profile filename") ) );
	settings->push_back( Setting( qls(""), qls("list-profiles"), qls("lists build in speed profiles"), qls("") ) );
	settings->push_back( Setting( qls(""), qls("add-language"), qls("adds a language to the language list"), qls("name[:XXX]") ) );

	return true;
}

bool OSMImporter::SetSetting( int id, QVariant data )
{
	switch( id ) {
	case 0:
		m_settings.speedProfile = qls(":/speed profiles/") + data.toString() + qls(".spp");
		break;
	case 1:
		m_settings.speedProfile = data.toString();
		break;
	case 2:
		{
			QDir dir( qls(":speed profiles/") );
			dir.setNameFilters( QStringList( qls("*.spp") ) );
			QStringList profiles = dir.entryList( QDir::Files, QDir::Name );
			profiles.replaceInStrings( qls(".spp"), qls("") );
            printf( "%s\n\n", printStringTable( profiles, 1, qls("Speed Profiles") ).toUtf8().constData() );
			break;
		}
	case 3:
		{
			QString language = data.toString();
			if ( !language.startsWith( qls("name") ) ) {
				qCritical() << qls("language entry has to start with \"name\"");
				return false;
			}
			m_settings.languageSettings.push_back( language );
		}
	default:
		return false;
	}

	return true;
}

#ifndef ZOMBOID
Q_EXPORT_PLUGIN2( osmimporter, OSMImporter )
#endif
