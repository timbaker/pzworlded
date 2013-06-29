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

#include "buildingwriter.h"

#include "building.h"
#include "buildingfloor.h"
#include "buildingobjects.h"
#include "buildingtemplates.h"
#include "buildingtiles.h"
#include "furnituregroups.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QXmlStreamWriter>

using namespace BuildingEditor;

#define VERSION1 1
#define VERSION2 2
#define VERSION_LATEST VERSION2

#ifdef Q_OS_WIN
// Hmmmm.  libtiled.dll defines the Properties class as so:
// class TILEDSHARED_EXPORT Properties : public QMap<QString,QString>
// Suddenly I'm getting a 'multiply-defined symbol' error.
// I found the solution here:
// http://www.archivum.info/qt-interest@trolltech.com/2005-12/00242/RE-Linker-Problem-while-using-QMap.html
template class __declspec(dllimport) QMap<QString, QString>;
#endif

class BuildingEditor::BuildingWriterPrivate
{
    Q_DECLARE_TR_FUNCTIONS(BuildingWriterPrivate)

public:
    BuildingWriterPrivate()
        : mBuilding(0)
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

    void writeBuilding(Building *building, QIODevice *device, const QString &absDirPath)
    {
        mMapDir = QDir(absDirPath);
        mBuilding = building;

        QXmlStreamWriter writer(device);
        writer.setAutoFormatting(true);
        writer.setAutoFormattingIndent(1);

        writer.writeStartDocument();

        writeBuilding(writer, building);

        writer.writeEndDocument();
    }

    void writeBuilding(QXmlStreamWriter &w, Building *building)
    {
        w.writeStartElement(QLatin1String("building"));

        initBuildingTileEntries();

        w.writeAttribute(QLatin1String("version"), QString::number(VERSION_LATEST));
        w.writeAttribute(QLatin1String("width"), QString::number(building->width()));
        w.writeAttribute(QLatin1String("height"), QString::number(building->height()));

        for (int i = 0; i < Building::TileCount; i++)
            w.writeAttribute(building->enumToString(i), entryIndex(building->tile(i)));

        writeBuildingTileEntries(w);

        writeFurniture(w);

        writeUserTiles(w);

        w.writeStartElement(QLatin1String("used_tiles"));
        QStringList usedTiles;
        foreach (BuildingTileEntry *entry, mBuilding->usedTiles())
            usedTiles += entryIndex(entry);
        w.writeCharacters(usedTiles.join(QLatin1String(" ")));
        w.writeEndElement();

        w.writeStartElement(QLatin1String("used_furniture"));
        QStringList usedFurniture;
        foreach (FurnitureTiles *ftiles, mBuilding->usedFurniture())
            usedFurniture += furnitureIndex(ftiles);
        w.writeCharacters(usedFurniture.join(QLatin1String(" ")));
        w.writeEndElement();

        foreach (Room *room, building->rooms())
            writeRoom(w, room);

        foreach (BuildingFloor *floor, building->floors())
            writeFloor(w, floor);

        w.writeEndElement(); // </building>
    }

    void writeRoom(QXmlStreamWriter &w, Room *room)
    {
        w.writeStartElement(QLatin1String("room"));
        w.writeAttribute(QLatin1String("Name"), room->Name);
        w.writeAttribute(QLatin1String("InternalName"), room->internalName);
        QString colorString = QString(QLatin1String("%1 %2 %3"))
                .arg(qRed(room->Color))
                .arg(qGreen(room->Color))
                .arg(qBlue(room->Color));
        w.writeAttribute(QLatin1String("Color"), colorString);
        for (int i = 0; i < Room::TileCount; i++)
            w.writeAttribute(room->enumToString(i), entryIndex(room->tile(i)));
        w.writeEndElement(); // </room>
    }

    void writeFurniture(QXmlStreamWriter &w)
    {
        foreach (BuildingFloor *floor, mBuilding->floors()) {
            foreach (BuildingObject *object, floor->objects()) {
                if (FurnitureObject *furniture = object->asFurniture()) {
                    FurnitureTiles *ftiles = furniture->furnitureTile()->owner();
                    if (!mFurnitureTiles.contains(ftiles))
                        mFurnitureTiles += ftiles;
                }
            }
        }

        foreach (FurnitureTiles *ftiles, mBuilding->usedFurniture()) {
            if (!mFurnitureTiles.contains(ftiles))
                mFurnitureTiles += ftiles;
        }

        foreach (FurnitureTiles *ftiles, mFurnitureTiles) {
            w.writeStartElement(QLatin1String("furniture"));
            if (ftiles->hasCorners())
                writeBoolean(w, QLatin1String("corners"), ftiles->hasCorners());
            if (ftiles->layer() != FurnitureTiles::LayerFurniture)
                w.writeAttribute(QLatin1String("layer"), ftiles->layerToString());
            writeFurnitureTile(w, ftiles->tile(FurnitureTile::FurnitureW));
            writeFurnitureTile(w, ftiles->tile(FurnitureTile::FurnitureN));
            writeFurnitureTile(w, ftiles->tile(FurnitureTile::FurnitureE));
            writeFurnitureTile(w, ftiles->tile(FurnitureTile::FurnitureS));
            if (ftiles->hasCorners()) {
                writeFurnitureTile(w, ftiles->tile(FurnitureTile::FurnitureSW));
                writeFurnitureTile(w, ftiles->tile(FurnitureTile::FurnitureNW));
                writeFurnitureTile(w, ftiles->tile(FurnitureTile::FurnitureNE));
                writeFurnitureTile(w, ftiles->tile(FurnitureTile::FurnitureSE));
            }
            w.writeEndElement(); // </furniture>
        }
    }

    void writeFurnitureTile(QXmlStreamWriter &w, FurnitureTile *ftile)
    {
        if (ftile->isEmpty())
            return;
        w.writeStartElement(QLatin1String("entry"));
        w.writeAttribute(QLatin1String("orient"), ftile->orientToString());
        if (!ftile->allowGrime())
            w.writeAttribute(QLatin1String("grime"), QLatin1String("false"));
        for (int x = 0; x < ftile->width(); x++)
            for (int y = 0; y < ftile->height(); y++)
                writeFurnitureTile(w, x, y, ftile->tile(x, y));
        w.writeEndElement(); // </entry>
    }

    void writeFurnitureTile(QXmlStreamWriter &w, int x, int y, BuildingTile *btile)
    {
        if (!btile)
            return;
        w.writeStartElement(QLatin1String("tile"));
        w.writeAttribute(QLatin1String("x"), QString::number(x));
        w.writeAttribute(QLatin1String("y"), QString::number(y));
        w.writeAttribute(QLatin1String("name"), btile->name());
        w.writeEndElement(); // </tile>
    }

    void initBuildingTileEntries()
    {
        foreach (BuildingTileEntry *entry, mBuilding->tiles())
            addEntry(entry);

        foreach (Room *room, mBuilding->rooms()) {
            foreach (BuildingTileEntry *entry, room->tiles())
                addEntry(entry);
        }

        foreach (BuildingFloor *floor, mBuilding->floors()) {
            foreach (BuildingObject *object, floor->objects()) {
#if 1
                foreach (BuildingTileEntry *entry, object->tiles())
                    addEntry(entry);
#else
                addEntry(object->tile());
                if (Door *door = object->asDoor()) {
                    addEntry(door->frameTile());
                } else if (RoofObject *roof = object->asRoof()) {
                    addEntry(roof->capTiles());
                    addEntry(roof->slopeTiles());
                    addEntry(roof->topTiles());
                } else if (WallObject *wall = object->asWall()) {
                    addEntry(wall->tile(WallObject::TileInterior));
                } else if (Window *window = object->asWindow()) {
                    addEntry(window->curtainsTile());
                }
#endif
            }
        }

        foreach (BuildingTileEntry *entry, mBuilding->usedTiles())
            addEntry(entry);
    }

    void writeBuildingTileEntries(QXmlStreamWriter &w)
    {
        foreach (BuildingTileEntry *entry, mTileEntries) {
            writeBuildingTileEntry(w, entry);
        }
    }

    void writeBuildingTileEntry(QXmlStreamWriter &w, BuildingTileEntry *entry)
    {
        w.writeStartElement(QLatin1String("tile_entry"));
        w.writeAttribute(QLatin1String("category"), entry->category()->name());
        for (int i = 0; i < entry->tileCount(); i++)
            writeBuildingTile(w, entry, i);
        w.writeEndElement(); // </tile_entry>
    }

    void writeBuildingTile(QXmlStreamWriter &w, BuildingTileEntry *entry, int index)
    {
        w.writeStartElement(QLatin1String("tile"));
        w.writeAttribute(QLatin1String("enum"), entry->category()->enumToString(index));
        w.writeAttribute(QLatin1String("tile"), entry->tile(index)->name());
        if (!entry->offset(index).isNull())
            writePoint(w, QLatin1String("offset"), entry->offset(index));
        w.writeEndElement(); // </tile>
    }

    void writeUserTiles(QXmlStreamWriter &w)
    {
        foreach (BuildingFloor *floor, mBuilding->floors()) {
            foreach (QString layerName, floor->grimeLayers()) {
                for (int x = 0; x <= floor->width(); x++) {
                    for (int y = 0; y <= floor->height(); y++) {
                        QString tileName = floor->grimeAt(layerName, x, y);
                        if (!tileName.isEmpty() && !mUserTilesMap.contains(tileName))
                            mUserTilesMap[tileName] = tileName;
                    }
                }
            }
        }

        w.writeStartElement(QLatin1String("user_tiles"));
        foreach (QString tileName, mUserTilesMap.values()) { // sorted
            w.writeStartElement(QLatin1String("tile"));
            w.writeAttribute(QLatin1String("tile"), tileName);
            w.writeEndElement(); // </tile>

        }
        w.writeEndElement(); // </user_tiles>
    }

    void writeFloor(QXmlStreamWriter &w, BuildingFloor *floor)
    {
        w.writeStartElement(QLatin1String("floor"));
//        w.writeAttribute(QLatin1String("level"), QString::number(floor->level()));

        foreach (BuildingObject *object, floor->objects())
            writeObject(w, object);

        // Write room indices.
        QString text;
        const QLatin1Char zero('0'), comma(','), newline('\n');
        text += newline;
        int count = 0, max = floor->height() * floor->width();
        for (int y = 0; y < floor->height(); y++) {
            for (int x = 0; x < floor->width(); x++) {
                if (Room *room = floor->GetRoomAt(x, y))
                    text += QString::number(mBuilding->rooms().indexOf(room) + 1);
                else
                    text += zero;
                if (++count < max)
                    text += comma;
            }
            text += newline;
        }
        w.writeStartElement(QLatin1String("rooms"));
        w.writeCharacters(text);
        w.writeEndElement();

        // Write user tile indices.
        foreach (QString layerName, floor->grimeLayers()) {
            if (floor->grime()[layerName]->isEmpty())
                continue;
            text.clear();
            text += newline;
            count = 0, max = (floor->height() + 1) * (floor->width() + 1);
            for (int y = 0; y <= floor->height(); y++) {
                for (int x = 0; x <= floor->width(); x++) {
                    QString tileName = floor->grimeAt(layerName, x, y);
                    if (tileName.isEmpty())
                        text += zero;
                    else
                        text += QString::number(mUserTilesMap.values().indexOf(tileName) + 1);
                    if (++count < max)
                        text += comma;
                }
                text += newline;
            }
            w.writeStartElement(QLatin1String("tiles"));
            w.writeAttribute(QLatin1String("layer"), layerName);
            w.writeCharacters(text);
            w.writeEndElement();
        }

        w.writeEndElement(); // </floor>
    }

    void writeObject(QXmlStreamWriter &w, BuildingObject *object)
    {
        w.writeStartElement(QLatin1String("object"));
        bool writeDir = true, writeTile = true;
        if (Door *door = object->asDoor()) {
            w.writeAttribute(QLatin1String("type"), QLatin1String("door"));
            w.writeAttribute(QLatin1String("FrameTile"), entryIndex(door->frameTile()));
        } else if (Window *window = object->asWindow()) {
            w.writeAttribute(QLatin1String("type"), QLatin1String("window"));
            w.writeAttribute(QLatin1String("CurtainsTile"), entryIndex(window->curtainsTile()));
        } else if (object->asStairs())
            w.writeAttribute(QLatin1String("type"), QLatin1String("stairs"));
        else if (FurnitureObject *furniture = object->asFurniture()) {
            w.writeAttribute(QLatin1String("type"), QLatin1String("furniture"));

            FurnitureTile *ftile = furniture->furnitureTile();
            w.writeAttribute(QLatin1String("FurnitureTiles"), furnitureIndex(ftile->owner()));
            w.writeAttribute(QLatin1String("orient"), ftile->orientToString());

            writeDir = false;
            writeTile = false;
        } else if (RoofObject *roof = object->asRoof()) {
            w.writeAttribute(QLatin1String("type"), QLatin1String("roof"));
            w.writeAttribute(QLatin1String("width"), QString::number(roof->width()));
            w.writeAttribute(QLatin1String("height"), QString::number(roof->height()));
            w.writeAttribute(QLatin1String("RoofType"), roof->typeToString());
            w.writeAttribute(QLatin1String("Depth"), roof->depthToString());
            writeBoolean(w, QLatin1String("cappedW"), roof->isCappedW());
            writeBoolean(w, QLatin1String("cappedN"), roof->isCappedN());
            writeBoolean(w, QLatin1String("cappedE"), roof->isCappedE());
            writeBoolean(w, QLatin1String("cappedS"), roof->isCappedS());
            w.writeAttribute(QLatin1String("CapTiles"), entryIndex(roof->capTiles()));
            w.writeAttribute(QLatin1String("SlopeTiles"), entryIndex(roof->slopeTiles()));
            w.writeAttribute(QLatin1String("TopTiles"), entryIndex(roof->topTiles()));
            writeDir = false;
            writeTile = false;
        } else if (WallObject *wall = object->asWall()) {
            w.writeAttribute(QLatin1String("type"), QLatin1String("wall"));
            w.writeAttribute(QLatin1String("length"), QString::number(wall->length()));
            w.writeAttribute(QLatin1String("InteriorTile"), entryIndex(wall->tile(WallObject::TileInterior)));
            w.writeAttribute(QLatin1String("ExteriorTrim"), entryIndex(wall->tile(WallObject::TileExteriorTrim)));
            w.writeAttribute(QLatin1String("InteriorTrim"), entryIndex(wall->tile(WallObject::TileInteriorTrim)));
        } else {
            qFatal("Unhandled object type in BuildingWriter::writeObject");
        }
        w.writeAttribute(QLatin1String("x"), QString::number(object->x()));
        w.writeAttribute(QLatin1String("y"), QString::number(object->y()));
        if (writeDir)
            w.writeAttribute(QLatin1String("dir"), object->dirString());
        if (writeTile)
            w.writeAttribute(QLatin1String("Tile"), entryIndex(object->tile()));
        w.writeEndElement(); // </object>
    }

    void writeBoolean(QXmlStreamWriter &w, const QString &name, bool value)
    {
        w.writeAttribute(name, value ? QLatin1String("true") : QLatin1String("false"));
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

    QString nameForEntry(BuildingTileEntry *entry)
    {
        QString name = entry->category()->name();
        for (int i = 0; i < entry->category()->enumCount(); i++)
            name += entry->category()->enumToString(i)
                    + entry->tile(i)->name();

        QString key = name + QLatin1Char('#');
        int n = 1;
        while (mEntriesByCategoryName.contains(key + QString::number(n)))
            n++;

        return name;
    }

    void addEntry(BuildingTileEntry *entry)
    {
        if (entry && !entry->isNone() && !mTileEntries.contains(entry)) {
            mEntriesByCategoryName[nameForEntry(entry)] = entry;
            mTileEntries = mEntriesByCategoryName.values(); // sorted
        }
    }

    QString entryIndex(BuildingTileEntry *entry)
    {
        if (entry && !entry->isNone())
            return QString::number(mTileEntries.indexOf(entry) + 1);
        return QString::number(0);
    }

    QString furnitureIndex(FurnitureTiles *ftiles)
    {
        int index = mFurnitureTiles.indexOf(ftiles);
        Q_ASSERT(index >= 0);
        return QString::number(index);
    }

    Building *mBuilding;
    QString mError;
    QDir mMapDir;
    QList<FurnitureTiles*> mFurnitureTiles;
    QList<BuildingTileEntry*> mTileEntries;
    QMap<QString,BuildingTileEntry*> mEntriesByCategoryName;
    QMap<QString,QString> mUserTilesMap;
};

/////

BuildingWriter::BuildingWriter()
    : d(new BuildingWriterPrivate)
{
}

BuildingWriter::~BuildingWriter()
{
    delete d;
}

bool BuildingWriter::write(Building *building, const QString &filePath)
{
    QTemporaryFile tempFile;
    if (!d->openFile(&tempFile))
        return false;

    write(building, &tempFile, QFileInfo(filePath).absolutePath());

    if (tempFile.error() != QFile::NoError) {
        d->mError = tempFile.errorString();
        return false;
    }

    // foo.tbx -> foo.tbx.bak
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

    // /tmp/tempXYZ -> foo.tbx
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

    if (filePath.endsWith(QLatin1String(".autosave")))
        if (backupFile.exists())
            backupFile.remove();

    return true;
}

void BuildingWriter::write(Building *building, QIODevice *device, const QString &absDirPath)
{
    d->writeBuilding(building, device, absDirPath);
}

QString BuildingWriter::errorString() const
{
    return d->mError;
}
