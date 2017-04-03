#ifndef _PACKET_
#define _PACKET_

#include <QtCore/QByteArray>
#include <QtCore/QDataStream>

#define PACKET_HELLO 0
#define PACKET_SYSTEM_CONFIG 1
#define PACKET_SHUTDOWN 2
#define PACKET_MOTOR_SET 3
#define PACKET_MOTOR_STATUS 4
#define PACKET_VIDEO_FRAME 5
#define PACKET_CONSOLE 6
#define PACKET_FOCUS 7
#define PACKET_OBJECT_SELECT 8
#define PACKET_OVERLAY_FRAME 9
#define PACKET_DISABLE_OVERLAY 10
#define PACKET_TARGET_LOCATION 11
#define PACKET_PAN_TILT 12

#define ROLE_CONTROL 0
#define ROLE_VIDEO 1

class Packet
{
  public:

    virtual void read(QDataStream &) = 0;
    virtual void pack(QDataStream &) = 0;
    virtual int type() = 0;
    
};

class PacketHello : public Packet
{
  public:

    virtual void read(QDataStream &);
    virtual void pack(QDataStream &);
    virtual int type() { return PACKET_HELLO; }

    qint32 role;
};

class PacketSystemConfig : public Packet
{
  public:

    virtual void read(QDataStream &);
    virtual void pack(QDataStream &);
    virtual int type() { return PACKET_SYSTEM_CONFIG; } 

    qint32 num_motors;
    
};

class PacketShutdown : public Packet
{
  public:

    virtual void read(QDataStream &);
    virtual void pack(QDataStream &);
    virtual int type() { return PACKET_SHUTDOWN; } 
    
};

class PacketMotorSet : public Packet
{
  public:
    
    virtual void read(QDataStream &);
    virtual void pack(QDataStream &);
    virtual int type() { return PACKET_MOTOR_SET; } 

    qint32 num_motor;
    qint32 val;
};

class PacketMotorStatus : public Packet
{
  public:
    
    virtual void read(QDataStream &);
    virtual void pack(QDataStream &);
    virtual int type() { return PACKET_MOTOR_STATUS; } 

    qint32 num_motor;
    qint32 val;
};

class PacketVideoFrame : public Packet
{
  public:
        
    virtual void read(QDataStream &);
    virtual void pack(QDataStream &);
    virtual int type() { return PACKET_VIDEO_FRAME; }
    
    qint32 camera;
    qint32 length;
    QByteArray frame;
};

class PacketOverlayFrame : public Packet
{
  public:
        
    virtual void read(QDataStream &);
    virtual void pack(QDataStream &);
    virtual int type() { return PACKET_OVERLAY_FRAME; }
    
    qint32 camera;
    qint32 length;
    QByteArray frame;
};

class PacketDisableOverlay : public Packet
{
  public:
    
    virtual void read(QDataStream &) {}
    virtual void pack(QDataStream &) {}
    virtual int type() { return PACKET_DISABLE_OVERLAY; }
    
};

class PacketConsole : public Packet
{
  public:
    
    virtual void read(QDataStream &);
    virtual void pack(QDataStream &);
    virtual int type() { return PACKET_CONSOLE; } 

    QString text;
    
};

class PacketFocus : public Packet
{
  public:
    
    virtual void read(QDataStream &);
    virtual void pack(QDataStream &);
    virtual int type() { return PACKET_FOCUS; } 

    qint32 left;
    qint32 top;
    qint32 right;
    qint32 bottom;
    qint32 camera;
    QString text;
    
};

class PacketObjectSelect: public Packet
{
  public:
    
    virtual void read(QDataStream &);
    virtual void pack(QDataStream &);
    virtual int type() { return PACKET_OBJECT_SELECT; } 

    qint32 x;
    qint32 y;
    qint32 r;
    qint32 g;
    qint32 b;
    qint32 camera;
    
};

class PacketTargetLocation : public Packet
{
  public:
    
    virtual void read(QDataStream &);
    virtual void pack(QDataStream &);
    virtual int type() { return PACKET_TARGET_LOCATION; } 

    double xoff;
    double yoff;
    double width;
    qint32 r;
    qint32 g;
    qint32 b;
    double leftangle1;
    double rightangle1;
    double leftangle2;
    double rightangle2;
    bool present;
    QString text;
    
};


class PacketPanTilt : public Packet
{
  public:
    
    virtual void read(QDataStream &);
    virtual void pack(QDataStream &);
    virtual int type() { return PACKET_PAN_TILT; } 

    qint32 pan;
    qint32 tilt;
    
};

Packet * read_packet(int);
void send_packet(int fd, Packet * p, bool reliable = true);

#endif
