#ifndef IDLETASKS_H
#define IDLETASKS_H

#include "singleton.h"

#include <QTimer>

class IdleTasks : public QObject, public Singleton<IdleTasks>
{
    Q_OBJECT

public:
    IdleTasks();

    void blockTasks(bool block);

signals:
    void idleTime();

private slots:
    void onTimer();

private:
    QTimer m_timer;
    int m_block_depth = 0;
    bool m_in_idle_time = false;
};

#endif // IDLETASKS_H
