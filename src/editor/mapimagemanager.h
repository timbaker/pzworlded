/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MAPIMAGEMANAGER_H
#define MAPIMAGEMANAGER_H

#include <QFileInfo>
#include <QImage>
#include <QMap>
#include <QObject>

class MapComposite;
class MapInfo;

namespace Tiled {
class Map;
}

class MapImage
{
public:
    MapImage(QImage image, MapInfo *mapInfo);

    QImage image() const {return mImage; }
    MapInfo *mapInfo() const { return mInfo; }

    QPointF tileToPixelCoords(qreal x, qreal y);

    QRectF tileBoundingRect(const QRect &rect);

    QRectF bounds();

    qreal scale();

    QPointF tileToImageCoords(qreal x, qreal y);

    QPointF tileToImageCoords(const QPoint &pos)
    { return tileToImageCoords(pos.x(), pos.y()); }

private:
    QImage mImage;
    MapInfo *mInfo;
};

class MapImageManager : public QObject
{
    Q_OBJECT

public:
    static MapImageManager *instance();
    static void deleteInstance();

    MapImage *getMapImage(const QString &mapName, const QString &relativeTo = QString());

protected:
    QImage generateMapImage(const QString &mapFilePath);
    QImage generateMapImage(MapComposite *mapComposite);

signals:
    
public slots:
    
private:
    MapImageManager();
    QFileInfo imageFileInfo(const QString &mapFilePath);

    QMap<QString,MapImage*> mMapImages;

    static MapImageManager *mInstance;
};

#endif // MAPIMAGEMANAGER_H
