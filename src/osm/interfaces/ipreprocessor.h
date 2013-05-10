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

#ifndef IPREPROCESSING_H
#define IPREPROCESSING_H

#include <QString>
#include <QtPlugin>

class IImporter;
class QWidget;
class QSettings;

class IPreprocessor
{
public:
	enum Type {
		Renderer, Router, GPSLookup, AddressLookup
	};

	virtual QString GetName() = 0;
	virtual int GetFileFormatVersion() = 0;
	virtual Type GetType() = 0;
	virtual bool LoadSettings( QSettings* settings ) = 0;
	virtual bool SaveSettings( QSettings* settings ) = 0;
	virtual bool Preprocess( IImporter* importer, QString dir ) = 0;
	virtual ~IPreprocessor() {}
};

Q_DECLARE_INTERFACE( IPreprocessor, "monav.IPreprocessor/1.2" )

#endif // IPREPROCESSING_H
