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

#ifndef ROADSDOCK_H
#define ROADSDOCK_H

#include <QDockWidget>

class CellDocument;
class Document;
class WorldDocument;

class QSpinBox;

class RoadsDock : public QDockWidget
{
    Q_OBJECT

public:
    RoadsDock(QWidget *parent = 0);

    void changeEvent(QEvent *e);
    void retranslateUi();

    void setDocument(Document *doc);
    void clearDocument();

private slots:
    void selectedRoadsChanged();
    void roadWidthSpinBoxValueChanged(int newValue);

private:
    CellDocument *mCellDoc;
    WorldDocument *mWorldDoc;
    QSpinBox *mRoadWidthSpinBox;
};

#endif // ROADSDOCK_H
