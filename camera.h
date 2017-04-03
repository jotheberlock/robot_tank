#ifndef _CAMERA_
#define _CAMERA_

#include "mythread.h"
#include <linux/videodev2.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>

#include <QtGui/QImage>

struct CameraBuffer
{
    void * start;
    size_t length;
};

class Camera : public MyThread
{
  public:

    Camera(const char *,int);
    ~Camera();
    
    virtual void init();
    virtual void shutdown();
    virtual void grabFrame();
    virtual void startCapture();
    virtual void stopCapture();

    void enumerateFormats();

    bool isInited() 
    {
        return inited;
    }

    virtual void run();
    
  protected:

    const char * dev;
    int fd;
    int camnum;
    bool inited;
    int nbuffers;
    
    int width;
    int height;
    int linestep;
    int sizeimage;
    char * compress_buffer;
    int compress_size;
    
    CameraBuffer * buffers;
    struct v4l2_requestbuffers reqbuf;
    
    void convertYUY2toRGB(const uchar *data, const QSize &s, QImage &destImage, qreal brightness, qreal contrast, qreal hue, qreal saturation);
    void buildYuy2Cache();    

    int cache_generation;
    
    int Yvalue[256];
    int cosHx256;
    int sinHx256;
    
};


#endif
