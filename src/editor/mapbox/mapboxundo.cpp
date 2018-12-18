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

#include "mapboxundo.h"

#include "worlddocument.h"

/////

AddRemoveMapboxFeature::AddRemoveMapboxFeature(WorldDocument *doc, WorldCell *cell, int index, MapBoxFeature* feature)
    : mDocument(doc)
    , mCell(cell)
    , mFeature(feature)
    , mIndex(index)
{
}

AddRemoveMapboxFeature::~AddRemoveMapboxFeature()
{
    delete mFeature;
}

void AddRemoveMapboxFeature::addFeature()
{
    mDocument->undoRedo().addMapboxFeature(mCell, mIndex, mFeature);
    mFeature = nullptr;
}

void AddRemoveMapboxFeature::removeFeature()
{
    mFeature = mDocument->undoRedo().removeMapboxFeature(mCell, mIndex);
}

/////

MoveMapboxPoint::MoveMapboxPoint(WorldDocument *doc, WorldCell* cell, int featureIndex, int pointIndex, const MapBoxPoint& point)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Move Mapbox Point"))
    , mDocument(doc)
    , mCell(cell)
    , mFeatureIndex(featureIndex)
    , mPointIndex(pointIndex)
    , mPoint(point)
{
}

void MoveMapboxPoint::swap()
{
    mPoint = mDocument->undoRedo().moveMapboxPoint(mCell, mFeatureIndex, mPointIndex, mPoint);
}

/////

AddRemoveMapboxProperty::AddRemoveMapboxProperty(WorldDocument *doc, WorldCell *cell, int featureIndex, int propertyIndex, const MapBoxProperty& property)
    : mDocument(doc)
    , mCell(cell)
    , mFeatureIndex(featureIndex)
    , mPropertyIndex(propertyIndex)
    , mProperty(property)
{
}

AddRemoveMapboxProperty::~AddRemoveMapboxProperty()
{
}

void AddRemoveMapboxProperty::addProperty() {
    mDocument->undoRedo().addMapboxProperty(mCell, mFeatureIndex, mPropertyIndex, mProperty);
}

void AddRemoveMapboxProperty::removeProperty()
{
    mProperty = mDocument->undoRedo().removeMapboxProperty(mCell, mFeatureIndex, mPropertyIndex);
}

/////

SetMapboxProperty::SetMapboxProperty(WorldDocument *doc, WorldCell* cell, int featureIndex, int pointIndex, const MapBoxProperty& property)
    : QUndoCommand(QCoreApplication::translate("Undo Commands", "Edit Mapbox Property"))
    , mDocument(doc)
    , mCell(cell)
    , mFeatureIndex(featureIndex)
    , mPropertyIndex(pointIndex)
    , mProperty(property)
{
}

void SetMapboxProperty::swap()
{
    mProperty = mDocument->undoRedo().setMapboxProperty(mCell, mFeatureIndex, mPropertyIndex, mProperty);
}
