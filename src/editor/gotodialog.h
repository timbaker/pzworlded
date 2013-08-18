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

#ifndef GOTODIALOG_H
#define GOTODIALOG_H

#include <QDialog>

namespace Ui {
class GoToDialog;
}

class World;

class GoToDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit GoToDialog(World *world, const QPoint &initial, QWidget *parent = 0);
    ~GoToDialog();

    int worldX() const;
    int worldY() const;
    
private slots:
    void worldXChanged(int val);
    void worldYChanged(int val);

    void cellXChanged(int val);
    void cellYChanged(int val);

    void posXChanged(int val);
    void posYChanged(int val);

private:
    Ui::GoToDialog *ui;
    World *mWorld;
    int mSynching;
};

#endif // GOTODIALOG_H
