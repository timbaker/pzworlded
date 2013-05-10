#include "types.h"
#include "utils/qthelpers.h"
#include <QTextStream>
#include <QFile>
#include <QSettings>

#define qls(x) QLatin1String(x)

namespace MoNav {

	SpeedProfile::SpeedProfile()
	{
		highways.clear();
		wayModificators.clear();
		nodeModificators.clear();

		acceleration = 2.78;
		decceleration = 2.78;
		tangentialAcceleration = 1.0;
		pedestrian = 10;
		otherCars = 10;

		loadAccessTree( qls(":/speed profiles/accessTree") );
		setAccess( qls("motorcar") );

		defaultCitySpeed = true;
		ignoreOneway = false;
		ignoreMaxspeed = false;
	}

	bool SpeedProfile::loadAccessTree( QString filename )
	{
		QFile treeFile( filename );
		if ( !openQFile( &treeFile, QIODevice::ReadOnly ) )
			return false;
		QTextStream stream( &treeFile );

		m_accessTree.clear();
		QString name;
		QString parent;
		// format:
		// name1 parent1
		// name2 parent2 ...
		// the source node has itself as a parent
		// a parent has to be defined before its referenced
		while ( true ) {
			stream >> name >> parent;
			if ( stream.status() != QTextStream::Ok )
				break;

			if ( m_accessTree.contains( name ) ) {
				qCritical() << qls("Illegal access tree, duplicate entry:") << name;
				return false;
			}

			if ( parent != name && !m_accessTree.contains( parent ) ) {
				qCritical() << qls("Illegal access tree, parent not yet defined:") << parent;
				return false;
			}

			m_accessTree[name] = parent;
		}

		return true;
	}

	bool SpeedProfile::setAccess( QString type )
	{
		accessList.clear();
		// traverse access tree and add all access types encountered
		while ( true ) {
			qDebug() << qls("OSM Importer: access list:") << type;
			accessList.push_back( type );
			if ( !m_accessTree.contains( type ) )
				return false;
			if ( type == m_accessTree[type] )
				break;
			type = m_accessTree[type];
		}
		return true;
	}

	QStringList SpeedProfile::accessTypes()
	{
		QStringList keys = m_accessTree.keys();
		keys.sort();
		return keys;
	}

	bool SpeedProfile::load( QString filename )
	{
		QSettings settings( filename, QSettings::IniFormat );

		defaultCitySpeed =  settings.value( qls("defaultCitySpeed") ).toBool();
		ignoreOneway = settings.value( qls("ignoreOneway") ).toBool();
		ignoreMaxspeed = settings.value( qls("ignoreMaxspeed") ).toBool();
		acceleration = settings.value( qls("acceleration") ).toDouble();
		decceleration = settings.value( qls("decceleration") ).toDouble();
		tangentialAcceleration = settings.value( qls("tangentialAcceleration") ).toDouble();
		pedestrian = settings.value( qls("pedestrian") ).toInt();
		otherCars = settings.value( qls("otherCars") ).toInt();

		QString accessType = settings.value( qls("accessType") ).toString();
		if ( !setAccess( accessType ) )
			return false;

		highways.clear();
		int highwayCount = settings.value( qls("highwayCount") ).toInt();
		for ( int i = 0; i < highwayCount; i++ ) {
			Highway highway;
			highway.priority = settings.value( QString( qls("highway.%1.priority") ).arg( i ) ).toInt();
			highway.value = settings.value( QString( qls("highway.%1.value") ).arg( i ) ).toString();
			highway.maxSpeed = settings.value( QString( qls("highway.%1.maxSpeed") ).arg( i ) ).toInt();
			highway.defaultCitySpeed = settings.value( QString( qls("highway.%1.defaultCitySpeed") ).arg( i ) ).toInt();
			highway.averageSpeed = settings.value( QString( qls("highway.%1.averageSpeed") ).arg( i ) ).toInt();
			highway.pedestrian = settings.value( QString( qls("highway.%1.pedestrian") ).arg( i ) ).toBool();
			highway.otherLeftPenalty = settings.value( QString( qls("highway.%1.otherLeftPenalty") ).arg( i ) ).toBool();
			highway.otherRightPenalty = settings.value( QString( qls("highway.%1.otherRightPenalty") ).arg( i ) ).toBool();
			highway.otherStraightPenalty = settings.value( QString( qls("highway.%1.otherStraightPenalty") ).arg( i ) ).toBool();
			highway.otherLeftEqual = settings.value( QString( qls("highway.%1.otherLeftEqual") ).arg( i ) ).toBool();
			highway.otherRightEqual = settings.value( QString( qls("highway.%1.otherRightEqual") ).arg( i ) ).toBool();
			highway.otherStraightEqual = settings.value( QString( qls("highway.%1.otherStraightEqual") ).arg( i ) ).toBool();
			highway.leftPenalty = settings.value( QString( qls("highway.%1.leftPenalty") ).arg( i ) ).toInt();
			highway.rightPenalty = settings.value( QString( qls("highway.%1.rightPenalty") ).arg( i ) ).toInt();

			highways.push_back( highway );
		}

		wayModificators.clear();
		int wayModificatorCount = settings.value( qls("wayModificatorsCount") ).toInt();
		for ( int i = 0; i < wayModificatorCount; i++ ) {
			WayModificator mod;
			mod.key = settings.value( QString( qls("wayModificator.%1.key") ).arg( i ) ).toString();
			mod.checkValue = settings.value( QString( qls("wayModificator.%1.checkValue") ).arg( i ) ).toBool();
			if ( mod.checkValue )
				mod.value = settings.value( QString( qls("wayModificator.%1.value") ).arg( i ) ).toString();
			mod.invert = settings.value( QString( qls("wayModificator.%1.invert") ).arg( i ) ).toBool();
			mod.type = ( MoNav::WayModificatorType ) settings.value( QString( qls("wayModificator.%1.type") ).arg( i ) ).toInt();
			mod.modificatorValue = settings.value( QString( qls("wayModificator.%1.modificatorValue") ).arg( i ) );

			wayModificators.push_back( mod);
		}

		nodeModificators.clear();
		int nodeModificatorCount = settings.value( qls("nodeModificatorsCount") ).toInt();
		for ( int i = 0; i < nodeModificatorCount; i++ ) {
			NodeModificator mod;
			mod.key = settings.value( QString( qls("nodeModificator.%1.key") ).arg( i ) ).toString();
			mod.checkValue = settings.value( QString( qls("nodeModificator.%1.checkValue") ).arg( i ) ).toBool();
			if ( mod.checkValue )
				mod.value = settings.value( QString( qls("nodeModificator.%1.value") ).arg( i ) ).toString();
			mod.invert = settings.value( QString( qls("nodeModificator.%1.invert") ).arg( i ) ).toBool();
			mod.type = ( MoNav::NodeModificatorType ) settings.value( QString( qls("nodeModificator.%1.type") ).arg( i ) ).toInt();
			mod.modificatorValue = settings.value( QString( qls("nodeModificator.%1.modificatorValue") ).arg( i ) );

			nodeModificators.push_back( mod );
		}

		if ( !settings.status() == QSettings::NoError ) {
			qCritical() << qls("error accessing file:") << filename << settings.status();
			return false;
		}

		qDebug() << qls("OSM Importer:: loaded speed profile:") << filename;
		return true;
	}

	bool SpeedProfile::save( QString filename )
	{
		QSettings settings( filename, QSettings::IniFormat );
		settings.clear();

		settings.setValue( qls("defaultCitySpeed"), defaultCitySpeed );
		settings.setValue( qls("ignoreOneway"), ignoreOneway );
		settings.setValue( qls("ignoreMaxspeed"), ignoreMaxspeed );
		settings.setValue( qls("acceleration"), acceleration );
		settings.setValue( qls("decceleration"), decceleration );
		settings.setValue( qls("tangentialAcceleration"), tangentialAcceleration );
		settings.setValue( qls("pedestrian"), pedestrian );
		settings.setValue( qls("otherCars"), otherCars );

		if ( accessList.size() == 0 ) {
			qCritical() << qls("no access type selected");
			return false;
		}

		settings.setValue( qls("accessType"), accessList.first() );

		settings.setValue( qls("highwayCount"), highways.size() );
		for ( int i = 0; i < highways.size(); i++ ) {
			const Highway& highway = highways[i];
			settings.setValue( QString( qls("highway.%1.priority") ).arg( i ), highway.priority );
			settings.setValue( QString( qls("highway.%1.value") ).arg( i ), highway.value );
			settings.setValue( QString( qls("highway.%1.maxSpeed") ).arg( i ), highway.maxSpeed );
			settings.setValue( QString( qls("highway.%1.defaultCitySpeed") ).arg( i ), highway.defaultCitySpeed );
			settings.setValue( QString( qls("highway.%1.averageSpeed") ).arg( i ), highway.averageSpeed );
			settings.setValue( QString( qls("highway.%1.pedestrian") ).arg( i ), highway.pedestrian );
			settings.setValue( QString( qls("highway.%1.otherLeftPenalty") ).arg( i ), highway.otherLeftPenalty );
			settings.setValue( QString( qls("highway.%1.otherRightPenalty") ).arg( i ), highway.otherRightPenalty );
			settings.setValue( QString( qls("highway.%1.otherStraightPenalty") ).arg( i ), highway.otherStraightPenalty );
			settings.setValue( QString( qls("highway.%1.otherLeftEqual") ).arg( i ), highway.otherLeftEqual );
			settings.setValue( QString( qls("highway.%1.otherRightEqual") ).arg( i ), highway.otherRightEqual );
			settings.setValue( QString( qls("highway.%1.otherStraightEqual") ).arg( i ), highway.otherStraightEqual );
			settings.setValue( QString( qls("highway.%1.leftPenalty") ).arg( i ), highway.leftPenalty );
			settings.setValue( QString( qls("highway.%1.rightPenalty") ).arg( i ), highway.rightPenalty );
		}

		settings.setValue( qls("wayModificatorsCount"), wayModificators.size() );
		for ( int i = 0; i < wayModificators.size(); i++ ) {
			MoNav::WayModificator mod = wayModificators[i];
			settings.setValue( QString( qls("wayModificator.%1.key") ).arg( i ), mod.key );
			settings.setValue( QString( qls("wayModificator.%1.checkValue") ).arg( i ), mod.checkValue );
			if ( mod.checkValue )
				settings.setValue( QString( qls("wayModificator.%1.value") ).arg( i ), mod.value );
			settings.setValue( QString( qls("wayModificator.%1.invert") ).arg( i ), mod.invert );
			settings.setValue( QString( qls("wayModificator.%1.type") ).arg( i ), ( int ) mod.type );
			settings.setValue( QString( qls("wayModificator.%1.modificatorValue") ).arg( i ), mod.modificatorValue );
		}

		settings.setValue( qls("nodeModificatorsCount"), nodeModificators.size() );
		for ( int i = 0; i < nodeModificators.size(); i++ ) {
			MoNav::NodeModificator mod = nodeModificators[i];
			settings.setValue( QString( qls("nodeModificator.%1.key") ).arg( i ), mod.key );
			settings.setValue( QString( qls("nodeModificator.%1.checkValue") ).arg( i ), mod.checkValue );
			if ( mod.checkValue )
				settings.setValue( QString( qls("nodeModificator.%1.value") ).arg( i ), mod.value );
			settings.setValue( QString( qls("nodeModificator.%1.invert") ).arg( i ), mod.invert );
			settings.setValue( QString( qls("nodeModificator.%1.type") ).arg( i ), ( int ) mod.type );
			settings.setValue( QString( qls("nodeModificator.%1.modificatorValue") ).arg( i ), mod.modificatorValue );
		}

		if ( !settings.status() == QSettings::NoError ) {
			qCritical() << qls("error accessing file:") << filename;
			return false;
		}

		return true;
	}

}

