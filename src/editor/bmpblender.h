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

#include <QCoreApplication>
#include <QMap>
#include <QRegion>
#include <QRgb>
#include <QStringList>

namespace BuildingEditor {
class FloorTileGrid;
}

namespace Tiled {
class BmpBlend;
class BmpRule;
class Map;
class Tile;
class TileLayer;
class Tileset;

namespace Internal {

class BmpRulesFile
{
    Q_DECLARE_TR_FUNCTIONS(BmpRulesFile)

public:
    BmpRulesFile();
    ~BmpRulesFile();

    bool read(const QString &fileName);
    bool write(const QString &fileName);

    QString errorString() const
    { return mError; }

    const QList<BmpRule*> &rules() const
    { return mRules; }
    QList<BmpRule*> rulesCopy() const;

private:
    void AddRule(int bitmapIndex, QRgb col, QStringList tiles,
                 QString layer, QRgb condition);
    void AddRule(int bitmapIndex, QRgb col, QStringList tiles,
                 QString layer);

    QRgb rgbFromStringList(const QStringList &rgb, bool &ok);

    QList<BmpRule*> mRules;
    QString mError;
};

class BmpBlendsFile
{
    Q_DECLARE_TR_FUNCTIONS(BmpBlendsFile)

public:
    BmpBlendsFile();
    ~BmpBlendsFile();

    bool read(const QString &fileName);
    bool write(const QString &fileName);

    QString errorString() const
    { return mError; }

    const QList<BmpBlend*> &blends() const
    { return mBlends; }
    QList<BmpBlend*> blendsCopy() const;

private:
    QList<BmpBlend*> mBlends;
    QString mError;
};

class BmpBlender : public QObject
{
    Q_OBJECT
public:
    BmpBlender(QObject *parent = 0);
    BmpBlender(Map *map, QObject *parent = 0);
    ~BmpBlender();

    void setMap(Map *map);

    void fromMap();
    void recreate();
    void update(int x1, int y1, int x2, int y2);

    QList<TileLayer*> tileLayers()
    { return mTileLayers.values(); }

    QStringList tileLayerNames()
    { return mTileLayers.keys(); }

    QStringList blendLayers()
    { return mBlendLayers; }

    void tilesetAdded(Tileset *ts);
    void tilesetRemoved(const QString &tilesetName);

signals:
    void layersRecreated();
    void regionAltered(const QRegion &region);

private:
    void initTiles();
    void imagesToTileNames(int x1, int y1, int x2, int y2);
    void blend(int x1, int y1, int x2, int y2);
    void tileNamesToLayers(int x1, int y1, int x2, int y2);

    Map *mMap;
    QMap<QString,BuildingEditor::FloorTileGrid*> mTileNameGrids;
    QMap<QString,TileLayer*> mTileLayers;

    QStringList mTilesetNames;
    QStringList mTileNames;
    QMap<QString,Tile*> mTileByName;

    QString getNeighbouringTile(int x, int y);
    BmpBlend *getBlendRule(int x, int y, const QString &tileName, const QString &layer);

    QList<BmpRule*> mRules;
    QMap<QRgb,QList<BmpRule*> > mRuleByColor;
    QStringList mRuleLayers;

    QList<BmpBlend*> mBlendList;
    QStringList mBlendLayers;
    QMap<QString,QList<BmpBlend*> > mBlendsByLayer;

    QString mError;
};

} // namespace Internal
} // namespace Tiled

#endif // BMPBLENDER_H
