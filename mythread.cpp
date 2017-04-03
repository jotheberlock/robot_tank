#include "mythread.h"

void MyThread::registerThread(QString n)
{
    threadname = n;
    threadlist_mutex.lock();
    threadlist.push_back(this);
    threadlist_mutex.unlock();
}

MyThread::~MyThread()
{
    threadlist_mutex.lock();
    threadlist.removeAll(this);
    threadlist_mutex.unlock();
}

void set_thread_status(QString s)
{
    QThread * t = QThread::currentThread();
    if (!t)
        return;
    
    threadlist_mutex.lock();
    for(int loopc=0; loopc<threadlist.size(); loopc++)
    {
        MyThread * my = threadlist.at(loopc);
        if ((QThread *)my == t)
        {
            my->setStatus(s);
        }
    }
    threadlist_mutex.unlock();
}


QMutex threadlist_mutex;
QList<MyThread *> threadlist;
