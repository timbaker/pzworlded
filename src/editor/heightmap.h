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

#ifndef HEIGHTMAP_H
#define HEIGHTMAP_H

#include <QRect>
#include <QString>

class HeightMapFile;

class HeightMapPrivate;

class HeightMap
{
public:
    HeightMap(HeightMapFile *hmFile);
    ~HeightMap();

    HeightMapFile *hmFile() const
    { return mFile; }

    int width() const;
    int height() const;
    QSize size() const { return QSize(width(), height()); }
    QRect bounds() const { return QRect(QPoint(), size()); }

    bool contains(int x, int y)
    { return bounds().contains(x, y); }

    void setHeightAt(const QPoint &p, int h);
    void setHeightAt(int x, int y, int h);
    int heightAt(const QPoint &p);
    int heightAt(int x, int y);

    void setCenter(int x, int y);

private:
    HeightMapFile *mFile;
    HeightMapPrivate *d;
};

#endif // HEIGHTMAP_H
