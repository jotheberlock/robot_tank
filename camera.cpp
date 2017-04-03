#include "camera.h"
#include "server.h"

#include <math.h>

inline qreal qSin(qreal v)
{
#ifdef QT_USE_MATH_H_FLOATS
    if (sizeof(qreal) == sizeof(float))
        return sinf(v);
    else
#endif
        return sin(v);
}

inline qreal qCos(qreal v)
{
#ifdef QT_USE_MATH_H_FLOATS
    if (sizeof(qreal) == sizeof(float))
        return cosf(v);
    else
#endif
        return cos(v);
}

#define CLIP_SHIFT_RIGHT_8(c) ((c) < 0 ? 0 : (c) > 0xffff ? 0xff : (c) >> 8)
#define CLIP_SHIFT_LEFT_8(c) ((c) < 0 ? 0 : (c) > 0xffff ? 0xff0000 : ( ((c) << 8) & 0xff0000) )
#define CLIP_NO_SHIFT(c) ((c) < 0 ? 0 : (c) > 0xffff ? 0xff00 : ((c) & 0xff00) )
#define CLIPPED_PIXEL(base, r, g, b) (0xff000000u | CLIP_SHIFT_LEFT_8(base+r) | CLIP_NO_SHIFT(base+g) | CLIP_SHIFT_RIGHT_8(base+b))

Camera::Camera(const char * d, int c)
{
    dev = d;
    camnum = c;
    fd = -1;
    inited = false;
    width = 0;
    height = 0;
    linestep = 0;
    sizeimage = 0;
    buffers = 0;
}

Camera::~Camera()
{
    shutdown();
}

void Camera::enumerateFormats()
{
    struct v4l2_fmtdesc desc;

    int count = 0;
    
    while (true)
    {
        desc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        desc.index = count;
        count++;
        int ret = ioctl(fd, VIDIOC_ENUM_FMT, &desc);

        if (ret == -1)
        {
            return;
        }
        
        printf("Camera %d format %d [%s] fourcc hex %x\n",
               camnum, desc.index, desc.description, desc.pixelformat);
    }
}

void Camera::init()
{
    inited = false;

    fd = open(dev, O_RDWR, 0);
    if (fd == -1)
    {
        return;
    }

    struct v4l2_capability caps;
    if (ioctl(fd, VIDIOC_QUERYCAP, &caps) == 0)
    {
        v4l2_format format;
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        ioctl(fd, VIDIOC_G_FMT, &format);

        format.fmt.pix.width = CAM_WIDTH;
        format.fmt.pix.height = CAM_HEIGHT;
                    
        ioctl(fd, VIDIOC_S_FMT, &format);
            
        ioctl(fd, VIDIOC_G_FMT, &format);
                    
        width = format.fmt.pix.width;
        height = format.fmt.pix.height;
        linestep = format.fmt.pix.bytesperline;
                    
        printf("Driver %s device %s\n", caps.driver, caps.card);
        printf("Width %d height %d pixel format %c%c%c%c linestep %d\n",
               width, height,
               format.fmt.pix.pixelformat & 0xff,
               (format.fmt.pix.pixelformat >> 8) & 0xff,
               (format.fmt.pix.pixelformat >> 16) & 0xff,
               (format.fmt.pix.pixelformat >> 24) & 0xff,
               linestep);
    }
    else
    {
        printf("Can't query\n");
        return;
    }

    memset(&reqbuf, 0, sizeof(reqbuf));
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    reqbuf.count = 20;

    if (-1 == ioctl (fd, VIDIOC_REQBUFS, &reqbuf)) {
        printf("Failure %s requesting buffers\n", strerror(errno));
        return;
    }

    printf("Got %d buffers\n", reqbuf.count);
    nbuffers = reqbuf.count;
    
    buffers = (CameraBuffer *)calloc (reqbuf.count, sizeof (*buffers));

    for (unsigned int i = 0; i < reqbuf.count; i++) {
        struct v4l2_buffer buffer;

        memset (&buffer, 0, sizeof (buffer));
        buffer.type = reqbuf.type;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;

        if (-1 == ioctl (fd, VIDIOC_QUERYBUF, &buffer)) {
            printf("Querybuf failure %s\n", strerror(errno));
            return;
        }

        buffers[i].length = buffer.length; /* remember for munmap() */

        buffers[i].start = mmap (NULL, buffer.length,
                                 PROT_READ | PROT_WRITE, /* recommended */
                                 MAP_SHARED,             /* recommended */
                                 fd, buffer.m.offset);

        if (MAP_FAILED == buffers[i].start) {
                /* If you do not exit here you should unmap() and free()
                   the buffers mapped so far. */
            printf("Mmap failure %s\n", strerror(errno));
            return;
        } else {
            printf("Buffer %d %p length %ld\n", i, buffers[i].start, buffers[i].length);
        }
    }
    
    inited = true;
}

void Camera::startCapture()
{
    for (int i = 0; i < nbuffers; ++i) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;
        
        if (-1 == ioctl (fd, VIDIOC_QBUF, &buf))
        {
            printf("Failure to queue buffer %s\n", strerror(errno));
            return;
        }
    }
    
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) == -1)
    {
        printf("Failure to turn on stream %s\n", strerror(errno));
        return;
    }
}

void Camera::stopCapture()
{
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(fd, VIDIOC_STREAMOFF, &type);
}

void Camera::shutdown()
{
    inited = false;
    
    for (unsigned int i = 0; i < reqbuf.count; i++)
        munmap (buffers[i].start, buffers[i].length);
}

void Camera::grabFrame()
{
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    
    if (-1 ==  ioctl (fd, VIDIOC_DQBUF, &buf)) {
        switch (errno) {
            case EAGAIN:
                return;
                
            case EIO:
            {
                printf("EIO on grabFrame\n");
                break;
            };
            
                    /* Could ignore EIO, see spec. */
                    /* fall through */
                
            default:
            {
                printf("Failure to grab frame %s\n", strerror(errno));
            }
        }
    }

    if (buf.flags | V4L2_BUF_FLAG_DONE)
    {    
        convertYUY2toRGB((const uchar *)(buffers[buf.index].start),
                         QSize(width, height),
                         server->image[camnum], server->brightness,
                         server->contrast, server->hue, server->saturation);
    }
    
    if (-1 == ioctl(fd, VIDIOC_QBUF, &buf))
    {
        printf("Failure to enqueue buffer %s\n", strerror(errno));
    }
}

void Camera::run()
{
    QString name = "Camera ";
    name += QString::number(camnum);
    registerThread(name);
    
    cache_generation = 0;
    
    init();
    enumerateFormats();
    startCapture();

    while (true)
    {
        if (!isInited())
        {
            stopCapture();
            init();
            startCapture();
        }

        if (!isInited())
        {
            usleep(100000);
            continue;
        }
        
        QTime start_time = QTime::currentTime();
        setStatus("Grabbing frame");
        grabFrame();
        setStatus("Frame grabbed");
        QTime grab_timer = QTime::currentTime();

        if (!server->fill_spot)
        {
            {
                setStatus("Locking frame send mutex");
                QMutexLocker qml(&server->frame_send_mutex);
                setStatus("Frame send mutex locked");
                server->frames[camnum].push(server->image[camnum]);
            }
            setStatus("Waking frame thread");
            server->frame_send_condition.wakeAll();
            setStatus("Frame thread woken");
        }

            /*
        if (!server->fill_spot)
        {
            QImage i = server->image[camnum];
            PacketVideoFrame pvf;
            pvf.camera = camnum;
            QBuffer buffer(&pvf.frame);
            buffer.open(QIODevice::WriteOnly);
            i.save(&buffer, "JPG");
            pvf.length = pvf.frame.size();
            server->broadcast(&pvf, ROLE_VIDEO);
        }
            */

        {
            setStatus("Finding spot");
            brain_thread->findSpot(camnum);
            setStatus("Sending spot");
            brain_thread->sendSpot(camnum);
            setStatus("Spot found and sent");
        }
        
        QTime end_time = QTime::currentTime();
        
        server->last_interval = start_time.msecsTo(end_time);
        server->grab_time = start_time.msecsTo(grab_timer);
        
        if (server->last_interval < server->interval)
        {
            usleep ( (server->interval - server->last_interval) * 1000);
        }        
    }
}

void Camera::convertYUY2toRGB(const uchar *data, const QSize &s, QImage &destImage, qreal brightness, qreal contrast, qreal hue, qreal saturation)
{
    if (cache_generation < server->yuy2_cache_dirty)
    {
        buildYuy2Cache();
        cache_generation = server->yuy2_cache_dirty;
    }

    const int w = s.width();
    const int h = s.height();

        //let's cache some computation
    if (destImage.size() != s) {
            //this will only allocate memory when needed
        destImage = QImage(w, h, QImage::Format_ARGB32_Premultiplied);

    }

    if (destImage.isNull()) {
        printf("Awoogah, can't allocate %d by %d image\n",
               w, h);
        return; //the system can't allocate the memory for the image drawing
    }

    QRgb *dest = reinterpret_cast<QRgb*>(destImage.bits());

        //the number of iterations is width * height / 2 because we treat 2 pixels at each iterations

    for (int c = w * h / 2; c > 0 ; --c) {

        const int y1 = Yvalue[*data++],	

            u = *data++ - 128,

            y2 = Yvalue[*data++],

            v = *data++ - 128;

        const int d = (u * cosHx256 + v * sinHx256) >> 8,

            e = (v * cosHx256 + u * sinHx256) >> 8;

        const int compRed = 409 * e,

            compGreen = -100 * d - 208 * e,

            compBlue = 516 * d;
            //first pixel

        *dest++ = CLIPPED_PIXEL(y1, compRed, compGreen, compBlue);

            //second pixel

        *dest++ = CLIPPED_PIXEL(y2, compRed, compGreen, compBlue);

    }

}

void Camera::buildYuy2Cache()
{    
    for(int i = 0;i<256;++i) {

        Yvalue[i] = qRound(((i - 16) * server->contrast +
                            server->brightness) * 298 + 128);
    }

    cosHx256 = qRound(qCos(server->hue) * server->contrast *
                      server->saturation * 256);
    sinHx256 = qRound(qSin(server->hue) * server->contrast *
                      server->saturation * 256);
}
