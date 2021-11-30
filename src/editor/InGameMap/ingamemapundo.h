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

#ifndef INGAMEMAPUNDO_H
#define INGAMEMAPUNDO_H

#include "ingamemapcell.h"

#include <QCoreApplication>
#include <QUndoCommand>

class WorldCell;
class WorldDocument;

class AddRemoveInGameMapFeature : public QUndoCommand
{
public:
    AddRemoveInGameMapFeature(WorldDocument *doc, WorldCell *cell, int index, InGameMapFeature* feature);
    ~AddRemoveInGameMapFeature();

protected:
    void addFeature();
    void removeFeature();

    WorldDocument *mDocument;
    WorldCell *mCell;
    InGameMapFeature* mFeature;
    int mIndex;
};

class AddInGameMapFeature : public AddRemoveInGameMapFeature
{
public:
    AddInGameMapFeature(WorldDocument *doc, WorldCell *cell, int index, InGameMapFeature* feature)
        : AddRemoveInGameMapFeature(doc, cell, index, feature)
    {
        setText(QCoreApplication::translate("Undo Commands", "Add InGameMap Feature"));
    }

    void undo() { removeFeature(); }
    void redo() { addFeature(); }
};

class RemoveInGameMapFeature : public AddRemoveInGameMapFeature
{
public:
    RemoveInGameMapFeature(WorldDocument *doc, WorldCell *cell, int index)
        : AddRemoveInGameMapFeature(doc, cell, index, nullptr)
    {
        setText(QCoreApplication::translate("Undo Commands", "Remove InGameMap Feature"));
    }

    void undo() { addFeature(); }
    void redo() { removeFeature(); }
};

class MoveInGameMapPoint : public QUndoCommand
{
public:
    MoveInGameMapPoint(WorldDocument *doc, WorldCell* cell, int featureIndex, int coordIndex, int pointIndex, const InGameMapPoint &newPos);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    WorldDocument *mDocument;
    WorldCell* mCell;
    int mFeatureIndex;
    int mCoordIndex;
    int mPointIndex;
    InGameMapPoint mPoint;
};

class AddRemoveInGameMapProperty : public QUndoCommand
{
public:
    AddRemoveInGameMapProperty(WorldDocument *doc, WorldCell *cell, int featureIndex, int propertyIndex, const InGameMapProperty& property);
    ~AddRemoveInGameMapProperty();

protected:
    void addProperty();
    void removeProperty();

    WorldDocument *mDocument;
    WorldCell *mCell;
    int mFeatureIndex;
    int mPropertyIndex;
    InGameMapProperty mProperty;
};

class AddInGameMapProperty : public AddRemoveInGameMapProperty
{
public:
    AddInGameMapProperty(WorldDocument *doc, WorldCell *cell, int featureIndex, int propertyIndex, const InGameMapProperty& property)
        : AddRemoveInGameMapProperty(doc, cell, featureIndex, propertyIndex, property)
    {
        setText(QCoreApplication::translate("Undo Commands", "Add InGameMap Property"));
    }

    void undo() { removeProperty(); }
    void redo() { addProperty(); }
};

class RemoveInGameMapProperty : public AddRemoveInGameMapProperty
{
public:
    RemoveInGameMapProperty(WorldDocument *doc, WorldCell *cell, int featureIndex, int propertyIndex)
        : AddRemoveInGameMapProperty(doc, cell, featureIndex, propertyIndex, InGameMapProperty())
    {
        setText(QCoreApplication::translate("Undo Commands", "Remove InGameMap Property"));
    }

    void undo() { addProperty(); }
    void redo() { removeProperty(); }
};

class SetInGameMapProperty : public QUndoCommand
{
public:
    SetInGameMapProperty(WorldDocument *doc, WorldCell *cell, int featureIndex, int propertyIndex, const InGameMapProperty& property);

    void undo() override { swap(); }
    void redo() override { swap(); }

private:
    void swap();

    WorldDocument *mDocument;
    WorldCell *mCell;
    int mFeatureIndex;
    int mPropertyIndex;
    InGameMapProperty mProperty;
};

class SetInGameMapProperties : public QUndoCommand
{
public:
    SetInGameMapProperties(WorldDocument *doc, WorldCell *cell, int featureIndex, const InGameMapProperties& properties);

    void undo() override { swap(); }
    void redo() override { swap(); }

private:
    void swap();

    WorldDocument *mDocument;
    WorldCell *mCell;
    int mFeatureIndex;
    InGameMapProperties mProperties;
};

class SetInGameMapCoordinates : public QUndoCommand
{
public:
    SetInGameMapCoordinates(WorldDocument *doc, WorldCell *cell, int featureIndex, int coordsIndex, const InGameMapCoordinates& coords);

    void undo() override { swap(); }
    void redo() override { swap(); }

private:
    void swap();

    WorldDocument *mDocument;
    WorldCell *mCell;
    int mFeatureIndex;
    int mCoordsIndex;
    InGameMapCoordinates mCoords;
};

class AddRemoveInGameMapHole : public QUndoCommand
{
public:
    AddRemoveInGameMapHole(WorldDocument *doc, WorldCell *cell, int featureIndex, int holeIndex, const InGameMapCoordinates &hole);

protected:
    void addHole();
    void removeHole();

    WorldDocument *mDocument;
    WorldCell *mCell;
    int mFeatureIndex;
    int mHoleIndex;
    InGameMapCoordinates mHole;
};

class AddInGameMapHole : public AddRemoveInGameMapHole
{
public:
    AddInGameMapHole(WorldDocument *doc, WorldCell *cell, int featureIndex, int holeIndex, const InGameMapCoordinates &hole)
        : AddRemoveInGameMapHole(doc, cell, featureIndex, holeIndex, hole)
    {
        setText(QCoreApplication::translate("Undo Commands", "Add InGameMap Hole"));
    }

    void undo() { removeHole(); }
    void redo() { addHole(); }
};

class RemoveInGameMapHole : public AddRemoveInGameMapHole
{
public:
    RemoveInGameMapHole(WorldDocument *doc, WorldCell *cell, int featureIndex, int holeIndex)
        : AddRemoveInGameMapHole(doc, cell, featureIndex, holeIndex, {})
    {
        setText(QCoreApplication::translate("Undo Commands", "Remove InGameMap Hole"));
    }

    void undo() { addHole(); }
    void redo() { removeHole(); }
};

class ConvertToInGameMapPolygon : public QUndoCommand
{
public:
    ConvertToInGameMapPolygon(WorldDocument *doc, WorldCell *cell, int featureIndex);

    void undo() override { swap(); }
    void redo() override { swap(); }

private:
    void swap();

    WorldDocument *mDocument;
    WorldCell *mCell;
    int mFeatureIndex;
};

#endif // INGAMEMAPUNDO_H
