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
            } else if (xml.name() == "texture") {
                if (WorldTexture *tex = readTexture()) {
                    mWorld->textureList() += tex;
                    mWorld->textureMap()[tex->mName] = tex;
                }
            } else
                readUnknownElement();
        }

        // Clean up in case of error
        if (xml.hasError()) {
            delete mWorld;
            mWorld = 0;
        }

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

    WorldTexture *readTexture()
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == "texture");

        const QXmlStreamAttributes atts = xml.attributes();
        WorldTexture *tex = new WorldTexture;
        tex->mName = atts.value(QLatin1String("name")).toString();
        tex->mFileName = resolveReference(atts.value(QLatin1String("file")).toString(), mPath);
        xml.skipCurrentElement();
        return tex;
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
            if (xml.name() == "path") {
                readPath(layer);
            } else
                readUnknownElement();
        }

        return layer;
    }

    WorldPath *readPath(WorldPathLayer *layer)
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == "path");
        const QXmlStreamAttributes atts = xml.attributes();
        bool visible = getBoolean(atts, QLatin1String("visible"), true);
        bool closed = getBoolean(atts, QLatin1String("closed"), true);
        double strokeWidth = atts.value(QLatin1String("stroke-width")).toString().toDouble();

        WorldPath *path = new WorldPath();
        path->setVisible(visible);
        path->setClosed(closed);
        path->setStrokeWidth(strokeWidth);
        layer->insertPath(layer->pathCount(), path);

        while (xml.readNextStartElement()) {
            if (xml.name() == "node") {
                if (WorldNode *node = readNode()) {
                    path->insertNode(path->nodeCount(), node);
                }
            } else if (xml.name() == "script") {
                if (WorldScript *script = readScript()) {
                    script->mPaths += path;
                    path->insertScript(path->scriptCount(), script);
                }
            } else if (xml.name() == "texture") {
                const QXmlStreamAttributes atts = xml.attributes();
                QString name = atts.value(QLatin1String("name")).toString();
                if (!mWorld->textureMap().contains(name)) {
                    xml.raiseError(tr("Unknown texture \"%1\"").arg(name));
//                    delete path;
                    return 0;
                }
                path->texture().mTexture = mWorld->textureMap()[name];

                double w, h;
                if (!readDoublePair(atts, QLatin1String("scale"), w, h))
                    return false;
                path->texture().mScale.setWidth(w);
                path->texture().mScale.setHeight(h);
                double x, y;

                if (!readDoublePair(atts, QLatin1String("scale"), x, y))
                    return false;
                path->texture().mTranslation.setX(x);
                path->texture().mTranslation.setY(y);

                if (atts.hasAttribute(QLatin1String("rotate")))
                    path->texture().mRotation = atts.value(QLatin1String("rotate")).toString().toDouble();

                QString s = atts.value(QLatin1String("align")).toString();
                path->texture().mAlignWorld = s == QLatin1String("world");


                xml.skipCurrentElement();
            } else
                readUnknownElement();
        }
        return path;
    }

    WorldNode *readNode()
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == "node");
        const QXmlStreamAttributes atts = xml.attributes();
        qreal x = atts.value(QLatin1String("x")).toString().toDouble();
        qreal y = atts.value(QLatin1String("y")).toString().toDouble();
        // TODO: verify reasonable coords
        xml.skipCurrentElement();
        return new WorldNode(x, y);
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

    bool getBoolean(const QXmlStreamAttributes &atts, const QString &name, bool defaultValue)
    {
        if (atts.hasAttribute(name)) {
            QString s = atts.value(name).toString();
            if (s == QLatin1String("true")) return true;
            if (s == QLatin1String("false")) return false;
            xml.raiseError(tr("Expected boolean but got \"%1\"").arg(s));
         }
        return defaultValue;
    }

    bool readDoublePair(const QXmlStreamAttributes &atts, const QString &name,
                        double &v1, double &v2)
    {
        if (atts.hasAttribute(name)) {
            QStringList vs = atts.value(name).toString().split(QLatin1Char(','), QString::SkipEmptyParts);
            if (vs.size() == 2) {
                bool ok;
                v1 = vs[0].toDouble(&ok);
                if (!ok) goto bogus;
                v2 = vs[1].toDouble(&ok);
                if (!ok) goto bogus;
                return true;
            }
        }
bogus:
        xml.raiseError(tr("Expected %1=double,double but got \"%2\"")
                       .arg(name).arg(atts.value(name).toString()));
        return false;
    }

private:
    QString mPath;
    PathWorld *mWorld;
    QString mError;
    QXmlStreamReader xml;
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
