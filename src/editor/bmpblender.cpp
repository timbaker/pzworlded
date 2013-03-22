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

#include "BuildingEditor/buildingfloor.h"
#include "simplefile.h"
#include "BuildingEditor/buildingtiles.h"

#include "map.h"
#include "tilelayer.h"
#include "tileset.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QSet>
#include <QTextStream>

using namespace Tiled;
using namespace Tiled::Internal;

BmpBlender::BmpBlender(Map *map) :
    mMap(map)
{
}

BmpBlender::~BmpBlender()
{
    qDeleteAll(mRules);
    qDeleteAll(mTileNameGrids);
}

bool BmpBlender::read()
{
#ifdef QT_NO_DEBUG
#ifdef WORLDED
    QString fileName = QApplication::applicationDirPath() + QLatin1String("/Rules.txt");
#else
    QString fileName = QApplication::applicationDirPath() + QLatin1String("/WorldEd/Rules.txt");
#endif
#else
    QString fileName = QLatin1String("C:/Programming/Tiled/PZWorldEd/PZWorldEd/Rules.txt"); // FIXME
#endif
    if (!readRules(fileName))
        return false;

#ifdef QT_NO_DEBUG
#ifdef WORLDED
    fileName = QApplication::applicationDirPath() + QLatin1String("/Blends.txt");
#else
    fileName = QApplication::applicationDirPath() + QLatin1String("/WorldEd/Blends.txt");
#endif
#else
    fileName = QLatin1String("C:/Programming/Tiled/PZWorldEd/PZWorldEd/Blends.txt"); // FIXME
#endif
    if (!readBlends(fileName))
        return false;

    return true;
}

bool BmpBlender::readRules(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        mError = tr("%1\n(while reading %2)").arg(file.errorString())
                .arg(QDir::toNativeSeparators(filePath));
        return false;
    }

    qDeleteAll(mRules);
    mRules.clear();
    mRuleByColor.clear();
    mRuleLayers.clear();

    QTextStream sr(&file);
    while (!sr.atEnd()) {
        QString line = sr.readLine();

        if (line.contains(QLatin1Char('#')))
            continue;
        if (line.trimmed().isEmpty())
            continue;

        QStringList lineSplit = line.split(QLatin1Char(','));
        int bmp = lineSplit[0].trimmed().toInt();
        QRgb col = qRgb(lineSplit[1].trimmed().toInt(),
                        lineSplit[2].trimmed().toInt(),
                        lineSplit[3].trimmed().toInt());
        QStringList choices = lineSplit[4].split(QLatin1Char(' '));
        int n = 0;
        foreach (QString choice, choices) {
            choices[n] = choice.trimmed();
            if (choices[n] == QLatin1String("null"))
                choices[n].clear();
            n++;
        }
        QRgb con = qRgb(0, 0, 0);

        QString layer = lineSplit[5].trimmed();
        bool hasCon = false;
        if (lineSplit.length() > 6) {
            con = qRgb(lineSplit[6].trimmed().toInt(),
                       lineSplit[7].trimmed().toInt(),
                       lineSplit[8].trimmed().toInt());
            hasCon = true;
        }

        if (hasCon) {
            AddRule(bmp, col, choices, layer, con);
        } else {
            AddRule(bmp, col, choices, layer);
        }
    }

    return true;
}

bool BmpBlender::readBlends(const QString &filePath)
{
    mBlendList.clear();
    mBlendsByLayer.clear();

    SimpleFile simpleFile;
    if (!simpleFile.read(filePath)) {
        mError = tr("%1\nwhile reading %2")
                .arg(simpleFile.errorString())
                .arg(filePath);
        return false;
    }

    QMap<QString,Blend::Direction> dirMap;
    dirMap[QLatin1String("n")] = Blend::N;
    dirMap[QLatin1String("s")] = Blend::S;
    dirMap[QLatin1String("e")] = Blend::E;
    dirMap[QLatin1String("w")] = Blend::W;
    dirMap[QLatin1String("nw")] = Blend::NW;
    dirMap[QLatin1String("sw")] = Blend::SW;
    dirMap[QLatin1String("ne")] = Blend::NE;
    dirMap[QLatin1String("se")] = Blend::SE;

    foreach (SimpleFileBlock block, simpleFile.blocks) {
        if (block.name == QLatin1String("blend")) {
            foreach (SimpleFileKeyValue kv, block.values) {
                if (kv.name != QLatin1String("layer") &&
                        kv.name != QLatin1String("mainTile") &&
                        kv.name != QLatin1String("blendTile") &&
                        kv.name != QLatin1String("dir") &&
                        kv.name != QLatin1String("exclude")) {
                    mError = tr("Unknown blend attribute '%1'").arg(kv.name);
                    return false;
                }
            }

            Blend::Direction dir = Blend::Unknown;
            QString dirName = block.value("dir");
            if (dirMap.contains(dirName))
                dir = dirMap[dirName];
            else {
                mError = tr("Unknown blend direction '%1'").arg(dirName);
                return false;
            }

            QStringList excludes;
            foreach (QString exclude, block.value("exclude").split(QLatin1String(" "), QString::SkipEmptyParts))
                excludes += BuildingEditor::BuildingTilesMgr::normalizeTileName(exclude);

            Blend blend(block.value("layer"),
                        BuildingEditor::BuildingTilesMgr::normalizeTileName(block.value("mainTile")),
                        BuildingEditor::BuildingTilesMgr::normalizeTileName(block.value("blendTile")),
                        dir, excludes);
            mBlendList += blend;
            mBlendsByLayer[blend.targetLayer] += mBlendList.count() - 1;
        } else {
            mError = tr("Unknown block name '%1'.\nProbable syntax error in Blends.txt.").arg(block.name);
            return false;
        }
    }

    QSet<QString> layers;
    foreach (Blend blend, mBlendList)
        layers.insert(blend.targetLayer);
    mBlendLayers = layers.values();

    return true;
}

void BmpBlender::recreate()
{
    qDeleteAll(mTileNameGrids);
    mTileNameGrids.clear();

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
    }

    const QRgb black = qRgb(0, 0, 0);

    // Hack - If a pixel is black, and the user-drawn map tile in 0_Floor is
    // one of the Rules.txt tiles, pretend that that pixel exists in the image.
    x1 -= 1, x2 += 1, y1 -= 1, y2 += 1;
    int index = mMap->indexOfLayer(QLatin1String("0_Floor"), Layer::TileLayerType);
    TileLayer *tl = (index == -1) ? 0 : mMap->layerAt(index)->asTileLayer();
    QList<Rule*> floor0Rules;
    foreach (Rule *rule, mRules) {
        if (rule->targetLayer == QLatin1String("0_Floor") &&
                rule->bitmapIndex == 0)
            floor0Rules += rule;
    }

    x1 = qBound(0, x1, mMap->width() - 1);
    x2 = qBound(0, x2, mMap->width() - 1);
    y1 = qBound(0, y1, mMap->height() - 1);
    y2 = qBound(0, y2, mMap->height() - 1);

    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            foreach (QString layerName, mRuleLayers)
                mTileNameGrids[layerName]->replace(x, y, QString());

            QRgb col = mMap->rbmpMain().pixel(x, y);
            QRgb col2 = mMap->rbmpVeg().pixel(x, y);

            // Hack - If a pixel is black, and the user-drawn map tile in 0_Floor is
            // one of the Rules.txt tiles, pretend that that pixel exists in the image.
            if (tl && col == black && adjacentToNonBlack(mMap->rbmpMain().rimage(),
                                                         mMap->rbmpVeg().rimage(),
                                                         x, y)) {
                if (Tile *tile = tl->cellAt(x, y).tile) {
                    QString tileName = BuildingEditor::BuildingTilesMgr::nameForTile(tile);
                    foreach (Rule *rule, floor0Rules) {
                        if (rule->tileChoices.contains(tileName))
                            col = rule->color;
                    }
                }
            }

            if (mRuleByColor.contains(col)) {
                QList<Rule*> rules = mRuleByColor[col];

                foreach (Rule *rule, rules) {
                    if (rule->bitmapIndex != 0)
                        continue;
                    if (!mTileNameGrids.contains(rule->targetLayer))
                        continue;
                    QString tileName = rule->tileChoices[mMap->bmp(0).rand(x, y) % rule->tileChoices.count()];
                    mTileNameGrids[rule->targetLayer]->replace(x, y, tileName);
                }
            }

            if (col2 != black && mRuleByColor.contains(col2)) {
                QList<Rule*> rules = mRuleByColor[col2];

                foreach (Rule *rule, rules) {
                    if (rule->bitmapIndex != 1)
                        continue;
                    if (rule->condition != col && rule->condition != black)
                        continue;
                    if (!mTileNameGrids.contains(rule->targetLayer))
                        continue;
                    QString tileName = rule->tileChoices[mMap->bmp(1).rand(x, y) % rule->tileChoices.count()];
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
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            QString tileName = grid->at(x, y);
            foreach (QString layerName, mBlendLayers) {
                Blend blend = getBlendRule(x, y, tileName, layerName);
                mTileNameGrids[layerName]->replace(x, y, blend.isNull() ? QString() : blend.blendTile);
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

    QMap<QString,Tileset*> tilesets;
    foreach (Tileset *ts, mMap->tilesets())
        tilesets[ts->name()] = ts;

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
                QString tilesetName;
                int tileID;
                if (BuildingEditor::BuildingTilesMgr::parseTileName(tileName, tilesetName, tileID)) {
                    if (tilesets.contains(tilesetName))
                        tl->setCell(x, y, Cell(tilesets[tilesetName]->tileAt(tileID)));
                }
            }
        }
    }

    if (recreated)
        emit layersRecreated();
}

QString BmpBlender::getNeighbouringTile(int x, int y)
{
    if (x < 0 || y < 0 || x >= mMap->width() || y >= mMap->height())
        return QString();
    BuildingEditor::FloorTileGrid *grid
            = mTileNameGrids[QLatin1String("0_Floor")];
    return grid->at(x, y);
}

BmpBlender::Blend BmpBlender::getBlendRule(int x, int y, const QString &tileName,
                                           const QString &layer)
{
    Blend lastBlend;
    if (tileName.isEmpty())
        return lastBlend;
    foreach (int index, mBlendsByLayer[layer]) {
        Blend &blend = mBlendList[index];
        Q_ASSERT(blend.targetLayer == layer);
        if (blend.targetLayer != layer)
            continue;

        if (tileName != blend.mainTile) {
            if (blend.ExclusionList.contains(tileName))
                continue;
            bool bPass = false;
            switch (blend.dir) {
            case Blend::N:
                bPass = getNeighbouringTile(x, y - 1) == blend.mainTile;
                break;
            case Blend::S:
                bPass = getNeighbouringTile(x, y + 1) == blend.mainTile;
                break;
            case Blend::E:
                bPass = getNeighbouringTile(x + 1, y) == blend.mainTile;
                break;
            case Blend::W:
                bPass = getNeighbouringTile(x - 1, y) == blend.mainTile;
                break;
            case Blend::NE:
                bPass = getNeighbouringTile(x, y - 1) == blend.mainTile &&
                        getNeighbouringTile(x + 1, y) == blend.mainTile;
                break;
            case Blend::SE:
                bPass = getNeighbouringTile(x, y + 1) == blend.mainTile &&
                        getNeighbouringTile(x + 1, y) == blend.mainTile;
                break;
            case Blend::NW:
                bPass = getNeighbouringTile(x, y - 1) == blend.mainTile &&
                        getNeighbouringTile(x - 1, y) == blend.mainTile;
                break;
            case Blend::SW:
                bPass = getNeighbouringTile(x, y + 1) == blend.mainTile &&
                        getNeighbouringTile(x - 1, y) == blend.mainTile;
                break;
            }

            if (bPass)
                lastBlend = blend;
        }
    }

    return lastBlend;
}

void BmpBlender::AddRule(int bitmapIndex, QRgb col, QStringList tiles,
                         QString layer, QRgb condition)
{
    QStringList normalizedTileNames;
    foreach (QString tileName, tiles)
        normalizedTileNames += BuildingEditor::BuildingTilesMgr::normalizeTileName(tileName);

    mRules += new Rule(bitmapIndex, col, normalizedTileNames, layer, condition);
    mRuleByColor[col] += mRules.last();
    if (!mRuleLayers.contains(layer))
        mRuleLayers += layer;
}

void BmpBlender::AddRule(int bitmapIndex, QRgb col, QStringList tiles,
                         QString layer)
{
    QStringList normalizedTileNames;
    foreach (QString tileName, tiles)
        normalizedTileNames += BuildingEditor::BuildingTilesMgr::normalizeTileName(tileName);

    mRules += new Rule(bitmapIndex, col, normalizedTileNames, layer);
    mRuleByColor[col] += mRules.last();
    if (!mRuleLayers.contains(layer))
        mRuleLayers += layer;
}
