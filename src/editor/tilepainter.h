/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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

#ifndef TILEPAINTER_H
#define TILEPAINTER_H

#include <QRect>
#include <QRegion>

class PathWorld;
class WorldPath;
class WorldTile;
class WorldTileLayer;
class WorldTileset;

class TilePainter
{
public:
    TilePainter(PathWorld *world);

    PathWorld *world() const
    { return mWorld; }

    void setLayer(WorldTileLayer *layer);

    void setTile(WorldTile *tile);

    void erase(int x, int y, int width, int height);
    void erase(const QRect &r);
    void erase(const QRegion &rgn);
    void erase(WorldPath *path);

    void fill(int x, int y, int width, int height);
    void fill(const QRect &r);
    void fill(const QRegion &rgn);
    void fill(WorldPath *path);

    void strokePath(WorldPath *path, qreal thickness);
    void fillPath(WorldPath *path);
    void tracePath(WorldPath *path, qreal offset);

    void noClip();
    void setClip(const QRect &rect);
    void setClip(WorldPath *path);

    typedef void (*TileFilterProc)(int x, int y, WorldTile *tile);

private:
    PathWorld *mWorld;
    WorldTileLayer *mLayer;
    WorldTile *mTile;

    enum ClipType
    {
        ClipNone,
        ClipRect,
        ClipPath
    };
    ClipType mClipType;
    QRect mClipRect;
    WorldPath *mClipPath;
};

#endif // TILEPAINTER_H
