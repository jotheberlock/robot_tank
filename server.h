#ifndef _SERVER_
#define _SERVER_

#include "mythread.h"
#include <QtCore/QWaitCondition>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QList>
#include <QtCore/QVector>
#include <QtCore/QBuffer>
#include <QtGui/QImage>
#include <QtCore/QTime>
#include <QtCore/QStack>

#include "packet.h"

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "motors.h"
#include "ai.h"
#include "camera.h"

#define CAM_WIDTH 320
#define CAM_HEIGHT 240
#define NUM_CAMERAS 2

#define EYE_DISTANCE 60.0
#define EYE_FOV 42.0

class ServerThread : public MyThread
{
public:

    ServerThread(int f)
    {
        fd = f;
        role = -1;
    }

    virtual void run();

    void sendRawPacket(char * data, int len)
        {
            if (::write(fd, data, len) < 0)
            {
                printf("Failed to send raw packet\n");
            }
        }
    
    virtual void sendPacket(Packet * b, bool reliable = true);
    bool getRole()
        {
            return role;
        }
        
  protected:

    int fd;
    QMutex lock;
    int role;
    
};

class FrameSendThread : public MyThread
{
  public:

    virtual void run();
};

class Server : public MyThread
{
public:

    Server();

    void init();
    
    virtual void run();
    void shutdown();

    void broadcast(Packet *,int,bool reliable = true);
    void broadcastRaw(char *,int,int);
    
    bool running() 
    {
        return is_running;
    }

    void setRunning(bool b)
    {
        is_running = b;
    }

    int numMotors()
    {
        return motors.size();
    }
    
    void sendStatuses();
    void setMotor(int, int);
    int getMotor(int);

    QString process_console(QString);

    QImage image[NUM_CAMERAS];
    
    int red, green, blue;
    int suggested_x[NUM_CAMERAS], suggested_y[NUM_CAMERAS];
    int needed_score;
    int objx[NUM_CAMERAS];
    int objy[NUM_CAMERAS];
    int objl[NUM_CAMERAS];
    int objr[NUM_CAMERAS];
    
    QMutex ai_mutex;
    
    int interval;
    int last_interval;
    
    int grab_time;
    int convert_time;
    int brain_time;
    int send_time;
    float vol_match;  // 0.5 == 50% of volume must match
    float screen_match;  // 0.5 == must fill 50% of screen
    int debounce_queue_size;
    int ai_speed;

    int yuy2_cache_dirty;
    
    bool ai_active;
    bool motors_allowed;
    bool fill_spot;
    bool apply_blur;

    bool teststereo;
    int tx1;
    int tx2;
    int tx3;
    int tx4;
    
    float brightness, contrast, hue, saturation;

    QMutex frame_send_mutex;
    QStack<QImage> frames[NUM_CAMERAS];
    QStack<QImage> oframes[NUM_CAMERAS];
    QWaitCondition frame_send_condition;
    
  protected:
    
    bool is_running;
    int ssock;
    
    QList<ServerThread *> threads;
    QVector<Motor *> motors;
    QVector<Camera *> cameras;
    
    QMutex lock;

    
};

extern Server * server;

#endif

