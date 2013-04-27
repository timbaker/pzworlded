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

#include "BuildingEditor/buildingfloor.h"
#include "simplefile.h"
#include "BuildingEditor/buildingtiles.h"

#include "map.h"
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

BmpBlender::BmpBlender(QObject *parent) :
    QObject(parent),
    mMap(0),
    mFakeTileGrid(0)
{
}

BmpBlender::BmpBlender(Map *map, QObject *parent) :
    QObject(parent),
    mMap(map),
    mFakeTileGrid(0),
    mAliases(map->bmpSettings()->aliases()),
    mRules(map->bmpSettings()->rules()),
    mBlendList(map->bmpSettings()->blends())
{
    fromMap();
}

BmpBlender::~BmpBlender()
{
//    qDeleteAll(mRules);
//    qDeleteAll(mBlendList);
    qDeleteAll(mTileNameGrids);
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
    qDeleteAll(mTileNameGrids);
    mTileNameGrids.clear();
    delete mFakeTileGrid;

    qDeleteAll(mTileLayers);
    mTileLayers.clear();

    update(0, 0, mMap->width(), mMap->height());
}

void BmpBlender::update(int x1, int y1, int x2, int y2)
{
    imagesToTileNames(x1, y1, x2, y2);
    blend(x1 - 1, y1 - 1, x2 + 1, y2 + 1);
    tileNamesToLayers(x1 - 1, y1 - 1, x2 + 1, y2 + 1);
}

void BmpBlender::tilesetAdded(Tileset *ts)
{
    if (mTilesetNames.contains(ts->name())) {
        initTiles();
        tileNamesToLayers(0, 0, mMap->width(), mMap->height());
    }
}

void BmpBlender::tilesetRemoved(const QString &tilesetName)
{
    if (mTilesetNames.contains(tilesetName)) {
        initTiles();
        tileNamesToLayers(0, 0, mMap->width(), mMap->height());
    }
}

static QStringList normalizeTileNames(const QStringList &tileNames)
{
    QStringList ret;
    foreach (QString tileName, tileNames) {
        Q_ASSERT_X(BuildingEditor::BuildingTilesMgr::legalTileName(tileName), "normalizeTileNames", (char*)tileName.toAscii().constData());
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

    mAliases = mMap->bmpSettings()->aliases();
    mAliasByName.clear();
    mAliasTiles.clear();
    foreach (BmpAlias *alias, mAliases) {
        mAliasByName[alias->name] = alias;
        mAliasTiles[alias->name] = normalizeTileNames(alias->tiles);
        foreach (QString tileName, alias->tiles)
            tileNames += tileName;
    }

    mRules = mMap->bmpSettings()->rules();
    mRuleByColor.clear();
    mRuleLayers.clear();
    mFloor0Rules.clear();
    mFloor0RuleTiles.clear();
    foreach (BmpRule *rule, mRules) {
        mRuleByColor[rule->color] += rule;
        if (!mRuleLayers.contains(rule->targetLayer))
            mRuleLayers += rule->targetLayer;
        foreach (QString tileName, rule->tileChoices) {
            if (BuildingEditor::BuildingTilesMgr::legalTileName(tileName)) {
                if (!mAliasByName.contains(tileName))
                    tileNames += tileName;
            }
        }
        if (rule->targetLayer == QLatin1String("0_Floor") && rule->bitmapIndex == 0) {
            mFloor0Rules += rule;
            QStringList tiles;
            foreach (QString tileName, rule->tileChoices) {
                if (tileName.isEmpty())
                    ;
                else if (!BuildingEditor::BuildingTilesMgr::legalTileName(tileName)) {
                    if (mAliasByName.contains(tileName))
                        tiles += mAliasByName[tileName]->tiles;
                } else
                    tiles += tileName;
            }
            mFloor0RuleTiles += normalizeTileNames(tiles);
        }
    }

    mBlendList = mMap->bmpSettings()->blends();
    mBlendsByLayer.clear();
    mBlendLayers.clear();
    mBlendExcludes.clear();
    QSet<QString> layers;
    foreach (BmpBlend *blend, mBlendList) {
        mBlendsByLayer[blend->targetLayer] += blend;
        layers.insert(blend->targetLayer);
        QStringList excludes;
        foreach (QString tileName, blend->ExclusionList) {
            if (!BuildingEditor::BuildingTilesMgr::legalTileName(tileName)) {
                if (mAliasByName.contains(tileName))
                    excludes += mAliasByName[tileName]->tiles;
            } else {
                excludes += tileName;
                tileNames += tileName;
            }
        }
        mBlendExcludes[blend] = normalizeTileNames(excludes);
        if (BuildingEditor::BuildingTilesMgr::legalTileName(blend->mainTile)) {
            if (!mAliasByName.contains(blend->mainTile))
                tileNames += blend->mainTile;
        }
        if (BuildingEditor::BuildingTilesMgr::legalTileName(blend->blendTile)) {
            if (!mAliasByName.contains(blend->blendTile))
                tileNames += blend->blendTile;
        }
    }
    mBlendLayers = layers.values();

    mTileNames = normalizeTileNames(tileNames.values());

    initTiles();
}

void BmpBlender::initTiles()
{
    QMap<QString,Tileset*> tilesets;
    foreach (Tileset *ts, mMap->tilesets())
        tilesets[ts->name()] = ts;

    mTilesetNames.clear();
    mTileByName.clear();
    foreach (QString tileName, mTileNames) {
        QString tilesetName;
        int tileID;
        if (BuildingEditor::BuildingTilesMgr::parseTileName(tileName, tilesetName, tileID)) {
            if (!mTilesetNames.contains(tilesetName))
                mTilesetNames += tilesetName;
            if (tilesets.contains(tilesetName))
                mTileByName[tileName] = tilesets[tilesetName]->tileAt(tileID);
        }
    }

    updateWarnings();
}

static bool adjacentToNonBlack(const QImage &image1, const QImage &image2, int x1, int y1)
{
    const QRgb black = qRgb(0, 0, 0);
    QRect r(0, 0, image1.width(), image1.height());
    for (int y = y1 - 1; y <= y1 + 1; y++) {
        for (int x = x1 - 1; x <= x1 + 1; x++) {
            if (!r.contains(x, y)) continue;
            if (image1.pixel(x, y) != black || image2.pixel(x, y) != black)
                return true;
        }
    }
    return false;
}

void BmpBlender::imagesToTileNames(int x1, int y1, int x2, int y2)
{
    if (mTileNameGrids.isEmpty()) {
        foreach (QString layerName, mRuleLayers + mBlendLayers) {
            if (!mTileNameGrids.contains(layerName)) {
                mTileNameGrids[layerName]
                        = new BuildingEditor::FloorTileGrid(mMap->width(),
                                                            mMap->height());
            }
        }
        mFakeTileGrid = new BuildingEditor::FloorTileGrid(mMap->width(),
                                                          mMap->height());
    }

    const QRgb black = qRgb(0, 0, 0);

    // Hack - If a pixel is black, and the user-drawn map tile in 0_Floor is
    // one of the Rules.txt tiles, pretend that that pixel exists in the image.
    x1 -= 1, x2 += 1, y1 -= 1, y2 += 1;
    int index = mMap->indexOfLayer(QLatin1String("0_Floor"), Layer::TileLayerType);
    TileLayer *floorLayer = (index == -1) ? 0 : mMap->layerAt(index)->asTileLayer();

    x1 = qBound(0, x1, mMap->width() - 1);
    x2 = qBound(0, x2, mMap->width() - 1);
    y1 = qBound(0, y1, mMap->height() - 1);
    y2 = qBound(0, y2, mMap->height() - 1);

    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            foreach (QString layerName, mRuleLayers)
                mTileNameGrids[layerName]->replace(x, y, QString());
            mFakeTileGrid->replace(x, y, QString());

            QRgb col = mMap->rbmpMain().pixel(x, y);
            QRgb col2 = mMap->rbmpVeg().pixel(x, y);

            if (mRuleByColor.contains(col)) {
                QList<BmpRule*> rules = mRuleByColor[col];

                foreach (BmpRule *rule, rules) {
                    if (rule->bitmapIndex != 0)
                        continue;
                    if (!mTileNameGrids.contains(rule->targetLayer))
                        continue;
                    // FIXME: may not be normalized
                    QString tileName = rule->tileChoices[mMap->bmp(0).rand(x, y) % rule->tileChoices.count()];
                    tileName = resolveAlias(tileName, mMap->bmp(0).rand(x, y));
                    mTileNameGrids[rule->targetLayer]->replace(x, y, tileName);
                }
            }

            // Hack - If a pixel is black, and the user-drawn map tile in 0_Floor is
            // one of the Rules.txt tiles, pretend that that pixel exists in the image.
            if (floorLayer && col == black && adjacentToNonBlack(mMap->rbmpMain().rimage(),
                                                                 mMap->rbmpVeg().rimage(),
                                                                 x, y)) {
                if (Tile *tile = floorLayer->cellAt(x, y).tile) {
                    QString tileName = BuildingEditor::BuildingTilesMgr::nameForTile(tile);
                    for (int i = 0; i < mFloor0Rules.size(); i++) {
                        BmpRule *rule = mFloor0Rules[i];
                        const QStringList &tiles = mFloor0RuleTiles[i];
                        if (tiles.contains(tileName)) {
                            // FIXME: may not be normalized
                            tileName = rule->tileChoices[mMap->bmp(0).rand(x, y) % rule->tileChoices.count()];
                            tileName = resolveAlias(tileName, mMap->bmp(0).rand(x, y));
                            mFakeTileGrid->replace(x, y, tileName);
                            col = rule->color;
                        }
                    }
                }
            }

            if (col2 != black && mRuleByColor.contains(col2)) {
                QList<BmpRule*> rules = mRuleByColor[col2];

                foreach (BmpRule *rule, rules) {
                    if (rule->bitmapIndex != 1)
                        continue;
                    if (rule->condition != col && rule->condition != black)
                        continue;
                    if (!mTileNameGrids.contains(rule->targetLayer))
                        continue;
                    // FIXME: may not be normalized
                    QString tileName = rule->tileChoices[mMap->bmp(1).rand(x, y) % rule->tileChoices.count()];
                    tileName = resolveAlias(tileName, mMap->bmp(1).rand(x, y));
                    mTileNameGrids[rule->targetLayer]->replace(x, y, tileName);
                }
            }
        }
    }
}

void BmpBlender::blend(int x1, int y1, int x2, int y2)
{
    x1 = qBound(0, x1, mMap->width() - 1);
    x2 = qBound(0, x2, mMap->width() - 1);
    y1 = qBound(0, y1, mMap->height() - 1);
    y2 = qBound(0, y2, mMap->height() - 1);

    BuildingEditor::FloorTileGrid *grid
            = mTileNameGrids[QLatin1String("0_Floor")];
    if (!grid)
        return;
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            QString tileName = grid->at(x, y);
            if (tileName.isEmpty())
                tileName = mFakeTileGrid->at(x, y);
            foreach (QString layerName, mBlendLayers) {
                if (BmpBlend *blend = getBlendRule(x, y, tileName, layerName)) {
                    QString tileName = resolveAlias(blend->blendTile, mMap->bmp(0).rand(x, y));
                    mTileNameGrids[layerName]->replace(x, y, tileName);
                } else
                    mTileNameGrids[layerName]->replace(x, y, QString());
            }
        }
    }
}

void BmpBlender::tileNamesToLayers(int x1, int y1, int x2, int y2)
{
    bool recreated = false;
    if (mTileLayers.isEmpty()) {
        foreach (QString layerName, mRuleLayers + mBlendLayers) {
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

    foreach (QString layerName, mTileLayers.keys()) {
        BuildingEditor::FloorTileGrid *grid = mTileNameGrids[layerName];
        TileLayer *tl = mTileLayers[layerName];
        for (int y = y1; y <= y2; y++) {
            for (int x = x1; x <= x2; x++) {
                QString tileName = grid->at(x, y);
                if (tileName.isEmpty()) {
                    tl->setCell(x, y, Cell(0));
                    continue;
                }
                tl->setCell(x, y, Cell(mTileByName[tileName]));
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
        const QStringList &tiles = mAliasTiles[tileName];
        return tiles[randForPos % tiles.size()];
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
    foreach (Tileset *ts, mMap->tilesets())
        tilesets[ts->name()] = ts;
    foreach (QString tilesetName, mTilesetNames) {
        if (!tilesets.contains(tilesetName))
            warnings += tr("Map is missing \"%1\" tileset.").arg(tilesetName);
    }

    foreach (QString layerName, mTileLayers.keys()) {
        int n = mMap->indexOfLayer(layerName, Layer::TileLayerType);
        if (n == -1)
            warnings += tr("Map is missing \"%1\" tile layer.").arg(layerName);
    }

    int ruleIndex = 1;
    foreach (BmpRule *rule, mRules) {
        foreach (QString tileName, rule->tileChoices) {
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
    foreach (BmpBlend *blend, mBlendList) {
        foreach (QString tileName, blend->ExclusionList) {
            if (!BuildingEditor::BuildingTilesMgr::legalTileName(tileName)) {
                if (!mAliasByName.contains(tileName))
                    warnings += tr("Blend %1 uses unknown alias '%2' for exclude.")
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

    if (warnings != mWarnings) {
        mWarnings = warnings;
        emit warningsChanged();
    }
}

QString BmpBlender::getNeighbouringTile(int x, int y)
{
    if (x < 0 || y < 0 || x >= mMap->width() || y >= mMap->height())
        return QString();
    BuildingEditor::FloorTileGrid *grid
            = mTileNameGrids[QLatin1String("0_Floor")];
    QString tileName = grid->at(x, y);
    if (tileName.isEmpty())
        tileName = mFakeTileGrid->at(x, y);
    return tileName;
}

BmpBlend *BmpBlender::getBlendRule(int x, int y, const QString &tileName,
                                   const QString &layer)
{
    BmpBlend *lastBlend = 0;
    if (tileName.isEmpty())
        return lastBlend;
    foreach (BmpBlend *blend, mBlendsByLayer[layer]) {
        Q_ASSERT(blend->targetLayer == layer);
        if (blend->targetLayer != layer)
            continue;

        QStringList mainTiles(blend->mainTile);
        if (mAliasByName.contains(blend->mainTile))
            mainTiles = mAliasTiles[blend->mainTile];

        if (!mainTiles.contains(tileName)) {
            if (mBlendExcludes[blend].contains(tileName))
                continue;
            bool bPass = false;
            switch (blend->dir) {
            case BmpBlend::N:
                bPass = mainTiles.contains(getNeighbouringTile(x, y - 1));
                break;
            case BmpBlend::S:
                bPass = mainTiles.contains(getNeighbouringTile(x, y + 1));
                break;
            case BmpBlend::E:
                bPass = mainTiles.contains(getNeighbouringTile(x + 1, y));
                break;
            case BmpBlend::W:
                bPass = mainTiles.contains(getNeighbouringTile(x - 1, y));
                break;
            case BmpBlend::NE:
                bPass = mainTiles.contains(getNeighbouringTile(x, y - 1)) &&
                        mainTiles.contains(getNeighbouringTile(x + 1, y));
                break;
            case BmpBlend::SE:
                bPass = mainTiles.contains(getNeighbouringTile(x, y + 1)) &&
                        mainTiles.contains(getNeighbouringTile(x + 1, y));
                break;
            case BmpBlend::NW:
                bPass = mainTiles.contains(getNeighbouringTile(x, y - 1)) &&
                        mainTiles.contains(getNeighbouringTile(x - 1, y));
                break;
            case BmpBlend::SW:
                bPass = mainTiles.contains(getNeighbouringTile(x, y + 1)) &&
                        mainTiles.contains(getNeighbouringTile(x - 1, y));
                break;
            default:
                break;
            }

            if (bPass)
                lastBlend = blend;
        }
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

    foreach (SimpleFileBlock block, simpleFile.blocks) {
        SimpleFileKeyValue kv;
        if (block.name == QLatin1String("rule")) {
            foreach (kv, block.values) {
                if (kv.name != QLatin1String("bitmap") &&
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
                if (!MapComposite::levelForLayer(layer)) {
                    mError = tr("Line %1: Invalid rule layer '%2'")
                            .arg(kv.lineNumber).arg(kv.value);
                    return false;
                }
            } else
                goto missingKV;

            if (block.keyValue("condition", kv)) {
                QRgb condition = rgbFromString(kv.value, ok);
                if (!ok) {
                    mError = tr("Line %1: Invalid rule condition '%2'")
                            .arg(kv.lineNumber).arg(kv.value);
                    return false;
                }
                AddRule(bitmapIndex, col, choices, layer, condition);
            } else
                AddRule(bitmapIndex, col, choices, layer);
        } else if (block.name == QLatin1String("alias")) {
            QString name;
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
                foreach (QString tileName, kv.values()) {
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

    foreach (BmpRule *rule, mRules) {
        SimpleFileBlock ruleBlock;
        ruleBlock.name = QLatin1String("rule");
        ruleBlock.addValue("bitmap", QString::number(rule->bitmapIndex));
        ruleBlock.addValue("color", QString::fromLatin1("%1 %2 %3")
                           .arg(qRed(rule->color))
                           .arg(qGreen(rule->color))
                           .arg(qBlue(rule->color)));

        QStringList tiles;
        foreach (QString tileName, rule->tileChoices) {
            QString tilesetName;
            int tileID;
            if (tileName.isEmpty())
                tileName = QLatin1String("null");
            else if (BuildingEditor::BuildingTilesMgr::parseTileName(
                         tileName, tilesetName, tileID)) {
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

void BmpRulesFile::AddRule(int bitmapIndex, QRgb col, QStringList tiles,
                         QString layer, QRgb condition)
{
    QStringList normalizedTileNames;
    foreach (QString tileName, tiles) {
        if (mAliasByName.contains(tileName))
            normalizedTileNames += tileName;
        else
            normalizedTileNames += BuildingEditor::BuildingTilesMgr::normalizeTileName(tileName);
    }

    mRules += new BmpRule(bitmapIndex, col, normalizedTileNames, layer, condition);
}

void BmpRulesFile::AddRule(int bitmapIndex, QRgb col, QStringList tiles,
                         QString layer)
{
    QStringList normalizedTileNames;
    foreach (QString tileName, tiles) {
        if (mAliasByName.contains(tileName))
            normalizedTileNames += tileName;
        else
            normalizedTileNames += BuildingEditor::BuildingTilesMgr::normalizeTileName(tileName);
    }

    mRules += new BmpRule(bitmapIndex, col, normalizedTileNames, layer);
}

QRgb BmpRulesFile::rgbFromString(const QString &string, bool &ok)
{
    QStringList rgb = string.split(QLatin1Char(' '), QString::SkipEmptyParts);
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
        bool hasCon = false;
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
            hasCon = true;
        }

        if (hasCon) {
            AddRule(bmpIndex, col, choices, layer, con);
        } else {
            AddRule(bmpIndex, col, choices, layer);
        }
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
    foreach (BmpAlias *alias, aliases)
        aliasToName[alias->name] = alias;

    foreach (SimpleFileBlock block, simpleFile.blocks) {
        SimpleFileKeyValue kv;
        if (block.name == QLatin1String("blend")) {
            foreach (kv, block.values) {
                if (kv.name != QLatin1String("layer") &&
                        kv.name != QLatin1String("mainTile") &&
                        kv.name != QLatin1String("blendTile") &&
                        kv.name != QLatin1String("dir") &&
                        kv.name != QLatin1String("exclude")) {
                    mError = tr("Unknown blend attribute '%1'").arg(kv.name);
                    return false;
                }
            }

            QString mainTile;
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

            BmpBlend *blend = new BmpBlend(block.value("layer"), mainTile,
                                           blendTile, dir, excludes);
            mBlends += blend;
        } else {
            mError = tr("Unknown block name '%1'.\nProbable syntax error in Blends.txt.").arg(block.name);
            return false;
        }
    }

    return true;
}

QList<BmpBlend *> BmpBlendsFile::blendsCopy() const
{
    QList<BmpBlend *> ret;
    foreach (BmpBlend *blend, mBlends)
        ret += new BmpBlend(blend);
    return ret;
}

/////

