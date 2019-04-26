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
