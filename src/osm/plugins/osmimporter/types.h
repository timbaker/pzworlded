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

#ifndef TYPES_H
#define TYPES_H

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>
#include <QHash>

namespace MoNav {

	enum NodeModificatorType {
		NodeModifyFixed = 0,
		NodeAccess = 1
	};

	struct NodeModificator {
		QString key;
		bool checkValue;
		QString value;
		bool invert;
		QVariant modificatorValue;
		NodeModificatorType type;
	};

	enum WayModificatorType {
		WayModifyFixed = 0,
		WayModifyPercentage = 1,
		WayAccess = 2,
		WayOneway = 3

	};

	struct WayModificator {
		QString key;
		bool checkValue;
		QString value;
		bool invert;
		QVariant modificatorValue;
		WayModificatorType type;
	};

	struct Highway {
		int priority;
		QString value;
		int maxSpeed;
		int defaultCitySpeed;
		int averageSpeed;
		bool pedestrian;
		bool otherLeftPenalty;
		bool otherLeftEqual;
		bool otherRightPenalty;
		bool otherRightEqual;
		bool otherStraightPenalty;
		bool otherStraightEqual;
		int leftPenalty;
		int rightPenalty;

		bool operator<( const Highway& right ) const
		{
			if ( priority != right.priority )
				return priority;
			return value < right.value;
		}
	};

	struct SpeedProfile {
		QVector< MoNav::Highway > highways;
		QVector< MoNav::WayModificator > wayModificators;
		QVector< MoNav::NodeModificator > nodeModificators;

		double acceleration;
		double decceleration;
		double tangentialAcceleration;
		int pedestrian;
		int otherCars;

		QStringList accessList;
		bool defaultCitySpeed;
		bool ignoreOneway;
		bool ignoreMaxspeed;

		SpeedProfile();
		bool load( QString filename );
		bool save( QString filename );
		bool setAccess( QString type );
		QStringList accessTypes();

	private:

		bool loadAccessTree( QString filename );

		QHash< QString, QString > m_accessTree;
	};
}

#endif // TYPES_H
