/*
 * Copyright 2019, Tim Baker <treectrl@users.sf.net>
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

#include "mapboxwindow.h"
#include "ui_mapboxwindow.h"

#include "mapboxgeojsongenerator.h"

#include "basegraphicsview.h"
#include "celldocument.h"
#include "cellscene.h"
#include "documentmanager.h"
#include "maprenderer.h"
#include "world.h"
#include "worlddocument.h"
#include "worldscene.h"

#include <QDebug>

#if defined(Q_OS_WIN) && (_MSC_VER >= 1600)
// Hmmmm.  libtiled.dll defines the Properties class as so:
// class TILEDSHARED_EXPORT Properties : public QMap<QString,QString>
// Suddenly I'm getting a 'multiply-defined symbol' error.
// I found the solution here:
// http://www.archivum.info/qt-interest@trolltech.com/2005-12/00242/RE-Linker-Problem-while-using-QMap.html
template class __declspec(dllimport) QMap<QString, QString>;
#endif

MapboxWindow::MapboxWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MapboxWindow)
{
    ui->setupUi(this);

    connect(ui->actionRefresh, &QAction::triggered, this, &MapboxWindow::refresh);
//    connect(ui->mapboxGLWidget, &MapboxGLWidget::mapChanged, this, &MapboxWindow::mapChanged);
    connect(ui->mapboxGLWidget, &MapboxGLWidget::gotoLocation, this, &MapboxWindow::gotoLocation);
}

MapboxWindow::~MapboxWindow()
{
    delete ui;
}

void MapboxWindow::setDocument(WorldDocument *worldDoc)
{
    mWorldDoc = worldDoc;
#if 0
    mGeoJSON = MapBoxGeojsonGenerator().generateJsonLayers(mWorldDoc);
    if (mStyleLoaded) {
        ui->mapboxGLWidget->setJson(mGeoJSON);
        mGeoJSON.clear();
    }
#endif
}

void MapboxWindow::setJson(const QMap<QString, QByteArray> &json)
{
#if 0
    mGeoJSON = json;
#endif
}

void MapboxWindow::closeEvent(QCloseEvent *e)
{
    e->accept();
}

void MapboxWindow::refresh()
{
#if 0
    mGeoJSON = MapBoxGeojsonGenerator().generateJsonLayers(mWorldDoc);
    ui->mapboxGLWidget->setJson(mGeoJSON);
    mGeoJSON.clear();
#endif
}

#if 0
void MapboxWindow::mapChanged(QMapboxGL::MapChange change)
{
    switch (change) {
    case QMapboxGL::MapChange::MapChangeDidFinishLoadingStyle: {
        try {
            mStyleLoaded = true;
            ui->mapboxGLWidget->setJson(mGeoJSON);
            mGeoJSON.clear();
        } catch (const std::exception& ex) {
            qDebug() << ex.what();
        }
        break;
    }
    default:
        break;
    }
}
#endif

#include "documentmanager.h"

void MapboxWindow::gotoLocation(int squareX, int squareY)
{
    auto& generateLotSettings = mWorldDoc->world()->getGenerateLotsSettings();
    qreal cellX = squareX / 300.0 - generateLotSettings.worldOrigin.x();
    qreal cellY = squareY / 300.0 - generateLotSettings.worldOrigin.y();

    WorldCell* cell = mWorldDoc->world()->cellAt(int(cellX), int(cellY));
    if (cell == nullptr)
        return;

    QPointF scenePos = dynamic_cast<WorldScene*>(
                mWorldDoc->view()->scene())->cellToPixelCoords(
                cellX,
                cellY);

    mWorldDoc->setSelectedCells(QList<WorldCell*>() << cell);
    mWorldDoc->view()->centerOn(scenePos);

    mWorldDoc->editCell(int(cellX), int(cellY));

    if (CellDocument *cellDoc = DocumentManager::instance()->findDocument(cell)) {
        DocumentManager::instance()->setCurrentDocument(cellDoc);
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
        QPointF tilePos(squareX - cell->x() * 300,
                        squareY - cell->y() * 300);
        cellDoc->view()->centerOn(cellDoc->scene()->renderer()->tileToPixelCoords(tilePos));
    }
}
