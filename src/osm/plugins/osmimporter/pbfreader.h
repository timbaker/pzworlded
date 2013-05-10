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

#ifndef PBFREADER_H
#define PBFREADER_H

#include "ientityreader.h"
#include "fileformat.pb.h"
#include "osmformat.pb.h"
#include "utils/qthelpers.h"
#include <QHash>
#include <QFile>
#include <string>
#include <zlib.h>
#include <bzlib.h>
#include "lzma/LzmaDec.h"

#define NANO ( 1000.0 * 1000.0 * 1000.0 )
#define MAX_BLOCK_HEADER_SIZE ( 64 * 1024 )
#define MAX_BLOB_SIZE ( 32 * 1024 * 1024 )

class PBFReader : public IEntityReader {

protected:

	enum Mode {
		ModeNode, ModeWay, ModeRelation, ModeDense
	};

public:

	PBFReader()
	{
		GOOGLE_PROTOBUF_VERIFY_VERSION;
	}

	virtual bool open( QString filename )
	{
		m_file.setFileName( filename );

		if ( !openQFile( &m_file, QIODevice::ReadOnly ) )
			return false;

		if ( !readBlockHeader() )
			return false;

		if ( m_blockHeader.type() != "OSMHeader" ) {
			qCritical() << "OSMHeader missing, found" << m_blockHeader.type().data() << "instead";
			return false;
		}

		if ( !readBlob() )
			return false;

		if ( !m_headerBlock.ParseFromArray( m_buffer.data(), m_buffer.size() ) ) {
			qCritical() << "failed to parse HeaderBlock";
			return false;
		}
		for ( int i = 0; i < m_headerBlock.required_features_size(); i++ ) {
			const std::string& feature = m_headerBlock.required_features( i );
			bool supported = false;
			if ( feature == "OsmSchema-V0.6" )
				supported = true;
			else if ( feature == "DenseNodes" )
				supported = true;

			if ( !supported ) {
				qCritical() << "required feature not supported:" << feature.data();
				return false;
			}
		}
		m_loadBlock = true;
		return true;
	}

	virtual void setNodeTags( QStringList tags )
	{
		for ( int i = 0; i < tags.size(); i++ )
			m_nodeTags.insert( tags[i], i );
	}

	virtual void setWayTags( QStringList tags )
	{
		for ( int i = 0; i < tags.size(); i++ )
			m_wayTags.insert( tags[i], i );
	}

	virtual void setRelationTags( QStringList tags )
	{
		for ( int i = 0; i < tags.size(); i++ )
			m_relationTags.insert( tags[i], i );
	}

	virtual EntityType getEntitiy( Node* node, Way* way, Relation* relation )
	{
		if ( m_loadBlock ) {
			if ( !readNextBlock() )
				return EntityNone;
			loadBlock();
			loadGroup();
		}

		switch ( m_mode ) {
		case ModeNode:
			parseNode( node );
			return EntityNode;
		case ModeWay:
			parseWay( way );
			return EntityWay;
		case ModeRelation:
			parseRelation( relation );
			return EntityRelation;
		case ModeDense:
			parseDense( node );
			return EntityNode;
		}

		return EntityNone;
	}

	virtual ~PBFReader()
	{
	}

protected:

	int convertNetworkByteOrder( char data[4] )
	{
		return ( ( ( unsigned ) data[0] ) << 24 ) | ( ( ( unsigned ) data[1] ) << 16 ) | ( ( ( unsigned ) data[2] ) << 8 ) | ( unsigned ) data[3];
	}

	void parseNode( IEntityReader::Node* node )
	{
		node->tags.clear();

		const PBF::Node& inputNode = m_primitiveBlock.primitivegroup( m_currentGroup ).nodes( m_currentEntity );
		node->id = inputNode.id();
		node->coordinate.latitude = ( ( double ) inputNode.lat() * m_primitiveBlock.granularity() + m_primitiveBlock.lat_offset() ) / NANO;
		node->coordinate.longitude = ( ( double ) inputNode.lon() * m_primitiveBlock.granularity() + m_primitiveBlock.lon_offset() ) / NANO;
		for ( int tag = 0; tag < inputNode.keys_size(); tag++ ) {
			int tagID = m_nodeTagIDs[inputNode.keys( tag )];
			if ( tagID == -1 )
				continue;
			Tag newTag;
			newTag.key = tagID;
			newTag.value = QString::fromUtf8( m_primitiveBlock.stringtable().s( inputNode.vals( tag ) ).data() );
			node->tags.push_back( newTag );
		}

		m_currentEntity++;
		if ( m_currentEntity >= m_primitiveBlock.primitivegroup( m_currentGroup ).nodes_size() ) {
			m_currentEntity = 0;
			m_currentGroup++;
			if ( m_currentGroup >= m_primitiveBlock.primitivegroup_size() )
				m_loadBlock = true;
			else
				loadGroup();
		}
	}

	void parseWay( IEntityReader::Way* way )
	{
		way->tags.clear();
		way->nodes.clear();

		const PBF::Way& inputWay = m_primitiveBlock.primitivegroup( m_currentGroup ).ways( m_currentEntity );
		way->id = inputWay.id();
		for ( int tag = 0; tag < inputWay.keys_size(); tag++ ) {
			int tagID = m_wayTagIDs[inputWay.keys( tag )];
			if ( tagID == -1 )
				continue;
			Tag newTag;
			newTag.key = tagID;
			newTag.value = QString::fromUtf8( m_primitiveBlock.stringtable().s( inputWay.vals( tag ) ).data() );
			way->tags.push_back( newTag );
		}

		long long lastRef = 0;
		for ( int i = 0; i < inputWay.refs_size(); i++ ) {
			lastRef += inputWay.refs( i );
			way->nodes.push_back( lastRef );
		}

		m_currentEntity++;
		if ( m_currentEntity >= m_primitiveBlock.primitivegroup( m_currentGroup ).ways_size() ) {
			m_currentEntity = 0;
			m_currentGroup++;
			if ( m_currentGroup >= m_primitiveBlock.primitivegroup_size() )
				m_loadBlock = true;
			else
				loadGroup();
		}
	}

	void parseRelation( IEntityReader::Relation* relation )
	{
		relation->tags.clear();
		relation->members.clear();

		const PBF::Relation& inputRelation = m_primitiveBlock.primitivegroup( m_currentGroup ).relations( m_currentEntity );
		relation->id = inputRelation.id();
		for ( int tag = 0; tag < inputRelation.keys_size(); tag++ ) {
			int tagID = m_relationTagIDs[inputRelation.keys( tag )];
			if ( tagID == -1 )
				continue;
			Tag newTag;
			newTag.key = tagID;
			newTag.value = QString::fromUtf8( m_primitiveBlock.stringtable().s( inputRelation.vals( tag ) ).data() );
			relation->tags.push_back( newTag );
		}

		long long lastRef = 0;
		for ( int i = 0; i < inputRelation.types_size(); i++ ) {
			RelationMember member;
			switch ( inputRelation.types( i ) ) {
			case PBF::Relation::NODE:
				member.type = RelationMember::Node;
				break;
			case PBF::Relation::WAY:
				member.type = RelationMember::Way;
				break;
			case PBF::Relation::RELATION:
				member.type = RelationMember::Relation;
			}
			lastRef += inputRelation.memids( i );
			member.ref = lastRef;
			member.role = m_primitiveBlock.stringtable().s( inputRelation.roles_sid( i ) ).data();
			relation->members.push_back( member );
		}

		m_currentEntity++;
		if ( m_currentEntity >= m_primitiveBlock.primitivegroup( m_currentGroup ).relations_size() ) {
			m_currentEntity = 0;
			m_currentGroup++;
			if ( m_currentGroup >= m_primitiveBlock.primitivegroup_size() )
				m_loadBlock = true;
			else
				loadGroup();
		}
	}

	void parseDense( IEntityReader::Node* node )
	{
		node->tags.clear();

		const PBF::DenseNodes& dense = m_primitiveBlock.primitivegroup( m_currentGroup ).dense();
		m_lastDenseID += dense.id( m_currentEntity );
		m_lastDenseLatitude += dense.lat( m_currentEntity );
		m_lastDenseLongitude += dense.lon( m_currentEntity );
		node->id = m_lastDenseID;
		node->coordinate.latitude = ( ( double ) m_lastDenseLatitude * m_primitiveBlock.granularity() + m_primitiveBlock.lat_offset() ) / NANO;
		node->coordinate.longitude = ( ( double ) m_lastDenseLongitude * m_primitiveBlock.granularity() + m_primitiveBlock.lon_offset() ) / NANO;

		while ( true ){
			if ( m_lastDenseTag >= dense.keys_vals_size() )
				break;

			int tagValue = dense.keys_vals( m_lastDenseTag );
			if ( tagValue == 0 ) {
				m_lastDenseTag++;
				break;
			}

			int tagID = m_nodeTagIDs[tagValue];

			if ( tagID == -1 ) {
				m_lastDenseTag += 2;
				continue;
			}

			Tag newTag;
			newTag.key = tagID;
			newTag.value = QString::fromUtf8( m_primitiveBlock.stringtable().s( dense.keys_vals( m_lastDenseTag + 1 ) ).data() );
			node->tags.push_back( newTag );
			m_lastDenseTag += 2;
		}

		m_currentEntity++;
		if ( m_currentEntity >= dense.id_size() ) {
			m_currentEntity = 0;
			m_currentGroup++;
			if ( m_currentGroup >= m_primitiveBlock.primitivegroup_size() )
				m_loadBlock = true;
			else
				loadGroup();
		}
	}

	void loadGroup()
	{
		const PBF::PrimitiveGroup& group = m_primitiveBlock.primitivegroup( m_currentGroup );
		if ( group.nodes_size() != 0 ) {
			m_mode = ModeNode;
		} else if ( group.ways_size() != 0 ) {
			m_mode = ModeWay;
		} else if ( group.relations_size() != 0 ) {
			m_mode = ModeRelation;
		} else if ( group.has_dense() )  {
			m_mode = ModeDense;
			m_lastDenseID = 0;
			m_lastDenseTag = 0;
			m_lastDenseLatitude = 0;
			m_lastDenseLongitude = 0;
			assert( group.dense().id_size() != 0 );
		} else
			assert( false );
	}

	void loadBlock()
	{
		m_loadBlock = false;
		m_currentGroup = 0;
		m_currentEntity = 0;
		int stringCount = m_primitiveBlock.stringtable().s_size();
		// precompute all strings that match a necessary tag
		m_nodeTagIDs.resize( m_primitiveBlock.stringtable().s_size() );
		for ( int i = 1; i < stringCount; i++ )
			m_nodeTagIDs[i] = m_nodeTags.value( m_primitiveBlock.stringtable().s( i ).data(), -1 );
		m_wayTagIDs.resize( m_primitiveBlock.stringtable().s_size() );
		for ( int i = 1; i < stringCount; i++ )
			m_wayTagIDs[i] = m_wayTags.value( m_primitiveBlock.stringtable().s( i ).data(), -1 );
		m_relationTagIDs.resize( m_primitiveBlock.stringtable().s_size() );
		for ( int i = 1; i < stringCount; i++ )
			m_relationTagIDs[i] = m_relationTags.value( m_primitiveBlock.stringtable().s( i ).data(), -1 );
	}

	bool readNextBlock()
	{
		if ( !readBlockHeader() )
			return false;

		if ( m_blockHeader.type() != "OSMData" ) {
			qCritical() << "invalid block type, found" << m_blockHeader.type().data() << "instead of OSMData";
			return false;
		}

		if ( !readBlob() )
			return false;

		if ( !m_primitiveBlock.ParseFromArray( m_buffer.data(), m_buffer.size() ) ) {
			qCritical() << "failed to parse PrimitiveBlock";
			return false;
		}
		return true;
	}

	bool readBlockHeader()
	{
		char sizeData[4];
		if ( m_file.read( sizeData, 4 * sizeof( char ) ) != 4 * sizeof( char ) )
			return false; // end of stream?

		int size = convertNetworkByteOrder( sizeData );
		if ( size > MAX_BLOCK_HEADER_SIZE || size < 0 ) {
			qCritical() << "BlockHeader size invalid:" << size;
			return false;
		}
		m_buffer.resize( size );
		int readBytes = m_file.read( m_buffer.data(), size );
		if ( readBytes != size ) {
			qCritical() << "failed to read BlockHeader";
			return false;
		}
		if ( !m_blockHeader.ParseFromArray( m_buffer.constData(), size ) ) {
			qCritical() << "failed to parse BlockHeader";
			return false;
		}
		return true;
	}

	bool readBlob()
	{
		int size = m_blockHeader.datasize();
		if ( size < 0 || size > MAX_BLOB_SIZE ) {
			qCritical() << "invalid Blob size:" << size;
			return false;
		}
		m_buffer.resize( size );
		int readBytes = m_file.read( m_buffer.data(), size );
		if ( readBytes != size ) {
			qCritical() << "failed to read Blob";
			return false;
		}
		if ( !m_blob.ParseFromArray( m_buffer.constData(), size ) ) {
			qCritical() << "failed to parse blob";
			return false;
		}

		if ( m_blob.has_raw() ) {
			const std::string& data = m_blob.raw();
			m_buffer.resize( data.size() );
			for ( unsigned i = 0; i < data.size(); i++ )
				m_buffer[i] = data[i];
		} else if ( m_blob.has_zlib_data() ) {
			if ( !unpackZlib() )
				return false;
		} else if ( m_blob.has_bzip2_data() ) {
			if ( !unpackBzip2() )
				return false;
		} else if ( m_blob.has_lzma_data() ) {
			if ( !unpackLzma() )
				return false;
		} else {
			qCritical() << "Blob contains no data";
			return false;
		}

		return true;
	}

	bool unpackZlib()
	{
		m_buffer.resize( m_blob.raw_size() );
		z_stream compressedStream;
		compressedStream.next_in = ( unsigned char* ) m_blob.zlib_data().data();
		compressedStream.avail_in = m_blob.zlib_data().size();
		compressedStream.next_out = ( unsigned char* ) m_buffer.data();
		compressedStream.avail_out = m_blob.raw_size();
		compressedStream.zalloc = Z_NULL;
		compressedStream.zfree = Z_NULL;
		compressedStream.opaque = Z_NULL;
		int ret = inflateInit( &compressedStream );
		if ( ret != Z_OK ) {
			qCritical() << "failed to init zlib stream";
			return false;
		}
		ret = inflate( &compressedStream, Z_FINISH );
		if ( ret != Z_STREAM_END ) {
			qCritical() << "failed to inflate zlib stream";
			return false;
		}
		ret = inflateEnd( &compressedStream );
		if ( ret != Z_OK ) {
			qCritical() << "failed to deinit zlib stream";
			return false;
		}
		return true;
	}

	bool unpackBzip2()
	{
		unsigned size = m_blob.raw_size();
		m_buffer.resize( size );
		m_bzip2Buffer.resize( m_blob.bzip2_data().size() );
		for ( unsigned i = 0; i < m_blob.bzip2_data().size(); i++ )
			m_bzip2Buffer[i] = m_blob.bzip2_data()[i];
		int ret = BZ2_bzBuffToBuffDecompress( m_buffer.data(), &size, m_bzip2Buffer.data(), m_bzip2Buffer.size(), 0, 0 );
		if ( ret != BZ_OK ) {
			qCritical() << "failed to unpack bzip2 stream";
			return false;
		}
		return true;
	}

	static void *SzAlloc( void *p, size_t size)
	{
		p = p;
		return malloc( size );
	}

	static void SzFree( void *p, void *address)
	{
		p = p;
		free( address );
	}

	bool unpackLzma()
	{
		ISzAlloc alloc = { SzAlloc, SzFree };
		ELzmaStatus status;
		SizeT destinationLength = m_blob.raw_size();
		SizeT sourceLength = m_blob.lzma_data().size() - LZMA_PROPS_SIZE + 8;
		int ret = LzmaDecode(
				( unsigned char* ) m_buffer.data(),
				&destinationLength,
				( const unsigned char* ) m_blob.lzma_data().data() + LZMA_PROPS_SIZE + 8,
				&sourceLength,
				( const unsigned char* ) m_blob.lzma_data().data(),
				LZMA_PROPS_SIZE + 8,
				LZMA_FINISH_END,
				&status,
				&alloc );

		if ( ret != SZ_OK )
			return false;

		return true;
	}

	PBF::BlockHeader m_blockHeader;
	PBF::Blob m_blob;

	PBF::HeaderBlock m_headerBlock;
	PBF::PrimitiveBlock m_primitiveBlock;

	int m_currentGroup;
	int m_currentEntity;
	bool m_loadBlock;

	Mode m_mode;

	QHash< QString, int > m_nodeTags;
	QHash< QString, int > m_wayTags;
	QHash< QString, int > m_relationTags;

	std::vector< int > m_nodeTagIDs;
	std::vector< int > m_wayTagIDs;
	std::vector< int > m_relationTagIDs;

	long long m_lastDenseID;
	long long m_lastDenseLatitude;
	long long m_lastDenseLongitude;
	int m_lastDenseTag;

	QFile m_file;
	QByteArray m_buffer;
	QByteArray m_bzip2Buffer;

};

#endif // PBFREADER_H
