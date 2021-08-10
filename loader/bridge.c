#include "bridge.h"
#include <limits.h>
#include <psp2/kernel/processmgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zlib.h"

#define SAVE_FILENAME "uxO:/data/ff3"
#define OBB_FILE                                                               \
  "ux0:/data/ff3/main.20004.com.square_enix.android_googleplay.FFIII_GP.obb"

unsigned char *header = NULL;
unsigned int header_length = 0;

void decodeArray(unsigned char *bArr, int size, uint key) {
  for (int n = 0; n < size; n++) {
    key = key * 0x41c64e6d + 0x3039;
    bArr[n] = bArr[n] ^ (unsigned char)(key >> 0x18);
  }
}

static int getInt(unsigned char *bArr, int i) {
  return *(unsigned int *)(&bArr[i]);
}

unsigned char *gzipRead(unsigned char *bArr, int *bArr_length) {
  unsigned int readInt = __builtin_bswap32(getInt(bArr, 0));
  unsigned char *bArr2 = calloc(readInt, sizeof(unsigned char));
  unsigned char *bArr3 = &bArr[4];

  z_stream infstream;
  infstream.zalloc = Z_NULL;
  infstream.zfree = Z_NULL;
  infstream.opaque = Z_NULL;
  // setup "b" as the input and "c" as the compressed output
  infstream.avail_in = *bArr_length - 4; // size of input
  infstream.next_in = bArr3;             // input char array
  infstream.avail_out = readInt;         // size of output
  infstream.next_out = bArr2;            // output char array

  // the actual DE-compression work.
  inflateInit2(&infstream, MAX_WBITS | 16);
  int ret = inflate(&infstream, Z_FULL_FLUSH);
  inflateEnd(&infstream);

  *bArr_length = readInt;
  return bArr2;
}

unsigned char *m476a(char *str, int* file_length) {
  printf("READ %s \n", str);
  int i;

  unsigned char *bArr = header;
  if (bArr != NULL) {
    int a = getInt(bArr, 0);
    int i2 = 0;
    i = 0;
    while (a > i2) {
      int i3 = (i2 + a) / 2;
      int i4 = i3 * 12;
      int a2 = getInt(header, i4 + 4);
      int i5 = 0;
      for (int i6 = 0; i6 < strlen(str) && i5 == 0; i6++) {
        i5 = (header[a2 + i6] & 0xFF) - (str[i6] & 0xFF);
      }
      if (i5 == 0) {
        i5 = header[a2 + strlen(str)] & 0xFF;
      }
      if (i5 == 0) {
        i = i4 + 8;
        a = i3;
        i2 = a;
      } else if (i5 > 0) {
        a = i3;
      } else {
        i2 = i3 + 1;
      }
    }
  } else {
    i = 0;
  }
  if (i == 0) {
    printf("NOT FOUND \n");
    return NULL;
  }

  printf("FOUND \n");

  const char *a = OBB_FILE;
  FILE *fp = fopen(a, "r");
  int a3 = getInt(header, i + 0);
  fseek(fp, a3, SEEK_SET);

  *file_length = getInt(header, i + 4);
  unsigned char *bArr2 = malloc(*file_length);

  for (int i7 = 0; i7 < *file_length;
       i7 += fread(&bArr2[i7], sizeof(unsigned char), *file_length - i7, fp)) {
  }

  decodeArray(bArr2, *file_length, a3 + 84861466u);

  unsigned char *a4 = gzipRead(bArr2, file_length);

  free(bArr2);

  return a4;
}



int readHeader() {
  const char *a = OBB_FILE;
  FILE *fp = fopen(a, "r");
  fseek(fp, 0L, SEEK_END);
  int length = ftell(fp);
  fseek(fp, 0L, SEEK_SET);

  unsigned char bArr[16];

  for (int i = 0; i < 16;
       i += fread(&bArr[i], sizeof(unsigned char), 16 - i, fp)) {
  }

  decodeArray(bArr, 16, 84861466u);

  if (getInt(bArr, 0) != 826495553) {
    printf("initFileTable: Header Error\n");
    return 0;
  } else if (length != getInt(bArr, 4)) {
    printf("initFileTable: Size Error\n");
    return 0;
  } else {
    unsigned int a2 = getInt(bArr, 8);
    header_length = getInt(bArr, 12);
    header = malloc(header_length);

    fseek(fp, (long)(a2 - 16), SEEK_CUR);

    for (int i = 0; i < header_length;
         i += fread(&header[i], sizeof(unsigned char), header_length - i, fp)) {
    }

    decodeArray(header, header_length, a2 + 84861466u);

    /*        FILE *f = fopen("ux0:/data/header.data", "wb");
            fwrite(header, sizeof(unsigned char), header_length, f);
            fclose(f);*/

    unsigned char *header2 = gzipRead(header, &header_length);

    free(header);

    header = header2;

    fclose(fp);
  }
  return 1;
}

unsigned char *decodeString(unsigned char *bArr, int *bArr_length) {
    /*FILE *f = fopen("ux0:/data/decodeString.data", "wb");
                fwrite(bArr, sizeof(unsigned char), *bArr_length, f);
                fclose(f);
                exit(1);*/



  int i = 5; // TODO
  if (i >= 6) {
    return bArr;
  }

  unsigned char *bArr3 = malloc(*bArr_length * 2);
  int i4 = (bArr[8] & 0xFF) | ((bArr[9] & 0xFF) << 8) |
           ((bArr[10] & 0xFF) << 16) | ((bArr[11] & 0xFF) << 24);
  int i5 = (i4 * 12) + 16;

  memcpy(bArr3, bArr, i5);

  // System.arraycopy(bArr, 0, bArr3, 0, i5);
  int i6 = i5;
  int i7 = 0;
  int i8 = 16;
  int i9 = 16;

  while (i7 < i4) {
    int i10 = i8 + 8;
    bArr3[i10 + 0] = (unsigned char)(i6 >> 0);
    bArr3[i10 + 1] = (unsigned char)(i6 >> 8);
    bArr3[i10 + 2] = (unsigned char)(i6 >> 16);
    bArr3[i10 + 3] = (unsigned char)(i6 >> 24);
    unsigned char b = bArr[i9 + 4];
    int i11 = i9 + 8;
    int i12 = ((bArr[i11 + 3] & 0xFF) << 24) | (bArr[i11 + 0] & 0xFF) |
              ((bArr[i11 + 1] & 0xFF) << 8) | ((bArr[i11 + 2] & 0xFF) << 16);
    int i13 = i6;
    for (int i14 = 0; i14 < b; i14++) {
      int i15 = 0;
      while (bArr[i12 + i15] != 0) {
        i15++;
      }

      memcpy(&bArr3[i13], &bArr[i12], i15);

      bArr3[i15 + i13] = 0;
      i12 += i15 + 1;
      i13 += i15 + 1;
    }
    i6 = i13 + 1;
    bArr3[i13] = 0;
    i9 += 12;
    i8 += 12;
    i7++;
  }
 
  unsigned char *bArr4 = malloc(i6);
  memcpy(bArr4, bArr3, i6);
  // System.arraycopy(bArr3, 0, bArr4, 0, i6);
  free(bArr3);
  *bArr_length = i6;
  return bArr4;
}

jni_array *loadFile(char *str) {
  char *lang[] = {"ja", "en",    "fr",    "de", "it",
                  "es", "zh_CN", "zh_TW", "ko", "th"};

  char *substring = strrchr(str, 46);

  substring = substring == NULL? str : substring;
  char temp_path[512];
  int file_length;
  sprintf(temp_path, "%s.lproj/%s", lang[5], str);
  unsigned char *a = m476a(temp_path, &file_length);
  if (a == NULL) {
    sprintf(temp_path, "files/%s", str);
    a = m476a(temp_path, &file_length);
  }
  if (a == NULL) {
    return NULL;
  }

  jni_array *result = malloc(sizeof(jni_array));
  result->elements = a;
  result->size = file_length;

  if (strcmp(substring, ".msd") || str[0] == 'e')
    return result;
  if (str[0] == 'n' && str[1] == 'o' && str[2] == 'a')
    return result;

  unsigned char *b = decodeString(a, &file_length);

  free(a);

  result->elements = b;
  result->size = file_length;

  return result;
}

jni_array * getSaveFileName(){

  char * buffer = SAVE_FILENAME;
  jni_array *result = malloc(sizeof(jni_array));
  result->elements = malloc(strlen(buffer) + 1);
  // Sets the value
  strcpy(result->elements, buffer);
  result->size = strlen(buffer) + 1;

  return result;
}

long getCurrentFrame(long j) {
  SceKernelSysClock ticks;
  sceKernelGetProcessTime(&ticks);
  while (1) {
    long j2 = ticks * 1000 * 3 / 100;
    if (j2 != j) {
      return j2;
    }
    sceKernelDelayThread(10);
    sceKernelGetProcessTime(&ticks);
  }
}
