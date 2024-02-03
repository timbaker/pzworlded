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

#ifndef WORLDVIEW_H
#define WORLDVIEW_H

#include "basegraphicsview.h"

#include <QGraphicsItem>

class MapImage;
class WorldBMP;
class WorldScene;

class WorldMiniMapItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)
public:
    WorldMiniMapItem(WorldScene *scene, QGraphicsItem *parent = 0);

    QRectF boundingRect() const;

    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0);

private slots:
    void worldResized();

    void bmpAdded(int index);
    void bmpAboutToBeRemoved(int index);
    void bmpCoordsChanged(int index);

    void showOtherWorlds(bool show);

private:
    struct WorldBMPImage
    {
        WorldBMP* bmp;
        MapImage* mapImage;
    };

    void insertBmp(int index, WorldBMP *bmp);

private:
    WorldScene *mScene;
    QList<WorldBMPImage> mImages;
};

class WorldView : public BaseGraphicsView
{
public:
    explicit WorldView(QWidget *parent = 0);

    void setScene(WorldScene *scene);

    void mouseMoveEvent(QMouseEvent *event);

    WorldScene *scene() const;

private:
    WorldMiniMapItem *mMiniMapItem;
};

#endif // WORLDVIEW_H
