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

#ifndef ASSET_UTIL_H
#define ASSET_UTIL_H

#include <QDebug>

#include <QtCore/qglobal.h>

#if defined(TILED_LIBRARY)
#  define TILEDSHARED_EXPORT Q_DECL_EXPORT
#else
#  define TILEDSHARED_EXPORT Q_DECL_IMPORT
#endif

class non_copyable
{
protected:
    non_copyable() = default;
    ~non_copyable() = default;

    Q_DISABLE_COPY(non_copyable)

    non_copyable(non_copyable &&) = delete;
};

class TILEDSHARED_EXPORT AssetParams {};

class TILEDSHARED_EXPORT AssetPath : public QString
{
public:
    AssetPath()
    {}

    AssetPath(const QString& string)
        : QString(string)
    {}

    bool isValid() const
    {
        return !isEmpty();
    }

    QString getString() const
    {
        return *this;
    }
};

#endif // ASSET_UTIL_H
