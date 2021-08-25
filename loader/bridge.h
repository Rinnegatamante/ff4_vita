#ifndef __BRIDGE_H__
#define __BRIDGE_H__

#include <stddef.h>
#include <stdint.h>

typedef struct {
  unsigned char *elements;
  int size;
} jni_bytearray;

jni_bytearray *loadFile(char *str);
jni_bytearray *loadSound(char *str);
jni_bytearray *loadRawFile(char *str);
jni_bytearray *getSaveFileName();
jni_bytearray *getSaveDataPath();
uint8_t isSoundFileExist(char *str);
void setFPS(int32_t i);
void createSaveFile(size_t size);
uint64_t getCurrentFrame(uint64_t j);
int readHeader();

typedef struct {
  int *elements;
  int size;
} jni_intarray;

jni_intarray *loadTexture(jni_bytearray *bArr);
int isDeviceAndroidTV();
jni_intarray *drawFont(char *str, int i, int i2, int i3);

void createEditText(char *str);
char *getEditText();
void initFont();

int getCurrentLanguage();

#endif