#include "motors.h"

#include "lestat/exceptions.h"
#include "lestat/comms.h"
#include "lestat/usbcomm.h"
#include "lestat/bluecomm.h"
#include "lestat/message.h"
#include "lestat/opcodes.h"

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <usb.h>
#include <string.h>

Comms * comm = 0;
usb_dev_handle * launcher = 0;

bool init_nxt()
{
    comm = new USBComm;

    try
      {
        comm->find();
      }
    catch(...)
      {
        printf("\n\nFailed to bind to NXT\n\n");
        comm = 0;
      }

    return (comm != 0);
}

usb_dev_handle * find_launcher()
{
    struct usb_bus *busses;
    
    usb_init();
    usb_find_busses();
    usb_find_devices();
    
    busses = usb_get_busses();

   	struct usb_bus *bus;
    
    for (bus = busses; bus; bus = bus->next) 
    {
        struct usb_device *dev;
    
        for (dev = bus->devices; dev; dev = dev->next) 
		{
            if (dev->descriptor.idVendor == 0x0a81)
			{
                return usb_open(dev);
            }
        }
    }

    return 0;
}

bool init_launcher()
{
    usb_dev_handle * dev = find_launcher();
    if (dev)
    {
        usb_reset(dev);
    };

    launcher = find_launcher();
    
    if (launcher)
    {

        usb_detach_kernel_driver_np(launcher, 0);

            //do stuff
        usb_set_configuration(launcher, 1);
        int claimed = usb_claim_interface(launcher, 0);
        if (claimed == 0)
        {
            printf("Claimed launcher 0\n");
            usb_set_altinterface(launcher, 0);
        }
        else
        {
            printf("Found launcher but failed to claim 0\n");
            return false;
        }
    }

    printf("Failure to find launcher\n");
    return false;
}


NXTMotor::NXTMotor(int u)
{
    unit = u;
}

bool NXTMotor::setMotorD(int s)
{
    if (!comm)
    {
        if (!init_nxt())
        {
            speed = 0;
            return false;
        } 
    }

    Opcodes op(comm);
    op.setOutputState(unit, s, 0x01, 0x00, 50, s == 0 ? 0x00 : 0x20, 0);
    speed = s;
    return true;
}

JrkMotor::JrkMotor(const char * p)
{
    path = p;
    fd = -1;
}

bool JrkMotor::setMotorD(int s)
{
    char buf[10];
    
    if (fd == -1)
    {
        fd = open(path, O_RDWR);
        if (fd == -1)
        {
            printf("Cannot has\n");
            speed = 0;
            return false;
        }

        buf[0] = 0xaa;
        if (::write(fd, buf, 1) == -1)
        {
            fd = -1;
            speed = 0;
            return false;
        }
    }

    int ret = -1;
    
    if (s == 0)
    {
        buf[0] = 0xff;
        ret = ::write(fd, buf, 1);
    }
    else if (s > 0)
    {
            
        buf[0] = 0xe1;
        buf[1] = s;
        ret = ::write(fd, buf, 2);
    }
    else
    {
        buf[0] = 0xe0;
        buf[1] = -s;
        ret = ::write(fd, buf, 2);
    }
    
    if (ret == -1)
    {
        fd = -1;
        speed = 0;
        return false;
    }
    else
    {
        speed = s;
        return true;
    }
}

MissileMotor::MissileMotor(int u)
{
    unit = u;
}

int MissileMotor::send_message(char* msg, int index)
{
	int j = usb_control_msg(launcher, 0x21, 0x9, 0x200, index, msg, 8, 1000);
    
    memset(msg, 0, 8);
	return j;
}

char move_byte = 0x0;
char old_move_byte = 0xff;

bool MissileMotor::setMotorD(int s)
{
    if (launcher == 0)
    {
        if (!init_launcher())
        {
            speed = 0;
            return false;
        } 
    }

    if (s > -20 && s < 20)
        s = 0;
    
	char msg[8];
    memset(msg, 0, 8);

    move_byte = 0;
    
    if (unit == 0)
    {
        move_byte = move_byte & 0xf3;
            
        if (s == 0)
        {
            move_byte = 0;
        }
        else if (s < 0)
        {
            move_byte = 4;
        }
        else
        {
            move_byte = 8;
        }
    }
    else if (unit == 1)
    {
        move_byte = move_byte & 0xfc;
            
        if (s == 0)
        {
            move_byte = 0;
        }
        else if (s < 0)
        {
            move_byte = 2;
        }
        else
        {
            move_byte = 1;
        }
    }
    else if (unit == 2)
    {
        if (s != 0)
        {
            move_byte = 16;
        }
        else
        {
            move_byte = 0;
        }
    }
    
    printf("Move_byte %d\n", move_byte);

    if (move_byte == old_move_byte)
    {
        speed = s;
        return true;
    }

    old_move_byte = move_byte;
    
	//send 0s
	int deally = send_message(msg, 1);
	//send control
	msg[0] = move_byte;
	deally = send_message(msg, 0);

	//and more zeroes
	deally = send_message(msg, 1);
    
    move_byte = 0;
    
    char ret[8];
    memset(ret, 0, 8);

    printf("Doing interrupt read\n");

        /*
    if (usb_interrupt_read(launcher, 1, ret, 8 ,60000) < 0)
    {
        printf("Failure to read\n");
    }
    else
    {
        printf("[%d][%d][%d][%d][%d][%d][%d][%d]\n",
               ret[0],ret[1],ret[2],ret[3],ret[4],ret[5],
               ret[6],ret[7]);
    }
        */
    
    printf("Now sleeping\n");
    usleep(250000);
    
	//send 0s
	deally = send_message(msg, 1);

	//send control
	msg[0] = move_byte;
	deally = send_message(msg, 0);

	//and more zeroes
	deally = send_message(msg, 1);
    
    speed = s;
    return true;
}

