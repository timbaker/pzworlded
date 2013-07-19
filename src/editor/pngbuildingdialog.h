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

#ifndef PNGBUILDINGDIALOG_H
#define PNGBUILDINGDIALOG_H

#include <QDialog>

class MapComposite;
class World;
class WorldCell;

namespace Tiled {
class ObjectGroup;
}

class QPainter;

namespace Ui {
class PNGBuildingDialog;
}

class PNGBuildingDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit PNGBuildingDialog(World *world, QWidget *parent = 0);
    ~PNGBuildingDialog();

    bool generateWorld(World *world);
    bool generateCell(WorldCell *cell);

    bool processObjectGroups(WorldCell *cell, MapComposite *mapComposite);
    bool processObjectGroup(WorldCell *cell, Tiled::ObjectGroup *objectGroup,
                            int levelOffset, const QPoint &offset);

private slots:
    void accept();
    void browse();
    
private:
    Ui::PNGBuildingDialog *ui;
    World *mWorld;
    QImage mImage;
    QPainter *mPainter;
    QColor mColor;
    QString mError;
};

#endif // PNGBUILDINGDIALOG_H
