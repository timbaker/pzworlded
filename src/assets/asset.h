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

#ifndef ASSET_H
#define ASSET_H

#include "asset_util.h"

#include <QObject>

class AssetManager;

enum struct AssetState
{
    EMPTY,
    READY,
    FAILURE
};

class TILEDSHARED_EXPORT Asset : public QObject, non_copyable
{
    Q_OBJECT
public:
    Asset(AssetPath path, AssetManager* manager);
    virtual ~Asset() {}

    AssetState getState();

    bool isEmpty() const;
    bool isReady() const;
    bool isFailure() const;

    AssetManager* getManager()
    {
        return m_manager;
    }

    void onCreated(AssetState state);

    int getRefCount() const
    {
        return m_ref_count;
    }

    AssetPath getPath() const
    {
        return m_path;
    }

    void addDependency(Asset* dependent_asset);
    void removeDependency(Asset* dependent_asset);

    // Hack
    void setManager(AssetManager* manager) { m_manager = manager; }

    virtual void setAssetParams(AssetParams* params);

signals:
    void stateChanged(AssetState old_state, AssetState new_state, Asset* asset);

protected:
    virtual void onBeforeReady() {}
    virtual void onBeforeEmpty() {}

private:
    friend class AssetManager;

    int addRef()
    {
        return ++m_ref_count;
    }

    int rmRef()
    {
        return --m_ref_count;
    }

protected:
    AssetManager* m_manager;
    AssetPath m_path;

private:
    int m_ref_count = 0;

private:
    friend class AssetPriv;
    AssetPriv* m_priv;
};

#endif // ASSET_H
