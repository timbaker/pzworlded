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

#include "bmptotmx.h"

#include "bmpblender.h"
#include "bmptotmxconfirmdialog.h"
#include "mainwindow.h"
#include "mapmanager.h"
#include "preferences.h"
#include "progress.h"
#include "simplefile.h"
#include "tilemetainfomgr.h"
#include "undoredo.h"
#include "worldcell.h"
#include "worlddocument.h"
#include "world.h"

#include "map.h"
#include "mapwriter.h"
#include "objectgroup.h"
#include "tile.h"
#include "tilelayer.h"
#include "tileset.h"

#include <QCoreApplication>
#include <QDebug>
#include <QtXml/QDomDocument>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QMessageBox>
#include <QUndoStack>
#include <QStringList>
#include <QXmlStreamWriter>

using namespace Tiled;

BMPToTMX *BMPToTMX::mInstance = 0;

BMPToTMX *BMPToTMX::instance()
{
    if (!mInstance)
        mInstance = new BMPToTMX();
    return mInstance;
}

void BMPToTMX::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

BMPToTMX::BMPToTMX(QObject *parent)
    : QObject(parent)
{
}

BMPToTMX::~BMPToTMX()
{
    qDeleteAll(mRules);
    qDeleteAll(mBlends);
}

bool BMPToTMX::generateWorld(WorldDocument *worldDoc, BMPToTMX::GenerateMode mode)
{
    mWorldDoc = worldDoc;
    World *world = mWorldDoc->world();

    QString tilesDirectory = TileMetaInfoMgr::instance()->tilesDirectory();
    if (tilesDirectory.isEmpty() || !QFileInfo(tilesDirectory).exists()) {
        mError = tr("The Tiles Directory could not be found.  Please set it in the Preferences.");
        return false;
    }
#if 0
    if (!TileMetaInfoMgr::instance()->readTxt()) {
        mError = tr("%1\n(while reading %2)")
                .arg(TileMetaInfoMgr::instance()->errorString())
                .arg(TileMetaInfoMgr::instance()->txtName());
        return false;
    }
#endif
    // At this point, TileMetaInfoMgr's tilesets are all marked missing and
    // have imageSource() paths relative to the Tiles Directory.  This call
    // will actually read in the tilesets and set the imageSource() paths.
    // FIXME: I don't need to load the tileset images, I just need the imageSource() paths.
    TileMetaInfoMgr::instance()->loadTilesets();

    // Figure out which files will be overwritten and give the user a chance to
    // cancel.
    QStringList fileNames;
    int bmpIndex;
    if (mode == GenerateSelected) {
        foreach (WorldCell *cell, mWorldDoc->selectedCells())
            if (shouldGenerateCell(cell, bmpIndex)) {
                QString fileName = tmxNameForCell(cell, world->bmps().at(bmpIndex));
                if (QFileInfo(fileName).exists()) {
                    Q_ASSERT(!fileNames.contains(fileName));
                    fileNames += fileName;
                }
            }
    } else {
        for (int y = 0; y < world->height(); y++) {
            for (int x = 0; x < world->width(); x++) {
                WorldCell *cell = world->cellAt(x, y);
                if (shouldGenerateCell(cell, bmpIndex)) {
                    QString fileName = tmxNameForCell(cell, world->bmps().at(bmpIndex));
                    if (QFileInfo(fileName).exists()) {
                        Q_ASSERT(!fileNames.contains(fileName));
                        fileNames += fileName;
                    }
                }
            }
        }
    }
    if (!fileNames.isEmpty()) {
        BMPToTMXConfirmDialog dialog(fileNames, MainWindow::instance());
        if (dialog.exec() != QDialog::Accepted)
            return true;
    }

    if (!LoadBaseXML()) {
        mError += tr("\n(while reading MapBaseXML.txt)");
        return false;
    }
    if (!LoadRules()) {
        mError += tr("\n(while reading Rules.txt)");
        return false;
    }
    if (!LoadBlends()) {
        mError += tr("\n(while reading Blends.txt)");
        return false;
    }

    PROGRESS progress(QLatin1String("Reading BMP images"));

    qDeleteAll(mImages);
    mImages.clear();
    foreach (WorldBMP *bmp, world->bmps()) {
        BMPToTMXImages *images = getImages(bmp->filePath(), bmp->pos());
        if (!images) {
            return false;
        }
        mImages += images;
    }

    progress.update(QLatin1String("Generating TMX files"));

    mUnknownColors.clear();
    mUnknownVegColors.clear();
    mNewFiles.clear();

    if (mode == GenerateSelected) {
        foreach (WorldCell *cell, worldDoc->selectedCells())
            if (!generateCell(cell))
                return false;
    } else {
        for (int y = 0; y < world->height(); y++) {
            for (int x = 0; x < world->width(); x++) {
                if (!generateCell(world->cellAt(x, y)))
                    return false;
            }
        }
    }

    reportUnknownColors();

    if (world->getBMPToTMXSettings().assignMapsToWorld)
        assignMapsToCells(worldDoc, mode);

    foreach (QString path, mNewFiles)
        MapManager::instance()->newMapFileCreated(path);

    // While displaying this, the MapManager's FileSystemWatcher might see some
    // changed .tmx files, which results in the PROGRESS dialog being displayed.
    // It's a bit odd to see the PROGRESS dialog blocked behind this messagebox.
    QMessageBox::information(MainWindow::instance(),
                             tr("BMP To TMX"), tr("Finished!"));

    return true;
}

bool BMPToTMX::generateCell(WorldCell *cell)
{
    int bmpIndex;
    if (!shouldGenerateCell(cell, bmpIndex))
        return true;

    BMPToTMXImages *images = mImages[bmpIndex];

    QImage bmp = images->mBmp;
    QImage bmpVeg = images->mBmpVeg;

    PROGRESS progress(tr("Generating TMX files (%1,%2)")
                      .arg(cell->x()).arg(cell->y()));

    // 300x300x(2+4)
    Entries.clear();
    Entries.resize(300);
    for (int x = 0; x < 300; x++) {
        Entries[x].resize(300);
        for (int y = 0; y < 300; y++)
            Entries[x][y].resize(2+mBlendLayers.size());
    }

    int sx = cell->x() * 300;
    int sy = cell->y() * 300;
    for (int y = sy; y < sy + 300; y++) {
        for (int x = sx; x < sx + 300; x++) {
            QRgb col = bmp.pixel(x - images->mBounds.x() * 300, y - images->mBounds.y() * 300);
            QRgb col2 = bmpVeg.pixel(x - images->mBounds.x() * 300, y - images->mBounds.y() * 300);

            if (mRulesByColor.contains(col)) {
                QList<BmpRule*> rules = mRulesByColor[col];

                foreach (BmpRule *rule, rules) {
                    if (rule->bitmapIndex != 0)
                        continue;
                    int index = 0;
                    if (rule->targetLayer == QLatin1String("0_Floor"))
                        index = 0;
                    else if (rule->targetLayer == QLatin1String("0_Vegetation"))
                        index = 1;
                    else
                        continue;

                    QString tileName = rule->tileChoices[qrand() % rule->tileChoices.count()];
                    tileName = resolveAlias(tileName, qrand());
                    Entries[x - sx][y - sy][index] = tileName;
                }
            } else {
                if (!mUnknownColors[images->mPath].contains(col)) {
                    mUnknownColors[images->mPath][col].rgb = col;
                    mUnknownColors[images->mPath][col].xy = QPoint(x, y);
                }
            }

            if (col2 != qRgb(0, 0, 0) && mRulesByColor.contains(col2)) {
                QList<BmpRule*> rules = mRulesByColor[col2];

                foreach (BmpRule *rule, rules) {
                    if (rule->bitmapIndex != 1)
                        continue;
                    if (rule->condition != col && rule->condition != qRgb(0, 0, 0))
                        continue;
                    int index = 0;
                    if (rule->targetLayer == QLatin1String("0_Floor"))
                        index = 0;
                    else if (rule->targetLayer == QLatin1String("0_Vegetation"))
                        index = 1;
                    else
                        continue;

                    QString tileName = rule->tileChoices[qrand() % rule->tileChoices.count()];
                    tileName = resolveAlias(tileName, qrand());
                    Entries[x - sx][y - sy][index] = tileName;
                }
            } else {
                if (col2 != qRgb(0, 0, 0) && !mUnknownVegColors[images->mPath].contains(col2)) {
                    mUnknownVegColors[images->mPath][col2].rgb = col2;
                    mUnknownVegColors[images->mPath][col2].xy = QPoint(x, y);
                }
            }
        }
    }

    BlendMap();

    return WriteMap(cell, bmpIndex);
}

QStringList BMPToTMX::supportedImageFormats()
{
    QStringList ret;
    foreach (QByteArray format, QImageReader::supportedImageFormats())
        ret += QString::fromAscii(format);
    return ret;
}

BMPToTMXImages *BMPToTMX::getImages(const QString &path, const QPoint &origin)
{
    QFileInfo info(path);
    if (!info.exists()) {
        mError = tr("The image file can't be found.\n%1").arg(path);
        return 0;
    }

    QFileInfo infoVeg(info.absolutePath() + QLatin1Char('/')
                      + info.completeBaseName() + QLatin1String("_veg.") + info.suffix());
    if (!infoVeg.exists()) {
        mError = tr("The image_veg file can't be found.\n%1").arg(path);
        return 0;
    }

    QImage image = loadImage(info.canonicalFilePath());
    if (image.isNull()) {
        return 0;
    }

    QImage imageVeg = loadImage(infoVeg.canonicalFilePath(),
                                QLatin1String("_veg"));
    if (imageVeg.isNull()) {
        return 0;
    }

    if (image.size() != imageVeg.size()) {
        mError = tr("The images aren't the same size.\n%1\n%2")
                .arg(info.canonicalFilePath())
                .arg(infoVeg.canonicalFilePath());
        return 0;
    }

    BMPToTMXImages *images = new BMPToTMXImages;
    images->mBmp = image;
    images->mBmpVeg = imageVeg;
    images->mPath = info.canonicalFilePath();
    images->mBounds = QRect(origin, QSize(image.width() / 300,
                                          image.height() / 300));
    return images;
}

QSize BMPToTMX::validateImages(const QString &path)
{
    QFileInfo info(path);
    if (!info.exists()) {
        mError = tr("The image file can't be found.\n%1").arg(path);
        return QSize();
    }

    QFileInfo infoVeg(info.absolutePath() + QLatin1Char('/')
                      + info.completeBaseName() + QLatin1String("_veg.") + info.suffix());
    if (!infoVeg.exists()) {
        mError = tr("The image_veg file can't be found.\n%1").arg(path);
        return QSize();
    }

    QImageReader image(info.canonicalFilePath());
    if (image.size().isEmpty()) {
        return QSize();
    }

    QImageReader imageVeg(infoVeg.canonicalFilePath());
    if (imageVeg.size().isEmpty()) {
        return QSize();
    }

    if (image.size() != imageVeg.size()) {
        mError = tr("The images aren't the same size.\n%1\n%2")
                .arg(info.canonicalFilePath())
                .arg(infoVeg.canonicalFilePath());
        return QSize();
    }

    return image.size();
}

void BMPToTMX::assignMapsToCells(WorldDocument *worldDoc, BMPToTMX::GenerateMode mode)
{
    mWorldDoc = worldDoc;

    mWorldDoc->undoStack()->beginMacro(tr("Assign Maps to Cells"));

    if (mode == GenerateSelected) {
        foreach (WorldCell *cell, worldDoc->selectedCells())
            assignMapToCell(cell);
    } else {
        World *world = worldDoc->world();
        for (int y = 0; y < world->height(); y++) {
            for (int x = 0; x < world->width(); x++) {
                assignMapToCell(world->cellAt(x, y));
            }
        }
    }

    mWorldDoc->undoStack()->endMacro();
}

QString BMPToTMX::defaultRulesFile() const
{
    return QCoreApplication::applicationDirPath() + QLatin1Char('/')
            + QLatin1String("Rules.txt");
}

QString BMPToTMX::defaultBlendsFile() const
{
    return QCoreApplication::applicationDirPath() + QLatin1Char('/')
            + QLatin1String("Blends.txt");
}

QString BMPToTMX::defaultMapBaseXMLFile() const
{
    return QCoreApplication::applicationDirPath() + QLatin1Char('/')
            + QLatin1String("MapBaseXML.txt");
}

bool BMPToTMX::shouldGenerateCell(WorldCell *cell, int &bmpIndex)
{
    // Get the top-most BMP covering the cell
    int n = 0;
    bmpIndex = -1;
    foreach (WorldBMP *bmp, cell->world()->bmps()) {
        if (bmp->bounds().contains(cell->pos()))
            bmpIndex = n;
        n++;
    }
    if (bmpIndex == -1)
        return false;

    return true;
}

void BMPToTMX::assignMapToCell(WorldCell *cell)
{
    // Get the top-most BMP covering the cell
    WorldBMP *bmp = 0;
    foreach (WorldBMP *bmp2, cell->world()->bmps()) {
        if (bmp2->bounds().contains(cell->pos()))
            bmp = bmp2;
    }
    if (bmp == 0)
        return;

#if 1
    QString fileName = tmxNameForCell(cell, bmp);
    if (cell->mapFilePath() != fileName)
        mWorldDoc->undoStack()->push(new SetCellMainMap(mWorldDoc, cell, fileName));
#else
    // QFileInfo::operator!= will fail if the files don't exist because it
    // uses canonicalFilePath() comparison
    QFileInfo infoCurrent(cell->mapFilePath());
    QFileInfo infoDesired(tmxNameForCell(cell, bmp));
    if (infoCurrent != infoDesired) {
        mWorldDoc->setCellMapName(cell, infoDesired.absoluteFilePath());
    }
#endif
}

QString BMPToTMX::tmxNameForCell(WorldCell *cell, WorldBMP *bmp)
{
    QString exportDir = mWorldDoc->world()->getBMPToTMXSettings().exportDir;
    QString prefix = QFileInfo(bmp->filePath()).completeBaseName();
    QString filePath = exportDir + QLatin1Char('/')
            + tr("%1_%2_%3.tmx").arg(prefix).arg(cell->x()).arg(cell->y());
    return filePath;
}

void BMPToTMX::reportUnknownColors()
{
    if (!mWorldDoc->world()->getBMPToTMXSettings().warnUnknownColors)
        return;

    foreach (BMPToTMXImages *images, mImages) {
        QMap<QRgb,UnknownColor> &map = mUnknownColors[images->mPath];
        if (map.size()) {
            QString msg = tr("Some unknown colors were found in %1:\n")
                    .arg(QFileInfo(images->mPath).fileName());
            int i = 0;
            foreach (QRgb rgb, map.keys()) {
                msg += tr("RGB=%1,%2,%3 at x,y=%4,%5\n")
                        .arg(qRed(rgb)).arg(qGreen(rgb)).arg(qBlue(rgb))
                        .arg(map[rgb].xy.x())
                        .arg(map[rgb].xy.y());
                if (++i == 5) {
                    if (map.size() > i)
                        msg += tr("...plus %1 more").arg(map.size() - i);
                    break;
                }
            }
            QMessageBox::warning(MainWindow::instance(), tr("Unknown colors used"),
                                 msg);
        }
        QMap<QRgb,UnknownColor> &mapVeg = mUnknownVegColors[images->mPath];
        if (mapVeg.size()) {
            QString suffix = QFileInfo(images->mPath).suffix();
            QString msg = tr("Some unknown colors were found in %1:\n")
                    .arg(QFileInfo(images->mPath).completeBaseName()
                         + QLatin1String("_veg.") + suffix);
            int i = 0;
            foreach (QRgb rgb, mapVeg.keys()) {
                msg += tr("RGB=%1,%2,%3 at x,y=%4,%5\n")
                        .arg(qRed(rgb)).arg(qGreen(rgb)).arg(qBlue(rgb))
                        .arg(mapVeg[rgb].xy.x())
                        .arg(mapVeg[rgb].xy.y());
                if (++i == 5) {
                    if (mapVeg.size() > i)
                        msg += tr("...plus %1 more").arg(mapVeg.size() - i);
                    break;
                }
            }
            QMessageBox::warning(MainWindow::instance(), tr("Unknown colors used"),
                                 msg);
            if (i == 5)
                break;
        }
    }
}

QImage BMPToTMX::loadImage(const QString &path, const QString &suffix)
{
    QImage image;
    if (!image.load(path)) {
        mError = tr("The image%1 file couldn't be loaded.").arg(suffix);
        return QImage();
    }

    if (image.width() % 300 || image.height() % 300) {
        mError = tr("The image%1 size isn't divisible by 300.").arg(suffix);
        return QImage();
    }

    // This is the fastest format for QImage::pixel() and QImage::setPixel().
    if (image.format() != QImage::Format_ARGB32)
        image = image.convertToFormat(QImage::Format_ARGB32);

    return image;
}

bool BMPToTMX::LoadBaseXML()
{
    QString path = mWorldDoc->world()->getBMPToTMXSettings().mapbaseFile;
    if (path.isEmpty())
        path = defaultMapBaseXMLFile();
    SimpleFile simple;
    if (!simple.read(path)) {
        mError = simple.errorString();
        return false;
    }

    mLayers.clear();

    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("layers")) {
            foreach (SimpleFileKeyValue kv, block.values) {
                if (kv.name == QLatin1String("tile")) {
                    mLayers += LayerInfo(kv.value, LayerInfo::Tile);
                } else if (kv.name == QLatin1String("object")) {
                    mLayers += LayerInfo(kv.value, LayerInfo::Object);
                } else {
                    mError = tr("Unknown layer type '%1'.\n%2")
                            .arg(kv.name)
                            .arg(path);
                    return false;
                }
            }
        } else {
            mError = tr("Unknown block name '%1'.\n%2")
                    .arg(block.name)
                    .arg(path);
            return false;
        }
    }

    if (!mLayers.size()) {
        mError = tr("Failed to read any layers from MapBaseXML.txt");
        return false;
    }

    return true;
}

bool BMPToTMX::LoadRules()
{
    QString path = mWorldDoc->world()->getBMPToTMXSettings().rulesFile;
    if (path.isEmpty())
        path = defaultRulesFile();

    Tiled::Internal::BmpRulesFile file;
    if (!file.read(path)) {
        mError = file.errorString();
        return false;
    }

    mRuleFileName = path;

    qDeleteAll(mRules);
    mRules.clear();
    mRulesByColor.clear();
    qDeleteAll(mAliases);
    mAliases = file.aliasesCopy();
    foreach (BmpAlias *alias, mAliases)
        mAliasByName[alias->name] = alias;
    foreach (BmpRule *rule, file.rules())
        AddRule(rule->bitmapIndex, rule->color, rule->tileChoices,
                rule->targetLayer, rule->condition);

    // Verify all the listed tiles exist.
    foreach (QList<BmpRule*> convs, mRulesByColor) {
        foreach (BmpRule *conv, convs) {
            foreach (QString tileName, conv->tileChoices) {
                if (mAliasByName.contains(tileName))
                    continue;
                if (!tileName.isEmpty() && getTileFromTileName(tileName) == 0) {
                    mError = tr("A tile listed in Rules.txt could not be found.\n");
                    mError += tr("The missing tile is called '%1'.\n\n").arg(tileName);
                    mError += tr("Please fix the invalid tile index or add the tileset\nif it is missing using the Tilesets dialog in TileZed.\n");
                    return false;
                }
            }
        }
    }

    return true;
}

bool BMPToTMX::LoadBlends()
{
    qDeleteAll(mBlends);
    mBlends.clear();
    mBlendsByLayer.clear();

    QString path = mWorldDoc->world()->getBMPToTMXSettings().blendsFile;
    if (path.isEmpty())
        path = defaultBlendsFile();

    Tiled::Internal::BmpBlendsFile file;
    if (!file.read(path, mAliases)) {
        mError = file.errorString();
        return false;
    }

    mBlendFileName = path;

    foreach (BmpBlend *blend, file.blends()) {
        mBlends += new BmpBlend(blend->targetLayer, blend->mainTile, blend->blendTile,
                                blend->dir, blend->ExclusionList);
        mBlendsByLayer[blend->targetLayer] += mBlends.last();
        QStringList excludes;
        foreach (QString tileName, blend->ExclusionList) {
            if (mAliasByName.contains(tileName))
                excludes += mAliasByName[tileName]->tiles;
            else
                excludes += tileName;
        }
        mBlendExcludes[blend] = excludes;
    }

    QSet<QString> layers;
    foreach (BmpBlend *blend, mBlends)
        layers.insert(blend->targetLayer);
    mBlendLayers = layers.values();

    return true;
}

void BMPToTMX::AddRule(int bitmapIndex, QRgb col, QStringList tiles, QString layer, QRgb condition)
{
    mRules += new BmpRule(bitmapIndex, col, tiles, layer, condition);
    mRulesByColor[col] += mRules.last();
}

void BMPToTMX::AddRule(int bitmapIndex, QRgb col, QStringList tiles, QString layer)
{
    mRules += new BmpRule(bitmapIndex, col, tiles, layer);
    mRulesByColor[col] += mRules.last();
}

bool BMPToTMX::BlendMap()
{
    for (int y = 0; y < 300; y++)
        for (int x = 0; x < 300; x++) {
            QString tile = Entries[x][y][0]; // 0_Floor

            for (int i = 0; i < mBlendLayers.size(); i++) {
                BmpBlend *blend = getBlendRule(x, y, tile, mBlendLayers[i]);
                if (blend != 0)
                    Entries[x][y][2+i] = blend->blendTile;
            }
        }

    return true;
}

QString BMPToTMX::getNeighbouringTile(int x, int y)
{
    if (x < 0 || y < 0 || x >= 300 || y >= 300)
        return QString();
    return Entries[x][y][0]; // 0_Floor
}

BmpBlend *BMPToTMX::getBlendRule(int x, int y, const QString &tileName, const QString &layer)
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
            mainTiles = mAliasByName[blend->mainTile]->tiles;

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

QString BMPToTMX::resolveAlias(const QString &tileName, int randForPos) const
{
    if (mAliasByName.contains(tileName)) {
        const QStringList &tiles = mAliasByName[tileName]->tiles;
        return tiles[randForPos % tiles.size()];
    }
    return tileName;
}

bool BMPToTMX::WriteMap(WorldCell *cell, int bmpIndex)
{
    Map map(Map::Isometric, 300, 300, 64, 32); // FIXME: or LevelIsometric
    foreach (Tiled::Tileset *ts, TileMetaInfoMgr::instance()->tilesets()) {
        map.addTileset(ts);
    }

    foreach (LayerInfo layer, mLayers) {
        if (layer.mType == LayerInfo::Tile) {
            TileLayer *tl = new TileLayer(layer.mName, 0, 0,
                                          map.width(), map.height());
            map.addLayer(tl);
            int index = -1;
            if (layer.mName == QLatin1String("0_Floor"))
                index = 0;
            else if (layer.mName == QLatin1String("0_Vegetation"))
                index = 1;
            else if (mBlendLayers.contains(layer.mName))
                index = 2 + mBlendLayers.indexOf(layer.mName);
            else
                continue;
            for (int y = 0; y < map.width(); y++) {
                for (int x = 0; x < map.height(); x++) {
                    QString tileName = Entries[x][y][index];
                    if (Tile *tile = getTileFromTileName(tileName))
                        tl->setCell(x, y, Cell(tile));
                }
            }
        } else if (layer.mType == LayerInfo::Object) {
            ObjectGroup *og = new ObjectGroup(layer.mName, 0, 0,
                                              map.width(), map.height());
            map.addLayer(og);
        }
    }

    map.rbmpSettings()->setBlendsFile(mBlendFileName);
    map.rbmpSettings()->setRulesFile(mRuleFileName);
    QList<BmpAlias*> aliases;
    foreach (BmpAlias *alias, mAliases)
        aliases += new BmpAlias(alias);
    map.rbmpSettings()->setAliases(aliases);
    QList<BmpBlend*> blends;
    foreach (BmpBlend *blend, mBlends)
        blends += new BmpBlend(blend);
    map.rbmpSettings()->setBlends(blends);
    QList<BmpRule*> rules;
    foreach (BmpRule *rule, mRules)
        rules += new BmpRule(rule);
    map.rbmpSettings()->setRules(rules);

    QString filePath = tmxNameForCell(cell, cell->world()->bmps().at(bmpIndex));
    if (!QFileInfo(filePath).exists())
        mNewFiles += filePath;

    MapWriter writer;
    MapWriter::LayerDataFormat format = MapWriter::CSV;
    if (mWorldDoc->world()->getBMPToTMXSettings().compress)
        format = MapWriter::Base64Zlib;
    writer.setLayerDataFormat(format);
    writer.setDtdEnabled(false);
    if (!writer.writeMap(&map, filePath)) {
        mError = writer.errorString();
        return false;
    }
    return true;
}

Tile *BMPToTMX::getTileFromTileName(const QString &tileName)
{
    QString tilesetName = tileName.mid(0, tileName.lastIndexOf(QLatin1Char('_')));
    int index = tileName.mid(tileName.lastIndexOf(QLatin1Char('_')) + 1).toInt();
    if (Tileset *ts = TileMetaInfoMgr::instance()->tileset(tilesetName))
        return ts->tileAt(index);
    return 0;
}
