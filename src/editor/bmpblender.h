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

#ifndef BMPBLENDER_H
#define BMPBLENDER_H

#include <QMap>
#include <QRgb>
#include <QStringList>
#include <QVector>

namespace BuildingEditor {
class FloorTileGrid;
}

namespace Tiled {
class Map;
class TileLayer;

namespace Internal {

class BmpBlender : public QObject
{
    Q_OBJECT
public:
    BmpBlender(Map *map);
    ~BmpBlender();

    bool read();
    bool readRules(const QString &filePath);
    bool readBlends(const QString &filePath);

    void recreate();
    void update(int x1, int y1, int x2, int y2);

signals:
    void layersRecreated();

public: // TODO: make private
    void imagesToTileNames(int x1, int y1, int x2, int y2);
    void blend(int x1, int y1, int x2, int y2);
    void tileNamesToLayers(int x1, int y1, int x2, int y2);

    Map *mMap;
    QMap<QString,BuildingEditor::FloorTileGrid*> mTileNameGrids;
    QMap<QString,TileLayer*> mTileLayers;

    class Rule
    {
    public:
        int bitmapIndex;
        QRgb color;
        QRgb condition;
        QStringList tileChoices;
        QString targetLayer;

        Rule(int bitmapIndex, QRgb col, QStringList tiles, QString layer, QRgb condition) :
            bitmapIndex(bitmapIndex),
            color(col),
            tileChoices(tiles),
            targetLayer(layer),
            condition(condition)
        {}
        Rule(int bitmapIndex, QRgb col, QStringList tiles, QString layer) :
            bitmapIndex(bitmapIndex),
            color(col),
            tileChoices(tiles),
            targetLayer(layer),
            condition(qRgb(0,0,0))
        {}
        Rule(int bitmapIndex, QRgb col, QString tile, QString layer);
        Rule(int bitmapIndex, QRgb col, QString tile, QString layer, QRgb condition);
    };

    class Blend
    {
    public:
        enum Direction {
            Unknown,
            N,
            S,
            E,
            W,
            NW,
            NE,
            SW,
            SE
        };

        QString targetLayer;
        QString mainTile;
        QString blendTile;
        Direction dir;
        QStringList ExclusionList;

        Blend()
        {}
        Blend(const QString &layer, const QString &main, const QString &blend,
              Direction dir, const QStringList &exclusions) :
            targetLayer(layer), mainTile(main), blendTile(blend), dir(dir),
            ExclusionList(exclusions)
        {}

        bool isNull()
        { return mainTile.isEmpty(); }
    };

    void AddRule(int bitmapIndex, QRgb col, QStringList tiles,
                 QString layer, QRgb condition);
    void AddRule(int bitmapIndex, QRgb col, QStringList tiles,
                 QString layer);

    QString getNeighbouringTile(int x, int y);
    Blend getBlendRule(int x, int y, const QString &tileName, const QString &layer);

    QList<Rule*> mRules;
    QMap<QRgb,QList<Rule*> > mRuleByColor;
    QStringList mRuleLayers;

    QList<Blend> mBlendList;
    QStringList mBlendLayers;
    QMap<QString,QList<int> > mBlendsByLayer;

    QString mError;
};

} // namespace Internal
} // namespace Tiled

#endif // BMPBLENDER_H
