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

#ifndef DEFAULTSFILE_H
#define DEFAULTSFILE_H

#include "world.h"

#include <QCoreApplication>

class DefaultsFile
{
    Q_DECLARE_TR_FUNCTIONS(DefaultsFile)

public:
    static void newWorld(World* world);
    static void oldWorld(World* world);

    DefaultsFile();
    ~DefaultsFile();

    QString txtName();
    QString txtPath();

    bool read(const QString &fileName);

    QString errorString() const
    { return mError; }

    PropertyEnumList mEnums;
    PropertyDefList mPropertyDefs;
    PropertyTemplateList mTemplates;

    ObjectType* mNullObjectType;
    ObjectTypeList mObjectTypes;
    ObjectGroupList mObjectGroups;

private:
    QString mError;
};

#endif // DEFAULTSFILE_H
