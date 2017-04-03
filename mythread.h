#ifndef _MYTHREAD_
#define _MYTHREAD_

#include <QtCore/QThread>
#include <QtCore/QMutex>
#include <QtCore/QTime>
#include <QtCore/QString>

class MyThread : public QThread
{
  public:

    virtual ~MyThread();
    virtual void registerThread(QString);

    QString threadName()
    {
        return threadname;
    }
    
    void setStatus(QString s)
    {
        threadstatus = s;
        last_update = QTime::currentTime();
    }

    QString status()
    {
        return threadstatus;
    }

    QTime & lastUpdate()
    {
        return last_update;
    }
    
  protected:

    QString threadname;
    QString threadstatus;
    QTime last_update;
    
};

void set_thread_status(QString s);

extern QMutex threadlist_mutex;
extern QList<MyThread *> threadlist;

#endif
