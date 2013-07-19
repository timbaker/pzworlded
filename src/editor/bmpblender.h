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
#include <QSet>
#include <QStringList>

namespace BuildingEditor {
class FloorTileGrid;
}

namespace Tiled {
class BmpAlias;
class BmpBlend;
class BmpRule;
class Map;
class MapRenderer;
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

    const QList<BmpAlias*> &aliases() const
    { return mAliases; }
    QList<BmpAlias*> aliasesCopy() const;

    const QList<BmpRule*> &rules() const
    { return mRules; }
    QList<BmpRule*> rulesCopy() const;

    void fromMap(Map *map);

private:
    void AddRule(const QString &label, int bitmapIndex, QRgb col, const QStringList &tiles,
                 const QString &layer, QRgb condition);

    QRgb rgbFromString(const QString &string, bool &ok);
    QRgb rgbFromStringList(const QStringList &rgb, bool &ok);

    bool isOldFormat(const QString &fileName);
    bool readOldFormat(const QString &fileName);

    QList<BmpAlias*> mAliases;
    QMap<QString,BmpAlias*> mAliasByName;
    QList<BmpRule*> mRules;
    QString mError;
};

class BmpBlendsFile
{
    Q_DECLARE_TR_FUNCTIONS(BmpBlendsFile)

public:
    BmpBlendsFile();
    ~BmpBlendsFile();

    bool read(const QString &fileName, const QList<BmpAlias *> &aliases);
    bool write(const QString &fileName);

    QString errorString() const
    { return mError; }

    const QList<BmpBlend*> &blends() const
    { return mBlends; }
    QList<BmpBlend*> blendsCopy() const;

    void fromMap(Map *map);

private:
    QString unpaddedTileName(const QString &tileName);

private:
    QList<BmpBlend*> mBlends;
    QStringList mAliasNames;
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
    void markDirty(int x1, int y1, int x2, int y2);
    void flush(const MapRenderer *renderer, const QRect &rect, const QPoint &mapPos);
    void flush(const QRect &rect);

    QList<TileLayer*> tileLayers()
    { return mTileLayers.values(); }

    QStringList tileLayerNames()
    { return mTileLayers.keys(); }

    QStringList blendLayers()
    { return mBlendLayers; }

    void tilesetAdded(Tileset *ts);
    void tilesetRemoved(const QString &tilesetName);

    QStringList warnings() const
    {
        QStringList ret(mWarnings.toList());
        ret.sort();
        return ret;
    }

    void setHack(bool hack) { mHack = hack; }
    QSet<Tile*> knownBlendTiles()
    { return mKnownBlendTiles; }
    void tilesToPixels(int x1, int y1, int x2, int y2);
    bool expectTile(const QString &layerName, int x, int y, Tile *tile);

signals:
    void layersRecreated();
    void regionAltered(const QRegion &region);
    void warningsChanged();

public slots:
    void updateWarnings();

private:
    QList<Tile *> tileNamesToTiles(const QStringList &names);
    void initTiles();
    void imagesToTileNames(int x1, int y1, int x2, int y2);
    void blend(int x1, int y1, int x2, int y2);
    void tileNamesToLayers(int x1, int y1, int x2, int y2);
    QString resolveAlias(const QString &tileName, int randForPos) const;

    Map *mMap;
    QMap<QString,BuildingEditor::FloorTileGrid*> mTileNameGrids;
    BuildingEditor::FloorTileGrid *mFakeTileGrid;
    QMap<QString,TileLayer*> mTileLayers;

    QStringList mTilesetNames;
    QStringList mTileNames;
    QMap<QString,Tile*> mTileByName;
    bool mInitTilesLater;

    QString getNeighbouringTile(int x, int y);
    BmpBlend *getBlendRule(int x, int y, const QString &tileName, const QString &layer,
                           const QVector<QString> &neighbors);

    QList<BmpAlias*> mAliases;
    QMap<QString,BmpAlias*> mAliasByName;
    QMap<QString,QStringList> mAliasTiles;

    QList<BmpRule*> mRules;
    QMap<QRgb,QList<BmpRule*> > mRuleByColor;
    QMap<BmpRule*,QStringList> mRuleTileNames;
    QStringList mRuleLayers;
    QList<BmpRule*> mFloor0Rules;
    QList<QStringList> mFloor0RuleTiles;
    QMap<Tile*,BmpRule*> mFloorTileToRule;

    QList<BmpBlend*> mBlendList;
    QStringList mBlendLayers;
    QMap<QString,QList<BmpBlend*> > mBlendsByLayer;
    QMap<BmpBlend*,QStringList> mBlendExcludes;
    QMap<BmpBlend*,QList<QList<Tile*> > > mBlendExclude2;
    QSet<QString> mBlendExclude2Layers;

    QSet<Tile*> mKnownBlendTiles;
    QMap<BmpBlend*,QList<Tile*> > mBlendTiles;
    bool mHack;
    typedef QHash<int,BmpBlend*> BlendGrid;
    QMap<QString,BlendGrid> mBlendGrids; // blend at each x,y

    QRegion mDirtyRegion;

    QSet<QString> mWarnings;

    QString mError;
};

} // namespace Internal
} // namespace Tiled

#endif // BMPBLENDER_H
