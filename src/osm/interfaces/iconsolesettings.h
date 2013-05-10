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

#ifndef ICONSOLESETTINGS_H
#define ICONSOLESETTINGS_H

#include <QVector>
#include <QVariant>

class IImporter;

// plugins can support this interface to provide the user with
// a way to set settings directly from the console
class IConsoleSettings
{

public:

	// discribes a single settings
	struct Setting {
		// provides the user with a quick way to access a feature
		// e.g., a user can use "-a" to access a settings with shortID "a"
		// if the shortID is not unique it might not be accessible for this user
		QString shortID;
		// provides the user with a way to access a feature
		// should be a unique string, otherwise it might not be possible
		// for the user to set this settings at all if the shortID is also not unqiue
		QString longID;
		// provides a short description of the settings that is displayed
		// with the "--help" command
		QString description;
		// the data type
		// not every type is supported, look at CommandLineParser's header to see a
		// list of supported types. Double, int and string are garantued to be supported
		QString type;

		Setting()
		{
		}

		Setting( QString shortID, QString longID, QString description, QString type )
			: shortID( shortID ), longID( longID ), description( description), type( type )
		{
		}
	};

	// returns the name of the module the settings belong to ( "main", plugin name, etc... )
	virtual QString GetModuleName() = 0;
	// returns the list of settings supported
	virtual bool GetSettingsList( QVector< Setting >* settings ) = 0;
	// informs the interface that a settings was set by the user
	virtual bool SetSetting( int id, QVariant data ) = 0;

	virtual ~IConsoleSettings() {}
};

Q_DECLARE_INTERFACE( IConsoleSettings, "monav.IConsoleSettings/1.0" )

#endif // ICONSOLESETTINGS_H
