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
