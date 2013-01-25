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

#include "worldreader.h"

#include "world.h"
#include "worldcell.h"

#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>

class WorldReaderPrivate
{
    Q_DECLARE_TR_FUNCTIONS(MapReader)

public:
    WorldReaderPrivate()
        : mWorld(0)
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

    World *readWorld(QIODevice *device, const QString &path)
    {
        mError.clear();
        mPath = path;
        World *world = 0;

        xml.setDevice(device);

        if (xml.readNextStartElement() && xml.name() == "world") {
            world = readWorld();
        } else {
            xml.raiseError(tr("Not a world file."));
        }

        return world;
    }

private:
    World *readWorld()
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == "world");

        const QXmlStreamAttributes atts = xml.attributes();
        const int width =
                atts.value(QLatin1String("width")).toString().toInt();
        const int height =
                atts.value(QLatin1String("height")).toString().toInt();

        mWorld = new World(width, height);

        while (xml.readNextStartElement()) {
            if (xml.name() == "propertydef")
                readPropertyDef();
            else if (xml.name() == "template")
                readTemplate();
            else if (xml.name() == "objectgroup")
                readObjectGroup();
            else if (xml.name() == "objecttype")
                readObjectType();
            else if (xml.name() == "road")
                readRoad();
            else if (xml.name() == "cell")
                readCell();
            else if (xml.name() == "BMPToTMX")
                readBMPToTMX();
            else if (xml.name() == "GenerateLots")
                readGenerateLots();
            else if (xml.name() == "bmp")
                readBMP();
            else
                readUnknownElement();
        }

        // Clean up in case of error
        if (xml.hasError()) {
            delete mWorld;
            mWorld = 0;
        }

        return mWorld;
    }

    void readPropertyDef()
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == "propertydef");

        const QXmlStreamAttributes atts = xml.attributes();
        const QString name = atts.value(QLatin1String("name")).toString();
        const QString defaultValue = atts.value(QLatin1String("default")).toString();
        QString desc;

        while (xml.readNextStartElement()) {
            if (xml.name() == "description") {
                desc = readDescription();
            } else
                readUnknownElement();
        }

        PropertyDef *pd = new PropertyDef(name, defaultValue, desc);
        mWorld->addPropertyDefinition(mWorld->propertyDefinitions().size(), pd);
    }

    QString readDescription()
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == "description");

        QString result;
        while (xml.readNext() != QXmlStreamReader::Invalid) {
            if (xml.isStartElement() || xml.isEndElement())
                break;
            if (xml.isCharacters() && !xml.isWhitespace())
                result = xml.text().toString();
        }
        return result;
    }

    void readTemplate()
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == "template");

        const QXmlStreamAttributes atts = xml.attributes();
        const QString name = atts.value(QLatin1String("name")).toString();
        const QString desc;

        PropertyTemplate *pt = new PropertyTemplate;
        pt->mName = name;
        mWorld->addPropertyTemplate(mWorld->propertyTemplates().size(), pt);

        while (xml.readNextStartElement()) {
            if (xml.name() == "description") {
                pt->mDescription = readDescription();
            } else if (xml.name() == "template") {
                readTemplateInstance(pt);
            } else if (xml.name() == "property") {
                readProperty(pt);
            } else
                readUnknownElement();
        }
    }

    void readObjectGroup()
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == "objectgroup");

        const QXmlStreamAttributes atts = xml.attributes();
        const QString name = atts.value(QLatin1String("name")).toString();
        if (name.isEmpty()) {
            xml.raiseError(tr("empty objectgroup name"));
            return;
        }

        WorldObjectGroup *og = mWorld->objectGroups().find(name);
        if (og) {
            xml.raiseError(tr("duplicate objectgroup \"%1\"").arg(name));
            return;
        }

        QString colorName = atts.value(QLatin1String("color")).toString();
        QColor color;
        if (!colorName.isEmpty())
            color = QColor(colorName);

        ObjectType *type = mWorld->nullObjectType();
        const QString typeName = atts.value(QLatin1String("defaulttype")).toString();
        if (!(type = mWorld->objectTypes().find(typeName))) {
            xml.raiseError(tr("unknown objecttype \"%1\"").arg(typeName));
            return;
        }

        og = new WorldObjectGroup(mWorld, name, color);
        og->setType(type);
        mWorld->insertObjectGroup(mWorld->objectGroups().size(), og);

        xml.skipCurrentElement();
    }

    void readObjectType()
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == "objecttype");

        const QXmlStreamAttributes atts = xml.attributes();
        const QString name = atts.value(QLatin1String("name")).toString();
        if (name.isEmpty()) {
            xml.raiseError(tr("empty objecttype name"));
            return;
        }

        ObjectType *ot = mWorld->objectTypes().find(name);
        if (ot) {
            xml.raiseError(tr("duplicate objecttype \"%1\"").arg(name));
            return;
        }

        ot = new ObjectType(name);
        mWorld->insertObjectType(mWorld->objectTypes().size(), ot);

        xml.skipCurrentElement();
    }

    void readProperty(PropertyHolder *ph)
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == "property");

        const QXmlStreamAttributes atts = xml.attributes();
        const QString name = atts.value(QLatin1String("name")).toString();
        const QString value = atts.value(QLatin1String("value")).toString();

        PropertyDef *pd = mWorld->propertyDefinitions().findPropertyDef(name);
        if (!pd) {
            xml.raiseError(tr("property has unknown propertydef \"%1\"").arg(name));
            return;
        }

        Property *p = new Property(pd, value);
        ph->addProperty(ph->properties().size(), p);

        xml.skipCurrentElement();
    }

    void readTemplateInstance(PropertyHolder *ph)
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == "template");

        const QXmlStreamAttributes atts = xml.attributes();
        const QString name = atts.value(QLatin1String("name")).toString();

        PropertyTemplate *pt = mWorld->propertyTemplates().find(name);
        if (!pt) {
            xml.raiseError(tr("unknown template \"%1\"").arg(name));
            return;
        }

        ph->addTemplate(ph->templates().size(), pt);

        xml.skipCurrentElement();
    }

    void readRoad()
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == "road");

        const QXmlStreamAttributes atts = xml.attributes();

        const int x1 =  atts.value(QLatin1String("x1")).toString().toInt();
        const int y1 = atts.value(QLatin1String("y1")).toString().toInt();
        const int x2 =  atts.value(QLatin1String("x2")).toString().toInt();
        const int y2 = atts.value(QLatin1String("y2")).toString().toInt();
        const int width = atts.value(QLatin1String("width")).toString().toInt();
        QString tileName = atts.value(QLatin1String("tile")).toString();
        QString linesName = atts.value(QLatin1String("traffic-lines")).toString();
        TrafficLines *lines = RoadTemplates::instance()->findLines(linesName);

        // No check wanted/needed on Object coordinates
        Road *road = new Road(mWorld, x1, y1, x2, y2, width, -1);
        road->setTileName(tileName);
        road->setTrafficLines(lines);
        mWorld->insertRoad(mWorld->roads().size(), road);

        while (xml.readNextStartElement()) {
            readUnknownElement();
        }
    }

    void readCell()
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == "cell");

        const QXmlStreamAttributes atts = xml.attributes();
        const int x =
                atts.value(QLatin1String("x")).toString().toInt();
        const int y =
                atts.value(QLatin1String("y")).toString().toInt();
        const QString mapName = atts.value(QLatin1String("map")).toString();

        if (!mWorld->contains(x, y))
            xml.raiseError(tr("Invalid cell coodinates %1,%2").arg(x).arg(y));
        else {
            WorldCell *cell = mWorld->cellAt(x, y);
            cell->setMapFilePath(resolveReference(mapName, mPath));

            while (xml.readNextStartElement()) {
                if (xml.name() == "template")
                    readTemplateInstance(cell);
                else if (xml.name() == "property")
                    readProperty(cell);
                else if (xml.name() == "lot")
                    readLot(cell);
                else if (xml.name() == "object")
                    readObject(cell);
                else
                    readUnknownElement();
            }

        }
    }

    void readLot(WorldCell *cell)
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == "lot");

        const QXmlStreamAttributes atts = xml.attributes();
        const int x =
                atts.value(QLatin1String("x")).toString().toInt();
        const int y =
                atts.value(QLatin1String("y")).toString().toInt();
        const int level =
                atts.value(QLatin1String("level")).toString().toInt();
        const QString mapName = atts.value(QLatin1String("map")).toString();
        const int width =
                atts.value(QLatin1String("width")).toString().toInt();
        const int height =
                atts.value(QLatin1String("height")).toString().toInt();

        // No check wanted/needed on Lot coordinates
        cell->addLot(resolveReference(mapName, mPath), x, y, level, width, height);

        xml.skipCurrentElement();
    }

    void readObject(WorldCell *cell)
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == "object");

        const QXmlStreamAttributes atts = xml.attributes();
        const QString name = atts.value(QLatin1String("name")).toString();

        const QString group = atts.value(QLatin1String("group")).toString();
        WorldObjectGroup *objGroup = mWorld->objectGroups().find(group);
        if (!objGroup) {
            xml.raiseError(tr("unknown object group \"%1\"").arg(group));
            return;
        }

        const QString type = atts.value(QLatin1String("type")).toString();
        ObjectType *objType = mWorld->objectTypes().find(type);
        if (!objType) {
            xml.raiseError(tr("unknown object type \"%1\"").arg(type));
            return;
        }

        const qreal x =
                atts.value(QLatin1String("x")).toString().toDouble();
        const qreal y =
                atts.value(QLatin1String("y")).toString().toDouble();
        int level =
                atts.value(QLatin1String("level")).toString().toInt();
        if (level < 0) level = 0;
        if (level > 500) level = 500;
        const qreal width =
                atts.value(QLatin1String("width")).toString().toDouble();
        const qreal height =
                atts.value(QLatin1String("height")).toString().toDouble();

        // No check wanted/needed on Object coordinates
        WorldCellObject *obj = new WorldCellObject(cell, name, objType, objGroup,
                                                   x, y, level, width, height);
        cell->insertObject(cell->objects().size(), obj);

        while (xml.readNextStartElement()) {
            if (xml.name() == "template")
                readTemplateInstance(obj);
            else if (xml.name() == "property")
                readProperty(obj);
            else
                readUnknownElement();
        }
    }

    void readBMPToTMX()
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == "BMPToTMX");

        BMPToTMXSettings settings;

        while (xml.readNextStartElement()) {
            if (xml.name() == "tmxexportdir") {
                QString path = xml.attributes().value(QLatin1String("path")).toString();
                settings.exportDir = resolveReference(path, mPath);
                xml.skipCurrentElement();
            } else if (xml.name() == "rulesfile") {
                QString path = xml.attributes().value(QLatin1String("path")).toString();
                settings.rulesFile = resolveReference(path, mPath);
                xml.skipCurrentElement();
            } else if (xml.name() == "blendsfile") {
                QString path = xml.attributes().value(QLatin1String("path")).toString();
                settings.blendsFile = resolveReference(path, mPath);
                xml.skipCurrentElement();
            } else if (xml.name() == "mapbasefile") {
                QString path = xml.attributes().value(QLatin1String("path")).toString();
                settings.mapbaseFile = resolveReference(path, mPath);
                xml.skipCurrentElement();
            } else if (xml.name() == "assign-maps-to-world") {
                QString value = xml.attributes().value(QLatin1String("checked")).toString();
                settings.assignMapsToWorld = value == QLatin1String("true");
                xml.skipCurrentElement();
            } else if (xml.name() == "warn-unknown-colors") {
                QString value = xml.attributes().value(QLatin1String("checked")).toString();
                settings.warnUnknownColors = value == QLatin1String("true");
                xml.skipCurrentElement();
            } else if (xml.name() == "compress") {
                QString value = xml.attributes().value(QLatin1String("checked")).toString();
                settings.compress = value == QLatin1String("true");
                xml.skipCurrentElement();
            } else
                readUnknownElement();
        }

        mWorld->setBMPToTMXSettings(settings);
    }

    void readGenerateLots()
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == "GenerateLots");

        GenerateLotsSettings settings;

        while (xml.readNextStartElement()) {
            if (xml.name() == "exportdir") {
                QString path = xml.attributes().value(QLatin1String("path")).toString();
                settings.exportDir = resolveReference(path, mPath);
                xml.skipCurrentElement();
            } else if (xml.name() == "ZombieSpawnMap") {
                QString path = xml.attributes().value(QLatin1String("path")).toString();
                settings.zombieSpawnMap = resolveReference(path, mPath);
                xml.skipCurrentElement();
            } else
                readUnknownElement();
        }

        mWorld->setGenerateLotsSettings(settings);
    }

    void readBMP()
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == "bmp");

        const QXmlStreamAttributes atts = xml.attributes();
        QString path = atts.value(QLatin1String("path")).toString();
        path = resolveReference(path, mPath);

        const int x = atts.value(QLatin1String("x")).toString().toInt();
        const int y = atts.value(QLatin1String("y")).toString().toInt();
        const int width = atts.value(QLatin1String("width")).toString().toInt();
        const int height = atts.value(QLatin1String("height")).toString().toInt();

        // No check wanted/needed on BMP coordinates
        WorldBMP *bmp = new WorldBMP(mWorld, x, y, width, height, path);
        mWorld->insertBmp(mWorld->bmps().count(), bmp);

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
        if (fileName == QLatin1String("."))
            return relativeTo;
        if (QDir::isRelativePath(fileName)) {
            QString path = relativeTo + QLatin1Char('/') + fileName;
            QFileInfo info(path);
            if (info.exists())
                return info.canonicalFilePath();
            return path;
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

WorldReader::WorldReader()
    : d(new WorldReaderPrivate)
{
}

WorldReader::~WorldReader()
{
    delete d;
}

World *WorldReader::readWorld(QIODevice *device, const QString &path)
{
    return d->readWorld(device, path);
}

World *WorldReader::readWorld(const QString &fileName)
{
    QFile file(fileName);
    if (!d->openFile(&file))
        return 0;

    return readWorld(&file, QFileInfo(fileName).absolutePath());
}

QString WorldReader::errorString() const
{
    return d->errorString();
}
