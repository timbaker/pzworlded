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

#include "preferences.h"
#include "progress.h"
#include "worldcell.h"
#include "worlddocument.h"
#include "world.h"

#include <QCoreApplication>
#include <QtXml/QDomDocument>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QMessageBox>
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

    mImages.clear();
    foreach (WorldBMP *bmp, mWorldDoc->world()->bmps()) {
        BMPToTMXImages *images = getImages(bmp->filePath(), bmp->pos());
        if (!images) {
            mImages.clear();
            return false;
        }
        mImages += images;
    }

    progress.update(QLatin1String("Generating TMX files"));

    World *world = worldDoc->world();

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

    QMessageBox::information(0, tr("BMP To TMX"), tr("Finished!"));

    return true;
}

bool BMPToTMX::generateCell(WorldCell *cell)
{
    int n = 0, bmpIndex = -1;
    foreach (WorldBMP *bmp, cell->world()->bmps()) {
        if (bmp->bounds().contains(cell->pos())) {
            if (bmpIndex != -1) {
                mError = tr("Multiple BMP images cover cell %1,%2")
                        .arg(cell->x()).arg(cell->y());
                return false;
            }
            bmpIndex = n;
        }
        n++;
    }
    if (bmpIndex == -1)
        return true;

    BMPToTMXImages *images = mImages[bmpIndex];

    QImage bmp = images->mBmp;
    QImage bmpVeg = images->mBmpVeg;

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

    QString exportDir = mWorldDoc->world()->getBMPToTMXSettings().exportDir;
    QFile file(exportDir + QLatin1Char('/') + QLatin1String("BMPToMap.tmx"));
    if (file.open(QIODevice::WriteOnly)) {
        QTextStream textStream;
        textStream.setDevice(&file);
        doc.save(textStream, 1);
        file.close();
    }

    return true;
}

BMPToTMXImages *BMPToTMX::getImages(const QString &path, const QPoint &origin)
{
    QFileInfo info(path);
    if (!info.exists()) {
        mError = tr("The image file can't be found.");
        return 0;
    }

    QFileInfo infoVeg(info.absolutePath() + QLatin1Char('/')
                      + info.completeBaseName() + QLatin1String("_veg.bmp"));
    if (!infoVeg.exists()) {
        mError = tr("The image_veg file can't be found.");
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
        mError = tr("The image size is different than the image_veg size.");
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
        mError = tr("The image file can't be found.");
        return QSize();
    }

    QFileInfo infoVeg(info.absolutePath() + QLatin1Char('/')
                      + info.completeBaseName() + QLatin1String("_veg.bmp"));
    if (!infoVeg.exists()) {
        mError = tr("The image_veg file can't be found.");
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
        mError = tr("The image size is different than the image_veg size.");
        return QSize();
    }

    return image.size();
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

    return image;
}

bool BMPToTMX::LoadBaseXML()
{
    QString path = mWorldDoc->world()->getBMPToTMXSettings().mapbaseFile;
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

    QString filePath = mWorldDoc->world()->getBMPToTMXSettings().blendsFile;
    SimpleFile simpleFile;
    if (!simpleFile.read(filePath)) {
        mError = tr("Failed to read %1").arg(filePath);
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
