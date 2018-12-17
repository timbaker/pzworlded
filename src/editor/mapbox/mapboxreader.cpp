/*
 * Copyright 2018, Tim Baker <treectrl@users.sf.net>
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

#include "mapboxreader.h"

#include "world.h"
#include "worldcell.h"

#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>

class MapBoxReaderPrivate
{
    Q_DECLARE_TR_FUNCTIONS(MapReader)

public:
    MapBoxReaderPrivate()
        : mWorld(nullptr)
    {

    }

    bool openFile(QFile *file)
    {
        if (!file->exists()) {
            mError = tr("File not found: %1").arg(file->fileName());
            return false;
        } else if (!file->open(QFile::ReadOnly | QFile::Text)) {
            mError = tr("Unable to read file: %1").arg(file->fileName());
            return false;
        }

        return true;
    }

    QString errorString() const
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

    World *readWorld(QIODevice *device, const QString &path, World* world)
    {
        mError.clear();
        mPath = path;

        xml.setDevice(device);

        if (xml.readNextStartElement() && xml.name() == QLatin1Literal("world")) {
            world = readWorld(world);
        } else {
            xml.raiseError(tr("Missing 'world' element."));
        }

        return world;
    }

private:
    World *readWorld(World* world)
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1Literal("world"));

        const QXmlStreamAttributes atts = xml.attributes();
//        const int width = atts.value(QLatin1Literal("width")).toString().toInt();
//        const int height = atts.value(QLatin1Literal("height")).toString().toInt();

        mWorld = world;

        for (int y = 0; y < world->height(); y++) {
            for (int x = 0; x < world->width(); x++) {
                if (WorldCell* cell = world->cellAt(x, y)) {
                    cell->mapBox() = WorldCellMapBox(cell);
                }
            }
        }

        while (xml.readNextStartElement()) {
            if (xml.name() == QLatin1Literal("cell"))
                readCell();
            else
                readUnknownElement();
        }

        // Clean up in case of error
        if (xml.hasError()) {
            mWorld = nullptr;
        }

        return mWorld;
    }

    void readCell()
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1Literal("cell"));

        const QXmlStreamAttributes atts = xml.attributes();
        const int x = atts.value(QLatin1Literal("x")).toString().toInt();
        const int y = atts.value(QLatin1Literal("y")).toString().toInt();

        if (!mWorld->contains(x, y))
            xml.raiseError(tr("Invalid cell coordinates %1,%2").arg(x).arg(y));
        else {
            WorldCell *cell = mWorld->cellAt(x, y);

            while (xml.readNextStartElement()) {
                if (xml.name() == QLatin1Literal("feature"))
                    readFeature(cell);
                else
                    readUnknownElement();
            }

        }
    }

    void readFeature(WorldCell* cell)
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1Literal("feature"));

        MapBoxFeature* feature = new MapBoxFeature(&cell->mapBox());

        while (xml.readNextStartElement()) {
            if (xml.name() == QLatin1Literal("geometry"))
                readFeatureGeometry(*feature);
            else if (xml.name() == QLatin1Literal("properties"))
                readFeatureProperties(*feature);
            else
                readUnknownElement();
        }

        cell->mapBox().mFeatures += feature;
    }

    void readFeatureGeometry(MapBoxFeature& feature)
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1Literal("geometry"));

        const QXmlStreamAttributes atts = xml.attributes();

        feature.mGeometry.mType = atts.value(QLatin1Literal("type")).toString();

        while (xml.readNextStartElement()) {
            if (xml.name() == QLatin1Literal("coordinates"))
                readGeometryCoordinates(feature.mGeometry);
            else
                readUnknownElement();
        }
    }

    void readGeometryCoordinates(MapBoxGeometry& geometry)
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1Literal("coordinates"));

        MapBoxCoordinates coordinates;

        while (xml.readNextStartElement()) {
            if (xml.name() == QLatin1Literal("point"))
                readGeometryCoordinatePoint(coordinates);
            else
                readUnknownElement();
        }

        geometry.mCoordinates += coordinates;
    }

    void readGeometryCoordinatePoint(MapBoxCoordinates& coordinates)
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1Literal("point"));

        const QXmlStreamAttributes atts = xml.attributes();

        MapBoxPoint point;
        point.x = atts.value(QLatin1Literal("x")).toDouble();
        point.y = atts.value(QLatin1Literal("y")).toDouble();

        coordinates += point;

        xml.skipCurrentElement();
    }

    void readFeatureProperties(MapBoxFeature& feature)
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1Literal("properties"));

        while (xml.readNextStartElement()) {
            if (xml.name() == QLatin1Literal("property"))
                readFeatureProperty(feature);
            else
                readUnknownElement();
        }
    }

    void readFeatureProperty(MapBoxFeature& feature)
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1Literal("property"));

        const QXmlStreamAttributes atts = xml.attributes();
        MapBoxProperty property;
        property.mKey = atts.value(QLatin1Literal("name")).toString();
        property.mValue = atts.value(QLatin1Literal("value")).toString();

        feature.mProperties += property;

        xml.skipCurrentElement();
    }

    void readUnknownElement()
    {
        qDebug() << "Unknown element (fixme):" << xml.name();
        xml.skipCurrentElement();
    }

    QString resolveReference(const QString &fileName, const QString &relativeTo)
    {
//        qDebug() << "resolveReference" << fileName << "relative to" << relativeTo;
        if (fileName.isEmpty())
            return fileName;
        if (fileName == QLatin1Literal("."))
            return relativeTo;
        if (QDir::isRelativePath(fileName)) {
            QString path = relativeTo + QLatin1Char('/') + fileName;
            QFileInfo info(path);
            if (info.exists())
                return info.canonicalFilePath();
            return QDir::cleanPath(path);
        }
        return fileName;
    }

private:
    QString mPath;
    World *mWorld;
    QString mError;
    QXmlStreamReader xml;
};

/////

MapBoxReader::MapBoxReader()
    : d(new MapBoxReaderPrivate)
{
}

MapBoxReader::~MapBoxReader()
{
    delete d;
}

World *MapBoxReader::readWorld(QIODevice *device, const QString &path, World* world)
{
    return d->readWorld(device, path, world);
}

World *MapBoxReader::readWorld(const QString &fileName, World* world)
{
    QFile file(fileName);
    if (!d->openFile(&file))
        return nullptr;

    return readWorld(&file, QFileInfo(fileName).absolutePath(), world);
}

QString MapBoxReader::errorString() const
{
    return d->errorString();
}
