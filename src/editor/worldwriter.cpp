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

#include "worldwriter.h"

#include "bmptotmx.h"
#include "world.h"
#include "worldcell.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QXmlStreamWriter>

class WorldWriterPrivate
{
    Q_DECLARE_TR_FUNCTIONS(WorldWriterPrivate)

public:
    WorldWriterPrivate()
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
        mMapDir = QDir(absDirPath);
        mWorld = world;

        QXmlStreamWriter writer(device);
        writer.setAutoFormatting(true);
        writer.setAutoFormattingIndent(1);

        writer.writeStartDocument();

        writeWorld(writer, world);

        writer.writeEndDocument();
    }

    void writeWorld(QXmlStreamWriter &w, World *world)
    {
        w.writeStartElement(QLatin1String("world"));

        w.writeAttribute(QLatin1String("version"), QLatin1String("1.0"));
        w.writeAttribute(QLatin1String("width"), QString::number(world->width()));
        w.writeAttribute(QLatin1String("height"), QString::number(world->height()));

        writeBMPToTMX(w);
        writeTMXToBMP(w);
        writeGenerateLots(w);
        writeLuaSettings(w);

        foreach (WorldBMP *bmp, world->bmps())
            writeBMP(w, bmp);

        foreach (QString path, world->otherWorlds())
            writeOtherWorld(w, path);

        foreach (PropertyEnum *pe, world->propertyEnums())
            writePropertyEnum(w, pe);
        foreach (PropertyDef *pd, world->propertyDefinitions())
            writePropertyDef(w, pd);
        foreach (PropertyTemplate *pt, world->propertyTemplates())
            writeTemplate(w, pt);

        // *** Types must come before groups
        foreach (ObjectType *ot, world->objectTypes()) {
            if (ot == mWorld->nullObjectType()) continue;
            writeObjectType(w, ot);
        }

        foreach (WorldObjectGroup *og, world->objectGroups()) {
            if (og == mWorld->nullObjectGroup()) continue;
            writeObjectGroup(w, og);
        }

        for (int y = 0; y < world->height(); y++) {
            for (int x = 0; x < world->width(); x++) {
                WorldCell *cell = world->cellAt(x, y);
                writeCell(w, cell);
            }
        }
        w.writeEndElement();
    }

    void writePropertyEnum(QXmlStreamWriter &w, PropertyEnum *pe)
    {
        w.writeStartElement(QLatin1String("propertyenum"));
        w.writeAttribute(QLatin1String("name"), pe->name());
        w.writeAttribute(QLatin1String("choices"), pe->values().join(QLatin1String(",")));
        w.writeAttribute(QLatin1String("multi"), pe->isMulti() ? QLatin1String("true") : QLatin1String("false"));
        w.writeEndElement(); // propertyenum
    }

    void writePropertyDef(QXmlStreamWriter &w, PropertyDef *pd)
    {
        w.writeStartElement(QLatin1String("propertydef"));
        w.writeAttribute(QLatin1String("name"), pd->mName);
        w.writeAttribute(QLatin1String("default"), pd->mDefaultValue);
        if (pd->mEnum)
            w.writeAttribute(QLatin1String("enum"), pd->mEnum->name());
        if (!pd->mDescription.isEmpty()) {
            w.writeStartElement(QLatin1String("description"));
            w.writeCharacters(pd->mDescription);
            w.writeEndElement(); // description
        }
        w.writeEndElement(); // propertydef
    }

    void writeTemplate(QXmlStreamWriter &w, PropertyTemplate *pt)
    {
        w.writeStartElement(QLatin1String("template"));
        w.writeAttribute(QLatin1String("name"), pt->mName);
        if (!pt->mDescription.isEmpty()) {
            w.writeStartElement(QLatin1String("description"));
            w.writeCharacters(pt->mDescription);
            w.writeEndElement(); // description
        }
        foreach (PropertyTemplate *child, pt->templates())
            writeTemplateInstance(w, child);
        foreach (Property *p, pt->properties())
            writeProperty(w, p);
        w.writeEndElement(); // template
    }

    void writeTemplateInstance(QXmlStreamWriter &w, PropertyTemplate *pt)
    {
        w.writeStartElement(QLatin1String("template"));
        w.writeAttribute(QLatin1String("name"), pt->mName);
        w.writeEndElement();
    }

    void writeObjectGroup(QXmlStreamWriter &w, WorldObjectGroup *og)
    {
        w.writeStartElement(QLatin1String("objectgroup"));
        w.writeAttribute(QLatin1String("name"), og->name());
        if (og->color().isValid() &&
                og->color() != WorldObjectGroup::defaultColor())
            w.writeAttribute(QLatin1String("color"), og->color().name());
        if (og->type() != mWorld->nullObjectType())
            w.writeAttribute(QLatin1String("defaulttype"), og->type()->name());
        w.writeEndElement();
    }

    void writeObjectType(QXmlStreamWriter &w, ObjectType *ot)
    {
        w.writeStartElement(QLatin1String("objecttype"));
        w.writeAttribute(QLatin1String("name"), ot->name());
        w.writeEndElement();
    }

    void writeProperty(QXmlStreamWriter &w, Property *p)
    {
        w.writeStartElement(QLatin1String("property"));
        w.writeAttribute(QLatin1String("name"), p->mDefinition->mName);
        w.writeAttribute(QLatin1String("value"), p->mValue);
        w.writeEndElement();
    }

    void writeCell(QXmlStreamWriter &w, WorldCell *cell)
    {
        if (cell->isEmpty())
            return;

        w.writeStartElement(QLatin1String("cell"));
        w.writeAttribute(QLatin1String("x"), QString::number(cell->x()));
        w.writeAttribute(QLatin1String("y"), QString::number(cell->y()));

        QString mapName = cell->mapFilePath();
        if (!mapName.isEmpty()) {
            QFileInfo fi(mapName);
            if (fi.isAbsolute())
                mapName = mMapDir.relativeFilePath(mapName);
        }
        w.writeAttribute(QLatin1String("map"), mapName);

        foreach (PropertyTemplate *pt, cell->templates())
            writeTemplateInstance(w, pt);

        foreach (Property *p, cell->properties())
            writeProperty(w, p);

        foreach (WorldCellLot *lot, cell->lots())
            writeLot(w, lot);

        foreach (WorldCellObject *obj, cell->objects())
            writeObject(w, obj);

        w.writeEndElement();
    }

    void writeLot(QXmlStreamWriter &w, WorldCellLot *lot)
    {
        w.writeStartElement(QLatin1String("lot"));

        w.writeAttribute(QLatin1String("x"), QString::number(lot->x()));
        w.writeAttribute(QLatin1String("y"), QString::number(lot->y()));
        w.writeAttribute(QLatin1String("level"), QString::number(lot->level()));
        w.writeAttribute(QLatin1String("width"), QString::number(lot->width()));
        w.writeAttribute(QLatin1String("height"), QString::number(lot->height()));

        QString mapName = lot->mapName();
        if (!mapName.isEmpty()) {
            QFileInfo fi(mapName);
            if (fi.isAbsolute())
                mapName = mMapDir.relativeFilePath(mapName);
        }
        w.writeAttribute(QLatin1String("map"), mapName);

        w.writeEndElement();
    }

    void writeObject(QXmlStreamWriter &w, WorldCellObject *obj)
    {
        w.writeStartElement(QLatin1String("object"));

        if (!obj->name().isEmpty())
            w.writeAttribute(QLatin1String("name"), obj->name());
        if (!obj->group()->name().isEmpty())
            w.writeAttribute(QLatin1String("group"), obj->group()->name());
        if (!obj->type()->name().isEmpty())
            w.writeAttribute(QLatin1String("type"), obj->type()->name());
        w.writeAttribute(QLatin1String("x"), QString::number(obj->x()));
        w.writeAttribute(QLatin1String("y"), QString::number(obj->y()));
        w.writeAttribute(QLatin1String("level"), QString::number(obj->level()));
        w.writeAttribute(QLatin1String("width"), QString::number(obj->width()));
        w.writeAttribute(QLatin1String("height"), QString::number(obj->height()));
        if (!obj->isVisible())
            w.writeAttribute(QLatin1String("visible"), QLatin1String("0"));

        foreach (PropertyTemplate *pt, obj->templates())
            writeTemplateInstance(w, pt);

        foreach (Property *p, obj->properties())
            writeProperty(w, p);

        w.writeEndElement();
    }

    void writeBMPToTMX(QXmlStreamWriter &w)
    {
        const BMPToTMXSettings settings = mWorld->getBMPToTMXSettings();

        QString path;

        w.writeStartElement(QLatin1String("BMPToTMX"));

        w.writeStartElement(QLatin1String("tmxexportdir"));
        path = relativeFileName(settings.exportDir);
        if (path.isEmpty() && !settings.exportDir.isEmpty())
            path = QLatin1String(".");
        w.writeAttribute(QLatin1String("path"), path);
        w.writeEndElement();

        // If these paths are empty, it means the default file is used.
        w.writeStartElement(QLatin1String("rulesfile"));
        path.clear();
        if (!settings.rulesFile.isEmpty())
            path = relativeFileName(settings.rulesFile);
        w.writeAttribute(QLatin1String("path"), path);
        w.writeEndElement();

        w.writeStartElement(QLatin1String("blendsfile"));
        path.clear();
        if (!settings.blendsFile.isEmpty())
            path = relativeFileName(settings.blendsFile);
        w.writeAttribute(QLatin1String("path"), path);
        w.writeEndElement();

        w.writeStartElement(QLatin1String("mapbasefile"));
        path.clear();
        if (!settings.mapbaseFile.isEmpty())
            path = relativeFileName(settings.mapbaseFile);
        w.writeAttribute(QLatin1String("path"), path);
        w.writeEndElement();

        w.writeStartElement(QLatin1String("assign-maps-to-world"));
        w.writeAttribute(QLatin1String("checked"),
                         QLatin1String(settings.assignMapsToWorld ? "true" : "false"));
        w.writeEndElement();

        w.writeStartElement(QLatin1String("warn-unknown-colors"));
        w.writeAttribute(QLatin1String("checked"),
                         QLatin1String(settings.warnUnknownColors ? "true" : "false"));
        w.writeEndElement();

        w.writeStartElement(QLatin1String("compress"));
        w.writeAttribute(QLatin1String("checked"),
                         QLatin1String(settings.compress ? "true" : "false"));
        w.writeEndElement();

        w.writeStartElement(QLatin1String("copy-pixels"));
        w.writeAttribute(QLatin1String("checked"),
                         QLatin1String(settings.copyPixels ? "true" : "false"));
        w.writeEndElement();

        w.writeStartElement(QLatin1String("update-existing"));
        w.writeAttribute(QLatin1String("checked"),
                         QLatin1String(settings.updateExisting ? "true" : "false"));
        w.writeEndElement();

        w.writeEndElement(); // </BMPToTMX>
    }

    void writeTMXToBMP(QXmlStreamWriter &w)
    {
        const TMXToBMPSettings settings = mWorld->getTMXToBMPSettings();

        QString path;

        w.writeStartElement(QLatin1String("TMXToBMP"));

        w.writeStartElement(QLatin1String("mainImage"));
        w.writeAttribute(QLatin1String("generate"),
                         QLatin1String(settings.doMain ? "true" : "false"));
        w.writeEndElement();

        w.writeStartElement(QLatin1String("vegetationImage"));
        w.writeAttribute(QLatin1String("generate"),
                         QLatin1String(settings.doVegetation ? "true" : "false"));
        w.writeEndElement();

        w.writeStartElement(QLatin1String("buildingsImage"));
        path.clear();
        if (!settings.buildingsFile.isEmpty())
            path = relativeFileName(settings.buildingsFile);
        w.writeAttribute(QLatin1String("path"), path);
        w.writeAttribute(QLatin1String("generate"),
                         QLatin1String(settings.doBuildings ? "true" : "false"));
        w.writeEndElement();

        w.writeEndElement(); // </TMXToBMP>
    }

    void writeGenerateLots(QXmlStreamWriter &w)
    {
        const GenerateLotsSettings &settings = mWorld->getGenerateLotsSettings();

        w.writeStartElement(QLatin1String("GenerateLots"));
        w.writeStartElement(QLatin1String("exportdir"));
        w.writeAttribute(QLatin1String("path"),
                         relativeFileName(settings.exportDir));
        w.writeEndElement();
        w.writeStartElement(QLatin1String("ZombieSpawnMap"));
        w.writeAttribute(QLatin1String("path"),
                         relativeFileName(settings.zombieSpawnMap));
        w.writeEndElement();
        w.writeStartElement(QLatin1String("TileDefFolder"));
        w.writeAttribute(QLatin1String("path"),
                         relativeFileName(settings.tileDefFolder));
        w.writeEndElement();
        w.writeStartElement(QLatin1String("worldOrigin"));
        writePoint(w, QLatin1String("origin"), settings.worldOrigin);
        w.writeEndElement();
        w.writeEndElement(); // </GenerateLots>
    }

    void writeLuaSettings(QXmlStreamWriter &w)
    {
        const LuaSettings &settings = mWorld->getLuaSettings();

        w.writeStartElement(QLatin1String("LuaSettings"));
        w.writeStartElement(QLatin1String("spawnPointsFile"));
        w.writeAttribute(QLatin1String("path"),
                         relativeFileName(settings.spawnPointsFile));
        w.writeEndElement();
        w.writeStartElement(QLatin1String("worldObjectsFile"));
        w.writeAttribute(QLatin1String("path"),
                         relativeFileName(settings.worldObjectsFile));
        w.writeEndElement();
        w.writeEndElement();
    }

    void writeBMP(QXmlStreamWriter &w, WorldBMP *bmp)
    {
        w.writeStartElement(QLatin1String("bmp"));

        w.writeAttribute(QLatin1String("path"), relativeFileName(bmp->filePath()));
        w.writeAttribute(QLatin1String("x"), QString::number(bmp->x()));
        w.writeAttribute(QLatin1String("y"), QString::number(bmp->y()));
        w.writeAttribute(QLatin1String("width"), QString::number(bmp->width()));
        w.writeAttribute(QLatin1String("height"), QString::number(bmp->height()));

        w.writeEndElement();
    }

    void writeOtherWorld(QXmlStreamWriter &w, const QString &path)
    {
        w.writeStartElement(QLatin1String("otherworld"));
        w.writeAttribute(QLatin1String("path"), relativeFileName(path));
        w.writeEndElement();
    }

    void writePoint(QXmlStreamWriter &w, const QString &name, const QPoint &p)
    {
        write2Int(w, name, p.x(), p.y());
    }

    void write2Int(QXmlStreamWriter &w, const QString &name, int v1, int v2)
    {
        QString value = QString::number(v1) + QLatin1Char(',') + QString::number(v2);
        w.writeAttribute(name, value);
    }

    QString relativeFileName(const QString &path)
    {
        if (!path.isEmpty()) {
            QFileInfo fi(path);
            if (fi.isAbsolute())
                return mMapDir.relativeFilePath(path);
        }
        return path;
    }

    World *mWorld;
    QString mError;
    QDir mMapDir;
};

/////

WorldWriter::WorldWriter()
    : d(new WorldWriterPrivate)
{
}

WorldWriter::~WorldWriter()
{
    delete d;
}

bool WorldWriter::writeWorld(World *world, const QString &filePath)
{
    QTemporaryFile tempFile;
    if (!d->openFile(&tempFile))
        return false;

    writeWorld(world, &tempFile, QFileInfo(filePath).absolutePath());

    if (tempFile.error() != QFile::NoError) {
        d->mError = tempFile.errorString();
        return false;
    }

    // foo.pzw -> foo.pzw.bak
    QFileInfo destInfo(filePath);
    QString backupPath = filePath + QLatin1String(".bak");
    QFile backupFile(backupPath);
    if (destInfo.exists()) {
        if (backupFile.exists()) {
            if (!backupFile.remove()) {
                d->mError = QString(QLatin1String("Error deleting file!\n%1\n\n%2"))
                        .arg(backupPath)
                        .arg(backupFile.errorString());
                return false;
            }
        }
        QFile destFile(filePath);
        if (!destFile.rename(backupPath)) {
            d->mError = QString(QLatin1String("Error renaming file!\nFrom: %1\nTo: %2\n\n%3"))
                    .arg(filePath)
                    .arg(backupPath)
                    .arg(destFile.errorString());
            return false;
        }
    }

    // /tmp/tempXYZ -> foo.pzw
    tempFile.close();
    if (!tempFile.rename(filePath)) {
        d->mError = QString(QLatin1String("Error renaming file!\nFrom: %1\nTo: %2\n\n%3"))
                .arg(tempFile.fileName())
                .arg(filePath)
                .arg(tempFile.errorString());
        // Try to un-rename the backup file
        if (backupFile.exists())
            backupFile.rename(filePath); // might fail
        return false;
    }

    // If anything above failed, the temp file should auto-remove, but not after
    // a successful save.
    tempFile.setAutoRemove(false);

    return true;
}

void WorldWriter::writeWorld(World *world, QIODevice *device, const QString &absDirPath)
{
    d->writeWorld(world, device, absDirPath);
}

QString WorldWriter::errorString() const
{
    return d->mError;
}
