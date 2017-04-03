#ifndef _AI_
#define _AI_

#include "mythread.h"
#include <QtCore/QMutex>
#include <QtCore/QList>
#include <QtCore/QVector>
#include <QtCore/QBuffer>
#include <QtCore/QQueue>
#include <QtGui/QImage>

class Entity
{
  public:
    
    int top, left, bottom, right;
    int pointcount;
    
};

class BrainThread : public MyThread
{
  public:

    BrainThread();

    virtual void run();

    void doBrain();
    void findSpot(int);
    void sendSpot(int);

    void calcOffset(int leftx, int rightx, double & xoffset, double & yoffset,
                    double & langle, double & rangle);
    
  protected:

    void mergeRegions(int val1, int val2, int w, int h, int l);
    int diff(int x,int y);
    int * getData(int x,int y,int l);

    QQueue<double> xes;
    QQueue<double> yes;
    QQueue<double> widths;
    
    Entity entities[10000];
    int nextent;    
    int biggest;
    int * data_buffer;
    int linestep;
    QString text;
    
};

extern BrainThread * brain_thread;

#endif
