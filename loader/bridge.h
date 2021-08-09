#ifndef __BRIDGE_H__
#define __BRIDGE_H__

#include <stdint.h>

typedef struct{
    void * elements;
    int size;
} jni_array;

jni_array * loadFile(char * str);
long getCurrentFrame(long j);
int readHeader();
#endif