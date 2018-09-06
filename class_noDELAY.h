//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// класс титановый велосипед для delay без delay().
// динамически управляемая генерация событий с динамически изменяемым интервалом времени между событиями.

#include <Arduino.h>

class noDELAY {

  public:
    unsigned long previous;
    unsigned long interval;
    unsigned long current;
    boolean s;
    boolean tick;

    noDELAY() {
      //previous = micros();
       previous = millis();
      s        =        0;
      tick     =        0;
    }

    void  stop()                        {
      s =  0;
    }
    void start()                        {
      s =  1;
    }
    void restart()                        {
      previous = current;
    }


    void  read(unsigned long _interval) {
      interval = _interval;
      //unsigned long current  =  micros();                      tick = 0;
       current  =  millis();                      tick = 0;
      if (s                  ==        0) {
        previous = current;
      }
      if (current - previous >  interval) {
        previous = current;
        tick = 1;
      }
    }

};
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
