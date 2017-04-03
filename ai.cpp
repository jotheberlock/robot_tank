#include "ai.h"
#include "server.h"
#include <math.h>


time_t last_say = 0;

BrainThread::BrainThread()
{
    data_buffer = new int[1024*768];
}

int BrainThread::diff(int x, int y)
{
    return 255 - abs(x-y);
}

int * BrainThread::getData(int x,int y,int l)
{
    int * ptr = data_buffer +
        (y * linestep) + x;
    return ptr;
}

void BrainThread::mergeRegions(int val1, int val2, int w, int h,int l)
{
    for (int loopc = 0; loopc<w; loopc++)
    {
        for (int loopc2 = 0; loopc2<h; loopc2++)
        {
            int * ptr = getData(loopc, loopc2,l);
            if (*ptr == val1)
            {
                *ptr = val2;
            }   
        }
    }

    if (entities[val1].left < entities[val2].left)
        entities[val2].left = entities[val1].left;
    if (entities[val1].top < entities[val2].top)
        entities[val2].top = entities[val1].top;
    if (entities[val1].right > entities[val2].right)
        entities[val2].right = entities[val1].right;
    if (entities[val1].bottom > entities[val2].bottom)
        entities[val2].bottom = entities[val1].bottom;

    entities[val2].pointcount += entities[val1].pointcount;
}

void BrainThread::findSpot(int current_camera)
{
    memset(data_buffer, 0, 1024*768*4);

    unsigned char tr = server->red;
    unsigned char tg = server->green;
    unsigned char tb = server->blue;

    nextent = 1;

    int width = server->image[current_camera].width();
    int height = server->image[current_camera].height();
    int linestep = server->image[current_camera].bytesPerLine() / 4;
         
    if (server->apply_blur)
    {
        QImage newimage(width, height, server->image[current_camera].format());
        
        for (int loopc = 0; loopc<width; loopc++)
        {
            for (int loopc2 = 0; loopc2<height; loopc2++)
            {
                int rcount = 0;
                int gcount = 0;
                int bcount = 0;
                int pcount = 0;
                
                for (int loopc3 = -1; loopc3 < 2; loopc3++)
                {
                    for (int loopc4 = -1; loopc4 < 2; loopc4++)
                    {
                        int x = loopc + loopc3;
                        int y = loopc2 + loopc4;
                        if (x > -1 && y > -1 && x < width && y < width)
                        {
                            QRgb rgb = server->image[current_camera].pixel(x,y);
                            rcount += qRed(rgb);
                            gcount += qGreen(rgb);
                            bcount += qBlue(rgb);
                            pcount++;
                        }
                    }
                }

                rcount /= pcount;
                gcount /= pcount;
                bcount /= pcount;

                newimage.setPixel(loopc,loopc2,qRgb(rcount,gcount,bcount));
            }
        }

        server->image[current_camera] = newimage;
    }
    
    for (int loopc = 1; loopc<width; loopc++)
    {
        for (int loopc2 = 1; loopc2<height; loopc2++)
        {
            QRgb * ptr = ((QRgb *)server->image[current_camera].scanLine(loopc2));
            
            if (ptr == 0)
            {
                return;
            }

            ptr += loopc;
            
            unsigned char r = qRed(*ptr);
            unsigned char g = qGreen(*ptr);
            unsigned char b = qBlue(*ptr);
            
            int score = diff(tr, r) + diff(tg, g) + diff(tb, b);

            if (score > server->needed_score)
            {
                int val1 = *getData(loopc-1, loopc2, linestep);
                int val2 = *getData(loopc, loopc2-1, linestep);

                if (server->fill_spot)
                {
                    *ptr = 0x0;
                }
                
                if (val1 && val2 && val1 != val2)
                {
                    mergeRegions(val2, val1,width,height,linestep);
                    val2 = val1;
                }

                if (val1 || val2)
                {
                    int * ptr = getData(loopc, loopc2, linestep);
                    *ptr = (val1 > val2) ? val1 : val2;
                    
                    Entity & et = entities[(*ptr)];
                    if (et.left > loopc)
                        et.left = loopc;
                    if (et.right < loopc)
                        et.right = loopc;
                    if (et.bottom < loopc2)
                        et.bottom = loopc2;
                    et.pointcount++;
                }
                else
                {
                    if (nextent > 10000)
                    {
                        printf("Entity overflow\n");
                        break;
                    }
                    
                    Entity et;
                    et.left = loopc;
                    et.top = loopc2;
                    et.right = loopc;
                    et.bottom = loopc2;
                    et.pointcount = 1;
                    entities[nextent] = et;
                    
                    int * ptr = getData(loopc, loopc2, linestep);
                    *ptr = nextent;
                    
                    nextent++;
                } 
            }
        }    
    }

    bool changed = true;

    printf("!! Merge pass!\n");
    
    while (changed)
    {
        changed = false;
        for (int loopc = 1; loopc<width; loopc++)
        {
            for (int loopc2 = 1; loopc2<height; loopc2++)
            {
                int val1 = *getData(loopc-1, loopc2,linestep);
                int val2 = *getData(loopc, loopc2-1,linestep);
                int val3 = *getData(loopc, loopc2,linestep);
                
                if (val1 && val3 && val1 != val3)
                {
                    mergeRegions(val1, val3,width,height,linestep);
                }
                
                if (val2 && val3 && val2 != val3)
                {
                    mergeRegions(val2, val3,width,height,linestep);
                }
            }
        } 
    }
    
    biggest = -1;
    int biggestsize = 0;

    int fits_suggested = -1;

    float svol = CAM_WIDTH * CAM_HEIGHT;
    
    printf("%d entities\n", nextent);
    for (int loopc=1; loopc<nextent; loopc++)
    {
        Entity & et = entities[loopc];
        
        if (server->suggested_x[current_camera] != -1 &&
            server->suggested_y[current_camera] != -1)
        {
            if (server->suggested_x[current_camera] >= et.left &&
                server->suggested_x[current_camera] <= et.right &&
                server->suggested_y[current_camera] >= et.top &&
                server->suggested_y[current_camera] <= et.bottom)
            {
                fits_suggested = loopc;

                server->suggested_x[current_camera] = (et.right - et.left) / 2;
                server->suggested_y[current_camera] = (et.bottom - et.top) / 2;
                server->suggested_x[current_camera] += et.left;
                server->suggested_y[current_camera] += et.top;
                
            }
        }
        
        if (et.pointcount > biggestsize)
        {
            float pc = (float)et.pointcount;
            
            float vol = (et.right - et.left) * (et.bottom - et.top);
                
            if ( ( (pc / vol ) > server->vol_match ) &&
                 ( (pc / svol ) > server->screen_match) )
            {
                biggest = loopc;
                biggestsize = et.pointcount;
            }
        }
    }

    if (fits_suggested != -1)
        biggest = fits_suggested;
}

void BrainThread::sendSpot(int current_camera)
{
    if (server->fill_spot && server->image[current_camera].bits())
    {
        set_thread_status("sendSpot getting frame send mutex");
        {
            QMutexLocker qml(&server->frame_send_mutex);
            server->frames[current_camera].push(server->image[current_camera]);
            set_thread_status("sendSpot releasing frame send mutex");
        }
        
        setStatus("sendSpot waking frame thread");
        server->frame_send_condition.wakeAll();
        setStatus("sendSpot frame thread woken");
    }
    
    PacketFocus pf;

    int xpos,ypos;
    
    if (biggest > -1)
    {
        pf.left = entities[biggest].left;
        pf.top = entities[biggest].top;
        pf.right = entities[biggest].right;
        pf.bottom = entities[biggest].bottom;
        pf.camera = current_camera;
        pf.text = text;
        
        printf("Biggest is %d %d %d %d points %d\n", pf.left, pf.top, pf.right, pf.bottom, entities[biggest].pointcount);
        
        xpos = pf.left + ((pf.right - pf.left) / 2);
        ypos = pf.top + ((pf.bottom - pf.top) / 2);
    }
    else
    {
        pf.left = -1;
        pf.top = -1;
        pf.right = -1;
        pf.bottom = -1;
        pf.camera = current_camera;
        xpos = -1;
        ypos = -1;
    }
    
    set_thread_status("sendSpot broadcasting focus");
    server->broadcast(&pf, ROLE_CONTROL);
    set_thread_status("sendSpot broadcasted focus");

    server->objx[current_camera] = xpos;
    server->objy[current_camera] = ypos;
    server->objl[current_camera] = pf.left;
    server->objr[current_camera] = pf.right;
}

void BrainThread::run()
{
    registerThread("Brain");
    while(1)
    {
        doBrain();
        usleep(100000);
    }
}

void BrainThread::doBrain()
{
    setStatus("Locking AI mutex");
    QMutexLocker qml(&server->ai_mutex);

    int xpos,ypos;

    printf(">>>> %d %d %d %d\n", server->objx[0],
           server->objy[0], server->objx[1],
           server->objy[1]);
    
    bool do_stuff = false;
    
    PacketTargetLocation ptl;
    
    if (server->objx[0] == -1 && !server->teststereo)
    {
        if (server->objx[1] == -1 && !server->teststereo)
        {
            xpos = -1;
            ypos = -1;
        }
        else
        {
            xpos = server->objx[1];
            ypos = server->objy[1];
        }
    }
    else
    {
        if (server->objx[1] == -1 && !server->teststereo)
        {
            xpos = server->objx[0];
            ypos = server->objy[0];
        }
        else
        {
                // Have stereo spot
            xpos = (server->objx[0] + server->objx[1]) / 2;
            ypos = (server->objy[0] + server->objy[1]) / 2;

            double leftoffset,rightoffset,doffset1,doffset2;
            
            if (server->teststereo)
            {
                server->objl[0] = server->tx1;
                server->objl[1] = server->tx2;
                server->objr[0] = server->tx3;
                server->objr[1] = server->tx4;
            }
            
            double la1,ra1,la2,ra2;

            
            calcOffset(server->objx[0], server->objx[1], leftoffset, doffset1,
                       la1, ra1);
            calcOffset(server->objx[0], server->objx[1], rightoffset, doffset2,
                       la2, ra2);

                /*
            calcOffset(server->objl[0], server->objl[1], leftoffset, doffset1,
                       la1, ra1);
            calcOffset(server->objr[0], server->objr[1], rightoffset, doffset2,
                       la2, ra2);
                */
            
            ptl.leftangle1 = la1;
            ptl.rightangle1 = ra1;
            ptl.leftangle2 = la2;
            ptl.rightangle2 = ra2;
            
            double width = rightoffset - leftoffset;
            if (width < 1)
                width = 120;
            
            printf(">> left %f right %f\n", leftoffset, rightoffset);
            
            xes.enqueue( leftoffset /*(rightoffset-leftoffset) / 2*/);
            yes.enqueue( (doffset1+doffset2) / 2);
            widths.enqueue(width);
            
            double xoffset = 0;
            double yoffset = 0;
            width = 0;
            
            for (int loopc=0;loopc<xes.size();loopc++)
            {
                xoffset += xes.at(loopc);
                yoffset += yes.at(loopc);
                width += widths.at(loopc);
            }

            xoffset /= xes.size();
            yoffset /= yes.size();
            width /= widths.size();

            while (xes.size() > server->debounce_queue_size)
            {
                xes.dequeue();
                yes.dequeue();
                widths.dequeue();
            }
            
            text = QString("X ")+QString::number((int)xoffset)+
                QString(" Y ") + QString::number((int)yoffset)+
                QString(" W ") + QString::number((int)width);

            ptl.xoff = xoffset;
            ptl.yoff = yoffset;
            ptl.width = width;

            do_stuff = true;
        }
    }
    
    ptl.present = do_stuff;
    ptl.r = server->red;
    ptl.g = server->green;
    ptl.b = server->blue;
    
    if (do_stuff && server->ai_active)
    {
        time_t t = time(0);
        if (t - last_say > 10)
        {
            char buf[4096];
            sprintf(buf,"echo \"Range %d centimeters\" | festival --tts",
                    (int)(ptl.yoff / 10));
            ::system(buf);
            
            last_say = t;
        }
        
        if (ptl.xoff < -25)
        {
            printf("Left\n");
            ptl.text = "Left";
            server->setMotor(0, -server->ai_speed);
            server->setMotor(1, server->ai_speed);
        }
        else if (ptl.xoff > 25)
        {
            printf("Right\n");
            ptl.text = "Right";
            server->setMotor(0, server->ai_speed);
            server->setMotor(1, -server->ai_speed);
        }
        else if (ptl.yoff > 300)
        {
            printf("Forward\n");
            ptl.text = "Forward";
            server->setMotor(0, server->ai_speed);
            server->setMotor(1, server->ai_speed);
        }
        else if (ptl.yoff < 100)
        {
            printf("Backward\n");
            ptl.text = "Backward";
            server->setMotor(0, -server->ai_speed);
            server->setMotor(1, -server->ai_speed);
        }
        else
        {
            server->setMotor(0, 0);
            server->setMotor(1, 0);
        }
    }
    else
    {
        server->setMotor(0, 0);
        server->setMotor(1, 0);
    }
    
    
    server->broadcast(&ptl, ROLE_CONTROL);
            
    server->sendStatuses();
    setStatus("Dropping out of doBrain");
}

void BrainThread::calcOffset(int leftx, int rightx, double &
                             xoffset, double & yoffset,
                             double & langle, double & rangle)
{
    double angles[2];

    int sp[2];
    sp[0] = leftx;
    sp[1] = rightx;
            
    for (int loopc=0; loopc<2; loopc++)
    {
        double p = sp[loopc];
        p -= (CAM_WIDTH / 2);
        p *= (EYE_FOV / CAM_WIDTH);
        angles[loopc] = p;
    };
    
    langle = angles[0] + 90;
    rangle = angles[1] + 90;
    
    angles[0] = 90 - angles[0];
    angles[1] = 90 + angles[1];
    
    double bangle = angles[0];
    
    double third_angle = 180.0 - (angles[0] + angles[1]);
    
    printf("Angles are left %f right %f end %f\n", angles[0],
           angles[1], third_angle);
    
    angles[0] = angles[0] * (3.142 / 180.0);
    angles[1] = angles[1] * (3.142 / 180.0);
    third_angle = third_angle * (3.142 / 180.0);

    double val = sin(third_angle) / EYE_DISTANCE;
    double distance1 = sin(angles[0]) / val;
    double distance2 = sin(angles[1]) / val;

        //printf("%f is %f, %f is %f %f (%f %f)\n", angles[0],
        // sin(angles[0]), angles[1], sin(angles[1]),
        //  val, sin(third_angle), EYE_DISTANCE);
            
    double distance = (distance1 + distance2) / 2;
            
    printf("Distance %f %f %f\n", distance1, distance2, distance);

        /*
    if (bangle > 90)
    {
        bangle = 180 - bangle;
    }
        */
    
    bangle = bangle * (3.142 / 180.0);
    yoffset = sin(bangle) * distance1;
    xoffset = (cos(bangle) * distance1);

    xoffset -= EYE_DISTANCE / 2;
}
