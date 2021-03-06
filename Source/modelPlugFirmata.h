#ifndef MODELPLUG_H
#define MODELPLUG_H

/*
#if defined(_MSC_VER)
    //  Microsoft VC++
    #define EXPORT __declspec(dllexport)
#else
    //  GCC
    #define EXPORT __attribute__((visibility("default")))
#endif
*/
#define EXPORT __declspec(dllexport) __stdcall


#ifdef _cplusplus
extern "C" {
#endif

EXPORT void* boardConstructor(char* port,unsigned char showCapabilitites,int samplingMs,int baudRate,unsigned char dtr);
EXPORT void boardDestructor(void* object);

EXPORT void updateBoard(int id);
EXPORT int getBoardId(void* object);


EXPORT double readAnalogPin  (int pin, double min, double max, double init, int id);
EXPORT int    readDigitalPin (int pin, int init, int id);
EXPORT void   writeAnalogPin (int pin, int id,double value);
EXPORT void   writeDigitalPin(int pin, int id,int value);
EXPORT void   writeServoPin  (int pin, int id,double value, int min, int max);

#ifdef _cplusplus
}
#endif

#endif