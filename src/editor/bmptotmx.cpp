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
#include "unknowncolorsdialog.h"
#include "worldcell.h"
#include "worlddocument.h"
#include "world.h"

#include "map.h"
#include "mapreader.h"
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
#include <QPainter>
#include <QStringList>
#include <QUndoStack>
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

    const BMPToTMXSettings &settings = world->getBMPToTMXSettings();

    // Figure out which files will be overwritten and give the user a chance to
    // cancel.
    QStringList fileNames;
    int bmpIndex;
    if (mode == GenerateSelected) {
        foreach (WorldCell *cell, mWorldDoc->selectedCells()) {
            if (shouldGenerateCell(cell, bmpIndex)) {
                QString fileName = tmxNameForCell(cell, world->bmps().at(bmpIndex));
                if (settings.updateExisting)
                    fileName = cell->mapFilePath();
                if (QFileInfo(fileName).exists()) {
                    Q_ASSERT(!fileNames.contains(fileName));
                    fileNames += fileName;
                }
            }
        }
    } else {
        for (int y = 0; y < world->height(); y++) {
            for (int x = 0; x < world->width(); x++) {
                WorldCell *cell = world->cellAt(x, y);
                if (shouldGenerateCell(cell, bmpIndex)) {
                    QString fileName = tmxNameForCell(cell, world->bmps().at(bmpIndex));
                    if (settings.updateExisting)
                        fileName = cell->mapFilePath();
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
        if (settings.updateExisting)
            dialog.updateExisting();
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

    // Try to free up some memory before loading large images.
    MapManager::instance()->purgeUnreferencedMaps();

    PROGRESS progress(QLatin1String("Reading BMP images"));

    foreach (WorldBMP *bmp, world->bmps()) {
        BMPToTMXImages *images = getImages(bmp->filePath(), bmp->pos());
        if (!images) {
            goto errorExit;
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
                goto errorExit;
    } else {
        for (int y = 0; y < world->height(); y++) {
            for (int x = 0; x < world->width(); x++) {
                if (!generateCell(world->cellAt(x, y)))
                    goto errorExit;
            }
        }
    }

    qDeleteAll(mImages);
    mImages.clear();

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

errorExit:
    qDeleteAll(mImages);
    mImages.clear();
    return false;
}

bool BMPToTMX::generateCell(WorldCell *cell)
{
    int bmpIndex;
    if (!shouldGenerateCell(cell, bmpIndex))
        return true;

    if (mWorldDoc->world()->getBMPToTMXSettings().updateExisting) {
        PROGRESS progress(tr("Updating TMX files (%1,%2)")
                          .arg(cell->x()).arg(cell->y()));
        return UpdateMap(cell, bmpIndex);
    }

    PROGRESS progress(tr("Generating TMX files (%1,%2)")
                      .arg(cell->x()).arg(cell->y()));

    return WriteMap(cell, bmpIndex);
}

QStringList BMPToTMX::supportedImageFormats()
{
    QStringList ret;
    foreach (QByteArray format, QImageReader::supportedImageFormats())
        ret += QString::fromLatin1(format);
    return ret;
}

BMPToTMXImages *BMPToTMX::getImages(const QString &path, const QPoint &origin,
                                    QImage::Format format)
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

    QImage image = loadImage(info.canonicalFilePath(), QString(), format);
    if (image.isNull()) {
        return 0;
    }

    QImage imageVeg = loadImage(infoVeg.canonicalFilePath(),
                                QLatin1String("_veg"), format);
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
    QPoint worldOrigin = mWorldDoc->world()->getGenerateLotsSettings().worldOrigin;
    QString filePath = exportDir + QLatin1Char('/')
            + tr("%1_%2_%3.tmx").arg(prefix).arg(worldOrigin.x() + cell->x()).arg(worldOrigin.y() + cell->y());
    return filePath;
}

void BMPToTMX::reportUnknownColors()
{
    if (!mWorldDoc->world()->getBMPToTMXSettings().warnUnknownColors)
        return;

    QList<QString> unknownColors = mUnknownColors.keys();
    QList<QString> unknownVegColors = mUnknownVegColors.keys();
    QSet<QString> imagePaths = QSet<QString>(unknownColors.begin(), unknownColors.end()) +
            QSet<QString>(unknownVegColors.begin(), unknownVegColors.end());

    foreach (QString imagePath, imagePaths) {
        QMap<QRgb,UnknownColor> &map = mUnknownColors[imagePath];
        if (map.size()) {
            QStringList unknown;
            foreach (QRgb rgb, map.keys()) {
                unknown += tr("RGB=%1,%2,%3")
                        .arg(qRed(rgb)).arg(qGreen(rgb)).arg(qBlue(rgb));
                for (int i = 0; i < map[rgb].xy.size(); i++)
                    unknown += tr("             at x,y=%4,%5")
                            .arg(map[rgb].xy[i].x())
                            .arg(map[rgb].xy[i].y());
            }
            UnknownColorsDialog dialog(QFileInfo(imagePath).fileName(),
                                       unknown, MainWindow::instance());
            dialog.exec();
        }
        QMap<QRgb,UnknownColor> &mapVeg = mUnknownVegColors[imagePath];
        if (mapVeg.size()) {
            QStringList unknown;
            foreach (QRgb rgb, mapVeg.keys()) {
                unknown += tr("RGB=%1,%2,%3")
                        .arg(qRed(rgb)).arg(qGreen(rgb)).arg(qBlue(rgb));
                for (int i = 0; i < mapVeg[rgb].xy.size(); i++)
                    unknown += tr("             at x,y=%4,%5")
                            .arg(mapVeg[rgb].xy[i].x())
                            .arg(mapVeg[rgb].xy[i].y());
            }
            QString suffix = QFileInfo(imagePath).suffix();
            QString fileName = QFileInfo(imagePath).completeBaseName()
                         + QLatin1String("_veg.") + suffix;
            UnknownColorsDialog dialog(fileName, unknown, MainWindow::instance());
            dialog.exec();
        }
    }
}

QImage BMPToTMX::loadImage(const QString &path, const QString &suffix,
                           QImage::Format format)
{
    QImage image;
    if (!image.load(path)) {
        mError = tr("The image%1 file couldn't be loaded.\n%2\n\nThere might not be enough memory.  Try closing any open Cells or restart the application.")
                .arg(suffix).arg(QDir::toNativeSeparators(path));
        return QImage();
    }

    if (image.width() % 300 || image.height() % 300) {
        mError = tr("The image%1 size isn't divisible by 300.").arg(suffix);
        return QImage();
    }

#if 0
    // This is the fastest format for QImage::pixel() and QImage::setPixel().
    if (image.format() != format) {
        image = image.convertToFormat(format);
        if (image.isNull()) {
            mError = tr("The image%1 file couldn't be loaded.\n%2\n\nThere might not be enough memory.  Try closing any open Cells or restart the application.")
                    .arg(suffix).arg(QDir::toNativeSeparators(path));
        }
    }
#endif

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
    mRulesByColor0.clear();
    mRulesByColor1.clear();
    qDeleteAll(mAliases);
    mAliases = file.aliasesCopy();
    foreach (BmpAlias *alias, mAliases)
        mAliasByName[alias->name] = alias;
    foreach (BmpRule *rule, file.rules())
        AddRule(rule);

    // Verify all the listed tiles exist.
    QString tileName;
    foreach (BmpAlias *alias, mAliases) {
        foreach (tileName, alias->tiles) {
            if (getTileFromTileName(tileName) == 0) {
                goto bogusTile;
            }
        }
    }

    foreach (BmpRule *rule, mRules) {
        foreach (tileName, rule->tileChoices) {
            if (mAliasByName.contains(tileName))
                continue;
            if (!tileName.isEmpty() && getTileFromTileName(tileName) == 0) {
                goto bogusTile;
            }
        }
    }

    return true;

bogusTile:
    mError = tr("A tile listed in Rules.txt could not be found.\n");
    mError += tr("The missing tile is called '%1'.\n\n").arg(tileName);
    mError += tr("Please fix the invalid tile index or add the tileset\nif it is missing using the Tilesets dialog in TileZed.\n");
    return false;
}

bool BMPToTMX::LoadBlends()
{
    qDeleteAll(mBlends);
    mBlends.clear();

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
        BmpBlend *blendCopy = new BmpBlend(blend);
        mBlends += blendCopy;
    }

    // Verify all the listed tiles exist.
    QString tileName;
    foreach (BmpBlend *blend, mBlends) {
        tileName = blend->blendTile;
        if (!mAliasByName.contains(tileName) && !getTileFromTileName(tileName))
            goto bogusTile;
        tileName = blend->mainTile;
        if (!mAliasByName.contains(tileName) && !getTileFromTileName(tileName))
            goto bogusTile;
        foreach (tileName, blend->ExclusionList) {
            if (!mAliasByName.contains(tileName) && !getTileFromTileName(tileName))
                goto bogusTile;
        }
    }

    return true;

bogusTile:
    mError = tr("A tile listed in Blends.txt could not be found.\n");
    mError += tr("The missing tile is called '%1'.\n\n").arg(tileName);
    mError += tr("Please fix the invalid tile index or add the tileset\nif it is missing using the Tilesets dialog in TileZed.\n");
    return false;
}

void BMPToTMX::AddRule(BmpRule *rule)
{
    mRules += new BmpRule(rule);
    if (rule->bitmapIndex == 0)
        mRulesByColor0[rule->color] += mRules.last();
    else
        mRulesByColor1[rule->color] += mRules.last();
}

bool BMPToTMX::WriteMap(WorldCell *cell, int bmpIndex)
{
    Map map(Map::LevelIsometric, 300, 300, 64, 32);
    foreach (Tiled::Tileset *ts, TileMetaInfoMgr::instance()->tilesets())
        map.addTileset(ts);

    map.rbmpSettings()->setBlendsFile(mBlendFileName);
    map.rbmpSettings()->setRulesFile(mRuleFileName);

    QList<BmpAlias*> aliases;
    foreach (BmpAlias *alias, mAliases)
        aliases += new BmpAlias(alias);
    map.rbmpSettings()->setAliases(aliases);

    QList<BmpRule*> rules;
    foreach (BmpRule *rule, mRules)
        rules += new BmpRule(rule);
    map.rbmpSettings()->setRules(rules);

    QList<BmpBlend*> blends;
    foreach (BmpBlend *blend, mBlends)
        blends += new BmpBlend(blend);
    map.rbmpSettings()->setBlends(blends);

    BMPToTMXSettings settings = mWorldDoc->world()->getBMPToTMXSettings();
    settings.copyPixels = true; // obsolete
    settings.compress = true; // obsolete

    if (bmpIndex != -1) {
        MapBmp &rbmpMain = map.rbmpMain();
        MapBmp &rbmpVeg = map.rbmpVeg();

        BMPToTMXImages *images = mImages[bmpIndex];
        QImage bmp = images->mBmp;
        QImage bmpVeg = images->mBmpVeg;
        int ix = (cell->x() - images->mBounds.x()) * 300;
        int iy = (cell->y() - images->mBounds.y()) * 300;
        rbmpMain.rimage() = bmp.copy(ix, iy, 300, 300).convertToFormat(QImage::Format_ARGB32);
        rbmpVeg.rimage() = bmpVeg.copy(ix, iy, 300, 300).convertToFormat(QImage::Format_ARGB32);

        if (settings.warnUnknownColors) {
            const QRgb black = qRgb(0, 0, 0);
            for (int y = 0; y < map.height(); y++) {
                for (int x = 0; x < map.width(); x++) {
                    QRgb rgb = rbmpMain.pixel(x, y);
                    if (!mRulesByColor0.contains(rgb)) {
                        if (mUnknownColors[images->mPath][rgb].xy.size() < 50) {
                            mUnknownColors[images->mPath][rgb].rgb = rgb;
                            mUnknownColors[images->mPath][rgb].xy += QPoint(ix + x, iy + y);
                        }
                    }
                    rgb = rbmpVeg.pixel(x, y);
                    if (rgb != black && !mRulesByColor1.contains(rgb)) {
                        if (mUnknownVegColors[images->mPath][rgb].xy.size() < 50) {
                            mUnknownVegColors[images->mPath][rgb].rgb = rgb;
                            mUnknownVegColors[images->mPath][rgb].xy += QPoint(ix + x, iy + y);
                        }
                    }
                }
            }
        }
    }

    Tiled::Internal::BmpBlender blender;
    QMap<QString,TileLayer*> blendLayers;
    if (!settings.copyPixels) {
        blender.setMap(&map);
        blender.flush(QRect(0, 0, map.width(), map.height()));
        foreach (TileLayer *blendLayer, blender.tileLayers())
            blendLayers[blendLayer->name()] = blendLayer;
    }

    foreach (LayerInfo layer, mLayers) {
        if (layer.mType == LayerInfo::Tile) {
            TileLayer *tl = new TileLayer(layer.mName, 0, 0,
                                          map.width(), map.height());
            map.addLayer(tl);
            if (!settings.copyPixels) {
                if (TileLayer *blendLayer = blendLayers[tl->name()])
                    tl->setCells(0, 0, blendLayer);
            }
        } else if (layer.mType == LayerInfo::Object) {
            ObjectGroup *og = new ObjectGroup(layer.mName, 0, 0,
                                              map.width(), map.height());
            map.addLayer(og);
        }
    }

    if (!settings.copyPixels) {
        map.rbmpMain().rimage().fill(qRgb(0, 0, 0));
        map.rbmpVeg().rimage().fill(qRgb(0, 0, 0));
    }

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

namespace {
class BMPToTMX_MapReader : public MapReader
{
protected:
    /**
     * Overridden to make sure the resolved reference is canonical.
     */
    QString resolveReference(const QString &reference, const QString &mapPath)
    {
        QString resolved = MapReader::resolveReference(reference, mapPath);
        QString canonical = QFileInfo(resolved).canonicalFilePath();

        // Make sure that we're not returning an empty string when the file is
        // not found.
        return canonical.isEmpty() ? resolved : canonical;
    }
};
}

bool BMPToTMX::UpdateMap(WorldCell *cell, int bmpIndex)
{
    QString filePath = cell->mapFilePath();
    if (filePath.isEmpty() || !QFileInfo(filePath).exists())
        return true;

    BMPToTMX_MapReader reader;
    Map *map = reader.readMap(filePath);
    if (!map) {
        mError = reader.errorString();
        return false;
    }

    MapBmp &rbmpMain = map->rbmpMain();
    MapBmp &rbmpVeg = map->rbmpVeg();

    BMPToTMXImages *images = mImages[bmpIndex];
    QImage bmp = images->mBmp;
    QImage bmpVeg = images->mBmpVeg;

    int ix = (cell->x() - images->mBounds.x()) * 300;
    int iy = (cell->y() - images->mBounds.y()) * 300;
    QPainter painter(&rbmpMain.rimage());
    painter.drawImage(0, 0, bmp, ix, iy, 300, 300);
    painter.end();
    QPainter painter2(&rbmpVeg.rimage());
    painter2.drawImage(0, 0, bmpVeg, ix, iy, 300, 300);
    painter2.end();

    MapWriter writer;
    MapWriter::LayerDataFormat format = MapWriter::CSV;
//    if (mWorldDoc->world()->getBMPToTMXSettings().compress)
        format = MapWriter::Base64Zlib;
    writer.setLayerDataFormat(format);
    writer.setDtdEnabled(false);
    if (!writer.writeMap(map, filePath)) {
        delete map;
        mError = writer.errorString();
        return false;
    }
    delete map;
    return true;
}

#include "BuildingEditor/buildingtiles.h"
Tile *BMPToTMX::getTileFromTileName(const QString &tileName)
{
    if (tileName.isEmpty())
        return 0;
    QString tilesetName;
    int tileID;
    if (BuildingEditor::BuildingTilesMgr::parseTileName(tileName, tilesetName, tileID)) {
        if (Tileset *ts = TileMetaInfoMgr::instance()->tileset(tilesetName))
            return ts->tileAt(tileID);
    }
    return 0;
}
