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

#ifndef MAPBOXWINDOW_H
#define MAPBOXWINDOW_H

#include <QMainWindow>
#include <QMapboxGL>

namespace Ui {
class MapboxWindow;
}

class MapboxWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MapboxWindow(QWidget *parent = nullptr);
    ~MapboxWindow();

    void setJson(const QMap<QString,QByteArray>& json);

private slots:
    void mapChanged(QMapboxGL::MapChange change);

private:
    QMap<QString,QByteArray> mGeoJSON;
    Ui::MapboxWindow *ui;
};

#endif // MAPBOXWINDOW_H
