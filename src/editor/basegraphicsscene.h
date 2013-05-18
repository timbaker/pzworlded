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

#ifndef BASEGRAPHICSSCENE_H
#define BASEGRAPHICSSCENE_H

#include <QGraphicsScene>

class AbstractTool;
class BaseGraphicsView;
class CellScene;
class LotPackScene;
class BasePathScene;
class WorldScene;

class BaseGraphicsScene : public QGraphicsScene
{
public:
    enum SceneType {
        CellSceneType,
        WorldSceneType,
        LotPackSceneType,
        PathSceneType
    };

    explicit BaseGraphicsScene(SceneType type, QObject *parent = 0);
    
    void setEventView(BaseGraphicsView *view) { mEventView = view; }
    virtual void setTool(AbstractTool *tool) = 0;

    bool isWorldScene() const { return mType == WorldSceneType; }
    bool isCellScene() const { return mType == CellSceneType; }
    bool isLotPackScene() const { return mType == LotPackSceneType; }
    bool isPathScene() const { return mType == PathSceneType; }

    virtual void viewTransformChanged(BaseGraphicsView *view)
    {
        Q_UNUSED(view)
    }

    WorldScene *asWorldScene();
    CellScene *asCellScene();
    LotPackScene *asLotPackScene();
    BasePathScene *asPathScene();

    void clearScene();

protected:
    BaseGraphicsView *mEventView;
    SceneType mType;
};

#endif // BASEGRAPHICSSCENE_H
