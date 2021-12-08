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

#include "luawriter.h"

#include "luatablewriter.h"
#include "world.h"
#include "worldcell.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QSet>

using namespace Lua;

class LuaWriterPrivate
{
    Q_DECLARE_TR_FUNCTIONS(LuaWriterPrivate)

public:
    LuaWriterPrivate()
        : mWorld(0)
    {

    }

    bool openFile(QFile *file)
    {
        if (!file->open(QIODevice::WriteOnly)) {
            mError = tr("Could not open file for writing.");
            return false;
        }

        return true;
    }

    void writeWorld(World *world, QIODevice *device, const QString &absDirPath)
    {
        Q_UNUSED(absDirPath)
        mWorld = world;

        LuaTableWriter w(device);
        this->w = &w;

        w.writeStartDocument();
        device->write("function TheWorld()\n");
        w.writeStartReturnTable();

        w.writeStartTable("propertydef");
        foreach (PropertyDef *pd, mWorld->propertyDefinitions())
            writePropertyDef(pd);
        w.writeEndTable();

        w.writeStartTable("cells");
        for (int y = 0; y < mWorld->width(); y++) {
            for (int x = 0; x < mWorld->height(); x++) {
                WorldCell *cell = mWorld->cellAt(x, y);
                writeCell(cell);
            }
        }
        w.writeEndTable();

        w.writeEndTable();
        device->write("\nend");
        w.writeEndDocument();
    }

    void writePropertyDef(PropertyDef *pd)
    {
        w->writeStartTable();
        w->setSuppressNewlines(true);
        w->writeKeyAndValue("name", pd->mName);
        writePropertyKeyAndValue("default", pd->mDefaultValue);
        w->writeEndTable();
        w->setSuppressNewlines(false);
    }

    void writeCell(WorldCell *cell)
    {
        if (cell->isEmpty())
            return;

        w->setSuppressNewlines(false);
        w->writeStartTable();
        w->setSuppressNewlines(true);

        w->writeKeyAndValue("x", cell->x());
        w->writeKeyAndValue("y", cell->y());
        if (!cell->mapFilePath().isEmpty())
            w->writeKeyAndValue("map", QFileInfo(cell->mapFilePath()).completeBaseName());

        PropertyList properties;
        resolveProperties(cell, properties);
        writePropertyList(properties);

        writeLotList(cell->lots());

        writeObjectList(cell->objects());

        if (properties.isEmpty() && cell->lots().isEmpty() && cell->objects().isEmpty())
            w->setSuppressNewlines(true);

        w->writeEndTable();
    }

    void writePropertyList(const PropertyList &properties)
    {
        if (properties.isEmpty())
            return;

        w->setSuppressNewlines(false);
        w->writeStartTable("properties");
        foreach (Property *p, properties)
            writeProperty(p);
        w->setSuppressNewlines(false);
        w->writeEndTable();
    }

    void writeProperty(Property *p)
    {
        w->setSuppressNewlines(false);
        w->writeStartTable();
        w->setSuppressNewlines(true);
        w->writeKeyAndValue("name", p->mDefinition->mName);
        writePropertyKeyAndValue("value", p->mValue);
        w->writeEndTable();
    }

    void writePropertyKeyAndValue(const QByteArray &_key, const QString &value)
    {
        bool isDouble;
        value.toDouble(&isDouble);
#if 1
        QByteArray key = _key;
        bool unquoted = key.length() && !isdigit(key[0]);
        for (int i = 0; i < key.length(); i++) {
            if (key[i] == '_' || isalnum(key[i])) continue;
            unquoted = false;
            break;
        }
        if (!unquoted)
            key = QString::fromUtf8("[\"%1\"]").arg(QString::fromUtf8(_key)).toUtf8();
#endif
        if (value == QLatin1String("true")
                || value == QLatin1String("false")
                || isDouble)
            w->writeKeyAndUnquotedValue(key, value.toUtf8());
        else
            w->writeKeyAndValue(key, value);
    }

    void resolveProperties(PropertyHolder *ph, PropertyList &result)
    {
        foreach (PropertyTemplate *pt, ph->templates())
            resolveProperties(pt, result);
        foreach (Property *p, ph->properties()) {
            result.removeAll(p->mDefinition);
            result += p;
        }
    }

    void writeLotList(const WorldCellLotList &lots)
    {
        if (lots.isEmpty())
            return;

        w->setSuppressNewlines(false);
        w->writeStartTable("lots");
        foreach (WorldCellLot *lot, lots)
            writeLot(lot);
        w->setSuppressNewlines(false);
        w->writeEndTable();
    }

    void writeLot(WorldCellLot *lot)
    {
        w->setSuppressNewlines(false);
        w->writeStartTable();
        w->setSuppressNewlines(true);
        w->writeKeyAndValue("x", lot->x());
        w->writeKeyAndValue("y", lot->y());
        w->writeKeyAndValue("level", lot->level());
        w->writeKeyAndValue("map", QFileInfo(lot->mapName()).completeBaseName());
        w->writeEndTable();
    }

    void writeObjectList(const WorldCellObjectList &objects)
    {
        if (objects.isEmpty())
            return;

        w->setSuppressNewlines(false);
        w->writeStartTable("objects");
        foreach (WorldCellObject *obj, objects)
            writeObject(obj);
        w->setSuppressNewlines(false);
        w->writeEndTable();
    }

    void writeObject(WorldCellObject *obj)
    {
        QPoint origin = mWorld->getGenerateLotsSettings().worldOrigin;

        w->writeStartTable();
        w->setSuppressNewlines(true);
        if (!obj->name().isEmpty())
            w->writeKeyAndValue("name", obj->name());
        w->writeKeyAndValue("type", obj->type()->name());
        if (obj->geometryType() == ObjectGeometryType::INVALID) {
            w->writeKeyAndValue("x", (obj->cell()->x() + origin.x()) * 300 + obj->x());
            w->writeKeyAndValue("y", (obj->cell()->y() + origin.y()) * 300 + obj->y());
            w->writeKeyAndValue("level", obj->level());
            w->writeKeyAndValue("width", obj->width());
            w->writeKeyAndValue("height", obj->height());
        } else {
            QString geometry;
            switch (obj->geometryType()) {
            case ObjectGeometryType::INVALID:
                break;
            case ObjectGeometryType::Point:
                geometry = QLatin1String("point");
                break;
            case ObjectGeometryType::Polygon:
                geometry = QLatin1String("polygon");
                break;
            case ObjectGeometryType::Polyline:
                geometry = QLatin1String("polyline");
                break;
            }
            w->writeKeyAndValue("level", obj->level());
            w->writeKeyAndValue("geometry", geometry);
            if (obj->isPolyline() && (obj->polylineWidth() > 0)) {
                w->writeKeyAndValue("lineWidth", obj->polylineWidth());
            }
            QBuffer buf;
            buf.open(QIODevice::ReadWrite);
            LuaTableWriter w2(&buf);
            w2.setSuppressNewlines(true);
            w2.writeStartTable();
            for (const auto &point : obj->points()) {
                w2.writeValue((obj->cell()->x() + origin.x()) * 300 + point.x);
                w2.writeValue((obj->cell()->x() + origin.x()) * 300 + point.y);
            }
            w2.writeEndTable();
            buf.close();
            w->writeKeyAndUnquotedValue("points", buf.data());
        }

        PropertyList properties;
        resolveProperties(obj, properties);
        writePropertyList(properties);

        w->writeEndTable();
        w->setSuppressNewlines(false);
    }

    void writeSpawnPoints(World *world, QIODevice *device)
    {
        mWorld = world;

        LuaTableWriter w(device);
        this->w = &w;

        w.writeStartDocument();
        device->write("function SpawnPoints()\n");
        w.writeStartReturnTable();

        QMap<QString,WorldCellObjectList> spawnByProfession;
        PropertyDef *pd = mWorld->propertyDefinition(QLatin1String("Professions"));

        for (int y = 0; y < mWorld->height(); y++) {
            for (int x = 0; x < mWorld->width(); x++) {
                WorldCell *cell = mWorld->cellAt(x, y);
                foreach (WorldCellObject *obj, cell->objects()) {
                    if (obj->isSpawnPoint()) {
                        PropertyList properties;
                        resolveProperties(obj, properties);
                        if (Property *p = properties.find(pd)) {
                            QStringList professions = p->mValue.split(QLatin1String(","), QString::SkipEmptyParts);
                            if (professions.contains(QLatin1String("all"))) {
                                if (pd->mEnum)
                                    professions = pd->mEnum->values();
                                professions.removeAll(QLatin1String("all"));
                            }
                            foreach (QString profession, professions)
                                spawnByProfession[profession] += obj;
                        }
                    }
                }
            }
        }

        QPoint origin = mWorld->getGenerateLotsSettings().worldOrigin;

        foreach (QString profession, spawnByProfession.keys()) {
            w.writeStartTable(profession.toUtf8());
            foreach (WorldCellObject *obj, spawnByProfession[profession]) {
                w.writeStartTable();
                w.setSuppressNewlines(true);
                w.writeKeyAndValue("worldX", obj->cell()->x() + origin.x());
                w.writeKeyAndValue("worldY", obj->cell()->y() + origin.y());
                w.writeKeyAndValue("posX", obj->x());
                w.writeKeyAndValue("posY", obj->y());
                w.writeKeyAndValue("posZ", obj->level());

                PropertyList properties;
                resolveProperties(obj, properties);
                foreach (Property *p, properties) {
                    if (p->mDefinition == pd) continue;
                    writePropertyKeyAndValue(p->mDefinition->mName.toUtf8(), p->mValue);
                }

                w.writeEndTable();
                w.setSuppressNewlines(false);
            }
            w.writeEndTable();
        }

        w.writeEndTable();

        device->write("\nend");
        w.writeEndDocument();
    }

    void writeWorldObjects(World *world, QIODevice *device)
    {
        mWorld = world;

        LuaTableWriter w(device);
        this->w = &w;

        w.writeStartDocument();
        w.writeStartTable("objects");

        QPoint origin = mWorld->getGenerateLotsSettings().worldOrigin;

        for (int y = 0; y < mWorld->height(); y++) {
            for (int x = 0; x < mWorld->width(); x++) {
                WorldCell *cell = mWorld->cellAt(x, y);
                foreach (WorldCellObject *obj, cell->objects()) {
                    w.writeStartTable();
                    w.setSuppressNewlines(true);
                    w.writeKeyAndValue("name", obj->name());
                    w.writeKeyAndValue("type", obj->type()->name());
                    if (obj->geometryType() == ObjectGeometryType::INVALID) {
                        w.writeKeyAndValue("x", (obj->cell()->x() + origin.x()) * 300 + obj->x());
                        w.writeKeyAndValue("y", (obj->cell()->y() + origin.y()) * 300 + obj->y());
                        w.writeKeyAndValue("z", obj->level());
                        w.writeKeyAndValue("width", obj->width());
                        w.writeKeyAndValue("height", obj->height());
                    } else {
                        QString geometry;
                        switch (obj->geometryType()) {
                        case ObjectGeometryType::INVALID:
                            break;
                        case ObjectGeometryType::Point:
                            geometry = QLatin1String("point");
                            break;
                        case ObjectGeometryType::Polygon:
                            geometry = QLatin1String("polygon");
                            break;
                        case ObjectGeometryType::Polyline:
                            geometry = QLatin1String("polyline");
                            break;
                        }
                        w.writeKeyAndValue("z", obj->level());
                        w.writeKeyAndValue("geometry", geometry);
                        if (obj->isPolyline() && (obj->polylineWidth() > 0)) {
                            w.writeKeyAndValue("lineWidth", obj->polylineWidth());
                        }
                        QBuffer buf;
                        buf.open(QIODevice::ReadWrite);
                        LuaTableWriter w2(&buf);
                        w2.setSuppressNewlines(true);
                        w2.writeStartTable();
                        QString pointStr;
                        for (const auto &point : obj->points()) {
                            w2.writeValue((obj->cell()->x() + origin.x()) * 300 + point.x);
                            w2.writeValue((obj->cell()->y() + origin.y()) * 300 + point.y);
                        }
                        w2.writeEndTable();
                        buf.close();
                        w.writeKeyAndUnquotedValue("points", buf.data());
                    }
                    PropertyList properties;
                    resolveProperties(obj, properties);
                    if (properties.size()) {

                        // Hack -- See if the "properties { ... }" string is short enough to inline it.
                        QBuffer buf;
                        buf.open(QIODevice::ReadWrite);
                        LuaTableWriter w2(&buf);
                        w2.setSuppressNewlines(true);
                        w2.writeStartTable("properties");
                        this->w = &w2;
                        foreach (Property *p, properties) {
                            writePropertyKeyAndValue(p->mDefinition->mName.toUtf8(), p->mValue);
                        }
                        this->w = &w;
                        w2.writeEndTable();
                        buf.close();
                        bool suppressNewlines = buf.data().length() <= 64; // UTF-8

                        w.setSuppressNewlines(suppressNewlines);
                        w.writeStartTable("properties");
                        foreach (Property *p, properties) {
                            writePropertyKeyAndValue(p->mDefinition->mName.toUtf8(), p->mValue);
                        }
                        w.writeEndTable();
                    }
                    w.writeEndTable();
                    w.setSuppressNewlines(false);
                }
            }
        }

        w.writeEndTable();

        w.writeEndDocument();
    }

    QString mError;
    World *mWorld;
    LuaTableWriter *w;
};

/////

LuaWriter::LuaWriter()
    : d(new LuaWriterPrivate)
{
}

LuaWriter::~LuaWriter()
{
    delete d;
}

bool LuaWriter::writeWorld(World *world, const QString &filePath)
{
    QFile file(filePath);
    if (!d->openFile(&file))
        return false;

    writeWorld(world, &file, QFileInfo(filePath).absolutePath());

    if (file.error() != QFile::NoError) {
        d->mError = file.errorString();
        return false;
    }

    return true;
}

void LuaWriter::writeWorld(World *world, QIODevice *device, const QString &absDirPath)
{
    d->writeWorld(world, device, absDirPath);
}

bool LuaWriter::writeSpawnPoints(World *world, const QString &filePath)
{
    QFile file(filePath);
    if (!d->openFile(&file))
        return false;

    d->writeSpawnPoints(world, &file);

    if (file.error() != QFile::NoError) {
        d->mError = file.errorString();
        return false;
    }

    return true;
}

bool LuaWriter::writeWorldObjects(World *world, const QString &filePath)
{
    QFile file(filePath);
    if (!d->openFile(&file))
        return false;

    d->writeWorldObjects(world, &file);

    if (file.error() != QFile::NoError) {
        d->mError = file.errorString();
        return false;
    }

    return true;
}

QString LuaWriter::errorString() const
{
    return d->mError;
}
