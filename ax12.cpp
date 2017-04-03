#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "ax12.h"

int dynamixel_fd = -1;
bool demo_mode = false;

void ax12Init(long baud)
{
    if (getenv("DEMO_MODE"))
        demo_mode = true;
    
    dynamixel_fd = open("/dev/ttyUSB0", O_RDWR);
    if (dynamixel_fd == -1)
    {
        dynamixel_fd = open("/dev/ttyUSB1", O_RDWR);
    }

    if (dynamixel_fd == -1)
    {
        printf("Can't open Dynamixel\n");
        if (!demo_mode)
            exit(0);
    }
    
}


/* Set the value of a single-byte register. */
void ax12SetRegister(int id, int regstart, int data)
{
    if (demo_mode)
        return;
    
    char buf[20];
    int checksum = ~((id + 4 + AX_WRITE_DATA + regstart + (data&0xff)) % 256);

    buf[0] = 0xff;
    buf[1] = 0xff;
    buf[2] = id;
    buf[3] = 0x4;
    buf[4] = AX_WRITE_DATA;
    buf[5] = regstart;
    buf[6] = data & 0xff;
    buf[7] = checksum;

    write(dynamixel_fd, buf, 8);
}
/* Set the value of a double-byte register. */
void ax12SetRegister2(int id, int regstart, int data)
{
    if (demo_mode)
        return;
    
    char buf[20];
    
    int checksum = ~((id + 5 + AX_WRITE_DATA + regstart + (data&0xFF) + ((data&0xFF00)>>8)) % 256);
    
    buf[0] = 0xff;
    buf[1] = 0xff;
    buf[2] = id;
    buf[3] = 0x5;
    buf[4] = AX_WRITE_DATA;
    buf[5] = regstart;
    buf[6] = data & 0xff;
    buf[7] = (data & 0xff00) >> 8;
    buf[8] = checksum;
    
    write(dynamixel_fd, buf, 9);
}
