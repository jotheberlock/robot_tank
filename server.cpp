#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <QtCore/QStringList>
#include <math.h>

#include "server.h"
#include "packet.h"
#include "zlib.h"
#include "jpeglib.h"

#include "ax12.h"

const char * video_device = 0;

void ServerThread::run()
{
        //int one = 1;
        //setsockopt(fd, SOL_TCP, TCP_NODELAY, &one, sizeof(one)); 

    while (1)
    {
        Packet * p = read_packet(fd);
            
        if (!p)
        {
            server->setRunning(false);
            return;
        }
        
        int type = p->type();
        
        if (type == PACKET_HELLO)
        {
            PacketHello * ph = (PacketHello *)p;
            role = ph->role;
            
            printf("Got hello, role %d\n", role);

            if (role == ROLE_CONTROL)
            {
                {
                    QMutexLocker qml(&lock);
                    PacketSystemConfig psc;
                    psc.num_motors = server->numMotors();
                    send_packet(fd, &psc);
                } 
            
                server->sendStatuses();
            }
        }
        else if (type == PACKET_SHUTDOWN)
        {
            printf("Got shutdown\n");
            server->setRunning(false);
            ::close(fd);
            return;
        }
        else if (type == PACKET_MOTOR_SET)
        {
            PacketMotorSet * pms = (PacketMotorSet *)p;
            printf("Setting motor %d to %d\n", pms->num_motor, pms->val);
            server->setMotor(pms->num_motor, pms->val);
            server->sendStatuses();
        }
        else if (type == PACKET_CONSOLE)
        {
            QString ret = "";
            PacketConsole * pc = (PacketConsole *)p;
            ret = server->process_console(pc->text);
            if (ret != "")
            {
                QMutexLocker qml(&lock);
                PacketConsole pc;
                pc.text = ret;
                send_packet(fd, &pc);
            }
        }
        else if (type == PACKET_OBJECT_SELECT)
        {
            PacketObjectSelect * pos = (PacketObjectSelect *)p;
            server->suggested_x[pos->camera] = pos->x;
            server->suggested_y[pos->camera] = pos->y;
            server->suggested_x[pos->camera] = pos->x;
            server->suggested_y[pos->camera] = pos->y;
                /*
            server->red = pos->r;
            server->green = pos->g;
            server->blue = pos->b;
                */
        }
        else if (type == PACKET_PAN_TILT)
        {
            PacketPanTilt * pt = (PacketPanTilt *)p;
            SetPosition(19, 1023 - pt->pan);
            SetPosition(20, 1023 - pt->tilt);
        } 
    }
}

void ServerThread::sendPacket(Packet * p, bool reliable)
{
    set_thread_status("sendPacket locking");
    QMutexLocker qml(&lock);
//    printf("Packet sent\n");
    set_thread_status("sendPacket sending");
    send_packet(fd, p, reliable);
    set_thread_status("sendPacket sent");
}

void FrameSendThread::run()
{
    registerThread("Frame send thread");
    
    while (true)
    {
        for (int camera=0; camera<NUM_CAMERAS; camera++)
        {
                //usleep(10);
            
            setStatus("Locking frame send mutex");
            server->frame_send_mutex.lock();
            setStatus("Waiting on frame send mutex");
            server->frame_send_condition.wait(&server->frame_send_mutex);
            setStatus("Frame send mutex locked");
            
            if (server->oframes[camera].size() > 0)
            {
                QImage i = server->oframes[camera].pop();
                server->oframes[camera].clear();
                
                setStatus("Unlocking frame send mutex 0");
                server->frame_send_mutex.unlock();
                setStatus("Unlocked frame send mutex 0");
        
                PacketOverlayFrame pvf;
                pvf.camera = camera;
                QBuffer buffer(&pvf.frame);
                buffer.open(QIODevice::WriteOnly);
                i.save(&buffer, "JPG");
                pvf.length = pvf.frame.size();
                setStatus("Broadcasting frame 0");
                server->broadcast(&pvf, ROLE_VIDEO, false);
                setStatus("Broadcasted frame 0");
            }
            else if (server->frames[camera].size() > 0)
            {
                QImage i = server->frames[camera].pop();
                server->frames[camera].clear();
                
                setStatus("Unlocking frame send mutex 1");
                server->frame_send_mutex.unlock();
                setStatus("Unlocked frame send mutex 1");
        
                PacketVideoFrame pvf;
                pvf.camera = camera;
                QBuffer buffer(&pvf.frame);
                buffer.open(QIODevice::WriteOnly);
                i.save(&buffer, "JPG");
                pvf.length = pvf.frame.size();
                
                setStatus("Broadcasting frame 1");
                server->broadcast(&pvf, ROLE_VIDEO, false);
                setStatus("Broadcasted frame 1");
            }
            else
            {
                setStatus("Unlocking frame send mutex 2");
                server->frame_send_mutex.unlock();
                setStatus("Unlocked frame send mutex 2");
            }
        }
    }
}

Server::Server()
{
    ai_active = false;
    is_running = true;
    
    for (int loopc=0;loopc<2;loopc++)
    {
        suggested_x[loopc] = -1;
        suggested_y[loopc] = -1;
        objx[loopc] = 0;
        objy[loopc] = 0;
        
    }

    motors_allowed = true;
    interval = 66;
    last_interval = 0;
    grab_time = 0;
    convert_time = 0;
    brain_time = 0;
    send_time = 0;
    vol_match = 0.5;
    screen_match = 0.02;
    debounce_queue_size = 0;
    ai_speed = 40;
    
    red = 255;
    green = 0;
    blue = 0;
    fill_spot = false;
    needed_score = 500;
    apply_blur = false;
    yuy2_cache_dirty = 1;
    
    JrkMotor * jm1 = new JrkMotor("/dev/ttyACM0");
    JrkMotor * jm2 = new JrkMotor("/dev/ttyACM2");
    jm2->setInverted(true);

    motors.push_back(jm1);
    motors.push_back(jm2);
    motors.push_back(new MissileMotor(0));
    motors.push_back(new MissileMotor(1));
    motors.push_back(new MissileMotor(2));

    brightness = 2;
    contrast = 0.6;
    hue = 0;
    saturation = 1;

    teststereo=false;
    tx1=0;
    tx2=0;
    tx3=0;
    tx4=0;
}

void Server::init()
{
    brain_thread = new BrainThread();
    brain_thread->start();

    Camera * cam1 = new Camera("/dev/video1", 0);
    Camera * cam2 = new Camera("/dev/video0", 1);
    cam1->start();
    cam2->start();
    cameras.push_back(cam1);
    cameras.push_back(cam2);
    
    ax12Init(57600);
}

QString Server::process_console(QString cmd)
{
    QStringList qsl = cmd.split(" ");
    
    if (cmd == "ping")
    {
        return "pong";
    }
    else if (cmd == "shutdown")
    {
        setRunning(false);
    }
    else if (cmd == "motors")
    {
        motors_allowed = !motors_allowed;
        return motors_allowed ? "Motors allowed" : "Motors disabled";
    }
    else if (cmd == "ai")
    {
        ai_active = !ai_active;
        if (ai_active)
        {
            if (::system("echo \"By your command\" | festival --tts") == -1)
            {
                return "system() failed";
            }
        }
        
        return ai_active ? "AI active" : "AI disabled";
    }
    else if (qsl[0] == "say")
    {
        char buf[4096];
        sprintf(buf, "echo \"%s\" | festival --tts",
                cmd.right(cmd.size() - 4).toAscii().data());
        if (::system(buf) == -1)
        {
            return "system() failed";
        }
            
    }  
    else if (cmd == "annoy")
    {
        if (::system("echo \"Psssst\" | festival --tts") == -1)
        {
            return "system() failed";
        }
    }
    else if (cmd == "halt")
    {
        setMotor(0,0);
        setMotor(1,0);
        ai_active = false;
    }
    else if (qsl[0] == "interval")
    {
        if (qsl.size() > 1)
        {
            interval = qsl[1].toLong();
        }

        return QString::number(interval) +
               QString(" / ") +
               QString::number(last_interval);
    }
    else if (qsl[0] == "track")
    {
        if (qsl.size() == 4)
        {
            red = qsl[1].toInt();
            green = qsl[2].toInt();
            blue = qsl[3].toInt();
        }
    }
    else if (qsl[0] == "setscore")
    {
        if (qsl.size() == 2)
        {
            needed_score = qsl[1].toInt();
        }
    }
    else if (qsl[0] == "reset_hints")
    {
        suggested_x[0] = -1;
        suggested_y[0] = -1;
        suggested_x[1] = -1;
        suggested_y[1] = -1;
    } 
    else if (qsl[0] == "showfill")
    {
        fill_spot = !fill_spot;
        if (fill_spot == false)
        {
            PacketDisableOverlay pdo;
            broadcast(&pdo, ROLE_CONTROL);
        }
        return fill_spot ? "Showfill on" : "Showfill off";
    }
    else if (qsl[0] == "blur")
    {
        apply_blur = !apply_blur;
        return fill_spot ? "Blur on" : "Blur off";
    }
    else if (qsl[0] == "brightness")
    {
        if (qsl.size() == 2)
        {
            brightness = qsl[1].toFloat();
            yuy2_cache_dirty++;
        }    
    }
    else if (qsl[0] == "contrast")
    {
        if (qsl.size() == 2)
        {
            contrast = qsl[1].toFloat();
            yuy2_cache_dirty++;
        }    
    }
    else if (qsl[0] == "hue")
    {
        if (qsl.size() == 2)
        {
            hue = qsl[1].toFloat();            
            yuy2_cache_dirty++;
        }    
    }
    else if (qsl[0] == "saturation")
    {
        if (qsl.size() == 2)
        {
            saturation = qsl[1].toFloat();
            yuy2_cache_dirty++;
        }    
    }
    else if (qsl[0] == "teststereo")
    {
        if (qsl.size() == 5)
        {
            teststereo=true;
            tx1 = qsl[1].toInt();
            tx2 = qsl[2].toInt();
            tx3 = qsl[3].toInt();
            tx4 = qsl[4].toInt();
        }
        else
        {
            teststereo=false;
        }
    }
    else if (qsl[0] == "fps")
    {
        if (qsl.size() == 2)
        {
            interval = 1000 / qsl[1].toInt();
        }
        
        return QString::number(interval) +
               QString(" / ") +
               QString::number(last_interval);
    }
    else if (qsl[0] == "voltarget")
    {
        if (qsl.size() == 2)
        {
            vol_match = qsl[1].toFloat();
        }
    }
    else if (qsl[0] == "screentarget")
    {
        if (qsl.size() == 2)
        {
            screen_match = qsl[1].toFloat();
        }
    }
    else if (qsl[0] == "debounce")
    {
        if (qsl.size() == 2)
        {
            debounce_queue_size = qsl[1].toInt();
        }
    }
    else if (qsl[0] == "aispeed")
    {
        if (qsl.size() == 2)
        {
            ai_speed = qsl[1].toInt();
        }
    }
    else if (qsl[0] == "perf")
    {
        QString ret = "Total time " +
            QString::number(last_interval) +
            QString(" fps ") +
            QString::number(1000 / last_interval) +
            QString(" grab ") + QString::number(grab_time) +
            QString(" convert ") + QString::number(convert_time) +
            QString(" brain ") + QString::number(brain_time) +
            QString(" send ") + QString::number(send_time);
        return ret; 
    }
    else if (cmd == "threads")
    {
        QString ret;
        threadlist_mutex.lock();
        for(int loopc=0; loopc<threadlist.size(); loopc++)
        {
            MyThread * my = threadlist.at(loopc);
            QString tstr = my->threadName();
            tstr += " ";
            tstr += my->status();
            tstr += " ";
            int ms = my->lastUpdate().msecsTo(QTime::currentTime());
            tstr += QString::number(ms);
            tstr += '\n';

            ret += tstr;
        }
        threadlist_mutex.unlock();
        return ret;
    }
    else
    {
        return "?";
    }

    return "";
}

BrainThread * brain_thread;

void Server::run()
{
    registerThread("Server");
    
    struct sockaddr_in addr;
    
    ssock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (ssock == -1)
    {
        printf("Can't create listen socket\n");
        exit(1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(6666);
    int rc = ::bind(ssock, (struct sockaddr *)&addr, sizeof(addr));
    if (rc != 0)
    {
        printf("Can't bind\n");
        exit(1);
    }

        /*
    struct linger linger;
    linger.l_onoff = 0; 
    linger.l_linger = 0; 
    rc = setsockopt(ssock, SOL_SOCKET, SO_LINGER, &linger,sizeof(linger));
    if (rc != 0)
    {
        printf("Can't disable linger\n");
        exit(1);
    }
    */
        
    rc = ::listen(ssock, 5);
    if (rc != 0)
    {
        printf("Can't set backlog\n");
        exit(1);
    }
    
    while (true)
    {
        rc = ::accept(ssock, 0, 0);
        if (rc == -1)
        {
            printf("Accept error\n");
            exit(1);
        }
        else
        {
            printf("Got new connection\n");
            ServerThread * st = new ServerThread(rc);
            {
                QMutexLocker qml(&lock);
                threads.push_back(st);
            } 
            
            st->start();
        }   
    }
}

void Server::broadcast(Packet * p, int role, bool reliable)
{
    if (threads.size() == 0)
        return;
    
        //printf("Got %d threads\n", threads.size());
    
    for (int loopc=0; loopc<threads.size(); loopc++)
    {
        if (threads[loopc]->getRole() == role)
        {
            threads[loopc]->sendPacket(p, reliable);
        }
    }
}

void Server::broadcastRaw(char * data, int len, int role)
{
    
    for (int loopc=0; loopc<threads.size(); loopc++)
    {
        if (threads[loopc]->getRole() == role)
        {
            threads[loopc]->sendRawPacket(data, len);
        }
    }    
}


void Server::shutdown()
{
    ::close(ssock);
    setMotor(0,0);
    setMotor(1,0);
}

void Server::sendStatuses()
{
    for (int loopc=0; loopc<2; loopc++)
    {
        printf("Sending statuses\n");
        PacketMotorStatus status;
        status.num_motor = loopc;
        status.val = getMotor(loopc);
        broadcast(&status, ROLE_CONTROL);
    } 
}

void Server::setMotor(int m, int v)
{
    if (m < 0 || m > motors.size())
    {
        printf("Invalid motor set %d\n", m);
    }
    
    if (!motors[m]->setMotor(v))
    {
        printf("Motor %d failure to set speed %d\n", m, v);
    }
}

int Server::getMotor(int m)
{
    if (m < 0 || m > motors.size())
    {
        printf("Invalid motor get %d\n", m);
    }
    
    return motors[m]->getMotor();
}

Server * server;

int main(int argc, char ** argv)
{
    signal(SIGPIPE, SIG_IGN);
    
    video_device = (argc == 1) ? "/dev/video0" : argv[1];
    
    server = new Server();

    for (int loopc=0; loopc<server->numMotors(); loopc++)
    {
        server->setMotor(loopc, 0);
    } 
    
    if (argc == 2 && !strcmp(argv[1],"halt"))
    {
        exit(0);
    }
    
    server->init();
    server->start();
    
    FrameSendThread * fst = new FrameSendThread();
    fst->start();
    
    while (server->running())
    {
        sleep(1);
    }

    server->shutdown();
}
