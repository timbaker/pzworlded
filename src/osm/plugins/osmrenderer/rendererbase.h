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

#ifndef RENDERERBASE_H
#define RENDERERBASE_H

#include <QObject>
#include <QCache>
#include <vector>
#include "interfaces/irenderer.h"
#include "utils/coordinates.h"
#include "brsettingsdialog.h"

class RendererBase : public QObject, public IRenderer
{
	Q_OBJECT
	Q_INTERFACES( IRenderer )

public:

	RendererBase();
	virtual ~RendererBase();
	virtual QString GetName() = 0;
	virtual void SetInputDirectory( const QString& dir );
	virtual void ShowSettings();
	virtual bool LoadData();
	virtual bool UnloadData();
	virtual int GetMaxZoom();
	virtual ProjectedCoordinate Move( int shiftX, int shiftY, const PaintRequest& request );
	virtual ProjectedCoordinate PointToCoordinate( int shiftX, int shiftY, const PaintRequest& request );
	virtual bool Paint( QPainter* painter, const PaintRequest& request );
	virtual void SetUpdateSlot( QObject* obj, const char* slot );

signals:
	void changed();

protected:

	//CALLBACKS FOR DERIVED CLASSES:

	// gets called after the user changed advanced settings
	virtual void advancedSettingsChanged();
	// gets called whenever a tile cache miss occurs
	// has to load / draw tile with coordinates (x,y)
	// magnification is passed as a hint. If the derived class can draw higher resolution tiles,
	// it may increase the tile size by this factor to match screen resolution
	virtual bool loadTile( int x, int y, int zoom, int magnification, QPixmap** tile ) = 0;
	// gets called when loading the map data
	virtual bool load() = 0;
	// gets called to unload the map data
	virtual void unload() = 0;

	// computes the tileID from x,y and zoom level
	long long tileID( int x, int y, int zoom );
	// sets up basic polygons for source / target / arrows
	void setupPolygons();
	// draws an arrow at position x,y with direction rotation
	void drawArrow( QPainter* painter, int x, int y, double rotation, QColor outer, QColor inner );
	// draws an indicator for a place / source / target
	// gets replaced by an arrow if it is outside the screen
	void drawIndicator( QPainter* painter, const QTransform& transform, const QTransform& inverseTransform, int x, int y, int sizeX, int sizeY, int virtualZoom, QColor outer, QColor inner );
	// draw an indicator circle that is not replaced by an arrow
	void drawCircle( QPainter* painter, const QTransform& transform, const QTransform& inverseTransform, int x, int y, int sizeX, int sizeY, int virtualZoom, QColor outer, QColor inner );
	// draws a polyline, clipping it if necessary and applying basic LOD
	void drawPolyline( QPainter* painter, const QRect& boundingBox, QVector< ProjectedCoordinate > line, QColor color );

	// should be used by derived classes to add additional settings
	QDialog* m_advancedSettings;
	// should be set by the derived class
	int m_tileSize;
	// has to be filled by the derived class with all possible zoom levels
	std::vector< int > m_zoomLevels;

	// the current map data directory
	QString m_directory;
	// the tile cache
	QCache< long long, QPixmap > m_cache;
	// is a map package loaded?
	bool m_loaded;
	// polygon for the arrow indicator
	QPolygonF m_arrow;

	// basic settings
	BRSettingsDialog::Settings m_settings;
	// basic settings dialog
	BRSettingsDialog* m_settingsDialog;

	// the last magnification factor
	int m_magnification;
};

#endif // RENDERERBASE_H
