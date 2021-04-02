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

#include "bmpblender.h"

#include "mapcomposite.h"
#include "tilesetmanager.h"

#include "BuildingEditor/buildingfloor.h"
#include "simplefile.h"
#include "BuildingEditor/buildingtiles.h"

#include "map.h"
#include "maplevel.h"
#include "maprenderer.h"
#include "tilelayer.h"
#include "tileset.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QSet>
#include <QTextStream>

using namespace Tiled;
using namespace Tiled::Internal;

// Version numbers for Blends.txt and Rules.txt
#define VERSION1 1
#define VERSION2 2
#define VERSION_LATEST VERSION2

static QString STR_Floor = QLatin1String("Floor");

BmpBlender::BmpBlender(QObject *parent) :
    QObject(parent),
    mMap(nullptr),
    mFakeTileGrid(nullptr),
    mInitTilesLater(true),
    mHack(false),
    mBlendEdgesEverywhere(false)
{
}

BmpBlender::BmpBlender(Map *map, QObject *parent) :
    QObject(parent),
    mMap(map),
    mFakeTileGrid(nullptr),
    mInitTilesLater(true),
    mHack(false),
    mBlendEdgesEverywhere(false)
{
    fromMap();
}

BmpBlender::~BmpBlender()
{
    qDeleteAll(mAliases);
    qDeleteAll(mRules);
    qDeleteAll(mBlendList);
    qDeleteAll(mTileGrids);
    delete mFakeTileGrid;
    qDeleteAll(mTileLayers);
}

void BmpBlender::setMap(Map *map)
{
    mMap = map;
    fromMap();
    recreate();
}

void BmpBlender::recreate()
{
    if (mFakeTileGrid) {
        qDeleteAll(mTileGrids);
        mTileGrids.clear();
        delete mFakeTileGrid;
        mFakeTileGrid = nullptr;

        qDeleteAll(mTileLayers);
        mTileLayers.clear();

        markDirty(0, 0, mMap->width() - 1, mMap->height() - 1);

        emit layersRecreated();
    }
}

void BmpBlender::markDirty(const QRegion &rgn)
{
    mDirtyRegion += rgn;
}

void BmpBlender::markDirty(const QRect &r)
{
    mDirtyRegion += r;
}

void BmpBlender::markDirty(int x1, int y1, int x2, int y2)
{
    mDirtyRegion += QRect(x1, y1, x2 - x1 + 1, y2 - y1 + 1);
}

void BmpBlender::flush(const MapRenderer *renderer, const QRect &rect, const QPoint &mapPos)
{
    if (mDirtyRegion.isEmpty())
        return;

    QPolygonF polygon;
    int level = 0;
    polygon << QPointF(renderer->pixelToTileCoords(rect.topLeft(), level) - mapPos);
    polygon << QPointF(renderer->pixelToTileCoords(rect.topRight(), level) - mapPos);
    polygon << QPointF(renderer->pixelToTileCoords(rect.bottomRight(), level) - mapPos);
    polygon << QPointF(renderer->pixelToTileCoords(rect.bottomLeft(), level) - mapPos);

    QRegion dirty = mDirtyRegion & polygon.boundingRect().toAlignedRect();
    if (dirty.isEmpty())
        return;
    mDirtyRegion -= dirty;

    if (mInitTilesLater) {
        initTiles();
        mInitTilesLater = false;
    }

    for (const QRect &r : dirty) {
        int x1 = r.left(), x2 = r.right(), y1 = r.top(), y2 = r.bottom();
        x1 -= 2;
        x2 += 2;
        y1 -= 2;
        y2 += 2;

        imagesToTileGrids(x1, y1, x2, y2);
        addEdgeTiles(x1, y1, x2, y2);
        tileGridsToLayers(x1, y1, x2, y2);
    }
}

void BmpBlender::flush(const QRect &rect)
{
    QRegion dirty = mDirtyRegion & rect;
    if (dirty.isEmpty())
        return;
    mDirtyRegion -= dirty;

    if (mInitTilesLater) {
        initTiles();
        mInitTilesLater = false;
    }

    int x1 = rect.left(), x2 = rect.right(), y1 = rect.top(), y2 = rect.bottom();
    x1 -= 2;
    x2 += 2;
    y1 -= 2;
    y2 += 2;

    imagesToTileGrids(x1, y1, x2, y2);
    addEdgeTiles(x1, y1, x2, y2);
    tileGridsToLayers(x1, y1, x2, y2);
}

void BmpBlender::tilesetAdded(Tileset *ts)
{
    if (mTilesetNames.contains(ts->name())) {
        mInitTilesLater = true;
        mDirtyRegion = QRegion(0, 0, mMap->width(), mMap->height());
    }
}

void BmpBlender::tilesetRemoved(const QString &tilesetName)
{
    if (mTilesetNames.contains(tilesetName)) {
        mInitTilesLater = true;
        mDirtyRegion = QRegion(0, 0, mMap->width(), mMap->height());
    }
}

void BmpBlender::tilesToPixels(int x1, int y1, int x2, int y2)
{
    int index = mMap->levelAt(0)->indexOfLayer(STR_Floor, Layer::TileLayerType);
    if (index == -1) return;
    TileLayer *floorLayer = mMap->levelAt(0)->layerAt(index)->asTileLayer();

    x1 = qBound(0, x1, mMap->width() - 1);
    x2 = qBound(0, x2, mMap->width() - 1);
    y1 = qBound(0, y1, mMap->height() - 1);
    y2 = qBound(0, y2, mMap->height() - 1);

    if (mInitTilesLater) {
        initTiles();
        mInitTilesLater = false;
    }

    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            QRgb col = mMap->rbmpMain().pixel(x, y);
            if (col != qRgb(0, 0, 0))
                continue;

            if (Tile *tile = floorLayer->cellAt(x, y).tile) {
                if (mFloorTileToRule.contains(tile))
                    mMap->rbmp(0).setPixel(x, y, mFloorTileToRule[tile]->mRule->color);
            }
        }
    }
}

// See if the given tile in the given layer should be there based on the rules
// and blends.
bool BmpBlender::expectTile(const QString &layerName, int x, int y, Tile *tile)
{
    if (mBlendGrids.contains(layerName)) {
        int index = x + y * mMap->width();
        if (mBlendGrids[layerName].contains(index)) {
            BlendWrapper *blendW = mBlendGrids[layerName][index];
            return blendW->mBlendTiles.contains(tile);
        }
    }
    return false;
}

void BmpBlender::setBlendEdgesEverywhere(bool enabled)
{
    mBlendEdgesEverywhere = enabled;
    markDirty(0, 0, mMap->width() - 1, mMap->height() - 1);
}

void BmpBlender::testBlendEdgesEverywhere(bool enabled, QRegion& tileSelection)
{
    if (mInitTilesLater) {
        initTiles();
        mInitTilesLater = false;
    }

    int x1 = 0;
    int x2 = mMap->width() - 1;
    int y1 = 0;
    int y2 = mMap->height() - 1;

    // First: blend with the setting the opposite of what it's being set to.
    mBlendEdgesEverywhere = !enabled;
    markDirty(0, 0, mMap->width() - 1, mMap->height() - 1);
    imagesToTileGrids(x1, y1, x2, y2);
    addEdgeTiles(x1, y1, x2, y2);
    tileGridsToLayers(x1, y1, x2, y2);

    // Save the tile layers so we can compare them.
    QMap<QString,TileLayer*> tileLayers = mTileLayers;
    mTileLayers.clear();

    // Second: blend with the setting at the desired value.
    mBlendEdgesEverywhere = enabled;
    markDirty(0, 0, mMap->width() - 1, mMap->height() - 1);
    imagesToTileGrids(x1, y1, x2, y2);
    addEdgeTiles(x1, y1, x2, y2);
    tileGridsToLayers(x1, y1, x2, y2);

    tileSelection = QRegion();

    for (QString layerName : mTileLayers.keys()) {
        TileLayer *layer1 = tileLayers[layerName];
        TileLayer *layer2 = mTileLayers[layerName];
        if (layer1 != nullptr && layer2 != nullptr) {
            QRegion diff = layer1->computeDiffRegion(layer2);
            if (diff.isEmpty() == false) {
                qDebug() << "EDGE-TILE-FIX: layers are different" << layerName;
                tileSelection |= diff;

            }
        } else {
            qDebug() << "EDGE-TILE-FIX: layer1" << layerName << "is null";
        }
    }

    for (QString layerName : tileLayers.keys()) {
        TileLayer *layer2 = mTileLayers[layerName];
        if (layer2 == nullptr) {
            qDebug() << "EDGE-TILE-FIX: layer2" << layerName << "is null";
        }
    }

    qDeleteAll(tileLayers);
}

static QStringList normalizeTileNames(const QStringList &tileNames)
{
    QStringList ret;
    foreach (QString tileName, tileNames) {
        if (tileName.isEmpty()) { // "null" in Rules.txt
            ret += tileName;
            continue;
        }
        Q_ASSERT_X(BuildingEditor::BuildingTilesMgr::legalTileName(tileName), "normalizeTileNames", (char*)tileName.toLatin1().constData());
        ret += BuildingEditor::BuildingTilesMgr::normalizeTileName(tileName);
    }
    return ret;
}

void BmpBlender::fromMap()
{
    QSet<QString> tileNames;

    // We have to take care that any alias references exist, because when
    // loading or clearing Rules.txt the aliases are changed before the rules.
    // Also, if the aliases change, Blends.txt may reference undefined aliases!

    qDeleteAll(mAliases);
    mAliases.clear();
    mAliasByName.clear();
    for (BmpAlias *alias : mMap->bmpSettings()->aliases()) {
        AliasWrapper *aliasW = new AliasWrapper(alias);
        mAliasByName[alias->name] = aliasW;
        aliasW->mTiles = normalizeTileNames(alias->tiles);
        for (const QString &tileName : alias->tiles) {
            tileNames += tileName;
        }
        mAliases += aliasW;
    }

    qDeleteAll(mRules);
    mRules.clear();
    mRuleByColor.clear();
    mRuleLayers.clear();
    mFloor0Rules.clear();
    for (BmpRule *rule : mMap->bmpSettings()->rules()) {
        RuleWrapper *ruleW = new RuleWrapper(rule);
        mRuleByColor[rule->color] += ruleW;
        if (!mRuleLayers.contains(rule->targetLayer)) {
            mRuleLayers += rule->targetLayer;
        }
        for (const QString &tileName : rule->tileChoices) {
            if (BuildingEditor::BuildingTilesMgr::legalTileName(tileName)) {
                if (!mAliasByName.contains(tileName)) {
                    tileNames += tileName;
                }
            }
        }
        QStringList tiles;
        for (const QString &tileName : rule->tileChoices) {
            if (tileName.isEmpty()) {// "null" in Rules.txt
                tiles += tileName;
            } else if (!BuildingEditor::BuildingTilesMgr::legalTileName(tileName)) {
                if (mAliasByName.contains(tileName)) {
                    tiles += mAliasByName[tileName]->mTiles;
                }
            } else {
                tiles += tileName;
            }
        }
        ruleW->mTileNames = normalizeTileNames(tiles);
        if (rule->targetLayer == STR_Floor && rule->bitmapIndex == 0) {
            mFloor0Rules += ruleW;
        }
        mRules += ruleW;
    }

    qDeleteAll(mBlendList);
    mBlendList.clear();
    mBlendsByLayer.clear();
    mBlendLayers.clear();
    QSet<QString> layers;
    for (BmpBlend *blend : mMap->bmpSettings()->blends()) {
        BlendWrapper *blendW = new BlendWrapper(blend);
        mBlendsByLayer[blend->targetLayer] += blendW;
        layers.insert(blend->targetLayer);
        QStringList excludes;
        for (const QString &tileName : blend->ExclusionList) {
            if (!BuildingEditor::BuildingTilesMgr::legalTileName(tileName)) {
                if (mAliasByName.contains(tileName)) {
                    excludes += mAliasByName[tileName]->mTiles;
                }
            } else {
                excludes += tileName;
                tileNames += tileName;
            }
        }
        for (int i = 0; i < blend->exclude2.size(); i += 2) {
            QString tileName = blend->exclude2[i];
            if (BuildingEditor::BuildingTilesMgr::legalTileName(tileName)) {
                tileNames += tileName;
            }
        }
        if (BuildingEditor::BuildingTilesMgr::legalTileName(blend->mainTile)) {
            if (!mAliasByName.contains(blend->mainTile))
                tileNames += blend->mainTile;
        }
        if (BuildingEditor::BuildingTilesMgr::legalTileName(blend->blendTile)) {
            if (!mAliasByName.contains(blend->blendTile))
                tileNames += blend->blendTile;
        }
        mBlendList += blendW;
    }
    mBlendLayers = layers.values();

    mTileNames = normalizeTileNames(tileNames.values());

    mBlendEdgesEverywhere = mMap->bmpSettings()->isBlendEdgesEverywhere();

    mInitTilesLater = true;

    mDirtyRegion = QRegion(QRect(QPoint(), mMap->size()));
}

QList<Tile *>& BmpBlender::tileNameToTiles(const QString &name, QList<Tile *>& tiles)
{
    if (name.isEmpty()) // "null" in Rules.txt
        tiles += nullptr;
    else if (mAliasByName.contains(name))
        tiles += tileNamesToTiles(mAliasByName[name]->mTiles);
    else if (mTileByName.contains(name))
        tiles += mTileByName[name];
    else
        tiles += TilesetManager::instance()->missingTile();
    return tiles;
}

QList<Tile *> BmpBlender::tileNameToTiles(const QString &name)
{
    QList<Tile*> tiles;
    return tileNameToTiles(name, tiles);
}

QList<Tile*> BmpBlender::tileNamesToTiles(const QStringList &names)
{
    QList<Tile*> ret;
    foreach (QString name, names) {
        tileNameToTiles(name, ret);
    }
    return ret;
}

void BmpBlender::initTiles()
{
    QMap<QString,Tileset*> tilesets;
    for (Tileset *ts : mMap->tilesets())
        tilesets[ts->name()] = ts;

    mTilesetNames.clear();
    mTileByName.clear();
    for (QString tileName : mTileNames) {
        QString tilesetName;
        int tileID;
        if (BuildingEditor::BuildingTilesMgr::parseTileName(tileName, tilesetName, tileID)) {
            if (!mTilesetNames.contains(tilesetName))
                mTilesetNames += tilesetName;
            if (tilesets.contains(tilesetName))
                mTileByName[tileName] = tilesets[tilesetName]->tileAt(tileID);
        }
    }

    mFloorTileToRule.clear();
    for (RuleWrapper *ruleW : mRules) {
        ruleW->mTiles = tileNamesToTiles(ruleW->mTileNames).toVector();
        if (ruleW->mRule->targetLayer != STR_Floor)
            continue;
        for (Tile *tile : ruleW->mTiles)
            mFloorTileToRule[tile] = ruleW;
    }

    mBlendExclude2Layers.clear();
    for (BlendWrapper *blendW : mBlendList) {
        blendW->mMainTiles = tileNameToTiles(blendW->mBlend->mainTile).toVector();
        blendW->mBlendTiles = tileNameToTiles(blendW->mBlend->blendTile).toVector();
        blendW->mExcludeTiles = tileNamesToTiles(blendW->mBlend->ExclusionList).toVector();
        blendW->mExclude2Tiles.clear();
        for (int i = 0; i < blendW->mBlend->exclude2.size(); i += 2) {
            blendW->mExclude2Tiles += tileNameToTiles(blendW->mBlend->exclude2[i]).toVector();
            mBlendExclude2Layers += blendW->mBlend->exclude2[i + 1];
        }
    }

    updateWarnings();

    // This list is for the benefit of PaintBMP().
    // It is a list of all known blend tiles.
    if (true/*mHack*/) {
        mKnownBlendTiles.clear();
        for (BlendWrapper *blendW : mBlendList) {
            mKnownBlendTiles += QSet<Tile*>(blendW->mBlendTiles.constBegin(), blendW->mBlendTiles.constEnd());
        }
    }
}

static bool adjacentToNonBlack(const QImage &image1, const QImage &image2, int x1, int y1)
{
    const QRgb black = qRgb(0, 0, 0);
    QRect r(0, 0, image1.width(), image1.height());
    for (int y = y1 - 2; y <= y1 + 2; y++) {
        for (int x = x1 - 2; x <= x1 + 2; x++) {
            if (!r.contains(x, y)) continue;
            if (image1.pixel(x, y) != black || image2.pixel(x, y) != black)
                return true;
        }
    }
    return false;
}

void BmpBlender::imagesToTileGrids(int x1, int y1, int x2, int y2)
{
    if (mTileGrids.isEmpty()) {
        for (QString layerName : mRuleLayers + mBlendLayers) {
            if (!mTileGrids.contains(layerName))
                mTileGrids[layerName] = new SparseTileGrid(mMap->width(), mMap->height());
        }
        mFakeTileGrid = new SparseTileGrid(mMap->width(), mMap->height());
    }

    const QRgb black = qRgb(0, 0, 0);

    // Hack - If a pixel is black, and the user-drawn map tile in 0_Floor is
    // one of the Rules.txt tiles, pretend that that pixel exists in the image.
    MapLevel *mapLevel = mMap->levelAt(0);
    int index = mapLevel->indexOfLayer(STR_Floor, Layer::TileLayerType);
    TileLayer *floorLayer = (index == -1) ? nullptr : mapLevel->layerAt(index)->asTileLayer();

    x1 = qBound(0, x1, mMap->width() - 1);
    x2 = qBound(0, x2, mMap->width() - 1);
    y1 = qBound(0, y1, mMap->height() - 1);
    y2 = qBound(0, y2, mMap->height() - 1);

    Cell emptyCell;

    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            for (QString layerName : mTileGrids.keys())
                mTileGrids[layerName]->replace(x, y, emptyCell);
            mFakeTileGrid->replace(x, y, emptyCell);
            for (QString layerName : mBlendGrids.keys())
                mBlendGrids[layerName].remove(x + y * mMap->width());

            QRgb col = mMap->rbmpMain().pixel(x, y);
            QRgb col2 = mMap->rbmpVeg().pixel(x, y);

            if (mRuleByColor.contains(col)) {
                for (RuleWrapper *ruleW : mRuleByColor[col]) {
                    if (ruleW->mRule->bitmapIndex != 0)
                        continue;
                    if (!mTileGrids.contains(ruleW->mRule->targetLayer))
                        continue;
                    if (!ruleW->mTiles.size())
                        continue;
                    Tile *tile = ruleW->mTiles[mMap->bmp(0).rand(x, y) % ruleW->mTiles.size()];
                    mTileGrids[ruleW->mRule->targetLayer]->replace(x, y, Cell(tile));
                }
            }

            // Hack - If a pixel is black, and the user-drawn map tile in 0_Floor is
            // one of the Rules.txt tiles, pretend that that pixel exists in the image.
            if (floorLayer && col == black) {
                if (Tile *tile = floorLayer->cellAt(x, y).tile) {
                    if (mFloorTileToRule.contains(tile)) {
                        RuleWrapper *ruleW = mFloorTileToRule[tile];
                        if (ruleW->mTiles.size()) {
                            Tile *tile = ruleW->mTiles[mMap->bmp(0).rand(x, y) % ruleW->mTiles.count()];
                            mFakeTileGrid->replace(x, y, Cell(tile));
                        }
                        col = ruleW->mRule->color;
                    }
                }
            }

            if (col2 != black && mRuleByColor.contains(col2)) {
                for (RuleWrapper *ruleW : mRuleByColor[col2]) {
                    if (ruleW->mRule->bitmapIndex != 1)
                        continue;
                    if (ruleW->mRule->condition != col && ruleW->mRule->condition != black)
                        continue;
                    if (!mTileGrids.contains(ruleW->mRule->targetLayer))
                        continue;
                    if (!ruleW->mTiles.size())
                        continue;
                    Tile *tile = ruleW->mTiles[mMap->bmp(1).rand(x, y) % ruleW->mTiles.size()];
                    mTileGrids[ruleW->mRule->targetLayer]->replace(x, y, Cell(tile));
                }
            }
        }
    }
}

void BmpBlender::addEdgeTiles(int x1, int y1, int x2, int y2)
{
    x1 = qBound(0, x1, mMap->width() - 1);
    x2 = qBound(0, x2, mMap->width() - 1);
    y1 = qBound(0, y1, mMap->height() - 1);
    y2 = qBound(0, y2, mMap->height() - 1);

    if (mTileGrids.contains(STR_Floor) == false)
        return;
    SparseTileGrid *grid = mTileGrids[STR_Floor];

    MapLevel *mapLevel = mMap->levelAt(0);

    QMap<QString,TileLayer*> mapLayers;
    for (const QString &layerName : mBlendExclude2Layers) {
        int n = mapLevel->indexOfLayer(layerName, Layer::TileLayerType);
        if (n != -1)
            mapLayers[layerName] = mapLevel->layerAt(n)->asTileLayer();
    }

    QVector<Tile*> neighbors(9);

    const Cell emptyCell;

    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            Tile *tile = grid->at(x, y).tile;
            if ((tile == nullptr) && ((mBlendEdgesEverywhere == true) ||
                                      adjacentToNonBlack(mMap->rbmpMain().rimage(), mMap->rbmpVeg().rimage(), x, y))) {
                tile = mFakeTileGrid->at(x, y).tile;
            }

            for (int dy = -1; dy <= +1; dy++)
                for (int dx = -1; dx <= +1; dx++)
                    neighbors[(dx + 1) + (dy + 1) * 3] = getNeighbouringTile(x + dx, y + dy);

            for (QString layerName : mBlendLayers) {
                BlendWrapper *blendW = getBlendRule(x, y, tile, layerName, neighbors);
                if (blendW != nullptr) {
                    for (int i = 0; i < blendW->mBlend->exclude2.size(); i += 2) {
                        if (mapLayers.contains(blendW->mBlend->exclude2[i + 1])) {
                            TileLayer *mapLayer = mapLayers[blendW->mBlend->exclude2[i + 1]];
                            if (Tile *tile = mapLayer->cellAt(x, y).tile) {
                                if (blendW->mExclude2Tiles[i/2].contains(tile)) {
                                    blendW = nullptr;
                                    break;
                                }
                            }
                        }
                    }
                }
                if (blendW == nullptr) {
                    mTileGrids[layerName]->replace(x, y, emptyCell);
                    if (true/*mHack*/) {
                        int index = x + y * mMap->width();
                        mBlendGrids[layerName].remove(index);
                    }
                    continue;
                }
                const QVector<Tile*> tiles = blendW->mBlendTiles;
                if (tiles.size()) {
                    Tile *tile = tiles[mMap->bmp(0).rand(x, y) % tiles.size()];
                    mTileGrids[layerName]->replace(x, y, Cell(tile));
                }
                if (true/*mHack*/) {
                    int index = x + y * mMap->width();
                    mBlendGrids[layerName][index] = blendW;
                }
            }
        }
    }
}

void BmpBlender::tileGridsToLayers(int x1, int y1, int x2, int y2)
{
    bool recreated = false;
    if (mTileLayers.isEmpty()) {
        for (const QString &layerName : mRuleLayers + mBlendLayers) {
            if (!mTileLayers.contains(layerName)) {
                mTileLayers[layerName] = new TileLayer(layerName, 0, 0,
                                                       mMap->width(), mMap->height());
            }
        }
        recreated = true;
    }

    x1 = qBound(0, x1, mMap->width() - 1);
    x2 = qBound(0, x2, mMap->width() - 1);
    y1 = qBound(0, y1, mMap->height() - 1);
    y2 = qBound(0, y2, mMap->height() - 1);

    const Cell emptyCell;

    MapLevel *mapLevel = mMap->levelAt(0);

    for (const QString &layerName : mTileLayers.keys()) {
        SparseTileGrid *grid = mTileGrids[layerName];
        TileLayer *tl = mTileLayers[layerName];
        BlendGrid &blendGrid = mBlendGrids[layerName];
        int n = mapLevel->indexOfLayer(layerName, Layer::TileLayerType);
        TileLayer *mapLayer = (n == -1) ? nullptr : mapLevel->layerAt(n)->asTileLayer();
        for (int y = y1; y <= y2; y++) {
            for (int x = x1; x <= x2; x++) {
                Tile *tile = grid->at(x, y).tile;
                if (tile == nullptr) {
                    tl->setCell(x, y, emptyCell);
                    continue;
                }
                // If the blend tile that is in the map is the expected one,
                // don't override it.  This prevents a map tile which should
                // be there from being overriden by this automatic one.
                if (mapLayer != nullptr) {
                    int index = x + y * mMap->width();
                    if (blendGrid.contains(index)) {
                        BlendWrapper *blendW = blendGrid[index];
                        Tile *tile = mapLayer->cellAt(x, y).tile;
                        if (blendW->mBlendTiles.contains(tile)) {
                            tl->setCell(x, y, emptyCell);
                            continue;
                        }
                    }
                }
                tl->setCell(x, y, Cell(tile));
            }
        }
    }

    if (recreated) {
        emit layersRecreated();
        updateWarnings();
    }

    QRect r(x1, y1, x2 - x1 + 1, y2 - y1 + 1);
    emit regionAltered(r);
}

QString BmpBlender::resolveAlias(const QString &tileName, int randForPos) const
{
    if (mAliasByName.contains(tileName)) {
        const QStringList &tiles = mAliasByName[tileName]->mTiles;
        return tiles.size() ? tiles[randForPos % tiles.size()] : QString();
    }
    return tileName;
}

void BmpBlender::updateWarnings()
{
    QSet<QString> warnings;

    if (mMap->bmpSettings()->rules().isEmpty())
        warnings += tr("Map has no rules.  Import some!");
    if (mMap->bmpSettings()->blends().isEmpty())
        warnings += tr("Map has no blends.  Import some!");

    QMap<QString,Tileset*> tilesets;
    for (Tileset *ts : mMap->tilesets())
        tilesets[ts->name()] = ts;
    for (const QString &tilesetName : mTilesetNames) {
        if (!tilesets.contains(tilesetName))
            warnings += tr("Map is missing \"%1\" tileset.").arg(tilesetName);
    }

    MapLevel *mapLevel = mMap->levelAt(0);

    for (const QString &layerName : mTileLayers.keys()) {
        int n = mapLevel->indexOfLayer(layerName, Layer::TileLayerType);
        if (n == -1)
            warnings += tr("Map is missing \"%1\" tile layer.").arg(layerName);
    }

    int ruleIndex = 1;
    for (RuleWrapper *ruleW : mRules) {
        for (const QString &tileName : ruleW->mRule->tileChoices) {
            if (!tileName.isEmpty()
                    && !BuildingEditor::BuildingTilesMgr::legalTileName(tileName)
                    && !mAliasByName.contains(tileName)) {
                // This shouldn't even be possible, since aliases are defined
                // in Rules.txt and wouldn't load if the alias were unknown.
                warnings += tr("Rule %1 uses unknown alias '%2'.").arg(ruleIndex).arg(tileName);
            }
        }
        ++ruleIndex;
    }

    int blendIndex = 1;
    for (BlendWrapper *blendW : mBlendList) {
        BmpBlend *blend = blendW->mBlend;
        for (const QString &tileName : blend->ExclusionList) {
            if (!BuildingEditor::BuildingTilesMgr::legalTileName(tileName)) {
                if (!mAliasByName.contains(tileName))
                    warnings += tr("Blend %1 uses unknown alias '%2' for exclude.")
                            .arg(blendIndex).arg(tileName);
            }
        }
        for (int i = 0; i < blend->exclude2.size() - 1; i += 2) {
            QString tileName = blend->exclude2[i];
            if (!BuildingEditor::BuildingTilesMgr::legalTileName(tileName)) {
                if (!mAliasByName.contains(tileName))
                    warnings += tr("Blend %1 uses unknown alias '%2' for exclude2.")
                            .arg(blendIndex).arg(tileName);
            }
        }
        if (!BuildingEditor::BuildingTilesMgr::legalTileName(blend->mainTile)) {
            if (!mAliasByName.contains(blend->mainTile))
                warnings += tr("Blend %1 uses unknown alias '%2' for mainTile.")
                        .arg(blendIndex).arg(blend->mainTile);
        }
        if (!BuildingEditor::BuildingTilesMgr::legalTileName(blend->blendTile)) {
            if (!mAliasByName.contains(blend->blendTile))
                warnings += tr("Blend %1 uses unknown alias '%2' for blendTile.")
                        .arg(blendIndex).arg(blend->blendTile);
        }
        ++blendIndex;
    }

    for (int y = 0; y < mMap->rbmpMain().height(); y++) {
        for (int x = 0; x < mMap->rbmpMain().width(); x++) {
            QRgb color = mMap->rbmpMain().pixel(x, y);
            if (color != qRgb(0,0,0) && !mRuleByColor.contains(color)) {
                warnings += tr("Map BMP image #%1 contains unknown color %2,%3,%4 at %5,%6")
                        .arg(0).arg(qRed(color)).arg(qGreen(color)).arg(qBlue(color)).arg(x).arg(y);
            }
            color = mMap->rbmpVeg().pixel(x, y);
            if (color != qRgb(0,0,0) && !mRuleByColor.contains(color)) {
                warnings += tr("Map BMP image #%1 contains unknown color %2,%3,%4 at %5,%6")
                        .arg(1).arg(qRed(color)).arg(qGreen(color)).arg(qBlue(color)).arg(x).arg(y);
            }
        }
    }

    if (warnings != mWarnings) {
        mWarnings = warnings;
        emit warningsChanged();
    }
}

Tile *BmpBlender::getNeighbouringTile(int x, int y)
{
    if (x < 0 || y < 0 || x >= mMap->width() || y >= mMap->height())
        return nullptr;
    SparseTileGrid *grid = mTileGrids[STR_Floor];
    Tile *tile = grid->at(x, y).tile;
    if (!tile)
        tile = mFakeTileGrid->at(x, y).tile;
    return tile;
}

BmpBlender::BlendWrapper *BmpBlender::getBlendRule(int x, int y, Tile *tile,
                                   const QString &layer,
                                   const QVector<Tile*> &neighbors)
{
    if ((mBlendEdgesEverywhere == false) && (tile == nullptr))
        return nullptr;

    BlendWrapper *lastBlend = nullptr;

#define NEIGHBOR(X,Y) neighbors[((X) - x + 1) + ((Y) - y + 1) * 3]

    for (BlendWrapper *blendW : mBlendsByLayer[layer]) {
        QVector<Tile*> &mainTiles = blendW->mMainTiles;
        if (mainTiles.contains(tile))
            continue;
        if (blendW->mExcludeTiles.contains(tile))
            continue;
        bool bPass = false;
        switch (blendW->mBlend->dir) {
        case BmpBlend::N:
            bPass = mainTiles.contains(NEIGHBOR(x, y - 1)) &&
                    !mainTiles.contains(NEIGHBOR(x - 1, y)) &&
                    !mainTiles.contains(NEIGHBOR(x + 1, y));
            break;
        case BmpBlend::S:
            bPass = mainTiles.contains(NEIGHBOR(x, y + 1)) &&
                    !mainTiles.contains(NEIGHBOR(x - 1, y)) &&
                    !mainTiles.contains(NEIGHBOR(x + 1, y));
            break;
        case BmpBlend::E:
            bPass = mainTiles.contains(NEIGHBOR(x + 1, y)) &&
                    !mainTiles.contains(NEIGHBOR(x, y - 1)) &&
                    !mainTiles.contains(NEIGHBOR(x, y + 1));
            break;
        case BmpBlend::W:
            bPass = mainTiles.contains(NEIGHBOR(x - 1, y)) &&
                    !mainTiles.contains(NEIGHBOR(x, y - 1)) &&
                    !mainTiles.contains(NEIGHBOR(x, y + 1));
            break;
        case BmpBlend::NE:
            bPass = mainTiles.contains(NEIGHBOR(x, y - 1)) &&
                    mainTiles.contains(NEIGHBOR(x + 1, y));
            break;
        case BmpBlend::SE:
            bPass = mainTiles.contains(NEIGHBOR(x, y + 1)) &&
                    mainTiles.contains(NEIGHBOR(x + 1, y));
            break;
        case BmpBlend::NW:
            bPass = mainTiles.contains(NEIGHBOR(x, y - 1)) &&
                    mainTiles.contains(NEIGHBOR(x - 1, y));
            break;
        case BmpBlend::SW:
            bPass = mainTiles.contains(NEIGHBOR(x, y + 1)) &&
                    mainTiles.contains(NEIGHBOR(x - 1, y));
            break;
        default:
            break;
        }
        if (bPass)
            lastBlend = blendW;
    }

    return lastBlend;
}

/////

BmpRulesFile::BmpRulesFile()
{
}

BmpRulesFile::~BmpRulesFile()
{
    qDeleteAll(mRules);
}

bool BmpRulesFile::read(const QString &fileName)
{
    if (isOldFormat(fileName))
        return readOldFormat(fileName);

    SimpleFile simpleFile;
    if (!simpleFile.read(fileName)) {
        mError = tr("%1\n(while reading %2)")
                .arg(simpleFile.errorString())
                .arg(QDir::toNativeSeparators(fileName));
        return false;
    }

    for (const SimpleFileBlock &block : simpleFile.blocks) {
        SimpleFileKeyValue kv;
        if (block.name == QLatin1String("rule")) {
            for (const SimpleFileKeyValue &kv : block.values) {
                if (kv.name != QLatin1String("label") &&
                        kv.name != QLatin1String("bitmap") &&
                        kv.name != QLatin1String("color") &&
                        kv.name != QLatin1String("tiles") &&
                        kv.name != QLatin1String("condition") &&
                        kv.name != QLatin1String("layer")) {
                    mError = tr("Line %1: Unknown rule attribute '%2'")
                            .arg(kv.lineNumber).arg(kv.name);
                    return false;
                }
            }

            bool ok;
            int bitmapIndex;
            if (block.keyValue("bitmap", kv)) {
                bitmapIndex = kv.value.toUInt(&ok);
                if (!ok || bitmapIndex > 1) {
                    mError = tr("Line %1: Invalid bitmap index '%2'")
                            .arg(kv.lineNumber).arg(kv.value);
                    return false;
                }
            } else {
missingKV:
                mError = tr("Line %1: Required attribute '%2' is missing")
                        .arg(block.lineNumber).arg(kv.name);
                return false;
            }

            QString label = block.value("label");


            QRgb col;
            if (block.keyValue("color", kv)) {
                col = rgbFromString(kv.value, ok);
                if (!ok) {
                    mError = tr("Line %1: Invalid rule color '%2'")
                            .arg(kv.lineNumber).arg(kv.value);
                    return false;
                }
            } else
                goto missingKV;

            QStringList choices;
            if (block.keyValue("tiles", kv)) {
                choices = kv.values();
                int n = 0;
                foreach (QString choice, choices) {
                    choices[n] = choice.trimmed();
                    if (choices[n] == QLatin1String("null"))
                        choices[n].clear();
                    else if (mAliasByName.contains(choices[n]))
                        ;
                    else if (!BuildingEditor::BuildingTilesMgr::legalTileName(choices[n])) {
                        mError = tr("Line %1: Invalid tile name '%2'")
                                .arg(kv.lineNumber).arg(choices[n]);
                        return false;
                    }
                    n++;
                }
                if (choices.size() < 1) {
                    mError = tr("Line %1: Rule needs 1 or more tile choices")
                            .arg(kv.lineNumber);
                    return false;
                }
            } else
                goto missingKV;

            QString layer;
            if (block.keyValue("layer", kv)) {
                layer = kv.value;
                if (simpleFile.version() == VERSION1) {
                    layer = MapComposite::layerNameWithoutPrefix(layer);
                }
#if 0
                if (!MapComposite::levelForLayer(layer)) {
                    mError = tr("Line %1: Invalid rule layer '%2'")
                            .arg(kv.lineNumber).arg(kv.value);
                    return false;
                }
#endif
            } else
                goto missingKV;

            QRgb condition = qRgb(0, 0, 0);
            if (block.keyValue("condition", kv)) {
                condition = rgbFromString(kv.value, ok);
                if (!ok) {
                    mError = tr("Line %1: Invalid rule condition '%2'")
                            .arg(kv.lineNumber).arg(kv.value);
                    return false;
                }
            }

            AddRule(label, bitmapIndex, col, choices, layer, condition);
        } else if (block.name == QLatin1String("alias")) {
            QString name;
            SimpleFileKeyValue kv;
            if (block.keyValue("name", kv)) {
                name = kv.value;
                if (name.isEmpty()) {
                    mError = tr("Line %1: Empty alias name").arg(kv.lineNumber);
                    return false;
                }
                if (mAliasByName.contains(name)) {
                    mError = tr("Line %1: Duplicate alias name '%2'")
                            .arg(kv.lineNumber).arg(name);
                    return false;
                }
            } else
                goto missingKV;

            QStringList tiles;
            if (block.keyValue("tiles", kv)) {
                for (const QString &tileName : kv.values()) {
                    if (!BuildingEditor::BuildingTilesMgr::legalTileName(tileName)) {
                        mError = tr("Line %1: Invalid tile name '%2'")
                                .arg(kv.lineNumber).arg(tileName);
                        return false;
                    }
                    tiles += BuildingEditor::BuildingTilesMgr::normalizeTileName(tileName);
                }
                if (tiles.size() < 1) {
                    mError = tr("Line %1: Alias requires 1 or more tiles")
                            .arg(kv.lineNumber);
                    return false;
                }
            } else
                goto missingKV;

            mAliases += new BmpAlias(name, tiles);
            mAliasByName[name] = mAliases.last();
        } else {
            mError = tr("Line %1: Unknown block name '%2'.\nProbable syntax error in Rules.txt.\n%3")
                    .arg(block.lineNumber).arg(block.name).arg(QDir::toNativeSeparators(fileName));
            return false;
        }
    }

    return true;
}

bool BmpRulesFile::write(const QString &fileName)
{
    SimpleFile simpleFile;

    foreach (BmpAlias *alias, mAliases) {
        SimpleFileBlock aliasBlock;
        aliasBlock.name = QLatin1String("alias");
        aliasBlock.addValue("name", alias->name);
        QStringList tiles;
        foreach (QString tileName, alias->tiles) {
            QString tilesetName;
            int tileID;
            if (BuildingEditor::BuildingTilesMgr::parseTileName(
                        tileName, tilesetName, tileID)) {
                tileName = BuildingEditor::BuildingTilesMgr::nameForTile2(tilesetName, tileID);
            }
            tiles += tileName;
        }
        aliasBlock.addValue("tiles", tiles);
        simpleFile.blocks += aliasBlock;
    }

    foreach (BmpRule *rule, mRules) {
        SimpleFileBlock ruleBlock;
        ruleBlock.name = QLatin1String("rule");
        if (!rule->label.isEmpty())
            ruleBlock.addValue("label", rule->label);
        ruleBlock.addValue("bitmap", QString::number(rule->bitmapIndex));
        ruleBlock.addValue("color", QString::fromLatin1("%1 %2 %3")
                           .arg(qRed(rule->color))
                           .arg(qGreen(rule->color))
                           .arg(qBlue(rule->color)));

        QStringList tiles;
        for (QString tileName : rule->tileChoices) {
            QString tilesetName;
            int tileID;
            if (mAliasByName.contains(tileName)) {
                //
            } else if (tileName.isEmpty()) {
                tileName = QLatin1String("null");
            } else if (BuildingEditor::BuildingTilesMgr::parseTileName(tileName, tilesetName, tileID)) {
                tileName = BuildingEditor::BuildingTilesMgr::nameForTile2(tilesetName, tileID);
            }
            tiles += tileName;
        }
        ruleBlock.addValue("tiles", tiles);

        ruleBlock.addValue("layer", rule->targetLayer);

        if (rule->condition != qRgb(0, 0, 0))
            ruleBlock.addValue("condition", QString::fromLatin1("%1 %2 %3")
                               .arg(qRed(rule->condition))
                               .arg(qGreen(rule->condition))
                               .arg(qBlue(rule->condition)));

        simpleFile.blocks += ruleBlock;
    }

    simpleFile.setVersion(1);
    if (!simpleFile.write(fileName)) {
        mError = simpleFile.errorString();
        return false;
    }
    return true;
}

QList<BmpAlias *> BmpRulesFile::aliasesCopy() const
{
    QList<BmpAlias *> ret;
    foreach (BmpAlias *alias, mAliases)
        ret += new BmpAlias(alias);
    return ret;
}

QList<BmpRule *> BmpRulesFile::rulesCopy() const
{
    QList<BmpRule *> ret;
    foreach (BmpRule *rule, mRules)
        ret += new BmpRule(rule);
    return ret;
}

void BmpRulesFile::fromMap(Map *map)
{
    mAliases = map->bmpSettings()->aliasesCopy();
    foreach (BmpAlias *alias, mAliases)
        mAliasByName[alias->name] = alias;
    mRules = map->bmpSettings()->rulesCopy();
}

void BmpRulesFile::AddRule(const QString &label, int bitmapIndex, QRgb col,
                           const QStringList &tiles,
                           const QString &layer, QRgb condition)
{
    QStringList normalizedTileNames;
    foreach (QString tileName, tiles) {
        if (mAliasByName.contains(tileName))
            normalizedTileNames += tileName;
        else
            normalizedTileNames += BuildingEditor::BuildingTilesMgr::normalizeTileName(tileName);
    }

    mRules += new BmpRule(label, bitmapIndex, col, normalizedTileNames, layer, condition);
}

QRgb BmpRulesFile::rgbFromString(const QString &string, bool &ok)
{
    QStringList rgb = string.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    return rgbFromStringList(rgb, ok);
}

QRgb BmpRulesFile::rgbFromStringList(const QStringList &rgb, bool &ok)
{
    if (rgb.size() != 3) {
        ok = false;
        return QRgb();
    }
    int r = rgb[0].trimmed().toInt(&ok);
    if (!ok || r < 0 || r > 255) {
        ok = false;
        return QRgb();
    }
    int g = rgb[1].trimmed().toInt(&ok);
    if (!ok || g < 0 || g > 255) {
        ok = false;
        return QRgb();
    }
    int b = rgb[2].trimmed().toInt(&ok);
    if (!ok || b < 0 || b > 255) {
        ok = false;
        return QRgb();
    }
    ok = true;
    return qRgb(r, g, b);
}

bool BmpRulesFile::isOldFormat(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QTextStream sr(&file);
    while (!sr.atEnd()) {
        QString line = sr.readLine();

        if (line.contains(QLatin1Char('#')))
            continue;
        if (line.trimmed().isEmpty())
            continue;

        QStringList lineSplit = line.split(QLatin1Char(','));
        if (lineSplit.length() != 6 && lineSplit.length() != 9) {
            continue;
        }

        return true;
    }

    return false;
}

bool BmpRulesFile::readOldFormat(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        mError = tr("%1\n(while reading %2)").arg(file.errorString())
                .arg(QDir::toNativeSeparators(fileName));
        return false;
    }

    qDeleteAll(mRules);
    mRules.clear();

    QTextStream sr(&file);
    int lineNo = 0;
    while (!sr.atEnd()) {
        QString line = sr.readLine();
        ++lineNo;

        if (line.contains(QLatin1Char('#')))
            continue;
        if (line.trimmed().isEmpty())
            continue;

        QStringList lineSplit = line.split(QLatin1Char(','));
        if (lineSplit.length() != 6 && lineSplit.length() != 9) {
            mError = tr("Line %1: Expected a comma-separated list of 6 or 9 values").arg(lineNo);
            return false;
        }
        bool ok;
        int bmpIndex = lineSplit[0].trimmed().toUInt(&ok);
        if (!ok || bmpIndex > 1) {
            mError = tr("Line %1: Expected BMP index 0 or 1").arg(lineNo);
            return false;
        }
        QRgb col = rgbFromStringList(lineSplit.mid(1, 3), ok);
        if (!ok) {
            mError = tr("Line %1: Invalid RGB triplet '%2 %3 %4'")
                    .arg(lineNo)
                    .arg(lineSplit[1].trimmed())
                    .arg(lineSplit[2].trimmed())
                    .arg(lineSplit[3].trimmed());
            return false;
        }
        QStringList choices = lineSplit[4].split(QLatin1Char(' '));
        int n = 0;
        foreach (QString choice, choices) {
            choices[n] = choice.trimmed();
            if (choices[n] == QLatin1String("null"))
                choices[n].clear();
            else if (!BuildingEditor::BuildingTilesMgr::legalTileName(choices[n])) {
                mError = tr("Line %1: Invalid tile name '%2'").arg(lineNo).arg(choices[n]);
                return false;
            }
            n++;
        }

        QString layer = lineSplit[5].trimmed();

        QRgb con = qRgb(0, 0, 0);
        if (lineSplit.length() > 6) {
            con = rgbFromStringList(lineSplit.mid(6, 3), ok);
            if (!ok) {
                mError = tr("Line %1: Invalid RGB triplet '%2 %3 %4'")
                        .arg(lineNo)
                        .arg(lineSplit[6].trimmed())
                        .arg(lineSplit[7].trimmed())
                        .arg(lineSplit[8].trimmed());
                return false;
            }
        }

        AddRule(QString(), bmpIndex, col, choices, layer, con);
    }

    file.close();
    write(fileName);

    return true;
}

/////

BmpBlendsFile::BmpBlendsFile()
{
}

BmpBlendsFile::~BmpBlendsFile()
{
    qDeleteAll(mBlends);
}

bool BmpBlendsFile::read(const QString &fileName, const QList<BmpAlias*> &aliases)
{
    qDeleteAll(mBlends);
    mBlends.clear();
    mAliasNames.clear();

    SimpleFile simpleFile;
    if (!simpleFile.read(fileName)) {
        mError = tr("%1\n(while reading %2)")
                .arg(simpleFile.errorString())
                .arg(QDir::toNativeSeparators(fileName));
        return false;
    }

    QMap<QString,BmpBlend::Direction> dirMap;
    dirMap[QLatin1String("n")] = BmpBlend::N;
    dirMap[QLatin1String("s")] = BmpBlend::S;
    dirMap[QLatin1String("e")] = BmpBlend::E;
    dirMap[QLatin1String("w")] = BmpBlend::W;
    dirMap[QLatin1String("nw")] = BmpBlend::NW;
    dirMap[QLatin1String("sw")] = BmpBlend::SW;
    dirMap[QLatin1String("ne")] = BmpBlend::NE;
    dirMap[QLatin1String("se")] = BmpBlend::SE;

    QMap<QString,BmpAlias*> aliasToName;
    for (BmpAlias *alias : aliases) {
        aliasToName[alias->name] = alias;
        mAliasNames += alias->name;
    }

    for (SimpleFileBlock block : simpleFile.blocks) {
        if (block.name == QLatin1String("blend")) {
            for (SimpleFileKeyValue &kv : block.values) {
                if (kv.name != QLatin1String("layer") &&
                        kv.name != QLatin1String("mainTile") &&
                        kv.name != QLatin1String("blendTile") &&
                        kv.name != QLatin1String("dir") &&
                        kv.name != QLatin1String("exclude") &&
                        kv.name != QLatin1String("exclude2")) {
                    mError = tr("Unknown blend attribute '%1'").arg(kv.name);
                    return false;
                }
            }

            QString mainTile;
            SimpleFileKeyValue kv;
            if (block.keyValue("mainTile", kv)) {
                mainTile = kv.value;
                if (!aliasToName.contains(mainTile)) {
                    if (!BuildingEditor::BuildingTilesMgr::legalTileName(mainTile)) {
                        mError = tr("Line %1: Invalid tile name '%2'")
                                .arg(kv.lineNumber).arg(mainTile);
                        return false;
                    }
                    mainTile = BuildingEditor::BuildingTilesMgr::normalizeTileName(mainTile);
                }

            } else {
missingKV:
                mError = tr("Line %1: Required attribute '%2' is missing")
                        .arg(block.lineNumber).arg(kv.name);
                return false;
            }

            QString blendTile;
            if (block.keyValue("blendTile", kv)) {
                blendTile = kv.value;
                if (!aliasToName.contains(blendTile)) {
                    if (!BuildingEditor::BuildingTilesMgr::legalTileName(blendTile)) {
                        mError = tr("Line %1: Invalid tile name '%2'")
                                .arg(kv.lineNumber).arg(blendTile);
                        return false;
                    }
                    blendTile = BuildingEditor::BuildingTilesMgr::normalizeTileName(blendTile);
                }
            } else
                goto missingKV;

            BmpBlend::Direction dir = BmpBlend::Unknown;
            if (block.keyValue("dir", kv)) {
                QString dirName = block.value("dir");
                if (dirMap.contains(dirName))
                    dir = dirMap[dirName];
                else {
                    mError = tr("Line %1: Unknown blend direction '%2'")
                            .arg(kv.lineNumber).arg(dirName);
                    return false;
                }
            } else
                goto missingKV;

            QStringList excludes;
            if (block.keyValue("exclude", kv)) {
                foreach (QString exclude, kv.values()) {
                    if (aliasToName.contains(exclude)) {
                        excludes += exclude;
                        continue;
                    }
                    if (!BuildingEditor::BuildingTilesMgr::legalTileName(exclude)) {
                        mError = tr("Invalid tile name '%1'").arg(exclude);
                        return false;
                    }
                    excludes += BuildingEditor::BuildingTilesMgr::normalizeTileName(exclude);
                }
            }

            QStringList exclude2;
            if (block.keyValue("exclude2", kv)) {
                QStringList values = kv.values();
                if (values.size() % 2) {
                    mError = tr("Line %1: Expected tile,layer pairs in exclude2")
                            .arg(kv.lineNumber);
                    return false;
                }
                for (int i = 0; i < values.size(); i += 2) {
                    QString tileName = values[i];
                    QString layerName = values[i + 1];
                    if (!MapComposite::levelForLayer(layerName)) {
                        mError = tr("Line %1: Invalid layer name '%2'")
                                .arg(kv.lineNumber).arg(layerName);
                        return false;
                    }
                    if (aliasToName.contains(tileName)) {
                        exclude2 += tileName;
                    } else {
                        if (!BuildingEditor::BuildingTilesMgr::legalTileName(tileName)) {
                            mError = tr("Line %1: Invalid tile name '%2'")
                                    .arg(kv.lineNumber).arg(tileName);
                            return false;
                        }
                        exclude2 += BuildingEditor::BuildingTilesMgr::normalizeTileName(tileName);
                    }
                    exclude2 += layerName;
                }
            }

            QString layerName = block.value("layer");
            if (simpleFile.version() == VERSION1) {
                layerName = MapComposite::layerNameWithoutPrefix(layerName);
            }

            BmpBlend *blend = new BmpBlend(layerName, mainTile,
                                           blendTile, dir, excludes, exclude2);
            mBlends += blend;
        } else {
            mError = tr("Unknown block name '%1'.\nProbable syntax error in Blends.txt.").arg(block.name);
            return false;
        }
    }

    return true;
}

bool BmpBlendsFile::write(const QString &fileName)
{
    SimpleFile simpleFile;

    for (BmpBlend *blend : mBlends) {
        SimpleFileBlock blendBlock;
        blendBlock.name = QLatin1String("blend");
        blendBlock.addValue("layer", blend->targetLayer);
        blendBlock.addValue("mainTile", unpaddedTileName(blend->mainTile));
        blendBlock.addValue("blendTile", unpaddedTileName(blend->blendTile));
        blendBlock.addValue("dir", blend->dirAsString());
        QStringList exclude;
        for (const QString &tileName : blend->ExclusionList) {
            exclude += unpaddedTileName(tileName);
        }
        blendBlock.addValue("exclude", exclude);
        QStringList exclude2;
        for (int i = 0; i < blend->exclude2.size() - 1; i += 2) {
            exclude2 += unpaddedTileName(blend->exclude2[i]);
            exclude2 += blend->exclude2[i+1];
        }
        blendBlock.addValue("exclude2", exclude2, 2);
        simpleFile.blocks += blendBlock;
    }

    simpleFile.setVersion(1);
    if (!simpleFile.write(fileName)) {
        mError = simpleFile.errorString();
        return false;
    }
    return true;
}

QList<BmpBlend *> BmpBlendsFile::blendsCopy() const
{
    QList<BmpBlend *> ret;
    for (BmpBlend *blend : mBlends) {
        ret += new BmpBlend(blend);
    }
    return ret;
}

void BmpBlendsFile::fromMap(Map *map)
{
    mBlends = map->bmpSettings()->blendsCopy();
    foreach (BmpAlias *alias, map->bmpSettings()->aliases())
        mAliasNames += alias->name;
}

QString BmpBlendsFile::unpaddedTileName(const QString &tileName)
{
    if (!mAliasNames.contains(tileName)) {
        QString tilesetName;
        int tileID;
        if (BuildingEditor::BuildingTilesMgr::parseTileName(
                    tileName, tilesetName, tileID)) {
            return BuildingEditor::BuildingTilesMgr::nameForTile2(tilesetName, tileID);
        }
    }
    return tileName;
}

/////
