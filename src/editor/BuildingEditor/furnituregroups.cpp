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

#include "furnituregroups.h"

#include "preferences.h"
#include "buildingfurniturefile.h"
#include "buildingtiles.h"
#include "buildingfloor.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>

using namespace BuildingEditor;

static const char *TXT_FILE = "BuildingFurniture.txt";

FurnitureGroups *FurnitureGroups::mInstance = 0;

FurnitureGroups *FurnitureGroups::instance()
{
    if (!mInstance)
        mInstance = new FurnitureGroups;
    return mInstance;
}

void FurnitureGroups::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

FurnitureGroups::FurnitureGroups() :
    QObject()
{
}

void FurnitureGroups::addGroup(FurnitureGroup *group)
{
    mGroups += group;
}

void FurnitureGroups::insertGroup(int index, FurnitureGroup *group)
{
    mGroups.insert(index, group);
}

FurnitureGroup *FurnitureGroups::removeGroup(int index)
{
    return mGroups.takeAt(index);
}

void FurnitureGroups::removeGroup(FurnitureGroup *group)
{
    mGroups.removeAll(group);
}

FurnitureTiles *FurnitureGroups::findMatch(FurnitureTiles *other)
{
    foreach (FurnitureGroup *candidate, mGroups) {
        if (FurnitureTiles *ftiles = candidate->findMatch(other))
            return ftiles;
    }
    return 0;
}

QString FurnitureGroups::txtName()
{
    return QLatin1String(TXT_FILE);
}

QString FurnitureGroups::txtPath()
{
    return Preferences::instance()->configPath(txtName());
}

bool FurnitureGroups::readTxt()
{
    QFileInfo info(txtPath());
    if (!info.exists()) {
        mError = tr("The %1 file doesn't exist.").arg(txtName());
        return false;
    }

#if !defined(WORLDED)
    if (!upgradeTxt())
        return false;

    if (!mergeTxt())
        return false;
#endif

    QString path = info.canonicalFilePath();
    BuildingFurnitureFile file;
    if (!file.read(path)) {
        mError = file.errorString();
        return false;
    }

    mRevision = file.getRevision();
    mSourceRevision = file.getSourceRevision();
    mGroups = file.takeGroups();
    return true;
#if 0
    FurnitureTiles *tiles = new FurnitureTiles;

    FurnitureTile *tile = new FurnitureTile(FurnitureTile::FurnitureW);
    tile->mTiles[0] = new BuildingTile(QLatin1String("furniture_seating_indoor_01"), 3);
    tile->mTiles[2] = new BuildingTile(QLatin1String("furniture_seating_indoor_01"), 2);
    tiles->mTiles[0] = tile;

    tile = new FurnitureTile(FurnitureTile::FurnitureN);
    tile->mTiles[0] = new BuildingTile(QLatin1String("furniture_seating_indoor_01"), 0);
    tile->mTiles[1] = new BuildingTile(QLatin1String("furniture_seating_indoor_01"), 1);
    tiles->mTiles[1] = tile;

    tile = new FurnitureTile(FurnitureTile::FurnitureE);
    tile->mTiles[0] = new BuildingTile(QLatin1String("furniture_seating_indoor_01"), 5);
    tile->mTiles[2] = new BuildingTile(QLatin1String("furniture_seating_indoor_01"), 4);
    tiles->mTiles[2] = tile;

    tile = new FurnitureTile(FurnitureTile::FurnitureS);
    tile->mTiles[0] = new BuildingTile(QLatin1String("furniture_seating_indoor_01"), 6);
    tile->mTiles[1] = new BuildingTile(QLatin1String("furniture_seating_indoor_01"), 7);
    tiles->mTiles[3] = tile;

    FurnitureGroup *group = new FurnitureGroup;
    group->mLabel = QLatin1String("Indoor Seating");
    group->mTiles += tiles;

    mGroups += group;

    return true;
#endif
}

bool FurnitureGroups::writeTxt()
{
#ifdef WORLDED
    return false;
#endif
    BuildingFurnitureFile file;
    if (!file.write(txtPath(), mRevision + 1, mSourceRevision, groups())) {
        mError = file.errorString();
        return false;
    }
    ++mRevision;
    return true;
}

int FurnitureGroups::setRevision(int revision)
{
    int old = mRevision;
    mRevision = revision;
    return old;
}

int FurnitureGroups::setSourceRevision(int sourceRevision)
{
    int old = mSourceRevision;
    mSourceRevision = sourceRevision;
    return old;
}

int FurnitureGroups::indexOf(const QString &name) const
{
    int index = 0;
    foreach (FurnitureGroup *group, mGroups) {
        if (group->mLabel == name)
            return index;
        ++index;
    }
    return -1;
}

void FurnitureGroups::tileChanged(FurnitureTile *ftile)
{
    emit furnitureTileChanged(ftile);
}

void FurnitureGroups::layerChanged(FurnitureTiles *ftiles)
{
    emit furnitureLayerChanged(ftiles);
}

void FurnitureGroups::grimeChanged(FurnitureTile *ftile)
{
    emit furnitureTileChanged(ftile);
}

bool FurnitureGroups::upgradeTxt()
{
    QString userPath = txtPath();

    BuildingFurnitureFile userFile;
    if (!userFile.read(userPath)) {
        mError = userFile.errorString();
        return false;
    }

    int userVersion = userFile.getVersion(); // may be zero for unversioned file
    if (userVersion == BuildingFurnitureFile::getVersionLatest()) {
        return true;
    }

    // Not the latest version -> upgrade it.

    // UPGRADE HERE

    // Write the user file out with the latest version and format.
    if (!userFile.write(userPath)) {
        mError = userFile.errorString();
        return false;
    }
    return true;
}

bool FurnitureGroups::mergeTxt()
{
    QString userPath = txtPath();

    BuildingFurnitureFile userFile;
    if (!userFile.read(userPath)) {
        mError = userFile.errorString();
        return false;
    }
    Q_ASSERT(userFile.getVersion() == BuildingFurnitureFile::getVersionLatest());

    QString sourcePath = Preferences::instance()->appConfigPath(txtName());

    BuildingFurnitureFile sourceFile;
    if (!sourceFile.read(sourcePath)) {
        mError = sourceFile.errorString();
        return false;
    }
    Q_ASSERT(sourceFile.getVersion() == BuildingFurnitureFile::getVersionLatest());

    int userSourceRevision = userFile.getSourceRevision();
    int sourceRevision = sourceFile.getRevision();
    if (sourceRevision == userSourceRevision) {
        return true;
    }

    // MERGE HERE

    QList<FurnitureGroup*> sourceGroups = sourceFile.takeGroups();
    QList<FurnitureGroup*> userGroups = userFile.takeGroups();
    QMap<QString,FurnitureGroup*> userGroupsByName;
    QMap<QString,int> userGroupIndexByName;
    int index = 0;
    for (FurnitureGroup *group : std::as_const(userGroups)) {
        const QString label = group->mLabel;
        userGroupsByName[label] = group;
        userGroupIndexByName[label] = index++;
    }

    QList<FurnitureGroup*> takenGroups;
    for (FurnitureGroup *sourceGroup : std::as_const(sourceGroups)) {
        if (userGroupsByName.contains(sourceGroup->mLabel)) {
            // A user-group with the same name as a source-group exists.
            // Copy unique source-furniture to the user-group.
            FurnitureGroup *userGroup = userGroupsByName[sourceGroup->mLabel];
            int userFurnitureIndex = 0;
            QList<FurnitureTiles*> takenTiles;
            for (FurnitureTiles *sourceTiles : std::as_const(sourceGroup->mTiles)) {
                if (FurnitureTiles *userTiles = userGroup->findMatch(sourceTiles)) {
                    userFurnitureIndex = userGroup->mTiles.indexOf(userTiles) + 1;
                } else {
                    sourceTiles->setGroup(userGroup);
                    userGroup->mTiles.insert(userFurnitureIndex, sourceTiles);
                    takenTiles += sourceTiles;
                    qDebug() << "FurnitureGroups.txt merge: inserted furniture in group" << sourceGroup->mLabel << "at" << userFurnitureIndex;
                    ++userFurnitureIndex;
                }
            }
            for (FurnitureTiles *sourceTiles : std::as_const(takenTiles)) {
                sourceGroup->mTiles.removeOne(sourceTiles);
            }
        } else {
            // The source-group doesn't exist in the user-file.
            // Copy the source-group to the user-file.
            userGroupsByName[sourceGroup->mLabel] = sourceGroup;
            int index = userGroupsByName.keys().indexOf(sourceGroup->mLabel); // insert group alphabetically
            userGroups.insert(index, sourceGroup);
            for (const QString &label : userGroupsByName.keys()) {
                if (userGroupIndexByName[label] >= index) {
                    userGroupIndexByName[label]++;
                }
            }
            userGroupIndexByName[sourceGroup->mLabel] = index;
            takenGroups += sourceGroup;
            qDebug() << "FurnitureGroups.txt merge: inserted group" << sourceGroup->mLabel << "at" << index;
        }
    }
    for (FurnitureGroup *group : std::as_const(takenGroups)) {
        sourceGroups.removeOne(group);
    }
    if (!userFile.write(userPath, sourceRevision + 1, sourceRevision, userGroups)) {
        mError = userFile.errorString();
        qDeleteAll(sourceGroups);
        qDeleteAll(userGroups);
        return false;
    }
    qDeleteAll(sourceGroups);
    qDeleteAll(userGroups);
    return true;
}

/////

FurnitureTile::FurnitureTile(FurnitureTiles *ftiles, FurnitureOrientation orient) :
    mOwner(ftiles),
    mOrient(orient),
    mSize(1, 1),
    mTiles(1, 0),
    mGrime(true)
{

}

FurnitureTile *FurnitureTile::createCopy(FurnitureTiles *ftiles)
{
    FurnitureTile *copy = new FurnitureTile(ftiles, orient());
    copy->mSize = size();
    copy->mTiles = tiles();
    copy->setAllowGrime(allowGrime());
    return copy;
}

void FurnitureTile::clear()
{
    mTiles.fill(0, 1);
    mSize = QSize(1, 1);
}

QSize FurnitureTile::size() const
{
    return mSize;
}

bool FurnitureTile::equals(FurnitureTile *other) const
{
    return other->mTiles == mTiles &&
            other->mOrient == mOrient &&
            other->mSize == mSize &&
            other->mGrime == mGrime;
}

void FurnitureTile::setTile(int x, int y, BuildingTile *btile)
{
    // Get larger if needed
    if ((btile != 0) && (x >= mSize.width() || y >= mSize.height())) {
        resize(qMax(mSize.width(), x + 1), qMax(mSize.height(), y + 1));
    }

    mTiles[x + y * mSize.width()] = btile;

    // Get smaller if needed
    if ((btile == 0) && (x == mSize.width() - 1 || y == mSize.height() - 1)) {
        int width = mSize.width(), height = mSize.height();
        while (width > 1 && columnEmpty(width - 1))
            width--;
        while (height > 1 && rowEmpty(height - 1))
            height--;
        if (width < mSize.width() || height < mSize.height())
            resize(width, height);
    }
}

BuildingTile *FurnitureTile::tile(int x, int y) const
{
    if (x < 0 || x >= mSize.width()) return 0;
    if (y < 0 || y >= mSize.height()) return 0;
    return mTiles[x + y * mSize.width()];
}

bool FurnitureTile::isEmpty() const
{
    foreach (BuildingTile *btile, mTiles)
        if (btile != 0)
            return false;
    return true;
}

FurnitureTile *FurnitureTile::resolved()
{
    if (isEmpty()) {
        if (mOrient == FurnitureE)
            return mOwner->tile(FurnitureW);
        if (mOrient == FurnitureN)
            return mOwner->tile(FurnitureW);
        if (mOrient == FurnitureS) {
            if (!mOwner->tile(FurnitureN)->isEmpty())
                return mOwner->tile(FurnitureN);
            return mOwner->tile(FurnitureW);
        }
    }
    return this;
}

bool FurnitureTile::isCornerOrient(FurnitureTile::FurnitureOrientation orient)
{
    return orient == FurnitureSW || orient == FurnitureSE ||
            orient == FurnitureNW || orient == FurnitureNE;
}

FloorTileGrid *FurnitureTile::toFloorTileGrid(QRegion &rgn)
{
    rgn = QRegion();
    if (size().isNull() || isEmpty())
        return 0;

    FloorTileGrid *tiles = new FloorTileGrid(width(), height());
    for (int x = 0; x < width(); x++) {
        for (int y = 0; y < height(); y++) {
            if (BuildingTile *btile = tile(x, y)) {
                if (!btile->isNone()) {
                    tiles->replace(x, y, btile->name());
                    rgn += QRect(x, y, 1, 1);
                }
            }
        }
    }
    if (rgn.isEmpty()) {
        delete tiles;
        return 0;
    }
    return tiles;
}

void FurnitureTile::resize(int width, int height)
{
    QVector<BuildingTile*> newTiles(width * height);
    for (int x = 0; x < qMin(width, mSize.width()); x++)
        for (int y = 0; y < qMin(height, mSize.height()); y++)
            newTiles[x + y * width] = mTiles[x + y * mSize.width()];
    mTiles = newTiles;
    mSize = QSize(width, height);
}

bool FurnitureTile::columnEmpty(int x)
{
    for (int y = 0; y < mSize.height(); y++)
        if (tile(x, y))
            return false;
    return true;
}

bool FurnitureTile::rowEmpty(int y)
{
    for (int x = 0; x < mSize.width(); x++)
        if (tile(x, y))
            return false;
    return true;
}

/////

FurnitureTiles::FurnitureTiles(bool corners) :
    mGroup(0),
    mTiles(8, 0),
    mCorners(corners),
    mLayer(LayerFurniture)
{
    mTiles[FurnitureTile::FurnitureW] = new FurnitureTile(this, FurnitureTile::FurnitureW);
    mTiles[FurnitureTile::FurnitureN] = new FurnitureTile(this, FurnitureTile::FurnitureN);
    mTiles[FurnitureTile::FurnitureE] = new FurnitureTile(this, FurnitureTile::FurnitureE);
    mTiles[FurnitureTile::FurnitureS] = new FurnitureTile(this, FurnitureTile::FurnitureS);
    mTiles[FurnitureTile::FurnitureSW] = new FurnitureTile(this, FurnitureTile::FurnitureSW);
    mTiles[FurnitureTile::FurnitureNW] = new FurnitureTile(this, FurnitureTile::FurnitureNW);
    mTiles[FurnitureTile::FurnitureNE] = new FurnitureTile(this, FurnitureTile::FurnitureNE);
    mTiles[FurnitureTile::FurnitureSE] = new FurnitureTile(this, FurnitureTile::FurnitureSE);
}

FurnitureTiles::~FurnitureTiles()
{
    qDeleteAll(mTiles);
}

FurnitureTiles *FurnitureTiles::createCopy(FurnitureGroup *group) const
{
    FurnitureTiles *copy = new FurnitureTiles(hasCorners());
    copy->setGroup(group);
    for (FurnitureTile *ftile : tiles()) {
        FurnitureTile *ftileCopy = ftile->createCopy(copy);
        copy->setTile(ftileCopy);
    }
    copy->setLayer(layer());
    // copy->mLayerNames = layerNames();
    return copy;
}

bool FurnitureTiles::isEmpty() const
{
    for (int i = 0; i < mTiles.size(); i++)
        if (!mTiles.isEmpty())
            return false;
    return true;
}

void FurnitureTiles::setTile(FurnitureTile *ftile)
{
    delete mTiles[ftile->orient()];
    mTiles[ftile->orient()] = ftile;
}

FurnitureTile *FurnitureTiles::tile(FurnitureTile::FurnitureOrientation orient) const
{
    return mTiles[orient];
}

FurnitureTile *FurnitureTiles::tile(int orient) const
{
    Q_ASSERT(orient >= 0 && orient < FurnitureTile::OrientCount);
    if (orient >= 0 && orient < FurnitureTile::OrientCount)
        return mTiles[orient];
    return 0;
}

bool FurnitureTiles::equals(const FurnitureTiles *other)
{
    if (other->mLayer != mLayer)
        return false;

    for (int i = 0; i < mTiles.size(); i++)
        if (!other->mTiles[i]->equals(mTiles[i]))
            return false;
    return true;
}

QString FurnitureTiles::layerToString(FurnitureTiles::FurnitureLayer layer)
{
    if (layer >= 0 && layer < mLayerNames.size()) {
        initNames();
        return mLayerNames[layer];
    }
    return QLatin1String("Invalid");
}

FurnitureTiles::FurnitureLayer FurnitureTiles::layerFromString(const QString &s)
{
    initNames();
    if (mLayerNames.contains(s))
        return static_cast<FurnitureLayer>(mLayerNames.indexOf(s));
    return InvalidLayer;
}

QStringList FurnitureTiles::mLayerNames;

void FurnitureTiles::initNames()
{
    if (mLayerNames.size())
        return;

    // Order must match FurnitureLayer enum
    mLayerNames += QLatin1String("Walls");
    mLayerNames += QLatin1String("RoofCap");
    mLayerNames += QLatin1String("WallOverlay");
    mLayerNames += QLatin1String("WallFurniture");
    mLayerNames += QLatin1String("Furniture");
    mLayerNames += QLatin1String("Frames");
    mLayerNames += QLatin1String("Doors");
    mLayerNames += QLatin1String("Roof");
    mLayerNames += QLatin1String("FloorFurniture");

    Q_ASSERT(mLayerNames.size() == LayerCount);
}

/////

FurnitureGroup *FurnitureGroup::createCopy() const
{
    FurnitureGroup *copy = new FurnitureGroup();
    copy->mLabel = mLabel;
    for (const FurnitureTiles *ftiles : mTiles) {
        copy->mTiles += ftiles->createCopy(copy);
    }
    return copy;
}

int FurnitureGroup::indexOf(const FurnitureTiles *ftiles) const
{
    return mTiles.indexOf(const_cast<FurnitureTiles*>(ftiles));
}

FurnitureTiles *FurnitureGroup::tiles(int index)
{
    return index >= 0 && index < mTiles.size() ? mTiles.at(index) : nullptr;
}

FurnitureTiles *FurnitureGroup::findMatch(FurnitureTiles *ftiles) const
{
    foreach (FurnitureTiles *candidate, mTiles) {
        if (candidate->equals(ftiles))
            return candidate;
    }
    return 0;
}
