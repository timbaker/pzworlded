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

    connect(ui->mapboxGLWidget, &MapboxGLWidget::mapChanged, this, &MapboxWindow::mapChanged);
}

MapboxWindow::~MapboxWindow()
{
    delete ui;
}

void MapboxWindow::setJson(const QMap<QString, QByteArray> &json)
{
    mGeoJSON = json;
}

void MapboxWindow::mapChanged(QMapboxGL::MapChange change)
{
    switch (change) {
    case QMapboxGL::MapChange::MapChangeDidFinishLoadingStyle: {
        try {
            ui->mapboxGLWidget->setJson(mGeoJSON);
        } catch (const std::exception& ex) {
            qDebug() << ex.what();
        }
        break;
    }
    default:
        break;
    }
}
