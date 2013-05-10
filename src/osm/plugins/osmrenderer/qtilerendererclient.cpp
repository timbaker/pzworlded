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

#include <QDir>
#include <QDebug>
#include <QDesktopServices>
#include <QPaintEngine>
#include <vector>
#include <algorithm>
#include <QImage>

#include "qtilerendererclient.h"
#include "utils/qthelpers.h"
#include "tile-write.h"

#define qls(x) QLatin1String(x)

struct tileref {
	int x;
	int y;
	int z;
	static bool sorter(const struct tileref &t1, const struct tileref &t2) {
		if(t1.x>t2.x) return true;
		if(t1.x<t2.x) return false;
		if(t1.y>t2.y) return true;
		if(t1.y<t2.y) return false;
		if(t1.z>t2.z) return true;
		return false;
	};
	bool operator==(const struct tileref &t) const {
		return(x==t.x && y==t.y && z==t.z);
	};
};
struct place_cache_e {
	long long id;
	bool used;
	std::vector<struct placename> placenames;
};

QtileRendererClient::QtileRendererClient()
{
	twriter = NULL;
	place_cache_zoom = 0;
}

QtileRendererClient::~QtileRendererClient()
{
	unload();
}

void QtileRendererClient::unload()
{
	if ( twriter == NULL )
		return;

	disconnect( this, SIGNAL(drawImage(QString,int,int,int,int)) );
	disconnect( this, SLOT(tileLoaded(int,int,int,int,QByteArray)) );
	disconnect( twriter, SIGNAL(image_finished(int,int,int,int,QByteArray)), this, SIGNAL(changed()) );
	m_renderThread->quit();
	m_renderThread->wait();
	m_renderThread->deleteLater();
	twriter->deleteLater();
	twriter = NULL;
	place_cache_zoom = 0;
}

QString QtileRendererClient::GetName()
{
    return qls("Qtile Renderer");
}

bool QtileRendererClient::IsCompatible( int fileFormatVersion )
{
	if ( fileFormatVersion == 1 )
		return true;
	return false;
}

bool QtileRendererClient::load()
{
	tileSize = 256;
	for(int i=0;i<=18;i++) m_zoomLevels.push_back(i);

	twriter = new TileWriter(m_directory);
	m_renderThread = new QThread( this );
	twriter->moveToThread( m_renderThread );
	connect( this, SIGNAL(drawImage(QString,int,int,int,int)), twriter, SLOT(draw_image(QString,int,int,int,int)) );
	connect( twriter, SIGNAL(image_finished(int,int,int,int,QByteArray)), this, SLOT(tileLoaded(int,int,int,int,QByteArray)) );
	connect( twriter, SIGNAL(image_finished(int,int,int,int,QByteArray)), this, SIGNAL(changed()) );
	m_renderThread->start();
	return true;
}

int QtileRendererClient::GetMaxZoom()
{
	return 18;
}

bool QtileRendererClient::loadTile( int x, int y, int zoom, int magnification, QPixmap** /*tile*/ )
{
    emit drawImage( qls(""), x, y, zoom, magnification );
	return false;
}

void QtileRendererClient::tileLoaded( int x, int y, int zoom, int magnification, QByteArray data )
{
	if ( this->m_magnification != magnification )
		return;
	QImage img( ( const uchar* ) data.constData(), tileSize * magnification , tileSize * magnification, QImage::Format_RGB888 );
	QPixmap* tile = new QPixmap( QPixmap::fromImage( img ) );
	long long id = tileID( x, y, zoom );
	m_cache.insert( id, tile, 256 * 256 * magnification * magnification * tile->depth() / 8 );
}

bool QtileRendererClient::Paint( QPainter* painter, const PaintRequest& request )
{
	//Do RendererBase's drawing
	if(!RendererBase::Paint(painter, request)) return false;

	//Work out which tiles were just drawn - FIXME this is copy/paste from RendererBase::Paint
	std::vector<struct tileref> used_tiles;

	int zoom = m_zoomLevels[request.zoom];

	int sizeX = painter->device()->width();
	int sizeY = painter->device()->height();

	if ( sizeX <= 1 && sizeY <= 1 )
		return true;
	double rotation = request.rotation;
	if ( fmod( rotation / 90, 1 ) < 0.01 )
		rotation = 90 * floor( rotation / 90 );
	else if ( fmod( rotation / 90, 1 ) > 0.99 )
		rotation = 90 * ceil( rotation / 90 );

	double tileFactor = 1u << zoom;

	QTransform transform = painter->worldTransform();
	QTransform inverseTransform = transform.inverted();

	const int xWidth = 1 << zoom;
	const int yWidth = 1 << zoom;

	QRect boundingBox = inverseTransform.mapRect( QRect( 0, 0, sizeX, sizeY ) );

	int minX = floor( ( double ) boundingBox.x() / m_tileSize + request.center.x * tileFactor );
	int maxX = ceil( ( double ) boundingBox.right() / m_tileSize + request.center.x * tileFactor );
	int minY = floor( ( double ) boundingBox.y() / m_tileSize + request.center.y * tileFactor );
	int maxY = ceil( ( double ) boundingBox.bottom() / m_tileSize + request.center.y * tileFactor );

	int posX = ( minX - request.center.x * tileFactor ) * m_tileSize;
	for ( int x = minX; x < maxX; ++x ) {
		int posY = ( minY - request.center.y * tileFactor ) * m_tileSize;
		for ( int y = minY; y < maxY; ++y ) {

			//QPixmap* tile = NULL;
			if ( x >= 0 && x < xWidth && y >= 0 && y < yWidth ) {
				struct tileref tile_ref;
				tile_ref.x = x; tile_ref.y = y;
				tile_ref.z = zoom;
				used_tiles.push_back(tile_ref);

			}
			posY += m_tileSize;
		}
		posX += m_tileSize;
	}

	//Now draw our text on the screen.
	//Go up to level 13 on all tiles
	for(std::vector<struct tileref>::iterator i = used_tiles.begin();
	i!=used_tiles.end(); i++) {
		while(i->z>=13) {
			i->x /= 2;
			i->y /= 2;
			i->z--;
		}
	}
	printf("Pre removing duplicates %d used_tiles\n", ( int ) used_tiles.size());
	//Sort and remove duplicates.
	std::sort(used_tiles.begin(), used_tiles.end(), tileref::sorter);
	used_tiles.erase(std::unique(used_tiles.begin(), used_tiles.end()),
						  used_tiles.end());
	printf("After removing duplicates %d used_tiles\n", ( int ) used_tiles.size());
	printf("                          %d cache entries\n", ( int ) place_cache.size());

	//Delete unneeded cache entries, and load the needed ones.
	for(std::vector<struct tileref>::iterator i=used_tiles.begin();
	i!=used_tiles.end(); i++) {
		long long used_tile_id = tileID(i->x, i->y, i->z);
		place_cache_e *pce = place_cache[used_tile_id];
		if(pce) {
			if(place_cache_zoom != zoom) {
				delete pce;
				pce=NULL;
				place_cache[used_tile_id] = NULL;
			}
			else pce->used = true;
		}
		if(!pce) { //Load new pce.
			place_cache_e *pce = new place_cache_e;
			pce->id = tileID(i->x, i->y, i->z);
			pce->used = true;
			twriter->get_placenames(i->x, i->y, i->z, zoom,
											pce->placenames);
			place_cache[used_tile_id] = pce;
		}
	}
    QFont smallFont(qls("Times"), 9);
	painter->setFont(smallFont);
	//Draw each loaded entry.
	for(place_cache_t::iterator i = place_cache.begin();
	i!=place_cache.end(); ) {
		if(!i->second || !i->second->used) {
			if(i->second) delete(i->second);
			place_cache.erase(i++);
			continue;
		}
		i->second->used = false; //Ready for next pass.
		for(std::vector<struct placename>::iterator j = i->second->placenames.begin();
		j!=i->second->placenames.end(); j++) {
			if(j->tilex<minX || j->tilex>maxX) continue;
			if(j->tiley<minY || j->tiley>maxY) continue;
			int posX = ( j->tilex - request.center.x * tileFactor ) * m_tileSize;
			int posY = ( j->tiley - request.center.y * tileFactor ) * m_tileSize;
			QTransform old = painter->worldTransform();
			QTransform unrotate(old);
			unrotate.translate(posX, posY);
			unrotate.rotate(-request.rotation);
			unrotate.translate(-posX, -posY);
			painter->setWorldTransform(unrotate);

			painter->setPen(QColor(255, 255, 255));
            painter->drawText(QPoint(posX+1, posY-1), qls((j->name.c_str())));
            painter->drawText(QPoint(posX+1, posY+1), qls((j->name.c_str())));
            painter->drawText(QPoint(posX-1, posY-1), qls((j->name.c_str())));
            painter->drawText(QPoint(posX-1, posY+1), qls((j->name.c_str())));
			if(j->type==3) painter->setPen(QColor(100, 100, 255));
			else painter->setPen(QColor(0, 0, 0));
            painter->drawText(QPoint(posX, posY), qls((j->name.c_str())));
			painter->setWorldTransform(old);
		}
		i++;
	}
	place_cache.erase(NULL);

	return true;
}

Q_EXPORT_PLUGIN2( qtilerendererclient, QtileRendererClient )
