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

#ifndef IGUISETTINGS_H
#define IGUISETTINGS_H

#include <QtPlugin>

class IImporter;
class QWidget;

// plugins can support this interface to provide the user
// with a graphical interface for its settings
class IGUISettings
{

public:

	// has to return a valid pointer to a settings window
	// the window can be displayed modal and nonmodal
	// the implementation should be able to cope with this
	// the settings window should never be accessed outside
	// FillSettingsWindow and ReadSettingsWindow!
	// the ownership of the settings window is transferred to
	// the caller
	virtual bool GetSettingsWindow( QWidget** window ) = 0;

	// should fill a settings window gotten from GetSettingsWindow
	// the settings window should represent the internal settings
	virtual bool FillSettingsWindow( QWidget* window ) = 0;

	// reads settings from a settings window fotten from GetSettingsWindow
	// the internal settings should be exactly restored if read from a
	// window filled with FillSettingsWindow
	virtual bool ReadSettingsWindow( QWidget* window ) = 0;

	// virtual descructor
	// must not free settings windows itself
	virtual ~IGUISettings() {}
};

Q_DECLARE_INTERFACE( IGUISettings, "monav.IGUISettings/1.0" )

#endif // IGUISETTINGS_H
