/*
 * Copyright 2019, Tim Baker <treectrl@users.sf.net>
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

#include "mapinfo.h"

#include "mapinfomanager.h"
//#include "mapmanager.h"

bool MapInfo::isLoading() const
{
    return isEmpty(); // && desired state == AssetState::READY
}

void MapInfo::setFrom(MapInfo *other)
{
    mOrientation = other->mOrientation;
    mWidth = other->mWidth;
    mHeight = other->mHeight;
    mTileWidth = other->mTileWidth;
    mTileHeight = other->mTileHeight;
    mFilePath = other->mFilePath;
    mPlaceholder = other->mPlaceholder;
    mBeingEdited = other->mBeingEdited;
#ifdef WORLDED
    mReferenceEpoch = other->mReferenceEpoch;
#endif
}
