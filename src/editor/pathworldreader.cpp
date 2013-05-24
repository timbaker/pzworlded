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

#include "pathworldreader.h"

#include "path.h"
#include "pathworld.h"

#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>

class PathWorldReaderPrivate
{
    Q_DECLARE_TR_FUNCTIONS(PathWorldReaderPrivate)

public:
    PathWorldReaderPrivate()
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

    PathWorld *readWorld(QIODevice *device, const QString &path)
    {
        mError.clear();
        mPath = path;
        PathWorld *world = 0;

        xml.setDevice(device);

        if (xml.readNextStartElement() && xml.name() == "world") {
            world = readWorld();
        } else {
            xml.raiseError(tr("Not a world file."));
        }

        return world;
    }

private:
    PathWorld *readWorld()
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == "world");

        const QXmlStreamAttributes atts = xml.attributes();
        const int version = atts.value(QLatin1String("version")).toString().toInt();
        const int width = atts.value(QLatin1String("width")).toString().toInt();
        const int height = atts.value(QLatin1String("height")).toString().toInt();

        mWorld = new PathWorld(width, height);
        mNextNodeId = 0;
        mNextPathId = 0;

        while (xml.readNextStartElement()) {
            if (xml.name() == "tileset") {
                if (WorldTileset *tileset = readTileset())
                    mWorld->insertTileset(mWorld->tilesetCount(), tileset);
            } else if (xml.name() == "level") {
                if (WorldLevel *level = readLevel())
                    mWorld->insertLevel(mWorld->levelCount(), level);
            } else if (xml.name() == "tile-alias") {
                if (WorldTileAlias *alias = readTileAlias())
                    mWorld->addTileAlias(alias);
            } else if (xml.name() == "tile-rule") {
                if (WorldTileRule *rule = readTileRule())
                    mWorld->addTileRule(rule);
            } else
                readUnknownElement();
        }

        // Clean up in case of error
        if (xml.hasError()) {
            delete mWorld;
            mWorld = 0;
        }

        mWorld->setNextIds(mNextNodeId + 1, mNextPathId + 1);

        return mWorld;
    }

    WorldTileset *readTileset()
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == "tileset");

        const QXmlStreamAttributes atts = xml.attributes();
        const QString name = atts.value(QLatin1String("name")).toString();
        const int columns = atts.value(QLatin1String("columns")).toString().toInt();
        const int rows = atts.value(QLatin1String("rows")).toString().toInt();

        xml.skipCurrentElement();

        WorldTileset *tileset = new WorldTileset(name, columns, rows);
        return tileset;
    }

    WorldLevel *readLevel()
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == "level");

        const QXmlStreamAttributes atts = xml.attributes();

        WorldLevel *wlevel = new WorldLevel(mWorld->levelCount());

        while (xml.readNextStartElement()) {
            if (xml.name() == "pathlayer") {
                if (WorldPathLayer *layer = readPathLayer())
                    wlevel->insertPathLayer(wlevel->pathLayerCount(), layer);
            } else if (xml.name() == "tilelayer") {
                if (WorldTileLayer *layer = readTileLayer())
                    wlevel->insertTileLayer(wlevel->tileLayerCount(), layer);
            } else
                readUnknownElement();
        }

        return wlevel;
    }

    WorldPathLayer *readPathLayer()
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == "pathlayer");

        const QXmlStreamAttributes atts = xml.attributes();
        const QString name = atts.value(QLatin1String("name")).toString();

        WorldPathLayer *layer = new WorldPathLayer(name);

        while (xml.readNextStartElement()) {
            if (xml.name() == "node") {
                if (WorldNode *node = readNode(layer)) {
                    layer->insertNode(layer->nodeCount(), node);
                    mNextNodeId = qMax(mNextNodeId, node->id);
                }
            } else if (xml.name() == "path") {
                readPath(layer);
            } else
                readUnknownElement();
        }

        return layer;
    }

    WorldNode *readNode(WorldPathLayer *layer)
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == "node");
        const QXmlStreamAttributes atts = xml.attributes();
        id_t id = atts.value(QLatin1String("id")).toString().toULong();
        // TODO: verify unique id
        qreal x = atts.value(QLatin1String("x")).toString().toDouble();
        qreal y = atts.value(QLatin1String("y")).toString().toDouble();
        // TODO: verify reasonable coords
        xml.skipCurrentElement();
        return new WorldNode(id, x, y);
    }

    WorldPath *readPath(WorldPathLayer *layer)
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == "path");
        const QXmlStreamAttributes atts = xml.attributes();
        id_t id = atts.value(QLatin1String("id")).toString().toULong();
        // TODO: verify unique id
        const QStringList nodeIds = atts.value(QLatin1String("nodes")).toString()
                .split(QLatin1Char(' '), QString::SkipEmptyParts);
        NodeList nodes;
        foreach (QString nodeId, nodeIds) {
            if (WorldNode *node = layer->node(nodeId.toULong()))
                nodes += node;
            else {
                xml.raiseError(tr("Invalid node id '%1' in path '%2'").arg(nodeId).arg(id));
            }
        }

        WorldPath *path = new WorldPath(id);
        layer->insertPath(layer->pathCount(), path);
        mNextPathId = qMax(mNextPathId, path->id);

        foreach (WorldNode *node, nodes)
            path->insertNode(path->nodeCount(), node);

        while (xml.readNextStartElement()) {
            if (xml.name() == "script") {
                if (WorldScript *script = readScript()) {
                    script->mPaths += path;
                    path->insertScript(path->scriptCount(), script);
                }
            } else
                readUnknownElement();
        }
        return path;
    }

    WorldScript *readScript()
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == "script");
        const QXmlStreamAttributes atts = xml.attributes();
        QString file = atts.value(QLatin1String("file")).toString();

        WorldScript *script = new WorldScript;
        script->mFileName = resolveReference(file, mPath);
        while (xml.readNextStartElement()) {
            if (xml.name() == "param") {
                const QXmlStreamAttributes atts = xml.attributes();
                script->mParams[atts.value(QLatin1String("name")).toString()] =
                        atts.value(QLatin1String("value")).toString();
                xml.skipCurrentElement();
            } else
                readUnknownElement();
        }
        return script;
    }

    WorldTileLayer *readTileLayer()
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == "tilelayer");

        const QXmlStreamAttributes atts = xml.attributes();
        const QString name = atts.value(QLatin1String("name")).toString();

        xml.skipCurrentElement();

        WorldTileLayer *layer = new WorldTileLayer(name);
        return layer;
    }

    WorldTileAlias *readTileAlias()
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == "tile-alias");

        const QXmlStreamAttributes atts = xml.attributes();
        const QString name = atts.value(QLatin1String("name")).toString();
        const QStringList tileNames = atts.value(QLatin1String("tiles")).toString()
                .split(QLatin1Char(' '), QString::SkipEmptyParts);

        TileList tiles;
        foreach (QString tileName, tileNames) {
            if (WorldTile *tile = mWorld->tile(tileName))
                tiles += tile;
        }

        xml.skipCurrentElement();

        WorldTileAlias *alias = new WorldTileAlias;
        alias->mName = name;
        alias->mTileNames = tileNames;
        alias->mTiles = tiles;
        return alias;
    }

    WorldTileRule *readTileRule()
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == "tile-rule");

        const QXmlStreamAttributes atts = xml.attributes();
        const QString name = atts.value(QLatin1String("name")).toString();
        const QStringList tileNames = atts.value(QLatin1String("tiles")).toString()
                .split(QLatin1Char(' '), QString::SkipEmptyParts);
        const QString layer = atts.value(QLatin1String("layer")).toString();

        TileList tiles;
        foreach (QString tileName, tileNames) {
            if (WorldTileAlias *alias = mWorld->tileAlias(tileName))
                tiles += alias->mTiles;
            else if (WorldTile *tile = mWorld->tile(tileName))
                tiles += tile;
        }

        xml.skipCurrentElement();

        WorldTileRule *rule = new WorldTileRule;
        rule->mName = name;
        rule->mTileNames = tileNames;
        rule->mTiles = tiles;
        rule->mLayer = layer;
        return rule;
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
    PathWorld *mWorld;
    QString mError;
    QXmlStreamReader xml;
    id_t mNextNodeId;
    id_t mNextPathId;
};

/////

PathWorldReader::PathWorldReader()
    : d(new PathWorldReaderPrivate)
{
}

PathWorldReader::~PathWorldReader()
{
    delete d;
}

PathWorld *PathWorldReader::readWorld(QIODevice *device, const QString &path)
{
    return d->readWorld(device, path);
}

PathWorld *PathWorldReader::readWorld(const QString &fileName)
{
    QFile file(fileName);
    if (!d->openFile(&file))
        return 0;

    return readWorld(&file, QFileInfo(fileName).absolutePath());
}

QString PathWorldReader::errorString() const
{
    return d->errorString();
}
