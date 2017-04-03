#ifndef _MOTORS_
#define _MOTORS_

class Motor
{
  public:

    virtual bool setMotor(int s)
    {
        if (invert)
        {
            s = -s;
        }

        return setMotorD(s);
    }

    virtual int getMotor()
    {
        int ret = getMotorD();
        if (invert)
        {
            ret = -ret;
        }

        return ret;
    }
    
    virtual bool setMotorD(int) = 0;
    virtual int getMotorD()
    {
        return speed;
    }

    void setInverted(bool i)
    {
        invert = i;
    }

    bool inverted()
    {
        return invert;
    }
    
  protected:

    int speed;
    bool invert;
    
};

class NXTMotor :  public Motor
{
  public:
    
    NXTMotor(int);
    virtual bool setMotorD(int);

  protected:

    int unit;
    
};

class JrkMotor : public Motor
{
  public:
    
    JrkMotor(const char *);
    virtual bool setMotorD(int);

  protected:

    const char * path;
    int fd;
    
};

class MissileMotor : public Motor
{
  public:

    MissileMotor(int);
    virtual bool setMotorD(int);

  protected:

    int send_message(char* msg, int index);
    int unit;
    
};

#endif
