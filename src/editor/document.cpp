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

#include "document.h"

#include "celldocument.h"
#include "worlddocument.h"

#include <QUndoStack>

Document::Document(DocumentType type)
    : QObject()
    , mUndoStack(0)
    , mType(type)
    , mView(0)
{
}

CellDocument *Document::asCellDocument()
{
    return isCellDocument() ? static_cast<CellDocument*>(this) : NULL;
}

WorldDocument *Document::asWorldDocument()
{
    return isWorldDocument() ? static_cast<WorldDocument*>(this) : NULL;
}

bool Document::isModified() const
{
    return mUndoStack->isClean() == false;
}
