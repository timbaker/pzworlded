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

#ifndef PATHWORLDDOCUMENT_H
#define PATHWORLDDOCUMENT_H

#include "document.h"

#include "global.h"

#include <QList>
#include <QPolygonF>
#include <QRectF>
#include <QSet>

class PathView;
class PathWorld;
class WorldChangeAtom;
class WorldChanger;
class WorldLookup;
class WorldNode;
class WorldPath;
class WorldPathLayer;
class WorldScript;
class WorldTileLayer;

class PathDocument : public Document
{
    Q_OBJECT
public:
    PathDocument(PathWorld *world, const QString &fileName = QString());
    ~PathDocument();

    void setFileName(const QString &fileName);
    const QString &fileName() const;

    bool save(const QString &filePath, QString &error);

    PathWorld *world() const
    { return mWorld; }

    PathView *view() const
    { return (PathView*)Document::view(); }

    WorldChanger *changer() const
    { return mChanger; }

    void setCurrentLevel(WorldLevel *wlevel);
    WorldLevel *currentLevel() { return mCurrentLevel; }

    void setCurrentPathLayer(WorldPathLayer *layer);
    WorldPathLayer *currentPathLayer() { return mCurrentPathLayer; }

    void setCurrentTileLayer(WorldTileLayer *layer);
    WorldTileLayer *currentTileLayer() { return mCurrentTileLayer; }

    const PathSet &hiddenPaths() const
    { return mHiddenPaths; }

signals:
    void currentLevelChanged(WorldLevel *wlevel);
    void currentPathLayerChanged(WorldPathLayer *layer);
    void currentTileLayerChanged(WorldTileLayer *layer);

private slots:
    void afterAddNode(WorldNode *node);
    void afterRemoveNode(WorldPathLayer *layer, int index, WorldNode *node);
    void afterMoveNode(WorldNode *node, const QPointF &prev);

    void afterAddPath(WorldPathLayer *layer, int index, WorldPath *path);
    void afterRemovePath(WorldPathLayer *layer, int index, WorldPath *path);
    void afterAddNodeToPath(WorldPath *path, int index, WorldNode *node);
    void afterRemoveNodeFromPath(WorldPath *path, int index, WorldNode *node);

    void afterAddScriptToPath(WorldPath *path, int index, WorldScript *script);
    void afterRemoveScriptFromPath(WorldPath *path, int index, WorldScript *script);

    void afterSetPathVisible(WorldPath *path, bool visible);

private:
    PathWorld *mWorld;
    QString mFileName;
    WorldLookup *mLookup;
    WorldChanger *mChanger;
    PathSet mHiddenPaths;

    WorldLevel *mCurrentLevel;
    WorldPathLayer *mCurrentPathLayer;
    WorldTileLayer *mCurrentTileLayer;
};

#endif // PATHWORLDDOCUMENT_H
