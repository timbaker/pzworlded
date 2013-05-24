/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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

#include "pathworldwriter.h"

#include "path.h"
#include "pathworld.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QXmlStreamWriter>

class PathWorldWriterPrivate
{
    Q_DECLARE_TR_FUNCTIONS(PathWorldWriterPrivate)

public:
    PathWorldWriterPrivate()
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

    void writeWorld(PathWorld *world, QIODevice *device, const QString &absDirPath)
    {
        mMapDir = QDir(absDirPath);
        mWorld = world;

        xml.setDevice(device);
        xml.setAutoFormatting(true);
        xml.setAutoFormattingIndent(1);

        xml.writeStartDocument();

        writeWorld(world);

        xml.writeEndDocument();
    }

    void writeWorld(PathWorld *world)
    {
        xml.writeStartElement(QLatin1String("world"));

        xml.writeAttribute(QLatin1String("version"), QLatin1String("1"));
        xml.writeAttribute(QLatin1String("width"), QString::number(world->width()));
        xml.writeAttribute(QLatin1String("height"), QString::number(world->height()));

        foreach (WorldTileset *tileset, world->tilesets())
            writeTileset(tileset);

        foreach (WorldTileAlias *ta, world->tileAliases())
            writeTileAlias(ta);

        foreach (WorldTileRule *tr, world->tileRules())
            writeTileRule(tr);

        foreach (WorldPathLayer *layer, world->pathLayers())
            writePathLayer(layer);

        foreach (WorldTileLayer *layer, world->tileLayers())
            writeTileLayer(layer);

        xml.writeEndElement(); // world
    }

    void writePathLayer(WorldPathLayer *layer)
    {
        xml.writeStartElement(QLatin1String("pathlayer"));
        xml.writeAttribute(QLatin1String("name"), layer->mName);
        foreach (WorldNode *node, layer->nodes())
            writeNode(node);
        foreach (WorldPath *path, layer->paths())
            writePath(path);
        xml.writeEndElement(); // pathlayer
    }

    void writeNode(WorldNode *node)
    {
        xml.writeStartElement(QLatin1String("node"));
        xml.writeAttribute(QLatin1String("id"), QString::number(node->id));
        xml.writeAttribute(QLatin1String("x"), QString::number(node->pos().x(), 'g', 5 + 7));
        xml.writeAttribute(QLatin1String("y"), QString::number(node->pos().y(), 'g', 5 + 7));
        xml.writeEndElement(); // node
    }

    void writePath(WorldPath *path)
    {
        xml.writeStartElement(QLatin1String("path"));
        xml.writeAttribute(QLatin1String("id"), QString::number(path->id));
        QStringList nodes;
        foreach (WorldNode *node, path->nodes) {
            nodes += QString::number(node->id);
        }
        xml.writeAttribute(QLatin1String("nodes"), nodes.join(QLatin1String(" ")));
        foreach (WorldScript *script, path->scripts())
            writeScript(script);
        xml.writeEndElement(); // path
    }

    void writeScript(WorldScript *script)
    {
        xml.writeStartElement(QLatin1String("script"));
        xml.writeAttribute(QLatin1String("file"), relativeFileName(script->mFileName));
        foreach (QString key, script->mParams.keys()) {
            xml.writeStartElement(QLatin1String("param"));
            xml.writeAttribute(QLatin1String("name"), key);
            xml.writeAttribute(QLatin1String("value"), script->mParams[key]);
            xml.writeEndElement(); // param
        }
        xml.writeEndElement(); // script
    }

    void writeTileset(WorldTileset *tileset)
    {
        xml.writeStartElement(QLatin1String("tileset"));
        xml.writeAttribute(QLatin1String("name"), tileset->mName);
        xml.writeAttribute(QLatin1String("columns"), QString::number(tileset->mColumns));
        xml.writeAttribute(QLatin1String("rows"), QString::number(tileset->mRows));
        xml.writeEndElement(); // tileset
    }

    void writeTileAlias(WorldTileAlias *ta)
    {
        xml.writeStartElement(QLatin1String("tile-alias"));
        xml.writeAttribute(QLatin1String("name"), ta->mName);
        xml.writeAttribute(QLatin1String("tiles"), ta->mTileNames.join(QLatin1String(" ")));
        xml.writeEndElement(); // tile-alias
    }

    void writeTileRule(WorldTileRule *tr)
    {
        xml.writeStartElement(QLatin1String("tile-rule"));
        xml.writeAttribute(QLatin1String("name"), tr->mName);
        xml.writeAttribute(QLatin1String("tiles"), tr->mTileNames.join(QLatin1String(" ")));
        xml.writeAttribute(QLatin1String("layer"), tr->mLayer);
        xml.writeEndElement(); // tile-rule
    }

    void writeTileLayer(WorldTileLayer *layer)
    {
        xml.writeStartElement(QLatin1String("tilelayer"));
        xml.writeAttribute(QLatin1String("name"), layer->mName);
        xml.writeEndElement(); // tilelayer
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

    QXmlStreamWriter xml;
    PathWorld *mWorld;
    QString mError;
    QDir mMapDir;
};

/////

PathWorldWriter::PathWorldWriter()
    : d(new PathWorldWriterPrivate)
{
}

PathWorldWriter::~PathWorldWriter()
{
    delete d;
}

bool PathWorldWriter::writeWorld(PathWorld *world, const QString &filePath)
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

void PathWorldWriter::writeWorld(PathWorld *world, QIODevice *device, const QString &absDirPath)
{
    d->writeWorld(world, device, absDirPath);
}

QString PathWorldWriter::errorString() const
{
    return d->mError;
}
