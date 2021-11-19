/*
 * Copyright 2018, Tim Baker <treectrl@users.sf.net>
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

#include "ingamemapundo.h"

#include "worlddocument.h"

/////

AddRemoveInGameMapFeature::AddRemoveInGameMapFeature(WorldDocument *doc, WorldCell *cell, int index, InGameMapFeature* feature)
    : mDocument(doc)
    , mCell(cell)
    , mFeature(feature)
    , mIndex(index)
{
}

AddRemoveInGameMapFeature::~AddRemoveInGameMapFeature()
{
    delete mFeature;
}

void AddRemoveInGameMapFeature::addFeature()
{
    mDocument->undoRedo().addInGameMapFeature(mCell, mIndex, mFeature);
    mFeature = nullptr;
}

void AddRemoveInGameMapFeature::removeFeature()
{
    mFeature = mDocument->undoRedo().removeInGameMapFeature(mCell, mIndex);
}

/////

MoveInGameMapPoint::MoveInGameMapPoint(WorldDocument *doc, WorldCell* cell, int featureIndex, int coordIndex, int pointIndex, const InGameMapPoint& point)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Move InGameMap Point"))
    , mDocument(doc)
    , mCell(cell)
    , mFeatureIndex(featureIndex)
    , mCoordIndex(coordIndex)
    , mPointIndex(pointIndex)
    , mPoint(point)
{
}

void MoveInGameMapPoint::swap()
{
    mPoint = mDocument->undoRedo().moveInGameMapPoint(mCell, mFeatureIndex, mCoordIndex, mPointIndex, mPoint);
}

/////

AddRemoveInGameMapProperty::AddRemoveInGameMapProperty(WorldDocument *doc, WorldCell *cell, int featureIndex, int propertyIndex, const InGameMapProperty& property)
    : mDocument(doc)
    , mCell(cell)
    , mFeatureIndex(featureIndex)
    , mPropertyIndex(propertyIndex)
    , mProperty(property)
{
}

AddRemoveInGameMapProperty::~AddRemoveInGameMapProperty()
{
}

void AddRemoveInGameMapProperty::addProperty() {
    mDocument->undoRedo().addInGameMapProperty(mCell, mFeatureIndex, mPropertyIndex, mProperty);
}

void AddRemoveInGameMapProperty::removeProperty()
{
    mProperty = mDocument->undoRedo().removeInGameMapProperty(mCell, mFeatureIndex, mPropertyIndex);
}

/////

SetInGameMapProperty::SetInGameMapProperty(WorldDocument *doc, WorldCell* cell, int featureIndex, int pointIndex, const InGameMapProperty& property)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Edit InGameMap Property"))
    , mDocument(doc)
    , mCell(cell)
    , mFeatureIndex(featureIndex)
    , mPropertyIndex(pointIndex)
    , mProperty(property)
{
}

void SetInGameMapProperty::swap()
{
    mProperty = mDocument->undoRedo().setInGameMapProperty(mCell, mFeatureIndex, mPropertyIndex, mProperty);
}

/////

SetInGameMapProperties::SetInGameMapProperties(WorldDocument *doc, WorldCell* cell, int featureIndex, const InGameMapProperties& properties)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Set InGameMap Properties"))
    , mDocument(doc)
    , mCell(cell)
    , mFeatureIndex(featureIndex)
    , mProperties(properties)
{
}

void SetInGameMapProperties::swap()
{
    mProperties = mDocument->undoRedo().setInGameMapProperties(mCell, mFeatureIndex, mProperties);
}

/////

SetInGameMapCoordinates::SetInGameMapCoordinates(WorldDocument *doc, WorldCell* cell, int featureIndex, int coordsIndex, const InGameMapCoordinates& coords)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Set InGameMap Coordinates"))
    , mDocument(doc)
    , mCell(cell)
    , mFeatureIndex(featureIndex)
    , mCoordsIndex(coordsIndex)
    , mCoords(coords)
{
}

void SetInGameMapCoordinates::swap()
{
    mCoords = mDocument->undoRedo().setInGameMapCoordinates(mCell, mFeatureIndex, mCoordsIndex, mCoords);
}

/////

AddRemoveInGameMapHole::AddRemoveInGameMapHole(WorldDocument *doc, WorldCell *cell, int featureIndex, int holeIndex, const InGameMapCoordinates &hole)
    : mDocument(doc)
    , mCell(cell)
    , mFeatureIndex(featureIndex)
    , mHoleIndex(holeIndex)
    , mHole(hole)
{

}

void AddRemoveInGameMapHole::addHole()
{
    mDocument->undoRedo().addInGameMapHole(mCell, mFeatureIndex, mHoleIndex, mHole);
}

void AddRemoveInGameMapHole::removeHole()
{
    mHole = mDocument->undoRedo().removeInGameMapHole(mCell, mFeatureIndex, mHoleIndex);
}
