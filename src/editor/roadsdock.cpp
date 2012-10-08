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

#include "roadsdock.h"

#include "celldocument.h"
#include "undoredo.h"
#include "scenetools.h"
#include "world.h"
#include "worlddocument.h"

#include <QEvent>
#include <QSpinBox>
#include <QUndoStack>
#include <QVBoxLayout>

RoadsDock::RoadsDock(QWidget *parent)
    : QDockWidget(parent)
    , mCellDoc(0)
    , mWorldDoc(0)
{
    setObjectName(QLatin1String("RoadsDock"));
    retranslateUi();

    QWidget *w = new QWidget(this);

    mRoadWidthSpinBox = new QSpinBox(w);
    mRoadWidthSpinBox->setMinimum(1);
    mRoadWidthSpinBox->setMaximum(50);
    mRoadWidthSpinBox->setValue(1);
    connect(mRoadWidthSpinBox, SIGNAL(valueChanged(int)),
            SLOT(roadWidthSpinBoxValueChanged(int)));

    QVBoxLayout *layout = new QVBoxLayout(w);
    layout->addWidget(mRoadWidthSpinBox);

    setWidget(w);

#if 0
    // Workaround since a tabbed dockwidget that is not currently visible still
    // returns true for isVisible()
    connect(this, SIGNAL(visibilityChanged(bool)),
            mView, SLOT(setVisible(bool)));
#endif
}

void RoadsDock::changeEvent(QEvent *e)
{
    QDockWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        retranslateUi();
        break;
    default:
        break;
    }
}

void RoadsDock::retranslateUi()
{
    setWindowTitle(tr("Roads"));
}

void RoadsDock::setDocument(Document *doc)
{
    if (mCellDoc)
        mCellDoc->disconnect(this);
    if (mWorldDoc)
        mWorldDoc->disconnect(this);

    mCellDoc = doc ? doc->asCellDocument() : 0;
    mWorldDoc = doc ? doc->asWorldDocument() : 0;

    if (mWorldDoc) {
        mRoadWidthSpinBox->setValue(CreateRoadTool::instance()->curretRoadWidth());
        connect(mWorldDoc, SIGNAL(selectedRoadsChanged()),
                SLOT(selectedRoadsChanged()));
    }
}

void RoadsDock::clearDocument()
{
    setDocument(0);
}

void RoadsDock::selectedRoadsChanged()
{
    if (!mWorldDoc)
        return;
    QList<Road*> selectedRoads = mWorldDoc->selectedRoads();
    if (selectedRoads.count() > 0) {
        Road *road = selectedRoads.first();
        mRoadWidthSpinBox->setValue(road->width());
    } else {
    }

}

void RoadsDock::roadWidthSpinBoxValueChanged(int newValue)
{
    CreateRoadTool::instance()->setCurrentRoadWidth(newValue);
    if (!mWorldDoc)
        return;
    QList<Road*> roads;
    foreach (Road *road, mWorldDoc->selectedRoads()) {
        if (road->width() == newValue)
            continue;
        roads += road;
    }
    int count = roads.count();
    if (!count)
        return;
    if (count > 1)
        mWorldDoc->undoStack()->beginMacro(tr("Change %1 Roads' Width to %2")
                                           .arg(count).arg(newValue));
    foreach (Road *road, roads) {
        if (road->width() == newValue)
            continue;
        mWorldDoc->changeRoadWidth(road, newValue);
    }
    if (count > 1)
        mWorldDoc->undoStack()->endMacro();
}
