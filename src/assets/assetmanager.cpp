#include "assetmanager.h"
#include "asset_p.h"

AssetManager::AssetManager()
{

}

void AssetManager::destroy()
{
    for (Asset* asset : m_assets)
    {
        if (!asset->isEmpty())
        {
            qDebug() << "destroying non-EMPTY asset";
        }
        destroyAsset(asset);
    }

    m_assets.clear();
}

void AssetManager::removeUnreferenced()
{
    if (!m_is_unload_enabled)
    {
        return;
    }

    for (Asset* asset : m_assets)
    {
        if (asset->getRefCount() == 0)
        {
            m_assets.remove(asset->getPath());
            destroyAsset(asset);
        }
    }
}

Asset *AssetManager::load(AssetPath path, AssetParams *params)
{
    if (!path.isValid())
    {
        return nullptr;
    }

    Asset* asset = get(path);

    if (asset == nullptr)
    {
        asset = createAsset(path, params);
        m_assets.insert(path.getString(), asset);
    }

    if (asset->isEmpty() && (asset->m_priv->m_desired_state == AssetState::EMPTY))
    {
        doLoad(asset, params);
    }

    asset->addRef();

    return asset;
}

void AssetManager::load(Asset *asset)
{
    if (asset->isEmpty() && (asset->m_priv->m_desired_state == AssetState::EMPTY))
    {
        doLoad(asset, nullptr);
    }

    asset->addRef();
}

void AssetManager::unload(AssetPath path)
{
    Asset* asset = get(path);
    if (asset != nullptr)
    {
        unload(asset);
    }
}

void AssetManager::unload(Asset *asset)
{
    int newRefCount = asset->rmRef();
    Q_ASSERT(newRefCount >= 0);
    if (newRefCount == 0 && m_is_unload_enabled)
    {
        doUnload(asset);
    }
}

void AssetManager::reload(AssetPath path)
{
    Asset* asset = get(path);
    if (asset != nullptr)
    {
        reload(asset);
    }
}

void AssetManager::reload(Asset *asset)
{
    doUnload(asset);
    doLoad(asset, nullptr);
}

void AssetManager::enableUnload(bool enable)
{
    m_is_unload_enabled = enable;
    if (!enable)
    {
        return;
    }
    for (Asset* asset : m_assets)
    {
        if (asset->getRefCount() == 0)
        {
            doUnload(asset);
        }
    }
}

Asset *AssetManager::get(AssetPath path)
{
    return m_assets[path];
}

void AssetManager::onLoadingSucceeded(Asset *asset)
{
    asset->m_priv->onLoadingSucceeded();
}

void AssetManager::onLoadingFailed(Asset *asset)
{
    asset->m_priv->onLoadingFailed();
}

void AssetManager::setTask(Asset *asset, AssetTask *task)
{
    if (asset->m_priv->m_task != nullptr)
    {
        if (task == nullptr)
        {
            asset->m_priv->m_task = nullptr;
        }
        return;
    }
    asset->m_priv->m_task = task;

}

void AssetManager::unloadData(Asset* asset)
{
    Q_UNUSED(asset);
}

void AssetManager::doLoad(Asset *asset, AssetParams *params)
{
    if (asset->m_priv->m_desired_state == AssetState::READY)
    {
        return;
    }

    asset->m_priv->m_desired_state = AssetState::READY;

    // The code this is based on (LumixEngine) didn't pass AssetParams around.
    // I added this to support reloading an asset.
    asset->setAssetParams(params);

    startLoading(asset);
}

void AssetManager::doUnload(Asset *asset)
{
    if (asset->m_priv->m_task != nullptr)
    {
        asset->m_priv->m_task->cancel();
        asset->m_priv->m_task = nullptr;
    }

    asset->m_priv->m_desired_state = AssetState::EMPTY;
    unloadData(asset);
    Q_ASSERT(asset->m_priv->m_empty_dep_count <= 1);

    asset->m_priv->m_empty_dep_count = 1;
    asset->m_priv->m_failed_dep_count = 0;
    asset->m_priv->checkState();
}

void AssetManager::onStateChanged(AssetState old_state, AssetState new_state, Asset *asset)
{

}
