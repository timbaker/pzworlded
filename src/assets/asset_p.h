#ifndef ASSET_P_H
#define ASSET_P_H

#include "assetmanager.h"
#include "assettask.h"

class AssetTask;

class TILEDSHARED_EXPORT AssetPriv : public QObject
{
    Q_OBJECT
public:
    Asset* m_asset;
    AssetState m_current_state = AssetState::EMPTY;
    AssetState m_desired_state = AssetState::EMPTY;
    int m_empty_dep_count = 1;
    int m_failed_dep_count = 0;
    AssetTask* m_task = nullptr;

    AssetPriv(Asset* asset)
        : m_asset(asset)
    {

    }

    void onCreated(AssetState state)
    {
        Q_ASSERT(m_empty_dep_count == 1);
        Q_ASSERT(m_failed_dep_count == 0);

        m_current_state = state;
        m_desired_state = AssetState::READY;
        m_failed_dep_count = (state == AssetState::FAILURE) ? 1 : 0;
        m_empty_dep_count = 0;
    }

    void addDependency(Asset* dependent_asset)
    {
        Q_ASSERT(m_desired_state != AssetState::EMPTY);
        connect(dependent_asset, &Asset::stateChanged, this, &AssetPriv::onStateChanged);
        if (dependent_asset->isEmpty())
        {
            ++m_empty_dep_count;
        }
        if (dependent_asset->isFailure())
        {
            ++m_failed_dep_count;
        }

        checkState();
    }

    void removeDependency(Asset* dependent_asset)
    {
        dependent_asset->disconnect(this);
        if (dependent_asset->isEmpty())
        {
            Q_ASSERT(m_empty_dep_count > 0);
            --m_empty_dep_count;
        }
        if (dependent_asset->isFailure())
        {
            Q_ASSERT(m_failed_dep_count > 0);
            --m_failed_dep_count;
        }
        checkState();
    }

    void onStateChanged(AssetState old_state, AssetState new_state, Asset* asset)
    {
        Q_UNUSED(asset);
        Q_ASSERT(old_state != new_state);
        Q_ASSERT(m_current_state != AssetState::EMPTY || m_desired_state != AssetState::EMPTY);

        if (old_state == AssetState::EMPTY)
        {
            Q_ASSERT(m_empty_dep_count > 0);
            --m_empty_dep_count;
        }
        if (old_state == AssetState::FAILURE)
        {
            Q_ASSERT (m_failed_dep_count > 0);
            --m_failed_dep_count;
        }

        if (new_state == AssetState::EMPTY)
        {
            ++m_empty_dep_count;
        }
        if (new_state == AssetState::FAILURE)
        {
            ++m_failed_dep_count;
        }

        checkState();
    }

    void onLoadingSucceeded()
    {
        Q_ASSERT(m_current_state != AssetState::READY);
        Q_ASSERT(m_empty_dep_count == 1);
        --m_empty_dep_count;
//        delete m_task;
        m_task = nullptr;
        checkState();
    }

    void onLoadingFailed()
    {
        Q_ASSERT(m_current_state != AssetState::READY);
        Q_ASSERT(m_empty_dep_count == 1);
        ++m_failed_dep_count;
        --m_empty_dep_count;
//        delete m_task;
        m_task = nullptr;
        checkState();
    }

    void checkState()
    {
        AssetState oldState = m_current_state;
        if (m_failed_dep_count > 0 && m_current_state != AssetState::FAILURE)
        {
            m_current_state = AssetState::FAILURE;
            m_asset->getManager()->onStateChanged(oldState, m_current_state, m_asset);
            emit m_asset->stateChanged(oldState, m_current_state, m_asset);
        }

        if (m_failed_dep_count == 0)
        {
            if (m_empty_dep_count == 0 && m_current_state != AssetState::READY && m_desired_state != AssetState::EMPTY)
            {
                m_asset->onBeforeReady();
                m_current_state = AssetState::READY;
                m_asset->getManager()->onStateChanged(oldState, m_current_state, m_asset);
                emit m_asset->stateChanged(oldState, m_current_state, m_asset);
            }

            if (m_empty_dep_count > 0 && m_current_state != AssetState::EMPTY)
            {
                m_asset->onBeforeEmpty();
                m_current_state = AssetState::EMPTY;
                m_asset->getManager()->onStateChanged(oldState, m_current_state, m_asset);
                emit m_asset->stateChanged(oldState, m_current_state, m_asset);
            }
        }
    }
};

#endif // ASSET_P_H
