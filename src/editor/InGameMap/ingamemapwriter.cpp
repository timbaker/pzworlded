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

#include "ingamemapwriter.h"

#include "world.h"
#include "worldcell.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QXmlStreamWriter>

class InGameMapWriterPrivate
{
    Q_DECLARE_TR_FUNCTIONS(InGameMapWriter)

public:
    InGameMapWriterPrivate()
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

        for (int y = 0; y < world->height(); y++) {
            for (int x = 0; x < world->width(); x++) {
                WorldCell *cell = world->cellAt(x, y);
                writeCell(w, cell);
            }
        }

        w.writeEndElement();
    }

    void writeCell(QXmlStreamWriter &w, WorldCell *cell)
    {
        if (cell->inGameMap().features().isEmpty())
            return;

        const QPoint worldOrigin = cell->world()->getGenerateLotsSettings().worldOrigin;

        w.writeStartElement(QLatin1String("cell"));
        w.writeAttribute(QLatin1String("x"), QString::number(worldOrigin.x() + cell->x()));
        w.writeAttribute(QLatin1String("y"), QString::number(worldOrigin.y() + cell->y()));

        for (auto* feature : qAsConst(cell->inGameMap().mFeatures)) {
            writeFeature(w, feature);
        }

        w.writeEndElement();
    }

    void writeFeature(QXmlStreamWriter &w, InGameMapFeature* feature)
    {
        w.writeStartElement(QLatin1String("feature"));

        w.writeStartElement(QLatin1String("geometry"));
        w.writeAttribute(QLatin1String("type"), feature->mGeometry.mType);
        for (auto& coords : feature->mGeometry.mCoordinates) {
            w.writeStartElement(QLatin1String("coordinates"));
            for (auto& point : coords) {
                w.writeStartElement(QLatin1String("point"));
                w.writeAttribute(QLatin1String("x"), QString::number(point.x));
                w.writeAttribute(QLatin1String("y"), QString::number(point.y));
                w.writeEndElement();
            }
            w.writeEndElement();
        }
        w.writeEndElement();

        w.writeStartElement(QLatin1String("properties"));
        for (auto& property : feature->mProperties) {
            w.writeStartElement(QLatin1String("property"));
            w.writeAttribute(QLatin1String("name"), property.mKey);
            w.writeAttribute(QLatin1String("value"), property.mValue);
            w.writeEndElement();
        }
        w.writeEndElement();

        w.writeEndElement();
    }

    World *mWorld;
    QString mError;
    QDir mMapDir;
};

/////

InGameMapWriter::InGameMapWriter()
    : d(new InGameMapWriterPrivate)
{
}

InGameMapWriter::~InGameMapWriter()
{
    delete d;
}

bool InGameMapWriter::writeWorld(World *world, const QString &filePath)
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
    if (tempFile.rename(filePath)) {
        // If anything above failed, the temp file should auto-remove, but not after
        // a successful save.
        tempFile.setAutoRemove(false);
    } else if (!tempFile.copy(filePath)) {
        d->mError = QString(QLatin1String("Error copying file!\nFrom: %1\nTo: %2\n\n%3"))
                .arg(tempFile.fileName())
                .arg(filePath)
                .arg(tempFile.errorString());
        // Try to un-rename the backup file
        if (backupFile.exists())
            backupFile.rename(filePath); // might fail
        return false;
    }

    return true;
}

void InGameMapWriter::writeWorld(World *world, QIODevice *device, const QString &absDirPath)
{
    d->writeWorld(world, device, absDirPath);
}

QString InGameMapWriter::errorString() const
{
    return d->mError;
}
