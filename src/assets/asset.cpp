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

#include "asset.h"
#include "asset_p.h"

Asset::Asset(AssetPath path, AssetManager *manager)
    : m_manager(manager)
    , m_path(path)
    , m_priv(new AssetPriv(this))
{
}

AssetState Asset::getState()
{
    return m_priv->m_current_state;
}

bool Asset::isEmpty() const
{
    return m_priv->m_current_state == AssetState::EMPTY;
}

bool Asset::isReady() const
{
    return m_priv->m_current_state == AssetState::READY;
}

bool Asset::isFailure() const
{
    return m_priv->m_current_state == AssetState::FAILURE;
}

void Asset::onCreated(AssetState state)
{
    m_priv->onCreated(state);
}

void Asset::addDependency(Asset* dependent_asset)
{
    m_priv->addDependency(dependent_asset);
}

void Asset::removeDependency(Asset* dependent_asset)
{
    m_priv->removeDependency(dependent_asset);
}

void Asset::setAssetParams(AssetParams *params)
{
    Q_UNUSED(params);
}
