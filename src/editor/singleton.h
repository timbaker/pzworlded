/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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

#ifndef SINGLETON_H
#define SINGLETON_H

#include <qglobal.h>

template<class T>
class Singleton
{
public:
    static T &instance()
    {
        Q_ASSERT(mInstance);
        return *mInstance;
    }

    static T *instancePtr()
    {
        Q_ASSERT(mInstance);
        return mInstance;
    }

    static void deleteInstance()
    {
        delete mInstance;
    }

    Singleton()
    {
        Q_ASSERT(!mInstance);
        mInstance = static_cast< T* >( this );
    }

private:
    Singleton(const Singleton<T> &other);
    Singleton<T> &operator =(const Singleton<T> &other);

protected:
    static T *mInstance;
};

#define SINGLETON_IMPL(T) template<> T *Singleton<T>::mInstance = 0;

#endif // SINGLETON_H
