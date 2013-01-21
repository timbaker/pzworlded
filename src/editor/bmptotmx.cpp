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

#include "bmptotmxconfirmdialog.h"
#include "mainwindow.h"
#include "preferences.h"
#include "progress.h"
#include "worldcell.h"
#include "worlddocument.h"
#include "world.h"

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
}

bool BMPToTMX::generateWorld(WorldDocument *worldDoc, BMPToTMX::GenerateMode mode)
{
    mWorldDoc = worldDoc;
    World *world = mWorldDoc->world();

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
        mError = tr("Error reading MapBaseXML.txt");
        return false;
    }
    if (!LoadRules()) {
        mError = tr("Error reading Rules.txt");
        return false;
    }
    if (!setupBlends())
        return false;

    PROGRESS progress(QLatin1String("Reading BMP images"));

    mImages.clear(); // FIXME: memory leak, images aren't freed
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

    QMessageBox::information(MainWindow::instance(),
                             tr("BMP To TMX"), tr("Finished!"));

    if (world->getBMPToTMXSettings().assignMapsToWorld)
        assignMapsToCells(worldDoc, mode);

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
            Entries[x][y].resize(2+blendLayers.size());
    }

    int sx = cell->x() * 300;
    int sy = cell->y() * 300;
    for (int y = sy; y < sy + 300; y++) {
        for (int x = sx; x < sx + 300; x++) {
            QRgb col = bmp.pixel(x - images->mBounds.x() * 300, y - images->mBounds.y() * 300);
            QRgb col2 = bmpVeg.pixel(x - images->mBounds.x() * 300, y - images->mBounds.y() * 300);

            if (Conversions.contains(col)) {
                QList<ConversionEntry> entries = Conversions[col];

                foreach (ConversionEntry entry, entries) {
                    if (entry.bitmapIndex != 0)
                        continue;
                    int index = 0;
                    if (entry.targetLayer == QLatin1String("0_Floor"))
                        index = 0;
                    else if (entry.targetLayer == QLatin1String("0_Vegetation"))
                        index = 1;
                    else
                        continue;

                    Entries[x - sx][y - sy][index] =
                            entry.tileChoices[qrand() % entry.tileChoices.count()];
                }
            } else {
                if (!mUnknownColors[images->mPath].contains(col)) {
                    mUnknownColors[images->mPath][col].rgb = col;
                    mUnknownColors[images->mPath][col].xy = QPoint(x, y);
                }
            }

            if (col2 != qRgb(0, 0, 0) && Conversions.contains(col2)) {
                QList<ConversionEntry> entries = Conversions[col2];

                foreach (ConversionEntry entry, entries) {
                    if (entry.bitmapIndex != 1)
                        continue;
                    if (entry.condition != col && entry.condition != qRgb(0, 0, 0))
                        continue;
                    int index = 0;
                    if (entry.targetLayer == QLatin1String("0_Floor"))
                        index = 0;
                    else if (entry.targetLayer == QLatin1String("0_Vegetation"))
                        index = 1;
                    else
                        continue;

                    Entries[x - sx][y - sy][index] =
                            entry.tileChoices[qrand() % entry.tileChoices.count()];
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

    QDomDocument doc;
    doc.setContent(baseXML);

    QDomElement root = doc.documentElement();
    QDomNode mapnode = root.firstChild();
    while (!mapnode.isNull()) {
        if (mapnode.nodeName() == QLatin1String("layer")) {
            QDomNode att = mapnode.attributes().namedItem(QLatin1String("name"));
            int floor = -1;
            if (!att.isNull() && att.nodeValue() == QLatin1String("0_Floor"))
                floor = 0;
            if (!att.isNull() && att.nodeValue() == QLatin1String("0_Vegetation"))
                floor = 1;
            if (!att.isNull() && blendLayers.contains(att.nodeValue()))
                floor = 2 + blendLayers.indexOf(att.nodeValue());

            if (floor != -1) {
                QDomNode dataNode = mapnode.firstChild();
                QDomText textNode = doc.createTextNode(toCSV(floor, Entries));
                dataNode.appendChild(textNode);
            }
        }
        mapnode = mapnode.nextSibling();
    }

    QString filePath = tmxNameForCell(cell, cell->world()->bmps().at(bmpIndex));
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        QTextStream textStream;
        textStream.setDevice(&file);
        doc.save(textStream, 1);
        file.close();
    } else {
        mError = tr("Error writing %1.").arg(filePath);
        return false;
    }

    return true;
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
    if (cell->mapFilePath() != tmxNameForCell(cell, bmp))
        mWorldDoc->setCellMapName(cell, tmxNameForCell(cell, bmp));
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
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    baseXML = file.readAll();
    file.seek(0);

    QXmlStreamReader xml;
    xml.setDevice(&file);

    if (xml.readNextStartElement() && xml.name() == "map") {
        while (xml.readNextStartElement()) {
            if (xml.name() == "tileset") {
                const QXmlStreamAttributes atts = xml.attributes();
                const int firstGID =
                        atts.value(QLatin1String("firstgid")).toString().toInt();
                const QString name = atts.value(QLatin1String("name")).toString();
                Tileset *set = new Tileset();
                set->firstGID = firstGID;
                set->name = name;
                Tilesets[name] = set;
                TilesetList += set;
            }
            xml.skipCurrentElement();
        }
    } else {
        xml.raiseError(tr("Not a map file."));
        return false;
    }

    return true;
}

bool BMPToTMX::LoadRules()
{
    QString path = mWorldDoc->world()->getBMPToTMXSettings().rulesFile;
    if (path.isEmpty())
        path = defaultRulesFile();
    QFile file(path);
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
            AddConversion(bmp, col, choices, layer, con);
        } else {
            AddConversion(bmp, col, choices, layer);
        }
    }

    return true;
}

bool BMPToTMX::setupBlends()
{
    blendList.clear();

    QString path = mWorldDoc->world()->getBMPToTMXSettings().blendsFile;
    if (path.isEmpty())
        path = defaultBlendsFile();
    SimpleFile simpleFile;
    if (!simpleFile.read(path)) {
        mError = tr("Failed to read %1").arg(path);
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
                excludes += exclude;

            blendList += Blend(block.value("layer"),
                               block.value("mainTile"),
                               block.value("blendTile"),
                               dir, excludes);
        } else {
            mError = tr("Unknown block name '%1'.\nProbable syntax error in Blends.txt.").arg(block.name);
            return false;
        }
    }

    QSet<QString> layers;
    foreach (Blend blend, blendList)
        layers.insert(blend.targetLayer);
    blendLayers = layers.values();

    return true;
}

void BMPToTMX::AddConversion(int bitmapIndex, QRgb col, QStringList tiles, QString layer, QRgb condition)
{
    Conversions[col] += ConversionEntry(bitmapIndex, col, tiles, layer, condition);
}

void BMPToTMX::AddConversion(int bitmapIndex, QRgb col, QStringList tiles, QString layer)
{
    Conversions[col] += ConversionEntry(bitmapIndex, col, tiles, layer);
}

bool BMPToTMX::BlendMap()
{
    for (int y = 0; y < 300; y++)
        for (int x = 0; x < 300; x++) {
            QString tile = Entries[x][y][0]; // 0_Floor

            for (int i = 0; i < blendLayers.size(); i++) {
                Blend blend = getBlendRule(x, y, tile, blendLayers[i]);
                if (!blend.isNull())
                    Entries[x][y][2+i] = blend.blendTile;
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

BMPToTMX::Blend BMPToTMX::getBlendRule(int x, int y, const QString &texture, const QString &layer)
{
    Blend lastBlend;
    if (texture.isEmpty())
        return lastBlend;
    foreach (Blend blend, blendList) {
        if (blend.targetLayer != layer)
            continue;

        if (texture != blend.mainTile) {
            if (blend.ExclusionList.contains(texture))
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

QString BMPToTMX::toCSV(int floor, QVector<QVector<QVector<QString> > > &Entries)
{
    QString b;
    b.reserve(300*300*4);
    b.append(QLatin1Char('\n'));
    for (int y = 0; y < 300; y++) {
        for (int x = 0; x < 300; x++) {
            QString tile = Entries[x][y][floor];

            if (tile.isEmpty())
                b.append(QLatin1Char('0'));
            else
                b.append(QString::number(getGIDFromTileName(tile)));

            if (y < 299 || x < 299)
                b.append(QLatin1Char(','));
        }
       b.append(QLatin1Char('\n'));
    }
    return b;
}

int BMPToTMX::getGIDFromTileName(const QString &name)
{
    QString tileset = name.mid(0, name.lastIndexOf(QLatin1Char('_')));
    int index = name.mid(name.lastIndexOf(QLatin1Char('_')) + 1).toInt();
    return Tilesets[tileset]->firstGID + index;
}
