#ifndef _CLIENT_
#define _CLIENT_

#include <QtCore/QThread>
#include <QtGui/QWidget>
#include <QtGui/QApplication>
#include <QtCore/QMutex>
#include <QtGui/QSlider>
#include <QtGui/QPushButton>
#include <QtGui/QHBoxLayout>
#include <QtGui/QVBoxLayout>
#include <QtGui/QKeyEvent>
#include <QtGui/QTextEdit>
#include <QtGui/QLineEdit>
#include <QtGui/QGraphicsView>

class ClientThread : public QThread
{
    Q_OBJECT
        
  public:

    ClientThread(const char * i, int r) { ip=i; role=r; }

    virtual void run();

    virtual void sendPacket(Packet *);

    void connect();
    
    signals:

    void gotNumMotors(int);
    void gotMotorStatus(int,int);
    void gotConsoleString(QString);
    
    void gotNewFrame();
    void gotNewOverlay();
    void gotFocus(int,int,int,int,int,QString);
    void gotTarget(bool,double,double,double,int,int,int,double,double,double,
                   double,QString);
    
  protected:

    int role;
    int sock;
    const char * ip;
    QMutex lock;
    
};

class VideoWindow : public QWidget
{
    Q_OBJECT
        
  public:

    VideoWindow(int c);

    void paintEvent(QPaintEvent *);
    virtual void keyPressEvent(QKeyEvent * event);
    virtual void mouseMoveEvent(QMouseEvent * event);
    
    void lock()
        {
            mutex.lock();
        }

    void unlock()
        {
            mutex.unlock();
        }
    
    QImage & getImage()
        {
            return image;
        }
    
    QImage & getOverlay()
        {
            return overlay;
        }

    virtual void mouseDoubleClickEvent(QMouseEvent *);
    
    public slots:

    void newImage();
    void newOverlay();
    
    void setFocus(int,int,int,int,int,QString);
    
  protected:

    QImage image;
    QImage overlay;
    
    QMutex mutex;
    int left,top,right,bottom;
    int camera;
    QString text;
    
};

class StereoWindow : public QWidget
{
    Q_OBJECT
        
  public:

    StereoWindow();

    void paintEvent(QPaintEvent *);

    public slots:

        void newImage();
    
  protected:

    QImage image;
};

extern ClientThread * ct;
extern ClientThread * ctv;
extern VideoWindow * video_window[2];

class Console : public QWidget
{
    Q_OBJECT

        public:

    Console();

    public slots:

    void stringReceived(QString);
    void returnPressed();

  protected:

    QTextEdit * window;
    QLineEdit * line;
    
};

extern Console * console;

class Map : public QGraphicsView
{
    Q_OBJECT
        
  public:

    Map();
    
    public slots:

    void targetUpdated(bool,double,double,double,int,int,int,
                       double,double,double,double,QString);

    virtual void keyPressEvent(QKeyEvent * event);
    
  protected:

    QGraphicsScene * scene;
    QGraphicsEllipseItem * target;
    QGraphicsTextItem * text;
    
    QGraphicsLineItem * l1;
    QGraphicsLineItem * r1;
    QGraphicsLineItem * l2;
    QGraphicsLineItem * r2;
    
    bool shown;

};

extern Map * map;

class ClientGUI : public QWidget
{
    Q_OBJECT

        public:

    ClientGUI(const char *);
    
    public slots:

    void numMotors(int);

    void shutdownClicked();
    void sliderMoved(int);
    void motorStatus(int,int);
    void setMotor(int,int);
    
    virtual void keyPressEvent(QKeyEvent * event);
    
  protected:

    QSlider ** sliders;
    int num_sliders;
    QHBoxLayout * layout;
    
    bool ignoring_sliders;
    
};

extern ClientGUI * cg;

#endif
