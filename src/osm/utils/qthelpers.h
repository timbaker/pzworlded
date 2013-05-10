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

#ifndef QTHELPERS_H
#define QTHELPERS_H

#include <QFile>
#include <QtDebug>
#include <QDataStream>
#include <QTime>
#include <QDir>

static inline QString fileInDirectory( QString directory, QString filename )
{
	QDir dir( directory );
	return dir.filePath( filename );
}

static inline bool openQFile( QFile* file, QIODevice::OpenMode mode )
{
	if ( !file->open( mode ) ) {
		qCritical() << "could not open file:" << file->fileName() << "," << mode;
		return false;
	}
	return true;
}

class FileStream : public QDataStream {

public:

	FileStream( QString filename ) : m_file( filename )
	{
	}

	bool open( QIODevice::OpenMode mode )
	{
		if ( !openQFile( &m_file, mode ) )
			return false;
		setDevice( &m_file );
		return true;
	}

protected:

	QFile m_file;
};

class Timer : public QTime {

public:

	Timer()
	{
		start();
	}
};

#endif // QTHELPERS_H
