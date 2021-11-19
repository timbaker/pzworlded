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

#include "ingamemapwriterbinary.h"

#include "world.h"
#include "worldcell.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QTemporaryFile>
#include <QXmlStreamWriter>

#define VERSION1 1
#define VERSION_LATEST 1

class InGameMapWriterBinaryPrivate
{
    Q_DECLARE_TR_FUNCTIONS(InGameMapWriterBinary)

public:
    InGameMapWriterBinaryPrivate()
        : mWorld(nullptr)
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

        QDataStream writer(device);
        writer.setByteOrder(QDataStream::LittleEndian);

        writeWorld(writer, world);
    }

    void writeWorld(QDataStream &w, World *world)
    {
        w << quint8('I') << quint8('G') << quint8('M') << quint8('B');

        w << qint32(VERSION_LATEST);

        w << qint32(world->width());
        w << qint32(world->height());

        writeStringTable(w, world);

        for (int y = 0; y < world->height(); y++) {
            for (int x = 0; x < world->width(); x++) {
                WorldCell *cell = world->cellAt(x, y);
                writeCell(w, cell);
            }
        }
    }

    void writeStringTable(QDataStream &w, World *world)
    {
        QStringList strings;

        auto addString = [&](const QString& str)
        {
            if (mStringTable.contains(str))
                return;
            mStringTable.insert(str, strings.size());
            strings += str;
        };

        for (int y = 0; y < world->height(); y++) {
            for (int x = 0; x < world->width(); x++) {
                WorldCell *cell = world->cellAt(x, y);
                for (auto* feature : qAsConst(cell->inGameMap().mFeatures)) {
                    addString(feature->mGeometry.mType);
                    for (auto& property : feature->mProperties) {
                        addString(property.mKey);
                        addString(property.mValue);
                    }
                }
            }
        }

        w << qint32(strings.size());
        for (const QString &str : qAsConst(strings)) {
            SaveString(w, str);
        }
    }

    void writeCell(QDataStream &w, WorldCell *cell)
    {
        if (cell->inGameMap().features().isEmpty()) {
            w << qint32(-1);
            return;
        }

        const QPoint worldOrigin = cell->world()->getGenerateLotsSettings().worldOrigin;
        w << qint32(worldOrigin.x() + cell->x());
        w << qint32(worldOrigin.y() + cell->y());

        w << qint32(cell->inGameMap().mFeatures.size());

        for (auto* feature : qAsConst(cell->inGameMap().mFeatures)) {
            writeFeature(w, feature);
        }
    }

    void writeFeature(QDataStream &w, InGameMapFeature* feature)
    {
        SaveStringIndex(w, feature->mGeometry.mType);

        w << qint8(feature->mGeometry.mCoordinates.size());
        for (auto& coords : feature->mGeometry.mCoordinates) {
            w << qint16(coords.size());
            for (auto& point : coords) {
                w << qint16(int(point.x));
                w << qint16(int(point.y));
            }
        }

        w << qint8(feature->mProperties.size());
        for (auto& property : feature->mProperties) {
            SaveStringIndex(w, property.mKey);
            SaveStringIndex(w, property.mValue);
        }
    }

    void SaveString(QDataStream& w, const QString& str)
    {
        QByteArray utf8 = str.toUtf8();
        w << qint16(utf8.length());
        for (int i = 0; i < utf8.length(); i++) {
            w << quint8(utf8.at(i));
        }
    }

    void SaveStringIndex(QDataStream& w, const QString& str)
    {
        w << qint16(mStringTable[str]);
    }

    World *mWorld;
    QString mError;
    QDir mMapDir;
    QMap<QString, int> mStringTable;
};

/////

InGameMapWriterBinary::InGameMapWriterBinary()
    : d(new InGameMapWriterBinaryPrivate)
{
}

InGameMapWriterBinary::~InGameMapWriterBinary()
{
    delete d;
}

bool InGameMapWriterBinary::writeWorld(World *world, const QString &filePath)
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

void InGameMapWriterBinary::writeWorld(World *world, QIODevice *device, const QString &absDirPath)
{
    d->writeWorld(world, device, absDirPath);
}

QString InGameMapWriterBinary::errorString() const
{
    return d->mError;
}
