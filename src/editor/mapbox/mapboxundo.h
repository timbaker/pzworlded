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

#ifndef MAPBOXUNDO_H
#define MAPBOXUNDO_H

#include "worldcellmapbox.h"

#include <QCoreApplication>
#include <QUndoCommand>

class WorldCell;
class WorldDocument;

class AddRemoveMapboxFeature : public QUndoCommand
{
public:
    AddRemoveMapboxFeature(WorldDocument *doc, WorldCell *cell, int index, MapBoxFeature* feature);
    ~AddRemoveMapboxFeature();

protected:
    void addFeature();
    void removeFeature();

    WorldDocument *mDocument;
    WorldCell *mCell;
    MapBoxFeature* mFeature;
    int mIndex;
};

class AddMapboxFeature : public AddRemoveMapboxFeature
{
public:
    AddMapboxFeature(WorldDocument *doc, WorldCell *cell, int index, MapBoxFeature* feature)
        : AddRemoveMapboxFeature(doc, cell, index, feature)
    {
        setText(QCoreApplication::translate("Undo Commands", "Add Mapbox Feature"));
    }

    void undo() { removeFeature(); }
    void redo() { addFeature(); }
};

class RemoveMapboxFeature : public AddRemoveMapboxFeature
{
public:
    RemoveMapboxFeature(WorldDocument *doc, WorldCell *cell, int index)
        : AddRemoveMapboxFeature(doc, cell, index, nullptr)
    {
        setText(QCoreApplication::translate("Undo Commands", "Remove Mapbox Feature"));
    }

    void undo() { addFeature(); }
    void redo() { removeFeature(); }
};

class MoveMapboxPoint : public QUndoCommand
{
public:
    MoveMapboxPoint(WorldDocument *doc, WorldCell* cell, int featureIndex, int pointIndex, const MapBoxPoint &newPos);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    WorldDocument *mDocument;
    WorldCell* mCell;
    int mFeatureIndex;
    int mPointIndex;
    MapBoxPoint mPoint;
};

class AddRemoveMapboxProperty : public QUndoCommand
{
public:
    AddRemoveMapboxProperty(WorldDocument *doc, WorldCell *cell, int featureIndex, int propertyIndex, const MapBoxProperty& property);
    ~AddRemoveMapboxProperty();

protected:
    void addProperty();
    void removeProperty();

    WorldDocument *mDocument;
    WorldCell *mCell;
    int mFeatureIndex;
    int mPropertyIndex;
    MapBoxProperty mProperty;
};

class AddMapboxProperty : public AddRemoveMapboxProperty
{
public:
    AddMapboxProperty(WorldDocument *doc, WorldCell *cell, int featureIndex, int propertyIndex, const MapBoxProperty& property)
        : AddRemoveMapboxProperty(doc, cell, featureIndex, propertyIndex, property)
    {
        setText(QCoreApplication::translate("Undo Commands", "Add Mapbox Property"));
    }

    void undo() { removeProperty(); }
    void redo() { addProperty(); }
};

class RemoveMapboxProperty : public AddRemoveMapboxProperty
{
public:
    RemoveMapboxProperty(WorldDocument *doc, WorldCell *cell, int featureIndex, int propertyIndex)
        : AddRemoveMapboxProperty(doc, cell, featureIndex, propertyIndex, MapBoxProperty())
    {
        setText(QCoreApplication::translate("Undo Commands", "Remove Mapbox Property"));
    }

    void undo() { addProperty(); }
    void redo() { removeProperty(); }
};

class SetMapboxProperty : public QUndoCommand
{
public:
    SetMapboxProperty(WorldDocument *doc, WorldCell *cell, int featureIndex, int propertyIndex, const MapBoxProperty& property);

    void undo() override { swap(); }
    void redo() override { swap(); }

private:
    void swap();

    WorldDocument *mDocument;
    WorldCell *mCell;
    int mFeatureIndex;
    int mPropertyIndex;
    MapBoxProperty mProperty;
};

class SetMapboxProperties : public QUndoCommand
{
public:
    SetMapboxProperties(WorldDocument *doc, WorldCell *cell, int featureIndex, const MapBoxProperties& properties);

    void undo() override { swap(); }
    void redo() override { swap(); }

private:
    void swap();

    WorldDocument *mDocument;
    WorldCell *mCell;
    int mFeatureIndex;
    MapBoxProperties mProperties;
};

class SetMapboxCoordinates : public QUndoCommand
{
public:
    SetMapboxCoordinates(WorldDocument *doc, WorldCell *cell, int featureIndex, int coordsIndex, const MapBoxCoordinates& coords);

    void undo() override { swap(); }
    void redo() override { swap(); }

private:
    void swap();

    WorldDocument *mDocument;
    WorldCell *mCell;
    int mFeatureIndex;
    int mCoordsIndex;
    MapBoxCoordinates mCoords;
};

#endif // MAPBOXUNDO_H
