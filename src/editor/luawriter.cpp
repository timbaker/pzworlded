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

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
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

    void writePropertyKeyAndValue(const QByteArray &key, const QString &value)
    {
        bool isDouble;
        value.toDouble(&isDouble);
        if (value == QLatin1String("true")
                || value == QLatin1String("false")
                || isDouble)
            w->writeKeyAndUnquotedValue(key, value.toAscii());
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

        w->writeStartTable("objects");
        foreach (WorldCellObject *obj, objects)
            writeObject(obj);
        w->setSuppressNewlines(false);
        w->writeEndTable();
    }

    void writeObject(WorldCellObject *obj)
    {
        w->writeStartTable();
        w->setSuppressNewlines(true);
        if (!obj->name().isEmpty())
            w->writeKeyAndValue("name", obj->name());
        w->writeKeyAndValue("type", obj->type()->name());
        w->writeKeyAndValue("x", obj->x());
        w->writeKeyAndValue("y", obj->y());
        w->writeKeyAndValue("level", obj->level());
        w->writeKeyAndValue("width", obj->width());
        w->writeKeyAndValue("height", obj->height());

        PropertyList properties;
        resolveProperties(obj, properties);
        writePropertyList(properties);

        w->writeEndTable();
        w->setSuppressNewlines(false);
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

QString LuaWriter::errorString() const
{
    return d->mError;
}
