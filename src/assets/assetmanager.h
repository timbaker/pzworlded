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

#ifndef ASSETMANAGER_H
#define ASSETMANAGER_H

#include "asset.h"
#include "asset_util.h"

#include <QMap>

class Asset;
class AssetTask;
class AssetTable : public QMap<QString, Asset*> {};

class TILEDSHARED_EXPORT AssetManager : public QObject, non_copyable
{
    Q_OBJECT
public:
    AssetManager();

public:
    void destroy();
    void removeUnreferenced();

    Asset* load(AssetPath path)
    {
        return load(path, nullptr);
    }

    Asset* load(AssetPath path, AssetParams* params);
    void load(Asset* asset);

    void unload(AssetPath path);
    void unload(Asset* asset);

    void reload(AssetPath path);
    void reload(Asset* asset);

    void enableUnload(bool enable);

protected:
    Asset* get(AssetPath path);

    virtual void startLoading(Asset* asset) = 0;
    void onLoadingSucceeded(Asset* asset);
    void onLoadingFailed(Asset* asset);

    void setTask(Asset* asset, AssetTask* task);

    virtual void unloadData(Asset *asset);

    virtual Asset* createAsset(AssetPath path, AssetParams* params) = 0;
    virtual void destroyAsset(Asset* asset) = 0;

private:
    void doLoad(Asset* asset, AssetParams* params);
    void doUnload(Asset* asset);

public slots:
    virtual void onStateChanged(AssetState old_state, AssetState new_state, Asset* asset);

protected:
    AssetTable m_assets;

private:
    bool m_is_unload_enabled = false;
};

#endif // ASSETMANAGER_H
