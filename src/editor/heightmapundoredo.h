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

#ifndef HEIGHTMAPUNDOREDO_H
#define HEIGHTMAPUNDOREDO_H

#include <QRegion>
#include <QUndoCommand>
#include <QVector>

class WorldDocument;

class HeightMapRegion
{
public:
    HeightMapRegion()
    {}
    HeightMapRegion(const QRegion &region, const QVector<int> &heights) :
        mRegion(region),
        mHeights(heights)
    {
        Q_ASSERT(width() * height() == mHeights.size());
    }

    int width() const { return bounds().width(); }
    int height() const { return bounds().height(); }
    QRect bounds() const { return mRegion.boundingRect(); }

    void setHeightAt(int x, int y, int height)
    {
        x -= bounds().left();
        y -= bounds().top();
        mHeights[x + y * width()] = height;
    }

    int heightAt(int x, int y) const
    {
        x -= bounds().left();
        y -= bounds().top();
        return mHeights[x + y * width()];
    }

    void resize(const QRegion &region)
    {
        QRect area = region.boundingRect();
        QVector<int> newHeights(area.width() * area.height());
        newHeights.fill(0);

        QRegion preserved = mRegion & region;
        foreach (QRect r, preserved.rects()) {
            for (int y = r.top(); y <= r.bottom(); ++y) {
                for (int x = r.left(); x <= r.right(); ++x) {
                    int index = x - area.x() + (y - area.y()) * area.width();
                    newHeights[index] = heightAt(x, y);
                }
            }
        }

        mRegion = region;
        mHeights = newHeights;
    }

    void merge(const HeightMapRegion *other, const QRegion &otherRegion)
    {
        resize(mRegion | otherRegion);
        foreach (QRect r, otherRegion.rects()) {
            for (int y = r.top(); y <= r.bottom(); ++y) {
                for (int x = r.left(); x <= r.right(); ++x) {
                    setHeightAt(x, y, other->heightAt(x, y));
                }
            }
        }
    }

    QRegion mRegion;
    QVector<int> mHeights;
};

class PaintHeightMap : public QUndoCommand
{
public:
    PaintHeightMap(WorldDocument *worldDoc, const HeightMapRegion &region,
                   bool mergeable);

    void undo();
    void redo();

    void paint(const HeightMapRegion &source);

    int id() const;
    bool mergeWith(const QUndoCommand *other);

private:
    WorldDocument *mWorldDocument;
    HeightMapRegion mSource;
    HeightMapRegion mErased;
    bool mMergeable;
};


class SetHeightMapFileName : public QUndoCommand
{
public:
    SetHeightMapFileName(WorldDocument *worldDoc, const QString &fileName);

    void undo() { swap(); }
    void redo() { swap(); }

    void swap();

private:
    WorldDocument *mWorldDocument;
    QString mFileName;
};

#endif // HEIGHTMAPUNDOREDO_H
