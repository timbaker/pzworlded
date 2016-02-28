/*
 * mapreader.cpp
 * Copyright 2008-2010, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 * Copyright 2010, Jeff Bland <jksb@member.fsf.org>
 * Copyright 2010, Dennis Honeyman <arcticuno@gmail.com>
 *
 * This file is part of libtiled.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "mapreader.h"

#include "compression.h"
#include "gidmapper.h"
#include "imagelayer.h"
#include "objectgroup.h"
#include "map.h"
#include "mapobject.h"
#include "tile.h"
#include "tilelayer.h"
#include "tileset.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#ifdef ZOMBOID
#include <QImageReader>
#include "qtlockedfile.h"
using namespace SharedTools;
#endif
#include <QXmlStreamReader>

using namespace Tiled;
using namespace Tiled::Internal;

namespace Tiled {
namespace Internal {

class MapReaderPrivate
{
    Q_DECLARE_TR_FUNCTIONS(MapReader)

public:
    MapReaderPrivate(MapReader *mapReader):
        p(mapReader),
        mMap(0),
        mReadingExternalTileset(false)
    {}

    Map *readMap(QIODevice *device, const QString &path);
    Tileset *readTileset(QIODevice *device, const QString &path);

#ifdef ZOMBOID
    bool openFile(QtLockedFile *file);
#else
    bool openFile(QFile *file);
#endif

    QString errorString() const;

private:
    void readUnknownElement();

    Map *readMap();

    Tileset *readTileset();
    void readTilesetTile(Tileset *tileset);
    void readTilesetImage(Tileset *tileset);

    TileLayer *readLayer();
    void readLayerData(TileLayer *tileLayer);
    void decodeBinaryLayerData(TileLayer *tileLayer,
                               const QStringRef &text,
                               const QStringRef &compression);
    void decodeCSVLayerData(TileLayer *tileLayer, const QString &text);

    /**
     * Returns the cell for the given global tile ID. Errors are raised with
     * the QXmlStreamReader.
     *
     * @param gid the global tile ID
     * @return the cell data associated with the given global tile ID, or an
     *         empty cell if not found
     */
    Cell cellForGid(uint gid);

    ImageLayer *readImageLayer();
    void readImageLayerImage(ImageLayer *imageLayer);

    ObjectGroup *readObjectGroup();
    MapObject *readObject();
    QPolygonF readPolygon();

    Properties readProperties();
    void readProperty(Properties *properties);

#ifdef ZOMBOID
    void readBmpSettings();
    QRgb rgbFromString(const QString &s, bool &ok);
    void readBmpAliases();
    void readBmpRules();
    void readBmpBlends();

    void readBmpImage();
    void readBmpPixels(int index, const QList<QRgb> &colors);
    void decodeBmpPixels(int bmpIndex, const QList<QRgb> &colors, const QStringRef &text);

    void readNoBlend();
    void decodeNoBlendBits(MapNoBlend *noBlend, const QStringRef &text);
#endif

    MapReader *p;

    QString mError;
    QString mPath;
    Map *mMap;
    GidMapper mGidMapper;
    bool mReadingExternalTileset;

    QXmlStreamReader xml;
};

} // namespace Internal
} // namespace Tiled

Map *MapReaderPrivate::readMap(QIODevice *device, const QString &path)
{
    mError.clear();
    mPath = path;
    Map *map = 0;

    xml.setDevice(device);

    if (xml.readNextStartElement() && xml.name() == QLatin1String("map")) {
        map = readMap();
    } else {
        xml.raiseError(tr("Not a map file."));
    }

    mGidMapper.clear();
    return map;
}

Tileset *MapReaderPrivate::readTileset(QIODevice *device, const QString &path)
{
    mError.clear();
    mPath = path;
    Tileset *tileset = 0;
    mReadingExternalTileset = true;

    xml.setDevice(device);

    if (xml.readNextStartElement() && xml.name() == QLatin1String("tileset"))
        tileset = readTileset();
    else
        xml.raiseError(tr("Not a tileset file."));

    mReadingExternalTileset = false;
    return tileset;
}

QString MapReaderPrivate::errorString() const
{
    if (!mError.isEmpty()) {
        return mError;
    } else {
        return tr("%3\n\nLine %1, column %2")
                .arg(xml.lineNumber())
                .arg(xml.columnNumber())
                .arg(xml.errorString());
    }
}

#ifdef ZOMBOID
bool MapReaderPrivate::openFile(QtLockedFile *file)
#else
bool MapReaderPrivate::openFile(QFile *file)
#endif
{
    if (!file->exists()) {
        mError = tr("File not found: %1").arg(file->fileName());
        return false;
    } else if (!file->open(QFile::ReadOnly | QFile::Text)) {
        mError = tr("Unable to read file: %1").arg(file->fileName());
        return false;
    }
#ifdef ZOMBOID
    if (!file->lock(QtLockedFile::ReadLock)) {
        mError = tr("Unable to lock file for reading: %1").arg(file->fileName());
        return false;
    }
#endif

    return true;
}

void MapReaderPrivate::readUnknownElement()
{
    qDebug() << "Unknown element (fixme):" << xml.name();
    xml.skipCurrentElement();
}

Map *MapReaderPrivate::readMap()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("map"));

    const QXmlStreamAttributes atts = xml.attributes();
    const int mapWidth =
            atts.value(QLatin1String("width")).toString().toInt();
    const int mapHeight =
            atts.value(QLatin1String("height")).toString().toInt();
    const int tileWidth =
            atts.value(QLatin1String("tilewidth")).toString().toInt();
    const int tileHeight =
            atts.value(QLatin1String("tileheight")).toString().toInt();

    const QString orientationString =
            atts.value(QLatin1String("orientation")).toString();
    const Map::Orientation orientation =
            orientationFromString(orientationString);

    if (orientation == Map::Unknown) {
        xml.raiseError(tr("Unsupported map orientation: \"%1\"")
                       .arg(orientationString));
    }

    mMap = new Map(orientation, mapWidth, mapHeight, tileWidth, tileHeight);

    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("properties"))
            mMap->mergeProperties(readProperties());
        else if (xml.name() == QLatin1String("tileset"))
            mMap->addTileset(readTileset());
        else if (xml.name() == QLatin1String("layer"))
            mMap->addLayer(readLayer());
        else if (xml.name() == QLatin1String("objectgroup"))
            mMap->addLayer(readObjectGroup());
        else if (xml.name() == QLatin1String("imagelayer"))
            mMap->addLayer(readImageLayer());
#ifdef ZOMBOID
        else if (xml.name() == QLatin1String("bmp-settings"))
            readBmpSettings();
        else if (xml.name() == QLatin1String("bmp-image"))
            readBmpImage();
        else if (xml.name() == QLatin1String("bmp-noblend"))
            readNoBlend();
#endif
        else
            readUnknownElement();
    }

    // Clean up in case of error
    if (xml.hasError()) {
        // The tilesets are not owned by the map
        qDeleteAll(mMap->tilesets());

        delete mMap;
        mMap = 0;
    }

    return mMap;
}

Tileset *MapReaderPrivate::readTileset()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("tileset"));

    const QXmlStreamAttributes atts = xml.attributes();
    const QString source = atts.value(QLatin1String("source")).toString();
    const uint firstGid =
            atts.value(QLatin1String("firstgid")).toString().toUInt();

    Tileset *tileset = 0;

    if (source.isEmpty()) { // Not an external tileset
        const QString name =
                atts.value(QLatin1String("name")).toString();
        const int tileWidth =
                atts.value(QLatin1String("tilewidth")).toString().toInt();
        const int tileHeight =
                atts.value(QLatin1String("tileheight")).toString().toInt();
        const int tileSpacing =
                atts.value(QLatin1String("spacing")).toString().toInt();
        const int margin =
                atts.value(QLatin1String("margin")).toString().toInt();

        if (tileWidth <= 0 || tileHeight <= 0
            || (firstGid == 0 && !mReadingExternalTileset)) {
            xml.raiseError(tr("Invalid tileset parameters for tileset"
                              " '%1'").arg(name));
        } else {
            tileset = new Tileset(name, tileWidth, tileHeight,
                                  tileSpacing, margin);

            while (xml.readNextStartElement()) {
                if (xml.name() == QLatin1String("tile")) {
                    readTilesetTile(tileset);
                } else if (xml.name() == QLatin1String("tileoffset")) {
                    const QXmlStreamAttributes oa = xml.attributes();
                    int x = oa.value(QLatin1String("x")).toString().toInt();
                    int y = oa.value(QLatin1String("y")).toString().toInt();
                    tileset->setTileOffset(QPoint(x, y));
                    xml.skipCurrentElement();
                } else if (xml.name() == QLatin1String("properties")) {
                    tileset->mergeProperties(readProperties());
                } else if (xml.name() == QLatin1String("image")) {
                    readTilesetImage(tileset);
                } else {
                    readUnknownElement();
                }
            }
        }
    } else { // External tileset
        const QString absoluteSource = p->resolveReference(source, mPath);
        QString error;
        tileset = p->readExternalTileset(absoluteSource, &error);

        if (!tileset) {
            xml.raiseError(tr("Error while loading tileset '%1': %2")
                           .arg(absoluteSource, error));
        }

        xml.skipCurrentElement();
    }

    if (tileset && !mReadingExternalTileset)
        mGidMapper.insert(firstGid, tileset);

    return tileset;
}

void MapReaderPrivate::readTilesetTile(Tileset *tileset)
{
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("tile"));

    const QXmlStreamAttributes atts = xml.attributes();
    const int id = atts.value(QLatin1String("id")).toString().toInt();

    if (id < 0 || id >= tileset->tileCount()) {
        xml.raiseError(tr("Invalid tile ID: %1").arg(id));
        return;
    }

    // TODO: Add support for individual tiles (then it needs to be added here)

    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("properties")) {
            Tile *tile = tileset->tileAt(id);
            tile->mergeProperties(readProperties());
        } else {
            readUnknownElement();
        }
    }
}

void MapReaderPrivate::readTilesetImage(Tileset *tileset)
{
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("image"));

    const QXmlStreamAttributes atts = xml.attributes();
    QString source = atts.value(QLatin1String("source")).toString();
    QString trans = atts.value(QLatin1String("trans")).toString();

    if (!trans.isEmpty()) {
        if (!trans.startsWith(QLatin1Char('#')))
            trans.prepend(QLatin1Char('#'));
        tileset->setTransparentColor(QColor(trans));
    }

    source = p->resolveReference(source, mPath);

    // Set the width that the tileset had when the map was saved
    const int width = atts.value(QLatin1String("width")).toString().toInt();
    mGidMapper.setTilesetWidth(tileset, width);

#ifdef ZOMBOID
#if 1
    // The tileset image is not read yet.  Just quickly create each Tile with
    // an all-white pixmap.
    const int height = atts.value(QLatin1String("height")).toString().toInt();
    tileset->loadFromNothing(QSize(width, height), source);
#elif 0
    QImageReader reader(source);
    if (reader.size().isValid() && tileset->loadFromNothing(reader.size(), source)) {
        // The tileset image is not read yet.  Just quickly create each Tile with
        // an all-white pixmap.
    } else {
        const int height = atts.value(QLatin1String("height")).toString().toInt();
        QImage image(width, height, QImage::Format_ARGB32);
        image.fill(Qt::red);
        tileset->loadFromImage(image, source);
        tileset->setMissing(true);
    }
#else
    if (p->tilesetImageCache()) {
        Tileset *cached = p->tilesetImageCache()->findMatch(tileset, source);
        if (!cached || !tileset->loadFromCache(cached)) {
            const QImage tilesetImage = p->readExternalImage(source);
            if (tileset->loadFromImage(tilesetImage, source))
                p->tilesetImageCache()->addTileset(tileset);
            else {
                const int height = atts.value(QLatin1String("height")).toString().toInt();
                QImage image(width, height, QImage::Format_ARGB32);
                image.fill(Qt::red);
                tileset->loadFromImage(image, source);
                tileset->setMissing(true);
            }
        }
        xml.skipCurrentElement();
        return;
    } else {
        const QImage tilesetImage = p->readExternalImage(source);
        if (!tileset->loadFromImage(tilesetImage, source)) {
            const int height = atts.value(QLatin1String("height")).toString().toInt();
            QImage image(width, height, QImage::Format_ARGB32);
            image.fill(Qt::red);
            tileset->loadFromImage(image, source);
            tileset->setMissing(true);
        }
    }
#endif
#else
    const QImage tilesetImage = p->readExternalImage(source);
    if (!tileset->loadFromImage(tilesetImage, source))
        xml.raiseError(tr("Error loading tileset image:\n'%1'").arg(source));
#endif
    xml.skipCurrentElement();
}

static void readLayerAttributes(Layer *layer,
                                const QXmlStreamAttributes &atts)
{
    const QStringRef opacityRef = atts.value(QLatin1String("opacity"));
    const QStringRef visibleRef = atts.value(QLatin1String("visible"));

    bool ok;
    const float opacity = opacityRef.toString().toFloat(&ok);
    if (ok)
        layer->setOpacity(opacity);

    const int visible = visibleRef.toString().toInt(&ok);
    if (ok)
        layer->setVisible(visible);
}

TileLayer *MapReaderPrivate::readLayer()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("layer"));

    const QXmlStreamAttributes atts = xml.attributes();
    const QString name = atts.value(QLatin1String("name")).toString();
    const int x = atts.value(QLatin1String("x")).toString().toInt();
    const int y = atts.value(QLatin1String("y")).toString().toInt();
    const int width = atts.value(QLatin1String("width")).toString().toInt();
    const int height = atts.value(QLatin1String("height")).toString().toInt();

    TileLayer *tileLayer = new TileLayer(name, x, y, width, height);
    readLayerAttributes(tileLayer, atts);

    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("properties"))
            tileLayer->mergeProperties(readProperties());
        else if (xml.name() == QLatin1String("data"))
            readLayerData(tileLayer);
        else
            readUnknownElement();
    }

    return tileLayer;
}

void MapReaderPrivate::readLayerData(TileLayer *tileLayer)
{
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("data"));

    const QXmlStreamAttributes atts = xml.attributes();
    QStringRef encoding = atts.value(QLatin1String("encoding"));
    QStringRef compression = atts.value(QLatin1String("compression"));

    int x = 0;
    int y = 0;

    while (xml.readNext() != QXmlStreamReader::Invalid) {
        if (xml.isEndElement())
            break;
        else if (xml.isStartElement()) {
            if (xml.name() == QLatin1String("tile")) {
                if (y >= tileLayer->height()) {
                    xml.raiseError(tr("Too many <tile> elements"));
                    continue;
                }

                const QXmlStreamAttributes atts = xml.attributes();
                uint gid = atts.value(QLatin1String("gid")).toString().toUInt();
                tileLayer->setCell(x, y, cellForGid(gid));

                x++;
                if (x >= tileLayer->width()) {
                    x = 0;
                    y++;
                }

                xml.skipCurrentElement();
            } else {
                readUnknownElement();
            }
        } else if (xml.isCharacters() && !xml.isWhitespace()) {
            if (encoding == QLatin1String("base64")) {
                decodeBinaryLayerData(tileLayer,
                                      xml.text(),
                                      compression);
            } else if (encoding == QLatin1String("csv")) {
                decodeCSVLayerData(tileLayer, xml.text().toString());
            } else {
                xml.raiseError(tr("Unknown encoding: %1")
                               .arg(encoding.toString()));
                continue;
            }
        }
    }
}

void MapReaderPrivate::decodeBinaryLayerData(TileLayer *tileLayer,
                                             const QStringRef &text,
                                             const QStringRef &compression)
{
#if QT_VERSION < 0x040800
    const QString textData = QString::fromRawData(text.unicode(), text.size());
    const QByteArray latin1Text = textData.toLatin1();
#else
    const QByteArray latin1Text = text.toLatin1();
#endif
    QByteArray tileData = QByteArray::fromBase64(latin1Text);
    const int size = (tileLayer->width() * tileLayer->height()) * 4;

    if (compression == QLatin1String("zlib")
        || compression == QLatin1String("gzip")) {
        tileData = decompress(tileData, size);
    } else if (!compression.isEmpty()) {
        xml.raiseError(tr("Compression method '%1' not supported")
                       .arg(compression.toString()));
        return;
    }

    if (size != tileData.length()) {
        xml.raiseError(tr("Corrupt layer data for layer '%1'")
                       .arg(tileLayer->name()));
        return;
    }

    const unsigned char *data =
            reinterpret_cast<const unsigned char*>(tileData.constData());
    int x = 0;
    int y = 0;

    for (int i = 0; i < size - 3; i += 4) {
        const uint gid = data[i] |
                         data[i + 1] << 8 |
                         data[i + 2] << 16 |
                         data[i + 3] << 24;

        tileLayer->setCell(x, y, cellForGid(gid));

        x++;
        if (x == tileLayer->width()) {
            x = 0;
            y++;
        }
    }
}

#if defined(ZOMBOID) /*&& defined(_DEBUG)*/
void QString_split(const QChar &sep, QString::SplitBehavior behavior, Qt::CaseSensitivity cs, const QString &in, QVector<int>& out)
{
    int start = 0;
    int end;
    while ((end = in.indexOf(sep, start, cs)) != -1) {
        if (start != end || behavior == QString::KeepEmptyParts) {
            out.append(start);
            out.append(end - start);
        }
        start = end + 1;
    }
    if (start != in.size() || behavior == QString::KeepEmptyParts) {
        out.append(start);
        out.append(in.size() - start);
    }
}
#endif

void MapReaderPrivate::decodeCSVLayerData(TileLayer *tileLayer, const QString &text)
{
#if defined(ZOMBOID) /*&& defined(_DEBUG)*/

    int start = 0;
    int end = text.length();
    while (start < end && text.at(start).isSpace())
        start++;
    int x = 0, y = 0;
    const QChar sep(QLatin1Char(','));
    const QChar nullChar(QLatin1Char('0'));
    const Cell emptyCell = cellForGid(0);
    while ((end = text.indexOf(sep, start, Qt::CaseSensitive)) != -1) {
        if (end - start == 1 && text.at(start) == nullChar) {
            tileLayer->setCell(x, y, emptyCell);
        } else {
            bool conversionOk;
            uint gid = text.mid(start, end - start).toUInt(&conversionOk);
            if (!conversionOk) {
                xml.raiseError(
                        tr("Unable to parse tile at (%1,%2) on layer '%3'")
                               .arg(x + 1).arg(y + 1).arg(tileLayer->name()));
                return;
            }
            tileLayer->setCell(x, y, cellForGid(gid));
        }
        start = end + 1;
        if (++x == tileLayer->width()) {
            ++y;
            if (y >= tileLayer->height()) {
                xml.raiseError(tr("Corrupt layer data for layer '%1'")
                               .arg(tileLayer->name()));
                return;
            }
            x = 0;
        }
    }
    end = text.size();
    while (start < end && text.at(end-1).isSpace())
        end--;
    if (end - start == 1 && text.at(start) == nullChar) {
        tileLayer->setCell(x, y, emptyCell);
    } else {
        bool conversionOk;
        uint gid = text.mid(start, end - start).toUInt(&conversionOk);
        if (!conversionOk) {
            xml.raiseError(
                    tr("Unable to parse tile at (%1,%2) on layer '%3'")
                           .arg(x + 1).arg(y + 1).arg(tileLayer->name()));
            return;
        }
        tileLayer->setCell(x, y, cellForGid(gid));
    }
#elif 0
    QString trimText = text.trimmed();
    static QVector<int> tiles;
    tiles.reserve(300*300*2);
    tiles.clear();
    QString_split(QLatin1Char(','), QString::KeepEmptyParts, Qt::CaseSensitive, trimText, tiles);

    if (tiles.count() / 2 != tileLayer->width() * tileLayer->height()) {
        xml.raiseError(tr("Corrupt layer data for layer '%1'")
                       .arg(tileLayer->name()));
        return;
    }

    QString tile;

    for (int y = 0; y < tileLayer->height(); y++) {
        for (int x = 0; x < tileLayer->width(); x++) {
            bool conversionOk;
            int k = (y * tileLayer->width() + x) * 2;
            tile = trimText.mid(tiles[k], tiles[k + 1]);
            const uint gid = tile.toUInt(&conversionOk);
            if (!conversionOk) {
                xml.raiseError(
                        tr("Unable to parse tile at (%1,%2) on layer '%3'")
                               .arg(x + 1).arg(y + 1).arg(tileLayer->name()));
                return;
            }
            tileLayer->setCell(x, y, cellForGid(gid));
        }
#if 1
        // Hack to keep the app responsive.
        // TODO: Move map reading to a worker thread. Only issue is tileset images
        // cannot be accessed outside the GUI thread.
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
#endif
    }
#else
    QString trimText = text.trimmed();
    QStringList tiles = trimText.split(QLatin1Char(','));

    if (tiles.length() != tileLayer->width() * tileLayer->height()) {
        xml.raiseError(tr("Corrupt layer data for layer '%1'")
                       .arg(tileLayer->name()));
        return;
    }

    for (int y = 0; y < tileLayer->height(); y++) {
        for (int x = 0; x < tileLayer->width(); x++) {
            bool conversionOk;
            const uint gid = tiles.at(y * tileLayer->width() + x)
                    .toUInt(&conversionOk);
            if (!conversionOk) {
                xml.raiseError(
                        tr("Unable to parse tile at (%1,%2) on layer '%3'")
                               .arg(x + 1).arg(y + 1).arg(tileLayer->name()));
                return;
            }
            tileLayer->setCell(x, y, cellForGid(gid));
        }
    }
#endif
}

Cell MapReaderPrivate::cellForGid(uint gid)
{
    bool ok;
    const Cell result = mGidMapper.gidToCell(gid, ok);

    if (!ok) {
        if (mGidMapper.isEmpty())
            xml.raiseError(tr("Tile used but no tilesets specified"));
        else
            xml.raiseError(tr("Invalid tile: %1").arg(gid));
    }

    return result;
}

ObjectGroup *MapReaderPrivate::readObjectGroup()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("objectgroup"));

    const QXmlStreamAttributes atts = xml.attributes();
    const QString name = atts.value(QLatin1String("name")).toString();
    const int x = atts.value(QLatin1String("x")).toString().toInt();
    const int y = atts.value(QLatin1String("y")).toString().toInt();
    const int width = atts.value(QLatin1String("width")).toString().toInt();
    const int height = atts.value(QLatin1String("height")).toString().toInt();

    ObjectGroup *objectGroup = new ObjectGroup(name, x, y, width, height);
    readLayerAttributes(objectGroup, atts);

    const QString color = atts.value(QLatin1String("color")).toString();
    if (!color.isEmpty())
        objectGroup->setColor(color);

    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("object"))
            objectGroup->addObject(readObject());
        else if (xml.name() == QLatin1String("properties"))
            objectGroup->mergeProperties(readProperties());
        else
            readUnknownElement();
    }

    return objectGroup;
}

ImageLayer *MapReaderPrivate::readImageLayer()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("imagelayer"));

    const QXmlStreamAttributes atts = xml.attributes();
    const QString name = atts.value(QLatin1String("name")).toString();
    const int x = atts.value(QLatin1String("x")).toString().toInt();
    const int y = atts.value(QLatin1String("y")).toString().toInt();
    const int width = atts.value(QLatin1String("width")).toString().toInt();
    const int height = atts.value(QLatin1String("height")).toString().toInt();

    ImageLayer *imageLayer = new ImageLayer(name, x, y, width, height);
    readLayerAttributes(imageLayer, atts);

    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("image"))
            readImageLayerImage(imageLayer);
        else if (xml.name() == QLatin1String("properties"))
            imageLayer->mergeProperties(readProperties());
        else
            readUnknownElement();
    }

    return imageLayer;
}

void MapReaderPrivate::readImageLayerImage(ImageLayer *imageLayer)
{
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("image"));

    const QXmlStreamAttributes atts = xml.attributes();
    QString source = atts.value(QLatin1String("source")).toString();
    QString trans = atts.value(QLatin1String("trans")).toString();

    if (!trans.isEmpty()) {
        if (!trans.startsWith(QLatin1Char('#')))
            trans.prepend(QLatin1Char('#'));
        imageLayer->setTransparentColor(QColor(trans));
    }

    source = p->resolveReference(source, mPath);

    const QImage imageLayerImage = p->readExternalImage(source);
    if (!imageLayer->loadFromImage(imageLayerImage, source))
        xml.raiseError(tr("Error loading image layer image:\n'%1'").arg(source));

    xml.skipCurrentElement();
}

static QPointF pixelToTileCoordinates(Map *map, int x, int y)
{
    const int tileHeight = map->tileHeight();
    const int tileWidth = map->tileWidth();

    if (map->orientation() == Map::Isometric) {
        // Isometric needs special handling, since the pixel values are based
        // solely on the tile height.
        return QPointF((qreal) x / tileHeight,
                       (qreal) y / tileHeight);
    } else {
        return QPointF((qreal) x / tileWidth,
                       (qreal) y / tileHeight);
    }
}

MapObject *MapReaderPrivate::readObject()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("object"));

    const QXmlStreamAttributes atts = xml.attributes();
    const QString name = atts.value(QLatin1String("name")).toString();
    const uint gid = atts.value(QLatin1String("gid")).toString().toUInt();
    const int x = atts.value(QLatin1String("x")).toString().toInt();
    const int y = atts.value(QLatin1String("y")).toString().toInt();
    const int width = atts.value(QLatin1String("width")).toString().toInt();
    const int height = atts.value(QLatin1String("height")).toString().toInt();
#ifdef ZOMBOID
    QString type = atts.value(QLatin1String("type")).toString();
#else
    const QString type = atts.value(QLatin1String("type")).toString();
#endif
    const QStringRef visibleRef = atts.value(QLatin1String("visible"));

    const QPointF pos = pixelToTileCoordinates(mMap, x, y);
    const QPointF size = pixelToTileCoordinates(mMap, width, height);

#ifdef ZOMBOID
    if (name == QLatin1String("lot") && !type.isEmpty()) {
        // Handle old files that don't include the extension.
        if (!type.endsWith(QLatin1String(".tmx")) &&
                !type.endsWith(QLatin1String(".tbx")))
            type += QLatin1String(".tmx");
        // Convert relative paths to absolute paths.
        type = p->resolveReference(type, mPath);
    }
#endif

    MapObject *object = new MapObject(name, type, pos, QSizeF(size.x(),
                                                              size.y()));
    if (gid) {
        const Cell cell = cellForGid(gid);
        object->setTile(cell.tile);
    }

    bool ok;
    const int visible = visibleRef.toString().toInt(&ok);
    if (ok)
        object->setVisible(visible);

    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("properties")) {
            object->mergeProperties(readProperties());
        } else if (xml.name() == QLatin1String("polygon")) {
            object->setPolygon(readPolygon());
            object->setShape(MapObject::Polygon);
        } else if (xml.name() == QLatin1String("polyline")) {
            object->setPolygon(readPolygon());
            object->setShape(MapObject::Polyline);
        } else {
            readUnknownElement();
        }
    }

    return object;
}

QPolygonF MapReaderPrivate::readPolygon()
{
    Q_ASSERT(xml.isStartElement() && (xml.name() == QLatin1String("polygon") ||
                                      xml.name() == QLatin1String("polyline")));

    const QXmlStreamAttributes atts = xml.attributes();
    const QString points = atts.value(QLatin1String("points")).toString();
    const QStringList pointsList = points.split(QLatin1Char(' '),
                                                QString::SkipEmptyParts);

    QPolygonF polygon;
    bool ok = true;

    foreach (const QString &point, pointsList) {
        const int commaPos = point.indexOf(QLatin1Char(','));
        if (commaPos == -1) {
            ok = false;
            break;
        }

        const int x = point.left(commaPos).toInt(&ok);
        if (!ok)
            break;
        const int y = point.mid(commaPos + 1).toInt(&ok);
        if (!ok)
            break;

        polygon.append(pixelToTileCoordinates(mMap, x, y));
    }

    if (!ok)
        xml.raiseError(tr("Invalid points data for polygon"));

    xml.skipCurrentElement();
    return polygon;
}

Properties MapReaderPrivate::readProperties()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("properties"));

    Properties properties;

    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("property"))
            readProperty(&properties);
        else
            readUnknownElement();
    }

    return properties;
}

void MapReaderPrivate::readProperty(Properties *properties)
{
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("property"));

    const QXmlStreamAttributes atts = xml.attributes();
    QString propertyName = atts.value(QLatin1String("name")).toString();
    QString propertyValue = atts.value(QLatin1String("value")).toString();

    while (xml.readNext() != QXmlStreamReader::Invalid) {
        if (xml.isEndElement()) {
            break;
        } else if (xml.isCharacters() && !xml.isWhitespace()) {
            if (propertyValue.isEmpty())
                propertyValue = xml.text().toString();
        } else if (xml.isStartElement()) {
            readUnknownElement();
        }
    }

    properties->insert(propertyName, propertyValue);
}

#ifdef ZOMBOID
void MapReaderPrivate::readBmpSettings()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("bmp-settings"));

    const QXmlStreamAttributes atts = xml.attributes();
#if 0
    int version = atts.value(QLatin1String("version")).toString().toInt();
#endif
    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("rules-file")) {
            const QXmlStreamAttributes atts = xml.attributes();
            QString fileName = atts.value(QLatin1String("file")).toString();
            if (!fileName.isEmpty()) {
                fileName = p->resolveReference(fileName, mPath);
                mMap->rbmpSettings()->setRulesFile(fileName);
            }
            xml.skipCurrentElement();
        } else if (xml.name() == QLatin1String("blends-file")) {
            const QXmlStreamAttributes atts = xml.attributes();
            QString fileName = atts.value(QLatin1String("file")).toString();
            if (!fileName.isEmpty()) {
                fileName = p->resolveReference(fileName, mPath);
                mMap->rbmpSettings()->setBlendsFile(fileName);
            }
            xml.skipCurrentElement();
        } else if (xml.name() == QLatin1String("aliases")) {
            readBmpAliases();
        } else if (xml.name() == QLatin1String("rules")) {
            readBmpRules();
        } else if (xml.name() == QLatin1String("blends")) {
            readBmpBlends();
        } else {
            readUnknownElement();
        }
    }
}

QRgb MapReaderPrivate::rgbFromString(const QString &s, bool &ok)
{
    QStringList parts = s.split(QLatin1Char(' '));
    if (parts.length() != 3) {
        ok = false;
        return 0;
    }
    int r, g, b;
    r = parts[0].toUInt(); // TODO: validate 0-255
    g = parts[1].toUInt();
    b = parts[2].toUInt();
    ok = true;
    return qRgb(r, g, b);
}

void MapReaderPrivate::readBmpAliases()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("aliases"));

    QList<BmpAlias*> aliases;

    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("alias")) {
            const QXmlStreamAttributes atts = xml.attributes();
            QString name = atts.value(QLatin1String("name")).toString();
            QStringList tiles = atts.value(QLatin1String("tiles")).toString()
                    .split(QLatin1Char(' '), QString::SkipEmptyParts);
            aliases += new BmpAlias(name, tiles);
            xml.skipCurrentElement();
        } else {
            readUnknownElement();
        }
    }

    mMap->rbmpSettings()->setAliases(aliases);
}

void MapReaderPrivate::readBmpRules()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("rules"));

    QList<BmpRule*> rules;

    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("rule")) {
            const QXmlStreamAttributes atts = xml.attributes();
            QString label = atts.value(QLatin1String("label")).toString();
            uint bitmapIndex = atts.value(QLatin1String("bitmapIndex")).toString().toUInt();
            bool ok;
            QRgb color = rgbFromString(atts.value(QLatin1String("color")).toString(), ok);
            QStringList tileChoices = atts.value(QLatin1String("tileChoices")).toString().split(QLatin1Char(' '));
            for (int i = 0; i < tileChoices.size(); i++)
                if (tileChoices[i] == QLatin1String("null"))
                    tileChoices[i].clear();
            QString targetLayer = atts.value(QLatin1String("targetLayer")).toString();
            QRgb condition = rgbFromString(atts.value(QLatin1String("condition")).toString(), ok);
            rules += new BmpRule(label, bitmapIndex, color, tileChoices, targetLayer, condition);
            xml.skipCurrentElement();
        } else {
            readUnknownElement();
        }
    }

    mMap->rbmpSettings()->setRules(rules);
}

void MapReaderPrivate::readBmpBlends()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("blends"));

    QMap<QString,BmpBlend::Direction> dirMap;
    dirMap[QLatin1String("n")] = BmpBlend::N;
    dirMap[QLatin1String("s")] = BmpBlend::S;
    dirMap[QLatin1String("e")] = BmpBlend::E;
    dirMap[QLatin1String("w")] = BmpBlend::W;
    dirMap[QLatin1String("nw")] = BmpBlend::NW;
    dirMap[QLatin1String("sw")] = BmpBlend::SW;
    dirMap[QLatin1String("ne")] = BmpBlend::NE;
    dirMap[QLatin1String("se")] = BmpBlend::SE;

    QList<BmpBlend*> blends;
    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("blend")) {
            const QXmlStreamAttributes atts = xml.attributes();
            QString targetLayer = atts.value(QLatin1String("targetLayer")).toString();
            QString mainTile = atts.value(QLatin1String("mainTile")).toString();
            QString blendTile = atts.value(QLatin1String("blendTile")).toString();
            QString dirString = atts.value(QLatin1String("dir")).toString();
            if (!dirMap.contains(dirString)) {
                xml.raiseError(tr("unknown BMP blend direction '%1").arg(dirString));
                return;
            }
            BmpBlend::Direction dir = dirMap[dirString];
            QStringList ExclusionList = atts.value(QLatin1String("ExclusionList"))
                    .toString().split(QLatin1Char(' '), QString::SkipEmptyParts);
            QStringList exclude2 = atts.value(QLatin1String("exclude2"))
                    .toString().split(QLatin1Char(' '), QString::SkipEmptyParts);
            blends += new BmpBlend(targetLayer, mainTile, blendTile, dir,
                                   ExclusionList, exclude2);
            xml.skipCurrentElement();
        } else {
            readUnknownElement();
        }
    }

    mMap->rbmpSettings()->setBlends(blends);
}

void MapReaderPrivate::readBmpImage()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("bmp-image"));

    const QXmlStreamAttributes atts = xml.attributes();
    int index = atts.value(QLatin1String("index")).toString().toUInt();
    uint seed = atts.value(QLatin1String("seed")).toString().toUInt();

    mMap->rbmp(index).rrands().setSeed(seed);

    QList<QRgb> colors;

    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("color")) {
            QString rgbString = xml.attributes().value(QLatin1String("rgb")).toString();
            if (rgbString.isEmpty()) {
bogusColor:
                xml.raiseError(tr("invalid bmp-image color '%1'").arg(rgbString));
                return;
            }
            QStringList split = rgbString.split(QLatin1Char(' '), QString::SkipEmptyParts);
            if (split.size() != 3)
                goto bogusColor;
            int r = split[0].toInt();
            int g = split[1].toInt();
            int b = split[2].toInt();
            colors += qRgb(r, g, b);

            xml.skipCurrentElement();
        } else if (xml.name() == QLatin1String("pixels")) {
            readBmpPixels(index, colors);
        } else {
            readUnknownElement();
        }
    }
}

void MapReaderPrivate::readBmpPixels(int index, const QList<QRgb> &colors)
{
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("pixels"));

    while (xml.readNext() != QXmlStreamReader::Invalid) {
        if (xml.isEndElement()) {
            break;
        } else if (xml.isCharacters() && !xml.isWhitespace()) {
            decodeBmpPixels(index, colors, xml.text());
        }
    }
}

void MapReaderPrivate::decodeBmpPixels(int bmpIndex, const QList<QRgb> &colors,
                                       const QStringRef &text)
{
#if QT_VERSION < 0x040800
    const QString textData = QString::fromRawData(text.unicode(), text.size());
    const QByteArray latin1Text = textData.toLatin1();
#else
    const QByteArray latin1Text = text.toLatin1();
#endif
    QByteArray tileData = QByteArray::fromBase64(latin1Text);
    const int size = (mMap->width() * mMap->height()) * 4;

    tileData = decompress(tileData, size);

    if (size != tileData.length()) {
        xml.raiseError(tr("Corrupt bmp data"));
        return;
    }

    const unsigned char *data =
            reinterpret_cast<const unsigned char*>(tileData.constData());
    int x = 0;
    int y = 0;

    for (int i = 0; i < size - 3; i += 4) {
        const quint32 n = data[i] |
                          data[i + 1] << 8 |
                          data[i + 2] << 16 |
                          data[i + 3] << 24;
        if (n > 0 && int(n) <= colors.size()) {
            QRgb rgb = colors[n - 1];
            mMap->rbmp(bmpIndex).setPixel(x, y, rgb);
        }

        x++;
        if (x == mMap->width()) {
            x = 0;
            y++;
        }
    }
}

void MapReaderPrivate::readNoBlend()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("bmp-noblend"));

    const QXmlStreamAttributes atts = xml.attributes();
    QString layerName = atts.value(QLatin1String("layer")).toString();

    MapNoBlend *noBlend = mMap->noBlend(layerName);

    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("bits")) {
            while (xml.readNext() != QXmlStreamReader::Invalid) {
                if (xml.isEndElement()) {
                    break;
                } else if (xml.isCharacters() && !xml.isWhitespace()) {
                    decodeNoBlendBits(noBlend, xml.text());
                }
            }
        } else {
            readUnknownElement();
        }
    }
}

void MapReaderPrivate::decodeNoBlendBits(MapNoBlend *noBlend, const QStringRef &text)
{
#if QT_VERSION < 0x040800
    const QString textData = QString::fromRawData(text.unicode(), text.size());
    const QByteArray latin1Text = textData.toLatin1();
#else
    const QByteArray latin1Text = text.toLatin1();
#endif
    QByteArray tileData = QByteArray::fromBase64(latin1Text);
    const int size = (noBlend->width() * noBlend->height());

    tileData = decompress(tileData, size);

    if (size != tileData.length()) {
        xml.raiseError(tr("Corrupt noblend data"));
        return;
    }

    const uchar *data = reinterpret_cast<const uchar*>(tileData.constData());
    int x = 0;
    int y = 0;

    for (int i = 0; i < size; i++) {
        noBlend->set(x, y, data[i] != 0);
        x++;
        if (x == noBlend->width()) {
            x = 0;
            y++;
        }
    }
}
#endif // ZOMBOID


MapReader::MapReader()
    : d(new MapReaderPrivate(this))
#ifdef ZOMBOID
    , mTilesetImageCache(0)
#endif
{
}

MapReader::~MapReader()
{
    delete d;
}

Map *MapReader::readMap(QIODevice *device, const QString &path)
{
    return d->readMap(device, path);
}

Map *MapReader::readMap(const QString &fileName)
{
#ifdef ZOMBOID
    SharedTools::QtLockedFile file(fileName);
#else
    QFile file(fileName);
#endif
    if (!d->openFile(&file))
        return 0;

    return readMap(&file, QFileInfo(fileName).absolutePath());
}

Tileset *MapReader::readTileset(QIODevice *device, const QString &path)
{
    return d->readTileset(device, path);
}

Tileset *MapReader::readTileset(const QString &fileName)
{
#ifdef ZOMBOID
    QtLockedFile file(fileName);
#else
    QFile file(fileName);
#endif
    if (!d->openFile(&file))
        return 0;

    Tileset *tileset = readTileset(&file, QFileInfo(fileName).absolutePath());
    if (tileset)
        tileset->setFileName(fileName);

    return tileset;
}

QString MapReader::errorString() const
{
    return d->errorString();
}

QString MapReader::resolveReference(const QString &reference,
                                    const QString &mapPath)
{
    if (QDir::isRelativePath(reference))
        return mapPath + QLatin1Char('/') + reference;
    else
        return reference;
}

QImage MapReader::readExternalImage(const QString &source)
{
    return QImage(source);
}

Tileset *MapReader::readExternalTileset(const QString &source,
                                        QString *error)
{
    MapReader reader;
    Tileset *tileset = reader.readTileset(source);
    if (!tileset)
        *error = reader.errorString();
    return tileset;
}
