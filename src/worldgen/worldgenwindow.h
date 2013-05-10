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

#ifndef WORLDGENWINDOW_H
#define WORLDGENWINDOW_H

#include <QMainWindow>

namespace Ui {
class WorldGenWindow;
}

namespace WorldGen {



class WorldGenWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    static WorldGenWindow *instance();
    static void deleteInstance();
    
private slots:
    void open();
    void osm();

private:
    Q_DISABLE_COPY(WorldGenWindow)
    explicit WorldGenWindow(QWidget *parent = 0);
    ~WorldGenWindow();

    static WorldGenWindow *mInstance;

    Ui::WorldGenWindow *ui;
};

} // namespace WorldGen

#endif // WORLDGENWINDOW_H
