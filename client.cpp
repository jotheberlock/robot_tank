#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#ifdef WIN32
#include <winsock.h>
#else
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#endif

#include <QtGui/QPainter>
#include <QtGui/QGraphicsEllipseItem>

#include "packet.h"
#include "client.h"
#include "zlib.h"

bool draw_overlays = false;
bool mouse_locked = false;
qint32 pan = 512;
qint32 tilt = 512;

#define PAN_LIMIT 200
#define TILT_LIMIT 200

void ClientThread::connect()
{
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < -1)
    {
        printf("socket\n");
        exit(1);
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(ip);
    servaddr.sin_port = htons(6666);

    if (::connect(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        printf("connect\n");
        exit(1);
    }

        //int one = 1;
        //setsockopt(sock, SOL_TCP, TCP_NODELAY, &one, sizeof(one));
  
    lock.lock();
    PacketHello ph;
    ph.role = role;
    send_packet(sock, &ph);
    lock.unlock();
}

void ClientThread::run()
{
    connect();
    
    while (true)
    {
            //lock.lock();
        Packet * p = read_packet(sock);
            //lock.unlock();
        
        if (!p)
        {
            printf("Client receive error\n");
            emit gotConsoleString("@@@ LOST CONNECTION @@@");
            connect();
            continue;
        }
        else
        {
            if (p->type() == PACKET_SYSTEM_CONFIG)
            {
                PacketSystemConfig * psc = (PacketSystemConfig *)p;
                printf("Got sysconfig\n");
                emit gotNumMotors(psc->num_motors);
            }
            else if (p->type() == PACKET_MOTOR_STATUS)
            {
                PacketMotorStatus * psm = (PacketMotorStatus *)p;
                emit gotMotorStatus(psm->num_motor, psm->val);
            }
            else if (p->type() == PACKET_CONSOLE)
            {
                PacketConsole * pc = (PacketConsole *)p;
                emit gotConsoleString(pc->text);
            }
            else if (p->type() == PACKET_FOCUS)
            {
                PacketFocus * pf = (PacketFocus *)p;
                emit gotFocus(pf->left, pf->top, pf->right, pf->bottom,pf->camera, pf->text);
            }
            else if (p->type() == PACKET_VIDEO_FRAME)
            {
                PacketVideoFrame * pvf = (PacketVideoFrame *)p;

                int c = pvf->camera;

                if (c>1)
                {
                    printf("Silly camera %d\n", c);
                    return;
                }
                
                video_window[c]->lock();

                QImage image;
                if (image.loadFromData(pvf->frame))
                {
                }
                else
                {
                    printf("Corrupt frame\n");
                }
                
                video_window[c]->getImage() = image;
                video_window[c]->unlock();
                emit gotNewFrame();
            }
            else if (p->type() == PACKET_OVERLAY_FRAME)
            {
                draw_overlays = true;
                
                PacketOverlayFrame * pvf = (PacketOverlayFrame *)p;

                int c = pvf->camera;

                if (c>1)
                {
                    printf("Silly camera %d\n", c);
                    return;
                }
                
                video_window[c]->lock();

                QImage image;
                if (image.loadFromData(pvf->frame))
                {
                    printf("Got overlay %d %d %d\n", c, image.width(), image.height());
                }
                else
                {
                    printf("Corrupt overlay frame, %d %d\n",
                           pvf->length, pvf->frame.length());
                }
                
                video_window[c]->getOverlay() = image;
                video_window[c]->unlock();
                emit gotNewOverlay();
            }
            else if (p->type() == PACKET_DISABLE_OVERLAY)
            {
                draw_overlays = false;
            }            
            else if (p->type() == PACKET_TARGET_LOCATION)
            {
                PacketTargetLocation * ptl = (PacketTargetLocation *)p;
                emit gotTarget(ptl->present, ptl->xoff, ptl->yoff, ptl->width,
                               ptl->r, ptl->g, ptl->b, ptl->leftangle1,
                               ptl->rightangle1, ptl->leftangle2,
                               ptl->rightangle2, ptl->text);
            }
        }
    }
}

void ClientThread::sendPacket(Packet * p)
{
    lock.lock();
    send_packet(sock, p);
    lock.unlock();
}

Console::Console()
{
    setWindowTitle("Console");
    resize(320,240);
    QVBoxLayout *  layout = new QVBoxLayout(this);
    window = new QTextEdit(this);
    line = new QLineEdit(this);
    layout->addWidget(window,1);
    layout->addWidget(line);
    window->setReadOnly(true);
    
    connect(line, SIGNAL(returnPressed()), this, SLOT(returnPressed()));
    connect(ct, SIGNAL(gotConsoleString(QString)), this,
            SLOT(stringReceived(QString)), Qt::QueuedConnection);
}

void Console::stringReceived(QString s)
{
    window->append(s);
}

void Console::returnPressed()
{
    QString val = line->text();
    PacketConsole pc;
    pc.text = val;
    ct->sendPacket(&pc);
    QString txt = "> ";
    txt += val;
    window->append(txt);
    line->clear();
}

VideoWindow::VideoWindow(int c)
{
    resize(320,240);
    setWindowTitle("Video feed");
    left = -1;
    top = -1;
    right = -1;
    bottom = -1;
    camera = c;
}


StereoWindow::StereoWindow()
{
    resize(320,240);
    setWindowTitle("Stereo feed");
    connect(ctv, SIGNAL(gotNewFrame()), this, SLOT(newImage()));
}

void StereoWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.drawImage(QRectF(0,0,width(),height()), image);
}

void StereoWindow::newImage()
{
    QImage left = video_window[0]->getImage();
    QImage right = video_window[1]->getImage();
    
    if (left.width() > 0 && right.width() > 0)
    {
        image = QImage(left.width(), left.height(), QImage::Format_RGB888);

        for (int loopc=0; loopc<left.width(); loopc++)
        {
            for (int loopc2=0; loopc2<left.height(); loopc2++)
            {
                QRgb lp = left.pixel(loopc,loopc2);
                QRgb rp = right.pixel(loopc,loopc2);

                QRgb sp = qRgb(qRed(lp), qGreen(rp),
                               qBlue(rp));
                image.setPixel(loopc,loopc2,sp);
            }
        }
        
        show();
        update();
    }
}

void VideoWindow::keyPressEvent(QKeyEvent * event)
{
     if (event->key() == Qt::Key_L)
     {
         mouse_locked = !mouse_locked;
         if (mouse_locked)
         {
             printf("Grabbing mouse!\n");
             grabMouse();
         }
         else
         {
             releaseMouse();
         }

         setMouseTracking(mouse_locked);
     }
     else
     {
         cg->keyPressEvent(event);
     }
}

void VideoWindow::mouseMoveEvent(QMouseEvent * event)
{
    if (mouse_locked)
    {
        printf("Got %d %d\n", event->x(), event->y());

        int xp = event->x(); 
        int yp = event->y();

        if (xp < 0)
            xp = 0;
            
        if (yp < 0)
            yp = 0;
        
        if (xp > width())
            xp = width();
        
        if (yp > height())
            yp = height();
        
        float ww = width();
        float hh = height();

        float pl = (PAN_LIMIT * 2);
        float tl = (TILT_LIMIT * 2);
        
        float pm = pl / ww;
        float tm = tl / hh;

        float xf = ((float)xp) * pm;
        float yf = ((float)yp) * tm;

        xf += (512 - PAN_LIMIT);
        yf += (512 - TILT_LIMIT);

        printf("Which scales to %f %f\n", xf, yf);
        
        PacketPanTilt pt;
        pt.pan = (qint32)xf;
        pt.tilt = (qint32)yf;
        ct->sendPacket(&pt);
    }
}

void VideoWindow::paintEvent(QPaintEvent *)
{
    mutex.lock();
    QPainter painter(this);

    if (draw_overlays)
    {
        painter.drawImage(QRectF(0,0,width(),height()), overlay);
    }
    else
    {
        painter.drawImage(QRectF(0,0,width(),height()), image);
    }
    
    mutex.unlock();
    
    if (left != -1 && top != -1 && right != -1 && bottom != -1)
    {
        float xscale = (float)width() / (float)image.width();
        float yscale = (float)height() / (float)image.height();

        int xx = (int) (((float)left) * xscale);
        int yy = (int) (((float)top) * yscale);
        int xx2 = (int) (((float)right) * xscale);
        int yy2 = (int) (((float)bottom) * yscale);
        
        painter.drawEllipse(xx,yy,xx2-xx,yy2-yy);

        int midx = ((xx2-xx) / 2) + xx;
        int midy = ((yy2-yy) / 2) + yy;
        
        painter.drawLine(xx,midy-1,xx2,midy-1);
        painter.drawLine(xx,midy+1,xx2,midy+1);
        painter.drawLine(midx-1,yy,midx-1,yy2);
        painter.drawLine(midx+1,yy,midx+1,yy2);
        painter.setPen(QPen(QColor(255,255,255)));
        painter.drawLine(xx,midy,xx2,midy);
        painter.drawLine(midx,yy,midx,yy2);
        painter.setPen(QPen(QColor(0,0,0)));
        if (text != "")
        {
            painter.drawText(xx2,midy,text);
        }
    }
}

void VideoWindow::setFocus(int l, int t, int r, int b, int c, QString te)
{
//    printf("%d %d %d %d %d %d\n", c, camera, left, top, right, bottom);
    
    if (c==camera)
    {
        left = l;
        top = t;
        right = r;
        bottom = b;
        text = te;
    }
}

void VideoWindow::newImage()
{
    show();
    update();
}

void VideoWindow::newOverlay()
{
    show();
    update();
}

void VideoWindow::mouseDoubleClickEvent(QMouseEvent * event)
{
    int xx = event->x();
    int yy = event->y();
    
    float xscale = (float)image.width() / (float)width();
    float yscale = (float)image.height() / (float)height();

    int rx = (int)(xscale * (float)xx);
    int ry = (int)(yscale * (float)yy);

    PacketObjectSelect pos;
    pos.x = rx;
    pos.y = ry;

    QRgb rgb = image.pixel(rx,ry);
    pos.r = qRed(rgb);
    pos.g = qGreen(rgb);
    pos.b = qBlue(rgb);
    pos.camera = camera;

    printf("Picking colour %d %d %d\n",pos.r,pos.g,pos.b);
    ct->sendPacket(&pos);
}

ClientGUI::ClientGUI(const char * ip)
{
    setWindowTitle("Control");
    connect(ct, SIGNAL(gotNumMotors(int)), this, SLOT(numMotors(int)),
            Qt::QueuedConnection);
    connect(ct, SIGNAL(gotMotorStatus(int,int)), this, SLOT(motorStatus(int,int)),
            Qt::QueuedConnection);
    connect(ct, SIGNAL(gotTarget(bool,double,double,double,int,int,int,
                                 double,double,double,double,QString)), map,
            SLOT(targetUpdated(bool,double,double,double,int,int,int,
                               double,double,double,double,QString)),
            Qt::QueuedConnection);
    
    ct->start();
    
    ctv = new ClientThread(ip, ROLE_VIDEO);
    ctv->start();

    StereoWindow * sw = new StereoWindow();
    
    connect(ctv, SIGNAL(gotNewFrame()), video_window[0], SLOT(newImage()), Qt::QueuedConnection);
    connect(ctv, SIGNAL(gotNewFrame()), video_window[1], SLOT(newImage()), Qt::QueuedConnection);
    connect(ctv, SIGNAL(gotNewOverlay()), video_window[0], SLOT(newOverlay()), Qt::QueuedConnection);
    connect(ctv, SIGNAL(gotNewOverlay()), video_window[1], SLOT(newOverlay()), Qt::QueuedConnection);
    connect(ct, SIGNAL(gotFocus(int,int,int,int,int,QString)), video_window[0], SLOT(setFocus(int,int,int,int,int,QString)), Qt::QueuedConnection);
    connect(ct, SIGNAL(gotFocus(int,int,int,int,int,QString)), video_window[1], SLOT(setFocus(int,int,int,int,int,QString)), Qt::QueuedConnection);

    layout = new QHBoxLayout;

    QPushButton * shutdown = new QPushButton("Shutdown");
    layout->addWidget(shutdown);
    setLayout(layout);

    connect(shutdown, SIGNAL(clicked()), this, SLOT(shutdownClicked()));
    sliders=0;
    num_sliders=0;
    ignoring_sliders = false;

    setGeometry(0,0,300,200);
}

void ClientGUI::numMotors(int n)
{
    printf("Got %d motors\n", n);

    num_sliders = n;
    
    sliders = new QSlider * [n];
    for (int loopc=0; loopc<n; loopc++)
    {
        QSlider * qs = new QSlider(Qt::Vertical);
        qs->setMaximum(127);
        qs->setMinimum(-127);
        layout->addWidget(qs);
        sliders[loopc] = qs;
        connect(qs, SIGNAL(valueChanged(int)), this, SLOT(sliderMoved(int)));
    }

    updateGeometry();
}

void ClientGUI::shutdownClicked()
{
    PacketShutdown ps;
    ct->sendPacket(&ps);
    exit(0);
}

void ClientGUI::setMotor(int motor, int pos)
{
    PacketMotorSet pms;
    pms.num_motor = motor;
    pms.val = pos;
    ct->sendPacket(&pms);    
}

void ClientGUI::sliderMoved(int pos)
{
    if (ignoring_sliders)
        return;
    
    printf("Got slider moved\n");
    
    for (int loopc=0; loopc<num_sliders; loopc++)
    {
        if (sender() == sliders[loopc])
        {
            printf("Index %d\n", loopc);
            setMotor(loopc, pos);
        }
    }
}

void ClientGUI::motorStatus(int motor, int val)
{
    if (motor >=0 && motor < num_sliders)
    {
        ignoring_sliders = true;
        sliders[motor]->setValue(val);
        ignoring_sliders = false;
    }
}

#define MOTORVAL 120

void ClientGUI::keyPressEvent(QKeyEvent * event)
{
    if (num_sliders > 1)
    {
        if (event->key() == Qt::Key_7)
        {
            setMotor(0, 0);
            setMotor(1, MOTORVAL);
        }
        else if (event->key() == Qt::Key_8)
        {
            setMotor(0, MOTORVAL);
            setMotor(1, MOTORVAL);
        }
        else if (event->key() == Qt::Key_9)
        {
            setMotor(0, MOTORVAL);
            setMotor(1, 0);
        }
        else if (event->key() == Qt::Key_4)
        {
            setMotor(0, -MOTORVAL);
            setMotor(1, MOTORVAL);
        }
        else if (event->key() == Qt::Key_5)
        {
            setMotor(0, 0);
            setMotor(1, 0);
        }
        else if (event->key() == Qt::Key_6)
        {
            setMotor(0, MOTORVAL);
            setMotor(1, -MOTORVAL);
        }
        else if (event->key() == Qt::Key_1)
        {
            setMotor(0, 0);
            setMotor(1, -MOTORVAL);
        }
        else if (event->key() == Qt::Key_2)
        {
            setMotor(0, -MOTORVAL);
            setMotor(1, -MOTORVAL);
        }
        else if (event->key() == Qt::Key_3)
        {
            setMotor(0, -MOTORVAL);
            setMotor(1, 0);
        }
    }
}

Map::Map()
    : QGraphicsView(0,0)
{
    shown = false;
    setInteractive(true);
    scene = 0;
    target = 0;
    setWindowTitle("Map");
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
}

void Map::keyPressEvent(QKeyEvent * event)
{
     if (event->key() == Qt::Key_Plus)
     {
         scale(1.1,1.1);
     }
     else if (event->key() == Qt::Key_Minus)
     {
         scale(0.9,0.9);
     }

     update();
}

void Map::targetUpdated(bool present, double xoff, double yoff, double width,
                        int r, int g, int b, double ll1, double rr1,
                        double ll2, double rr2, QString t)
{
    if (!shown)
    {
        shown = true;
        scene = new QGraphicsScene();
        setScene(scene);
        QRectF pos = QRectF(-60,-60,120,120);
        QPen edge(QColor(0,0,0));
        QBrush fill(QColor(r,g,b));
        target=scene->addEllipse(pos,edge,fill);
        text=scene->addText("");
        
        scene->addLine(QLineF(-30,30,30,30));
        scene->addLine(QLineF(-30,0,30,0));
        scene->addLine(QLineF(-30,0,-30,2000));
        scene->addLine(QLineF(30,0,30,2000));

        QPen grey(QColor(100,100,100));
        
        QGraphicsLineItem * a1 = scene->addLine(QLineF(0,0,0,2000));
        a1->setPos(-30,0);
        a1->setPen(grey);
        a1->rotate(-20);

        QGraphicsLineItem * a2 = scene->addLine(QLineF(0,0,0,2000));
        a2->setPos(-30,0);
        a2->setPen(grey);
        a2->rotate(20);
        
        QGraphicsLineItem * a3 = scene->addLine(QLineF(0,0,0,2000));
        a3->setPos(30,0);
        a3->setPen(grey);
        a3->rotate(-20);

        QGraphicsLineItem * a4 = scene->addLine(QLineF(0,0,0,2000));
        a4->setPos(30,0);
        a4->setPen(grey);
        a4->rotate(20);

        for (int loopc=0; loopc<10; loopc++)
        {
            QGraphicsLineItem * line = scene->addLine(QLineF(-2000,0,
                                                             2000,0));
            line->setPen(grey);
            line->setPos(0, loopc*200);
            QGraphicsTextItem * t = scene->addText(QString::number(loopc*200));
            t->setPos(0,loopc*200);
        }

        l1 = scene->addLine(QLineF(0,0,2000,0));
        l1->setPen(QPen(QColor(255,0,0)));
        l1->setPos(30,0);
        l1->setVisible(false);
        
        r1 = scene->addLine(QLineF(0,0,2000,0));
        r1->setPen(QPen(QColor(0,0,255)));
        r1->setPos(-30,0);
        r1->setVisible(false);
        
        l2 = scene->addLine(QLineF(0,0,2000,0));
        l2->setPen(QPen(QColor(255,0,0)));
        l2->setPos(30,0);
        l2->setVisible(false);
        
        r2 = scene->addLine(QLineF(0,0,2000,0));
        r2->setPen(QPen(QColor(0,0,255)));
        r2->setPos(-30,0);
        r2->setVisible(false);
        
        centerOn(0,0);
        
        show();
    }

    if (present)
        printf("%f %f %f %f\n", ll1, rr1, ll2, rr2);

    target->setRect(-(width/2), -20, width, 40);
        
    target->setPos(-xoff,yoff);
    target->setVisible(present);

    QString mytext =  QString("X ")+QString::number((int)xoff)+
                QString(" Y ") + QString::number((int)yoff)+
        QString(" W ") + QString::number((int)width)
        + QString(" ") + t;
    
    text->setPos(-xoff,yoff);
    text->setPlainText(mytext);
    text->setVisible(present);

    l1->setTransform(QTransform());
    l1->rotate(ll1);
    l1->setVisible(present);
    
    r1->setTransform(QTransform());
    r1->rotate(rr1);
    r1->setVisible(present);
    
    l2->setTransform(QTransform());
    l2->rotate(ll2);
    l2->setVisible(present);
    
    r2->setTransform(QTransform());
    r2->rotate(rr2);
    r2->setVisible(present);
    
    update();
}

VideoWindow * video_window[2];
Console * console;
ClientThread * ct;
ClientThread * ctv;
ClientGUI * cg;
Map * map;

int main(int argc, char ** argv)
{
#ifdef WIN32
  WSADATA data;
  WSAStartup(MAKEWORD(2,2), &data);
#endif
    QApplication app(argc, argv);
    
    ct = new ClientThread(argc == 1 ? "127.0.0.1" : argv[1], ROLE_CONTROL);
    
    video_window[0] = new VideoWindow(0);
    video_window[1] = new VideoWindow(1);
        
    console = new Console();

    map = new Map();
    map->targetUpdated(false, 0, 0, 0, 255, 0, 0, 0, 0, 0, 0, "");
    
    cg = new ClientGUI(argc == 1 ? "127.0.0.1" : argv[1]);
    cg->show();
    console->show();
    
    app.exec();   
}


#ifndef WIN32
#include "client.moc"
#endif

