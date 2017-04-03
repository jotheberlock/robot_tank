#include "packet.h"

#include "mythread.h"

#ifdef WIN32
#include <winsock.h>
#else
#include <netinet/in.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#endif

#include <stdio.h>
#include <time.h>
#include <errno.h>

void PacketHello::read(QDataStream & ds)
{
    ds >> role;
}

void PacketHello::pack(QDataStream & ds)
{
    ds << role;
}

void PacketSystemConfig::read(QDataStream & ds)
{
    ds >> num_motors;
}

void PacketSystemConfig::pack(QDataStream & ds)
{
    ds << num_motors;
}

void PacketShutdown::read(QDataStream &)
{
}

void PacketShutdown::pack(QDataStream &)
{
}

void PacketMotorSet::read(QDataStream & ds)
{
    ds >> num_motor;
    ds >> val;
}

void PacketMotorSet::pack(QDataStream & ds)
{
    ds << num_motor;
    ds << val;
}

void PacketMotorStatus::read(QDataStream & ds)
{
    ds >> num_motor;
    ds >> val;
}

void PacketMotorStatus::pack(QDataStream & ds)
{
    ds << num_motor;
    ds << val;
}

void PacketVideoFrame::read(QDataStream & ds)
{
    ds >> camera;
    ds >> length;

    char * data = new char[length];
    ds.readRawData(data, length);
    frame = QByteArray(data,length);
    delete[] data;
}

void PacketVideoFrame::pack(QDataStream & ds)
{
    ds << camera;
    ds << frame.size();
    ds.writeRawData(frame.constData(), frame.size());
}

void PacketOverlayFrame::read(QDataStream & ds)
{
    ds >> camera;
    ds >> length;

    char * data = new char[length];
    ds.readRawData(data, length);
    frame = QByteArray(data,length);
    delete[] data;
}

void PacketOverlayFrame::pack(QDataStream & ds)
{
    ds << camera;
    ds << frame.size();
    ds.writeRawData(frame.constData(), frame.size());
}

void PacketConsole::read(QDataStream & ds)
{
    ds >> text;
}

void PacketConsole::pack(QDataStream & ds)
{
    ds << text;
}

void PacketFocus::read(QDataStream & ds)
{
    ds >> left;
    ds >> top;
    ds >> right;
    ds >> bottom;
    ds >> camera;
    ds >> text;
}

void PacketFocus::pack(QDataStream & ds)
{
    ds << left;
    ds << top;
    ds << right;
    ds << bottom;
    ds << camera;
    ds << text;
}

void PacketObjectSelect::read(QDataStream & ds)
{
    ds >> x;
    ds >> y;
    ds >> r;
    ds >> g;
    ds >> b;
    ds >> camera;
}

void PacketObjectSelect::pack(QDataStream & ds)
{
    ds << x;
    ds << y;
    ds << r;
    ds << g;
    ds << b;
    ds << camera;
}

void PacketTargetLocation::read(QDataStream & ds)
{
    ds >> xoff;
    ds >> yoff;
    ds >> width;
    ds >> r;
    ds >> g;
    ds >> b;
    ds >> leftangle1;
    ds >> rightangle1;
    ds >> leftangle2;
    ds >> rightangle2;
    ds >> present;
    ds >> text;
}

void PacketTargetLocation::pack(QDataStream & ds)
{
    ds << xoff;
    ds << yoff;
    ds << width;
    ds << r;
    ds << g;
    ds << b;
    ds << leftangle1;
    ds << rightangle1;
    ds << leftangle2;
    ds << rightangle2;
    ds << present;
    ds << text;
}

void PacketPanTilt::read(QDataStream & ds)
{
    ds >> pan;
    ds >> tilt;
}

void PacketPanTilt::pack(QDataStream & ds)
{
    ds << pan;
    ds << tilt;
}

Packet * read_packet(int fd)
{
    Packet * ret = 0;
    
    unsigned int idb;
    unsigned int lenb;

    int retval;
        
    retval = ::recv(fd, (char *)&idb, 4, 0);
    if (retval < 4)
        return 0;
    
    retval=  ::recv(fd, (char *)&lenb, 4, 0);
    if (retval < 4)
        return 0;
    
    unsigned int id = ntohl(idb);
    unsigned int len = ntohl(lenb);
    
        //printf(">> Read %d %d  ", id, len);
    
    char * buf = new char[len];
    int to_read = len;
    int ptr = 0;
    while (to_read > 0)
    {
        int ret = ::recv(fd, buf+ptr, len-ptr, 0);
            //printf("[%d] ", ret);
        if (ret < 1)
        {
            printf("Read failure\n");
            return 0;
        }
        else
        {
            ptr += ret;
            to_read -= ret;
        }
    }

        //printf("\n");
    
    QByteArray data(buf, len);
    QDataStream qds(&data, QIODevice::ReadWrite);
    
    switch(id)
    {
        case PACKET_HELLO:
        {
            ret = new PacketHello;
            ret->read(qds);
            break;
        }
        case PACKET_SYSTEM_CONFIG:
        {
            ret = new PacketSystemConfig;
            ret->read(qds);
            break;
        }
        case PACKET_SHUTDOWN:
        {
            ret = new PacketShutdown;
            ret->read(qds);
            break;
        }
        case PACKET_MOTOR_SET:
        {
            ret = new PacketMotorSet;
            ret->read(qds);
            break;
        }
        case PACKET_MOTOR_STATUS:
        {
            ret = new PacketMotorStatus;
            ret->read(qds);
            break;
        }
        case PACKET_VIDEO_FRAME:
        {
            ret = new PacketVideoFrame;
            ret->read(qds);
            break;
        }
        case PACKET_CONSOLE:
        {
            ret = new PacketConsole;
            ret->read(qds);
            break;
        }
        case PACKET_FOCUS:
        {
            ret = new PacketFocus;
            ret->read(qds);
            break;
        }
        case PACKET_OBJECT_SELECT:
        {
            ret = new PacketObjectSelect;
            ret->read(qds);
            break;
        } 
        case PACKET_OVERLAY_FRAME:
        {
            ret = new PacketOverlayFrame;
            ret->read(qds);
            break;
        }
        case PACKET_DISABLE_OVERLAY:
        {
            ret = new PacketDisableOverlay;
            ret->read(qds);
            break;
        } 
        case PACKET_TARGET_LOCATION:
        {
            ret = new PacketTargetLocation;
            ret->read(qds);
            break;
        } 
        case PACKET_PAN_TILT:
        {
            ret = new PacketPanTilt;
            ret->read(qds);
            break;
        } 
        default:
        {
            printf("Unknown packet %d!\n", id);
        }
    }

    delete[] buf;
    return ret;
}

void send_packet(int fd, Packet * p, bool reliable)
{
    set_thread_status("Sending packet");
    QByteArray data;
    QDataStream qds(&data, QIODevice::ReadWrite);
    
    set_thread_status("Packing packet");
    p->pack(qds);
       
    unsigned int idb = htonl(p->type());
    unsigned int lenb = htonl(data.size());

    char * buf = new char[8 + data.size()];
    unsigned int * hp = (unsigned int *)buf;
    *hp = idb;
    *(hp+1) = lenb;
    memcpy(buf+8,data.constData(),data.size());
    
    set_thread_status("Entering select()");
    
        // Wait for fd to become free
    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(fd, &writefds);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500;
    select(fd+1, NULL, &writefds, NULL, &tv);

    if (!FD_ISSET(fd, &writefds))
    {
        printf("Timeout on select()\n");
        set_thread_status("Select() timed out");

        if (!reliable)
        {
            delete[] buf;
            return;
        }
    }
    
    set_thread_status("Sending packet");

    int send_size = 8+data.size();
    char * ptr = buf;

    while (send_size)
    {
        int ret1 = ::send(fd, ptr, 8+data.size(),MSG_NOSIGNAL /*| MSG_DONTWAIT*/);
        if (ret1 == -1)
        {
            printf("Send() error %s\n", strerror(errno));
            set_thread_status("Failed to send packet");
            delete[] buf;
            return;
        }
        else
        {
            send_size -= ret1;
            ptr += ret1;
        }
    }

    delete[] buf;
    
    set_thread_status("Sent packet");
}
