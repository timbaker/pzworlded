/*
 * mapwriter.cpp
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

#include "mapwriter.h"

#include "compression.h"
#include "gidmapper.h"
#include "map.h"
#include "mapobject.h"
#include "imagelayer.h"
#include "objectgroup.h"
#include "tile.h"
#include "tilelayer.h"
#include "tileset.h"

#include <QCoreApplication>
#include <QDir>
#include <QXmlStreamWriter>
#ifdef ZOMBOID
#include "qtlockedfile.h"
using namespace SharedTools;
#endif

using namespace Tiled;
using namespace Tiled::Internal;

namespace Tiled {
namespace Internal {

class MapWriterPrivate
{
    Q_DECLARE_TR_FUNCTIONS(MapReader)

public:
    MapWriterPrivate();

    void writeMap(const Map *map, QIODevice *device,
                  const QString &path);

    void writeTileset(const Tileset *tileset, QIODevice *device,
                      const QString &path);

#ifdef ZOMBOID
    bool openFile(QtLockedFile *file);
#else
    bool openFile(QFile *file);
#endif

    QString mError;
    MapWriter::LayerDataFormat mLayerDataFormat;
    bool mDtdEnabled;

private:
    void writeMap(QXmlStreamWriter &w, const Map *map);
    void writeTileset(QXmlStreamWriter &w, const Tileset *tileset,
                      uint firstGid);
    void writeTileLayer(QXmlStreamWriter &w, const TileLayer *tileLayer);
    void writeLayerAttributes(QXmlStreamWriter &w, const Layer *layer);
    void writeObjectGroup(QXmlStreamWriter &w, const ObjectGroup *objectGroup);
    void writeObject(QXmlStreamWriter &w, const MapObject *mapObject);
    void writeImageLayer(QXmlStreamWriter &w, const ImageLayer *imageLayer);
    void writeProperties(QXmlStreamWriter &w,
                         const Properties &properties);
#ifdef ZOMBOID
    QString rgbString(QRgb rgb);
    void writeBmpSettings(QXmlStreamWriter &w, const BmpSettings *settings);
    void writeBmpImage(QXmlStreamWriter &w, int index, const MapBmp &bmp);
    void writeNoBlend(QXmlStreamWriter &w, MapNoBlend *noBlend);
#endif

    QDir mMapDir;     // The directory in which the map is being saved
    GidMapper mGidMapper;
    bool mUseAbsolutePaths;
};

} // namespace Internal
} // namespace Tiled


MapWriterPrivate::MapWriterPrivate()
    : mLayerDataFormat(MapWriter::Base64Gzip)
    , mDtdEnabled(false)
    , mUseAbsolutePaths(false)
{
}

#ifdef ZOMBOID
bool MapWriterPrivate::openFile(QtLockedFile *file)
#else
bool MapWriterPrivate::openFile(QFile *file)
#endif
{
    if (!file->open(QIODevice::WriteOnly)) {
        mError = tr("Could not open file for writing.");
        return false;
    }
#ifdef ZOMBOID
    if (!file->lock(QtLockedFile::WriteLock)) {
        mError = tr("Could not lock file for writing.");
        return false;
    }
#endif

    return true;
}

static QXmlStreamWriter *createWriter(QIODevice *device)
{
    QXmlStreamWriter *writer = new QXmlStreamWriter(device);
    writer->setAutoFormatting(true);
    writer->setAutoFormattingIndent(1);
    return writer;
}

void MapWriterPrivate::writeMap(const Map *map, QIODevice *device,
                                const QString &path)
{
    mMapDir = QDir(path);
    mUseAbsolutePaths = path.isEmpty();

    QXmlStreamWriter *writer = createWriter(device);
    writer->writeStartDocument();

    if (mDtdEnabled) {
        writer->writeDTD(QLatin1String("<!DOCTYPE map SYSTEM \""
                                       "http://mapeditor.org/dtd/1.0/"
                                       "map.dtd\">"));
    }

    writeMap(*writer, map);
    writer->writeEndDocument();
    delete writer;
}

void MapWriterPrivate::writeTileset(const Tileset *tileset, QIODevice *device,
                                    const QString &path)
{
    mMapDir = QDir(path);
    mUseAbsolutePaths = path.isEmpty();

    QXmlStreamWriter *writer = createWriter(device);
    writer->writeStartDocument();

    if (mDtdEnabled) {
        writer->writeDTD(QLatin1String("<!DOCTYPE tileset SYSTEM \""
                                       "http://mapeditor.org/dtd/1.0/"
                                       "map.dtd\">"));
    }

    writeTileset(*writer, tileset, 0);
    writer->writeEndDocument();
    delete writer;
}

void MapWriterPrivate::writeMap(QXmlStreamWriter &w, const Map *map)
{
    w.writeStartElement(QLatin1String("map"));

    const QString orientation = orientationToString(map->orientation());

    w.writeAttribute(QLatin1String("version"), QLatin1String("1.0"));
    w.writeAttribute(QLatin1String("orientation"), orientation);
    w.writeAttribute(QLatin1String("width"), QString::number(map->width()));
    w.writeAttribute(QLatin1String("height"), QString::number(map->height()));
    w.writeAttribute(QLatin1String("tilewidth"),
                     QString::number(map->tileWidth()));
    w.writeAttribute(QLatin1String("tileheight"),
                     QString::number(map->tileHeight()));

    writeProperties(w, map->properties());

    mGidMapper.clear();
    uint firstGid = 1;
    foreach (Tileset *tileset, map->tilesets()) {
        writeTileset(w, tileset, firstGid);
        mGidMapper.insert(firstGid, tileset);
        firstGid += tileset->tileCount();
    }

    foreach (const Layer *layer, map->layers()) {
        const Layer::Type type = layer->type();
        if (type == Layer::TileLayerType)
            writeTileLayer(w, static_cast<const TileLayer*>(layer));
        else if (type == Layer::ObjectGroupType)
            writeObjectGroup(w, static_cast<const ObjectGroup*>(layer));
        else if (type == Layer::ImageLayerType)
            writeImageLayer(w, static_cast<const ImageLayer*>(layer));
    }

#ifdef ZOMBOID
    writeBmpSettings(w, map->bmpSettings());
    writeBmpImage(w, 0, map->bmpMain());
    writeBmpImage(w, 1, map->bmpVeg());
    foreach (MapNoBlend *noBlend, map->noBlends())
        writeNoBlend(w, noBlend);
#endif

    w.writeEndElement();
}

void MapWriterPrivate::writeTileset(QXmlStreamWriter &w, const Tileset *tileset,
                                    uint firstGid)
{
    w.writeStartElement(QLatin1String("tileset"));
    if (firstGid > 0)
        w.writeAttribute(QLatin1String("firstgid"), QString::number(firstGid));

    const QString &fileName = tileset->fileName();
    if (!fileName.isEmpty()) {
        QString source = fileName;
        if (!mUseAbsolutePaths)
            source = mMapDir.relativeFilePath(source);
        w.writeAttribute(QLatin1String("source"), source);

        // Tileset is external, so no need to write any of the stuff below
        w.writeEndElement();
        return;
    }

    w.writeAttribute(QLatin1String("name"), tileset->name());
    w.writeAttribute(QLatin1String("tilewidth"),
                     QString::number(tileset->tileWidth()));
    w.writeAttribute(QLatin1String("tileheight"),
                     QString::number(tileset->tileHeight()));
    const int tileSpacing = tileset->tileSpacing();
    const int margin = tileset->margin();
    if (tileSpacing != 0)
        w.writeAttribute(QLatin1String("spacing"),
                         QString::number(tileSpacing));
    if (margin != 0)
        w.writeAttribute(QLatin1String("margin"), QString::number(margin));

    const QPoint offset = tileset->tileOffset();
    if (!offset.isNull()) {
        w.writeStartElement(QLatin1String("tileoffset"));
        w.writeAttribute(QLatin1String("x"), QString::number(offset.x()));
        w.writeAttribute(QLatin1String("y"), QString::number(offset.y()));
        w.writeEndElement();
    }

    // Write the tileset properties
    writeProperties(w, tileset->properties());

    // Write the image element
    const QString &imageSource = tileset->imageSource();
    if (!imageSource.isEmpty()) {
        w.writeStartElement(QLatin1String("image"));
        QString source = imageSource;
        if (!mUseAbsolutePaths)
            source = mMapDir.relativeFilePath(source);
        w.writeAttribute(QLatin1String("source"), source);

        const QColor transColor = tileset->transparentColor();
        if (transColor.isValid())
            w.writeAttribute(QLatin1String("trans"), transColor.name().mid(1));

#ifdef ZOMBOID
        if (!tileset->imageSource2x().isEmpty()) {
            if (tileset->imageWidth() > 0)
                w.writeAttribute(QLatin1String("width"),
                                 QString::number(tileset->imageWidth() / 2));
            if (tileset->imageHeight() > 0)
                w.writeAttribute(QLatin1String("height"),
                                 QString::number(tileset->imageHeight() / 2));
        } else {
#endif
        if (tileset->imageWidth() > 0)
            w.writeAttribute(QLatin1String("width"),
                             QString::number(tileset->imageWidth()));
        if (tileset->imageHeight() > 0)
            w.writeAttribute(QLatin1String("height"),
                             QString::number(tileset->imageHeight()));
#ifdef ZOMBOID
        }
#endif
        w.writeEndElement();
    }

    // Write the properties for those tiles that have them
    for (int i = 0; i < tileset->tileCount(); ++i) {
        const Tile *tile = tileset->tileAt(i);
        const Properties properties = tile->properties();
        if (!properties.isEmpty()) {
            w.writeStartElement(QLatin1String("tile"));
            w.writeAttribute(QLatin1String("id"), QString::number(i));
            writeProperties(w, properties);
            w.writeEndElement();
        }
    }

    w.writeEndElement();
}

void MapWriterPrivate::writeTileLayer(QXmlStreamWriter &w,
                                      const TileLayer *tileLayer)
{
    w.writeStartElement(QLatin1String("layer"));
    writeLayerAttributes(w, tileLayer);
    writeProperties(w, tileLayer->properties());

    QString encoding;
    QString compression;

    if (mLayerDataFormat == MapWriter::Base64
            || mLayerDataFormat == MapWriter::Base64Gzip
            || mLayerDataFormat == MapWriter::Base64Zlib) {

        encoding = QLatin1String("base64");

        if (mLayerDataFormat == MapWriter::Base64Gzip)
            compression = QLatin1String("gzip");
        else if (mLayerDataFormat == MapWriter::Base64Zlib)
            compression = QLatin1String("zlib");

    } else if (mLayerDataFormat == MapWriter::CSV)
        encoding = QLatin1String("csv");

    w.writeStartElement(QLatin1String("data"));
    if (!encoding.isEmpty())
        w.writeAttribute(QLatin1String("encoding"), encoding);
    if (!compression.isEmpty())
        w.writeAttribute(QLatin1String("compression"), compression);

    if (mLayerDataFormat == MapWriter::XML) {
        for (int y = 0; y < tileLayer->height(); ++y) {
            for (int x = 0; x < tileLayer->width(); ++x) {
                const uint gid = mGidMapper.cellToGid(tileLayer->cellAt(x, y));
                w.writeStartElement(QLatin1String("tile"));
                w.writeAttribute(QLatin1String("gid"), QString::number(gid));
                w.writeEndElement();
            }
        }
    } else if (mLayerDataFormat == MapWriter::CSV) {
        QString tileData;

        for (int y = 0; y < tileLayer->height(); ++y) {
            for (int x = 0; x < tileLayer->width(); ++x) {
                const uint gid = mGidMapper.cellToGid(tileLayer->cellAt(x, y));
                tileData.append(QString::number(gid));
                if (x != tileLayer->width() - 1
                    || y != tileLayer->height() - 1)
                    tileData.append(QLatin1String(","));
            }
            tileData.append(QLatin1String("\n"));
        }

        w.writeCharacters(QLatin1String("\n"));
        w.writeCharacters(tileData);
    } else {
        QByteArray tileData;
        tileData.reserve(tileLayer->height() * tileLayer->width() * 4);

        for (int y = 0; y < tileLayer->height(); ++y) {
            for (int x = 0; x < tileLayer->width(); ++x) {
                const uint gid = mGidMapper.cellToGid(tileLayer->cellAt(x, y));
                tileData.append((char) (gid));
                tileData.append((char) (gid >> 8));
                tileData.append((char) (gid >> 16));
                tileData.append((char) (gid >> 24));
            }
        }

        if (mLayerDataFormat == MapWriter::Base64Gzip)
            tileData = compress(tileData, Gzip);
        else if (mLayerDataFormat == MapWriter::Base64Zlib)
            tileData = compress(tileData, Zlib);

        w.writeCharacters(QLatin1String("\n   "));
        w.writeCharacters(QString::fromLatin1(tileData.toBase64()));
        w.writeCharacters(QLatin1String("\n  "));
    }

    w.writeEndElement(); // </data>
    w.writeEndElement(); // </layer>
}

void MapWriterPrivate::writeLayerAttributes(QXmlStreamWriter &w,
                                            const Layer *layer)
{
    w.writeAttribute(QLatin1String("name"), layer->name());
    w.writeAttribute(QLatin1String("width"), QString::number(layer->width()));
    w.writeAttribute(QLatin1String("height"), QString::number(layer->height()));
    w.writeAttribute(QLatin1String("level"), QString::number(layer->level()));
    const int x = layer->x();
    const int y = layer->y();
    const qreal opacity = layer->opacity();
    if (x != 0)
        w.writeAttribute(QLatin1String("x"), QString::number(x));
    if (y != 0)
        w.writeAttribute(QLatin1String("y"), QString::number(y));
    if (!layer->isVisible())
        w.writeAttribute(QLatin1String("visible"), QLatin1String("0"));
    if (opacity != qreal(1))
        w.writeAttribute(QLatin1String("opacity"), QString::number(opacity));
}

void MapWriterPrivate::writeObjectGroup(QXmlStreamWriter &w,
                                        const ObjectGroup *objectGroup)
{
    w.writeStartElement(QLatin1String("objectgroup"));

    if (objectGroup->color().isValid())
        w.writeAttribute(QLatin1String("color"),
                         objectGroup->color().name());

    writeLayerAttributes(w, objectGroup);
    writeProperties(w, objectGroup->properties());

    foreach (const MapObject *mapObject, objectGroup->objects())
        writeObject(w, mapObject);

    w.writeEndElement();
}

class TileToPixelCoordinates
{
public:
    TileToPixelCoordinates(Map *map)
    {
        if (map->orientation() == Map::Isometric) {
            // Isometric needs special handling, since the pixel values are
            // based solely on the tile height.
            mMultiplierX = map->tileHeight();
            mMultiplierY = map->tileHeight();
        } else {
            mMultiplierX = map->tileWidth();
            mMultiplierY = map->tileHeight();
        }
    }

    QPoint operator() (qreal x, qreal y) const
    {
        return QPoint(qRound(x * mMultiplierX),
                      qRound(y * mMultiplierY));
    }

private:
    int mMultiplierX;
    int mMultiplierY;
};

void MapWriterPrivate::writeObject(QXmlStreamWriter &w,
                                   const MapObject *mapObject)
{
    w.writeStartElement(QLatin1String("object"));
    const QString &name = mapObject->name();
#ifdef ZOMBOID
    QString type = mapObject->type();
#else
    const QString &type = mapObject->type();
#endif
    if (!name.isEmpty())
        w.writeAttribute(QLatin1String("name"), name);
#ifdef ZOMBOID
    if (name == QLatin1String("lot") && !type.isEmpty()) {
        if (!type.endsWith(QLatin1String(".tmx")) &&
                !type.endsWith(QLatin1String(".tbx")))
            type += QLatin1String(".tmx");
        Q_ASSERT(QDir::isAbsolutePath(type));
        if (QDir::isAbsolutePath(type))
            type = mMapDir.relativeFilePath(type);
    }
#endif
    if (!type.isEmpty())
        w.writeAttribute(QLatin1String("type"), type);

    if (mapObject->tile()) {
        const uint gid = mGidMapper.cellToGid(Cell(mapObject->tile()));
        w.writeAttribute(QLatin1String("gid"), QString::number(gid));
    }

    // Convert from tile to pixel coordinates
    const ObjectGroup *objectGroup = mapObject->objectGroup();
    const TileToPixelCoordinates toPixel(objectGroup->map());

    QPoint pos = toPixel(mapObject->x(), mapObject->y());
    QPoint size = toPixel(mapObject->width(), mapObject->height());

    w.writeAttribute(QLatin1String("x"), QString::number(pos.x()));
    w.writeAttribute(QLatin1String("y"), QString::number(pos.y()));

    if (size.x() != 0)
        w.writeAttribute(QLatin1String("width"), QString::number(size.x()));
    if (size.y() != 0)
        w.writeAttribute(QLatin1String("height"), QString::number(size.y()));

    if (!mapObject->isVisible())
        w.writeAttribute(QLatin1String("visible"), QLatin1String("0"));

    writeProperties(w, mapObject->properties());

    const QPolygonF &polygon = mapObject->polygon();
    if (!polygon.isEmpty()) {
        if (mapObject->shape() == MapObject::Polygon)
            w.writeStartElement(QLatin1String("polygon"));
        else
            w.writeStartElement(QLatin1String("polyline"));

        QString points;
        foreach (const QPointF &point, polygon) {
            const QPoint pos = toPixel(point.x(), point.y());
            points.append(QString::number(pos.x()));
            points.append(QLatin1Char(','));
            points.append(QString::number(pos.y()));
            points.append(QLatin1Char(' '));
        }
        points.chop(1);
        w.writeAttribute(QLatin1String("points"), points);
        w.writeEndElement();
    }

    w.writeEndElement();
}

void MapWriterPrivate::writeImageLayer(QXmlStreamWriter &w,
                                        const ImageLayer *imageLayer)
{
    w.writeStartElement(QLatin1String("imagelayer"));
    writeLayerAttributes(w, imageLayer);

    // Write the image element
    const QString &imageSource = imageLayer->imageSource();
    if (!imageSource.isEmpty()) {
        w.writeStartElement(QLatin1String("image"));
        QString source = imageSource;
        if (!mUseAbsolutePaths)
            source = mMapDir.relativeFilePath(source);
        w.writeAttribute(QLatin1String("source"), source);

        const QColor transColor = imageLayer->transparentColor();
        if (transColor.isValid())
            w.writeAttribute(QLatin1String("trans"), transColor.name().mid(1));

        w.writeEndElement();
    }

    writeProperties(w, imageLayer->properties());

    w.writeEndElement();
}

void MapWriterPrivate::writeProperties(QXmlStreamWriter &w,
                                       const Properties &properties)
{
    if (properties.isEmpty())
        return;

    w.writeStartElement(QLatin1String("properties"));

    Properties::const_iterator it = properties.constBegin();
    Properties::const_iterator it_end = properties.constEnd();
    for (; it != it_end; ++it) {
        w.writeStartElement(QLatin1String("property"));
        w.writeAttribute(QLatin1String("name"), it.key());

        const QString &value = it.value();
        if (value.contains(QLatin1Char('\n'))) {
            w.writeCharacters(value);
        } else {
            w.writeAttribute(QLatin1String("value"), it.value());
        }
        w.writeEndElement();
    }

    w.writeEndElement();
}

#ifdef ZOMBOID
QString MapWriterPrivate::rgbString(QRgb rgb)
{
    return tr("%1 %2 %3").arg(qRed(rgb)).arg(qGreen(rgb)).arg(qBlue(rgb));
}

#define BMP_SETTINGS_VERSION 1
void MapWriterPrivate::writeBmpSettings(QXmlStreamWriter &w,
                                        const BmpSettings *settings)
{
    w.writeStartElement(QLatin1String("bmp-settings"));
    w.writeAttribute(QLatin1String("version"), QString::number(BMP_SETTINGS_VERSION));

    w.writeStartElement(QLatin1String("rules-file"));
    QString fileName = settings->rulesFile();
    if (QDir::isAbsolutePath(fileName))
        fileName = mMapDir.relativeFilePath(fileName);
    w.writeAttribute(QLatin1String("file"), fileName);
    w.writeEndElement(); // <rules-file>

    w.writeStartElement(QLatin1String("blends-file"));
    fileName = settings->blendsFile();
    if (QDir::isAbsolutePath(fileName))
        fileName = mMapDir.relativeFilePath(fileName);
    w.writeAttribute(QLatin1String("file"), fileName);
    w.writeEndElement(); // <blends-file>

    w.writeStartElement(QLatin1String("edges-everywhere"));
    w.writeAttribute(QLatin1String("value"), QLatin1String(settings->isBlendEdgesEverywhere() ? "true" : "false"));
    w.writeEndElement(); // <edges-everywhere>

    w.writeStartElement(QLatin1String("aliases"));
    foreach (BmpAlias *alias, settings->aliases()) {
        w.writeStartElement(QLatin1String("alias"));
        w.writeAttribute(QLatin1String("name"), alias->name);
        w.writeAttribute(QLatin1String("tiles"), alias->tiles.join(QLatin1String(" ")));
        w.writeEndElement();
    }
    w.writeEndElement();

    w.writeStartElement(QLatin1String("rules"));
    foreach (BmpRule *rule, settings->rules()) {
        w.writeStartElement(QLatin1String("rule"));
        w.writeAttribute(QLatin1String("label"), rule->label);
        w.writeAttribute(QLatin1String("bitmapIndex"), QString::number(rule->bitmapIndex));
        w.writeAttribute(QLatin1String("color"), rgbString(rule->color));
        QStringList tileChoices;
        foreach (QString tileName, rule->tileChoices) {
            if (tileName.isEmpty())
                tileName = QLatin1String("null");
            tileChoices += tileName;
        }
        w.writeAttribute(QLatin1String("tileChoices"), tileChoices.join(QLatin1String(" ")));
        w.writeAttribute(QLatin1String("targetLayer"), rule->targetLayer);
        w.writeAttribute(QLatin1String("condition"), rgbString(rule->condition));
        w.writeEndElement(); // <rule>
    }
    w.writeEndElement(); // <rules>

    w.writeStartElement(QLatin1String("blends"));
    foreach (BmpBlend *blend, settings->blends()) {
        w.writeStartElement(QLatin1String("blend"));
        w.writeAttribute(QLatin1String("targetLayer"), blend->targetLayer);
        w.writeAttribute(QLatin1String("mainTile"), blend->mainTile);
        w.writeAttribute(QLatin1String("blendTile"), blend->blendTile);
        w.writeAttribute(QLatin1String("dir"), blend->dirAsString());
        w.writeAttribute(QLatin1String("ExclusionList"), blend->ExclusionList.join(QLatin1String(" ")));
        w.writeAttribute(QLatin1String("exclude2"), blend->exclude2.join(QLatin1String(" ")));
        w.writeEndElement(); // <blend>
    }
    w.writeEndElement(); // <blends>

    w.writeEndElement(); // <bmp-settings>
}

void MapWriterPrivate::writeBmpImage(QXmlStreamWriter &w,
                                     int index, const MapBmp &bmp)
{
    QList<QRgb> colors = bmp.colors();
    if (colors.isEmpty())
        return;

    struct ColorCompare {
        bool operator()(const QRgb& a, const QRgb& b) const {
            if (qRed(a) < qRed(b)) return true;
            if (qRed(a) > qRed(b)) return false;
            if (qGreen(a) < qGreen(b)) return true;
            if (qGreen(a) > qGreen(b)) return false;
            return qBlue(a) < qBlue(b);
        }
    };
    std::sort(colors.begin(), colors.end(), ColorCompare());

    w.writeStartElement(QLatin1String("bmp-image"));
    w.writeAttribute(QLatin1String("index"), QString::number(index));
    w.writeAttribute(QLatin1String("seed"), QString::number(bmp.rands().seed()));

    foreach (QRgb rgb, colors) {
        w.writeStartElement(QLatin1String("color"));
        w.writeAttribute(QLatin1String("rgb"), tr("%1 %2 %3")
                         .arg(qRed(rgb))
                         .arg(qGreen(rgb))
                         .arg(qBlue(rgb)));
        w.writeEndElement();
    }

    w.writeStartElement(QLatin1String("pixels"));
    QString data;
    QByteArray tileData;
    tileData.reserve(bmp.height() * bmp.width() * 4);

    const QRgb black = qRgb(0, 0, 0);
    for (int y = 0; y < bmp.height(); ++y) {
        for (int x = 0; x < bmp.width(); ++x) {
            QRgb rgb = bmp.pixel(x, y);
            quint32 n = (rgb == black) ? 0 : (colors.indexOf(rgb) + 1);
            tileData.append((unsigned char) (n)); // FIXME: big/little endian
            tileData.append((unsigned char) (n >> 8));
            tileData.append((unsigned char) (n >> 16));
            tileData.append((unsigned char) (n >> 24));
        }
    }

    tileData = compress(tileData, Gzip);
    data = QString::fromLatin1(tileData.toBase64());

    w.writeCharacters(QLatin1String("\n   "));
    w.writeCharacters(data);
    w.writeCharacters(QLatin1String("\n  "));
    w.writeEndElement();

    w.writeEndElement();
}

void MapWriterPrivate::writeNoBlend(QXmlStreamWriter &w, MapNoBlend *noBlend)
{
    QByteArray data;
    int numTrue = 0;
    for (int y = 0; y < noBlend->width(); y++) {
        for (int x = 0; x < noBlend->height(); x++) {
            data.append(uchar(noBlend->get(x, y) ? 1 : 0));
            if (noBlend->get(x, y)) numTrue++;
        }
    }

    if (numTrue == 0)
        return;

    w.writeStartElement(QLatin1String("bmp-noblend"));
    w.writeAttribute(QLatin1String("layer"), noBlend->layerName());

    w.writeStartElement(QLatin1String("bits"));
    w.writeCharacters(QLatin1String("\n   "));
    QString chars = QString::fromLatin1(compress(data, Gzip).toBase64());
    w.writeCharacters(chars);
    w.writeCharacters(QLatin1String("\n  "));
    w.writeEndElement(); // bits

    w.writeEndElement(); // bmp-noblend
}
#endif // ZOMBOID

MapWriter::MapWriter()
    : d(new MapWriterPrivate)
{
}

MapWriter::~MapWriter()
{
    delete d;
}

void MapWriter::writeMap(const Map *map, QIODevice *device,
                         const QString &path)
{
    d->writeMap(map, device, path);
}

bool MapWriter::writeMap(const Map *map, const QString &fileName)
{
#ifdef ZOMBOID
    QtLockedFile file(fileName);
#else
    QFile file(fileName);
#endif
    if (!d->openFile(&file))
        return false;

    writeMap(map, &file, QFileInfo(fileName).absolutePath());

    if (file.error() != QFile::NoError) {
        d->mError = file.errorString();
        return false;
    }

    return true;
}

void MapWriter::writeTileset(const Tileset *tileset, QIODevice *device,
                             const QString &path)
{
    d->writeTileset(tileset, device, path);
}

bool MapWriter::writeTileset(const Tileset *tileset, const QString &fileName)
{
#ifdef ZOMBOID
    QtLockedFile file(fileName);
#else
    QFile file(fileName);
#endif
    if (!d->openFile(&file))
        return false;

    writeTileset(tileset, &file, QFileInfo(fileName).absolutePath());

    if (file.error() != QFile::NoError) {
        d->mError = file.errorString();
        return false;
    }

    return true;
}

QString MapWriter::errorString() const
{
    return d->mError;
}

void MapWriter::setLayerDataFormat(MapWriter::LayerDataFormat format)
{
    d->mLayerDataFormat = format;
}

MapWriter::LayerDataFormat MapWriter::layerDataFormat() const
{
    return d->mLayerDataFormat;
}

void MapWriter::setDtdEnabled(bool enabled)
{
    d->mDtdEnabled = enabled;
}

bool MapWriter::isDtdEnabled() const
{
    return d->mDtdEnabled;
}
