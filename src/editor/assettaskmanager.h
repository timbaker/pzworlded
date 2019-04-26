#ifndef ASSETTASKMANAGER_H
#define ASSETTASKMANAGER_H

#include "singleton.h"

#include <QList>
#include <QTimer>

class AssetTask;

class AssetTaskManager : public QObject, public Singleton<AssetTaskManager>
{
    Q_OBJECT

public:
    AssetTaskManager();

    void submit(AssetTask* task);

public slots:
    void updateAsyncTransactions();

private:
    QTimer m_timer;
    QList<AssetTask*> m_pending;
    QList<AssetTask*> m_in_progress;
};

#endif // ASSETTASKMANAGER_H
