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

#include "basepathscene.h"

#include "basepathrenderer.h"
#include "path.h"
#include "pathdocument.h"
#include "world.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>

#ifdef Q_OS_WIN
// Hmmmm.  libtiled.dll defines the Properties class as so:
// class TILEDSHARED_EXPORT Properties : public QMap<QString,QString>
// Suddenly I'm getting a 'multiply-defined symbol' error.
// I found the solution here:
// http://www.archivum.info/qt-interest@trolltech.com/2005-12/00242/RE-Linker-Problem-while-using-QMap.html
template class __declspec(dllimport) QMap<QString, QString>;
#endif

/////

BasePathScene::BasePathScene(PathDocument *doc, QObject *parent) :
    BaseGraphicsScene(PathSceneType),
    mDocument(doc)
{
}

BasePathScene::~BasePathScene()
{
    delete mRenderer;
}

void BasePathScene::setDocument(PathDocument *doc)
{
    mDocument = doc;

    mPathLayerItems.clear();

#if 0

    foreach (WorldScript *ws, world()->scripts()) {
        QRect r = ws->mRegion.boundingRect().translated(-cell()->pos() * 300);
        if (!r.intersects(QRect(QPoint(), QSize(300,300)))) continue;
        Lua::LuaScript ls(world(), ws);
        ls.runFunction("run");
    }
#endif

    foreach (WorldPath::Layer *layer, world()->layers()) {
        PathLayerItem *item = new PathLayerItem(layer, this);
#if 0
        if (floor && ts) {
            foreach (WorldPath::Path *path, layer->paths()) {
                if (path->bounds().intersects(QRect(cell()->pos()*300, QSize(300,300)))) {
                    QString script;
                    if (path->tags.contains(QLatin1String("landuse"))) {
                        QString v = path->tags[QLatin1String("landuse")];
                        if (v == QLatin1String("forest") || v == QLatin1String("wood"))
                            ;
                        else if (v == QLatin1String("park"))
                            ;
                        else if (v == QLatin1String("grass"))
                            ;
                    } else if (path->tags.contains(QLatin1String("natural"))) {
                        QString v = path->tags[QLatin1String("natural")];
                        if (v == QLatin1String("water"))
                            ;

                    } else if (path->tags.contains(QLatin1String("leisure"))) {
                        QString v = path->tags[QLatin1String("leisure")];
                        if (v == QLatin1String("park"))
                            script = QLatin1String("C:/Programming/Tiled/PZWorldEd/park.lua");
                    }
                    if (path->tags.contains(QLatin1String("highway"))) {
                        qreal width;
                        QString v = path->tags[QLatin1String("highway")];
                        if (v == QLatin1String("residential")) width = 6; /// #1
                        else if (v == QLatin1String("pedestrian")) width = 5;
                        else if (v == QLatin1String("secondary")) width = 5;
                        else if (v == QLatin1String("secondary_link")) width = 5;
                        else if (v == QLatin1String("tertiary")) width = 4; /// #3
                        else if (v == QLatin1String("tertiary_link")) width = 4;
                        else if (v == QLatin1String("bridleway")) width = 4;
                        else if (v == QLatin1String("private")) width = 4;
                        else if (v == QLatin1String("service")) width = 6/2; /// #2
                        else if (v == QLatin1String("path")) width = 3; /// #2
                        else continue;
                        script = QLatin1String("C:/Programming/Tiled/PZWorldEd/road.lua");
                    }
                    if (!script.isEmpty()) {
#if 1
#else
                        Lua::LuaScript ls(mMap);
                        ls.mMap.mCellPos = cell()->pos();
                        Lua::LuaPath lp(path);
                        ls.mPath = &lp;
                        QString output;
                        if (ls.dofile(script, output)) {
                            foreach (Lua::LuaLayer *ll, ls.mMap.mLayers) {
                                // Apply changes to tile layers.
                                if (Lua::LuaTileLayer *tl = ll->asTileLayer()) {
                                    if (tl->mOrig == 0)
                                        continue; // Ignore new layers.
                                    if (!tl->mCloneTileLayer || tl->mAltered.isEmpty())
                                        continue; // No changes.
                                    TileLayer *source = tl->mCloneTileLayer->copy(tl->mAltered);
                                    QRect r = tl->mAltered.boundingRect();
                                    tl->mOrig->asTileLayer()->setCells(r.x(), r.y(), source, tl->mAltered);
                                    delete source;
                                }
                            }
                        }
#endif
#if 0
                        QPolygonF poly = WorldPath::strokePath(path, width);
                        QRegion rgn = WorldPath::polygonRegion(poly);
                        rgn.translate(poly.boundingRect().toAlignedRect().topLeft() - cell()->pos() * 300);
                        if (!rgn.boundingRect().intersects(floor->bounds()))
                            continue;
                        foreach (QRect r, rgn.rects()) {
                            for (int y = r.top(); y <= r.bottom(); y++) {
                                for (int x = r.left(); x <= r.right(); x++) {
                                    if (floor->contains(x, y))
                                        floor->setCell(x, y, Cell(ts->tileAt(96)));
                                }
                            }
                        }
#endif
                    }
                }
            }
        }

#endif
        addItem(item);
        mPathLayerItems += item;
    }
}

World *BasePathScene::world() const
{
    return mDocument ? mDocument->world() : 0;
}

void BasePathScene::setRenderer(BasePathRenderer *renderer)
{
    mRenderer = renderer;
}

/////

PathLayerItem::PathLayerItem(WorldPath::Layer *layer, BasePathScene *scene, QGraphicsItem *parent) :
    QGraphicsItem(parent),
    mLayer(layer),
    mScene(scene)
{
    setFlag(ItemUsesExtendedStyleOption);
}

QRectF PathLayerItem::boundingRect() const
{
    return mScene->renderer()->sceneBounds(QRect(0, 0,
                                                 mScene->world()->width() * 300,
                                                 mScene->world()->height() * 300), mLayer->mLevel);
}

void PathLayerItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
#if 0
    QRectF bounds = QRectF(0, 0, 300, 300).translated(mScene->cell()->x() * 300,
                                                      mScene->cell()->y() * 300);

    painter->setBrush(QColor(0,0,128,128));
    foreach (WorldScript *ws, mScene->document()->lookupScripts(bounds)) {
        QRect r = ws->mRegion.boundingRect().translated(-mScene->cell()->pos() * 300);
        if (!r.intersects(QRect(QPoint(), QSize(300,300)))) continue;
        QRegion rgn = ws->mRegion.translated(-mScene->cell()->pos() * 300);
        foreach (QRect r, rgn.rects()) {
            painter->drawPolygon(mScene->renderer()->tileToPixelCoords(r));
        }
    }

    return;
#endif
#if 1
    WorldPath::Rect bounds = option->exposedRect;
    foreach (WorldPath::Path *path, mLayer->paths(bounds)) {
        painter->setBrush(Qt::NoBrush);
        QPolygonF pf = path->polygon();
        if (path->isClosed()) {
            if (path->tags.contains(QLatin1String("landuse"))) {
                QString v = path->tags[QLatin1String("landuse")];
                if (v == QLatin1String("forest") || v == QLatin1String("wood"))
                    painter->setBrush(Qt::darkGreen);
                else if (v == QLatin1String("park"))
                    painter->setBrush(Qt::green);
                else if (v == QLatin1String("grass"))
                    painter->setBrush(QColor(Qt::green).lighter());
            } else if (path->tags.contains(QLatin1String("natural"))) {
                QString v = path->tags[QLatin1String("natural")];
                if (v == QLatin1String("water"))
                    painter->setBrush(Qt::blue);

            } else if (path->tags.contains(QLatin1String("leisure"))) {
                QString v = path->tags[QLatin1String("leisure")];
                if (v == QLatin1String("park"))
                    painter->setBrush(Qt::green);
            }
            painter->drawPolygon(mScene->renderer()->toScene(pf, mLayer->level()));
        } else {
            painter->drawPolyline(mScene->renderer()->toScene(pf, mLayer->level()));
            if (path->tags.contains(QLatin1String("highway"))) {
                qreal width = 6;
                QColor color = QColor(0,0,0,128);
                QString v = path->tags[QLatin1String("highway")];
                qreal residentialWidth = 6;
                if (v == QLatin1String("residential")) width = 6; /// #1
                else if (v == QLatin1String("pedestrian")) width = 5, color = QColor(0,64,0,128);
                else if (v == QLatin1String("secondary")) width = 5, color = QColor(0,0,128,128);
                else if (v == QLatin1String("secondary_link")) width = 5, color = QColor(0,0,128,128);
                else if (v == QLatin1String("tertiary")) width = 4, color = QColor(0,128,0,128); /// #3
                else if (v == QLatin1String("tertiary_link")) width = 4, color = QColor(0,128,0,128);
                else if (v == QLatin1String("bridleway")) width = 4, color = QColor(128,128,0,128);
                else if (v == QLatin1String("private")) width = 4, color = QColor(128,0,0,128);
                else if (v == QLatin1String("service")) width = residentialWidth/2, color = QColor(0,0,64,128); /// #2
                else if (v == QLatin1String("path")) width = 3, color = QColor(128,64,64,128); /// #2
                else continue;

                pf = WorldPath::strokePath(path, width);
                painter->setBrush(color);
                painter->drawPolygon(mScene->renderer()->toScene(pf, mLayer->level()));
            }
        }
    }
#endif
}
