#include "bridge.h"
#include <limits.h>
#include <math.h>
#include <vitasdk.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stb_image.h"
#include "stb_truetype.h"

#include "zlib.h"
#include <unicode/ucnv.h>
#include <unicode/ustring.h>
#include <vitaGL.h>

#include "config.h"
#include "dialog.h"

#include "shaders/movie_f.h"
#include "shaders/movie_v.h"

#define SAVE_FILENAME "ux0:/data/ff4"
#define OBB_FILE "ux0:/data/ff4/main.obb"
#define SAVE_FILE "ux0:data/ff4/save.bin"

#define FB_ALIGNMENT 0x40000
#define ALIGN_MEM(x, align) (((x) + ((align) - 1)) & ~((align) - 1))

extern float postfx_pos[8];
extern float postfx_texcoord[8];
extern uint8_t in_movie;

SceAvPlayerHandle movie_player;

GLuint movie_frame[2];
uint8_t movie_frame_idx = 0;
SceGxmTexture *movie_tex[2];
GLuint movie_fs;
GLuint movie_vs;
GLuint movie_prog;

SceUID audio_thid;
int audio_new;
int audio_port;
int audio_len;
int audio_freq;
int audio_mode;

enum {
  PLAYER_INACTIVE,
  PLAYER_ACTIVE,
  PLAYER_STOP,
};

int player_state = PLAYER_INACTIVE;

void *mem_alloc(void *p, uint32_t align, uint32_t size) {
  return memalign(align, size);
}

void mem_free(void *p, void *ptr) {
  free(ptr);
}

void *gpu_alloc(void *p, uint32_t align, uint32_t size) {
  if (align < FB_ALIGNMENT) {
    align = FB_ALIGNMENT;
  }
  size = ALIGN_MEM(size, align);
  return vglAlloc(size, VGL_MEM_SLOW);
}

void gpu_free(void *p, void *ptr) {
  glFinish();
  vglFree(ptr);
}

void movie_player_audio_init(void) {
  audio_port = -1;
  for (int i = 0; i < 8; i++) {
    if (sceAudioOutGetConfig(i, SCE_AUDIO_OUT_CONFIG_TYPE_LEN) >= 0) {
      audio_port = i;
      break;
    }
  }

  if (audio_port == -1) {
    audio_port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, 1024, 48000, SCE_AUDIO_OUT_MODE_STEREO);
    audio_new = 1;
  } else {
    audio_len = sceAudioOutGetConfig(audio_port, SCE_AUDIO_OUT_CONFIG_TYPE_LEN);
    audio_freq = sceAudioOutGetConfig(audio_port, SCE_AUDIO_OUT_CONFIG_TYPE_FREQ);
    audio_mode = sceAudioOutGetConfig(audio_port, SCE_AUDIO_OUT_CONFIG_TYPE_MODE);
    audio_new = 0;
  }
}

void movie_player_audio_shutdown(void) {
  if (audio_new) {
    sceAudioOutReleasePort(audio_port);
  } else {
    sceAudioOutSetConfig(audio_port, audio_len, audio_freq, (SceAudioOutMode)audio_mode);
  }
}

int movie_player_audio_thread(SceSize args, void *argp) {
  SceAvPlayerFrameInfo frame;
  memset(&frame, 0, sizeof(SceAvPlayerFrameInfo));

  while (player_state == PLAYER_ACTIVE && sceAvPlayerIsActive(movie_player)) {
    if (sceAvPlayerGetAudioData(movie_player, &frame)) {
      sceAudioOutSetConfig(audio_port, 1024, frame.details.audio.sampleRate, frame.details.audio.channelCount == 1 ? SCE_AUDIO_OUT_MODE_MONO : SCE_AUDIO_OUT_MODE_STEREO);
      sceAudioOutOutput(audio_port, frame.pData);
    } else {
      sceKernelDelayThread(1000);
    }
  }

  return sceKernelExitDeleteThread(0);
}

int movie_first_frame_drawn;
void movie_player_draw(void) {
  if (player_state == PLAYER_ACTIVE) {
    if (sceAvPlayerIsActive(movie_player)) {
      SceAvPlayerFrameInfo frame;
      if (sceAvPlayerGetVideoData(movie_player, &frame)) {
        movie_first_frame_drawn = 1;
        movie_frame_idx = (movie_frame_idx + 1) % 2;
        sceGxmTextureInitLinear(
          movie_tex[movie_frame_idx],
          frame.pData,
          SCE_GXM_TEXTURE_FORMAT_YVU420P2_CSC1,
          frame.details.video.width,
          frame.details.video.height, 0);
        sceGxmTextureSetMinFilter(movie_tex[movie_frame_idx], SCE_GXM_TEXTURE_FILTER_LINEAR);
        sceGxmTextureSetMagFilter(movie_tex[movie_frame_idx], SCE_GXM_TEXTURE_FILTER_LINEAR);
      }
      if (movie_first_frame_drawn) {
        glUseProgram(movie_prog);
        glBindTexture(GL_TEXTURE_2D, movie_frame[movie_frame_idx]);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, &postfx_pos[0]);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, &postfx_texcoord[0]);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glUseProgram(0);
      }
    } else {
      player_state = PLAYER_STOP;
    }
  }

  if (player_state == PLAYER_STOP) {
    sceAvPlayerStop(movie_player);
    sceKernelWaitThreadEnd(audio_thid, NULL, NULL);
    sceAvPlayerClose(movie_player);
    movie_player_audio_shutdown();
    player_state = PLAYER_INACTIVE;
    glClear(GL_COLOR_BUFFER_BIT);
  }
}

int movie_player_inited = 0;
void movie_player_init() {
  if (movie_player_inited)
    return;

  sceSysmoduleLoadModule(SCE_SYSMODULE_AVPLAYER);

  glGenTextures(2, movie_frame);
  for (int i = 0; i < 2; i++) {
    glBindTexture(GL_TEXTURE_2D, movie_frame[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 960, 544, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    movie_tex[i] = vglGetGxmTexture(GL_TEXTURE_2D);
    vglFree(vglGetTexDataPointer(GL_TEXTURE_2D));
  }

  movie_vs = glCreateShader(GL_VERTEX_SHADER);
  glShaderBinary(1, &movie_vs, 0, movie_v, size_movie_v);

  movie_fs = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderBinary(1, &movie_fs, 0, movie_f, size_movie_f);

  movie_prog = glCreateProgram();
  glAttachShader(movie_prog, movie_vs);
  glAttachShader(movie_prog, movie_fs);
  glBindAttribLocation(movie_prog, 0, "inPos");
  glBindAttribLocation(movie_prog, 1, "inTex");
  glLinkProgram(movie_prog);
	
  movie_player_inited = 1;
}


void playMovie() {
  SceIoStat st;
  if (sceIoGetstat("ux0:data/ff4/opening.mp4", &st) < 0)
    return;

  movie_player_init();
  movie_player_audio_init();

  SceAvPlayerInitData playerInit;
  sceClibMemset(&playerInit, 0, sizeof(SceAvPlayerInitData));

  playerInit.memoryReplacement.allocate = mem_alloc;
  playerInit.memoryReplacement.deallocate = mem_free;
  playerInit.memoryReplacement.allocateTexture = gpu_alloc;
  playerInit.memoryReplacement.deallocateTexture = gpu_free;

  playerInit.basePriority = 0xA0;
  playerInit.numOutputVideoFrameBuffers = 2;
  playerInit.autoStart = GL_TRUE;

  movie_player = sceAvPlayerInit(&playerInit);

  sceAvPlayerAddSource(movie_player, "ux0:data/ff4/opening.mp4");

  audio_thid = sceKernelCreateThread("movie_audio_thread", movie_player_audio_thread, 0x10000100 - 10, 0x4000, 0, 0, NULL);
  sceKernelStartThread(audio_thid, 0, NULL);

  player_state = PLAYER_ACTIVE;
  movie_first_frame_drawn = 0;
}

void stopMovie() {
  player_state = PLAYER_STOP;
}

uint8_t getMovieState() {
  movie_player_draw();
  if (!(player_state == PLAYER_ACTIVE && sceAvPlayerIsActive(movie_player)))
    return 0;
  return 1;
}

unsigned char *header = NULL;
int header_length = 0;

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
  inflate(&infstream, Z_FULL_FLUSH);
  inflateEnd(&infstream);

  *bArr_length = readInt;
  return bArr2;
}

unsigned char *m476a(char *str, int *file_length) {
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
    return NULL;
  }

  const char *a = OBB_FILE;
  FILE *fp = fopen(a, "r");
  int a3 = getInt(header, i);
  fseek(fp, a3, SEEK_SET);

  *file_length = getInt(header, i + 4);
  unsigned char *bArr2 = malloc(*file_length);

  for (int i7 = 0; i7 < *file_length;
       i7 += fread(&bArr2[i7], sizeof(unsigned char), *file_length - i7, fp)) {
  }

  fclose(fp);

  decodeArray(bArr2, *file_length, a3 + 419430400u);

  unsigned char *a4 = gzipRead(bArr2, file_length);

  free(bArr2);

  return a4;
}

uint8_t isFileExist(char *str) {
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
    return 0;
  }
  
  return 1;
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

  decodeArray(bArr, 16, 419430400u);

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

    decodeArray(header, header_length, a2 + 419430400u);

    unsigned char *header2 = gzipRead(header, &header_length);

    free(header);

    header = header2;

    fclose(fp);
  }
  return 1;
}

unsigned char *decodeString(unsigned char *bArr, int *bArr_length) {
  return bArr;
}

jni_bytearray *loadFile(char *str) {
  //printf("loadFile(%s)\n", str);
  char *lang[] = {"ja", "en", "fr", "de", "it", "es", "zh_CN", "zh_TW", "ko", "pt_BR", "ru", "th"};

  char *substring = strrchr(str, 46);

  substring = substring == NULL ? str : substring;
  char temp_path[512];
  int file_length;
  sprintf(temp_path, "%s.lproj/%s", lang[getCurrentLanguage()], str);
  unsigned char *a = m476a(temp_path, &file_length);
  if (a == NULL) {
    sprintf(temp_path, "files/%s", str);
    a = m476a(temp_path, &file_length);
  }
  if (a == NULL) {
    return NULL;
  }

  jni_bytearray *result = malloc(sizeof(jni_bytearray));
  result->elements = a;
  result->size = file_length;

  if (strcmp(substring, ".msd") || str[0] == 'e')
    return result;
  if (str[0] == 'n' && str[1] == 'o' && str[2] == 'a')
    return result;

  unsigned char *b = decodeString(a, &file_length);

  if (b != a)
    free(a);

  result->elements = b;
  result->size = file_length;

  return result;
}

jni_bytearray *loadRawFile(char *str) {

  int file_length;
  unsigned char *a = m476a(str, &file_length);

  if (a == NULL) {
    return NULL;
  }

  jni_bytearray *result = malloc(sizeof(jni_bytearray));
  result->elements = a;
  result->size = file_length;

  return result;
}

jni_bytearray *loadSound(char *str) {

  char str2[128], path[256];
  int file_length;
  if (strlen(str) == 0 || !strstr(str, "voice/")) {
    sprintf(str2, "%s.akb", str);
  } else {
    if (options.redub)
      sprintf(str2, "ja%s", &str[8]);
    else
      sprintf(str2, "%s", &str[6]);
  }
  
  sprintf(path, "files/SOUND/BGM/%s", str2);
  unsigned char *a = m476a(path, &file_length);
  if (a == NULL) {
    sprintf(path, "files/SOUND/SE/%s", str2);
    a = m476a(path, &file_length);
    if (a == NULL) {
      sprintf(path, "files/SOUND/VOICE/%s", str2);
      a = m476a(path, &file_length);
      if (a == NULL) {
        return NULL;
      }
    }
  }

  jni_bytearray *result = malloc(sizeof(jni_bytearray));
  result->elements = a;
  result->size = file_length;

  return result;
}

uint8_t isSoundFileExist(char *str) {
  char str2[128], path[256];
  if (strlen(str) == 0 || !strstr(str, "voice/")) {
    sprintf(str2, "%s.akb", str);
  } else {
    if (options.redub)
      sprintf(str2, "ja%s", &str[8]);
    else
      sprintf(str2, "%s", &str[6]);
  }
  
  sprintf(path, "files/SOUND/BGM/%s", str2);
  uint8_t res = isFileExist(path);
  if (!res) {
    sprintf(path, "files/SOUND/SE/%s", str2);
    res = isFileExist(path);
    if (!res) {
      sprintf(path, "files/SOUND/VOICE/%s", str2);
      res = isFileExist(path);
    }
  }

  return res;
}

jni_bytearray *getSaveFileName() {

  char *buffer = SAVE_FILENAME;
  jni_bytearray *result = malloc(sizeof(jni_bytearray));
  result->elements = malloc(strlen(buffer) + 1);
  // Sets the value
  strcpy((char *)result->elements, buffer);
  result->size = strlen(buffer) + 1;

  return result;
}

jni_bytearray *getSaveDataPath() {

  char *buffer = SAVE_FILE;
  jni_bytearray *result = malloc(sizeof(jni_bytearray));
  result->elements = malloc(strlen(buffer) + 1);
  // Sets the value
  strcpy((char *)result->elements, buffer);
  result->size = strlen(buffer) + 1;

  return result;
}

void createSaveFile(size_t size) {
  char *buffer = malloc(size);
  FILE *fd = fopen(SAVE_FILENAME "/save.bin", "wb");
  fwrite(buffer, sizeof(char), size, fd);
  fclose(fd);
  free(buffer);
}

uint64_t j3 = 0;
int32_t framerate = 30;

void setFPS(int32_t i) {
  if (options.battle_fps && i == 15) {
    framerate = options.battle_fps;
  } else {
    framerate = i;
  }
}

uint64_t getCurrentFrame(uint64_t j) {

  while (1) {
    uint64_t j2 = sceKernelGetProcessTimeWide() * framerate / 1000000;
    if (j2 != j3) {
      j3 = j2;
      return j2;
    }
    sceKernelDelayThread(1000);
  }
}

#define RGBA8(r, g, b, a)                                                      \
  ((((a)&0xFF) << 24) | (((b)&0xFF) << 16) | (((g)&0xFF) << 8) |               \
   (((r)&0xFF) << 0))

jni_intarray *loadTexture(jni_bytearray *bArr) {
  //printf("loadTexture(%X)\n", bArr);
  jni_intarray *texture = malloc(sizeof(jni_intarray));

  int x, y, channels_in_file;
  unsigned char *temp = stbi_load_from_memory(bArr->elements, bArr->size, &x,
                                              &y, &channels_in_file, 4);

  texture->size = x * y + 2;
  texture->elements = malloc(texture->size * sizeof(int));
  texture->elements[0] = x;
  texture->elements[1] = y;

  for (int n = 0; n < y; n++) {
    for (int m = 0; m < x; m++) {
      unsigned char *color = (unsigned char *)&(((uint32_t *)temp)[n * x + m]);
      texture->elements[2 + n * x + m] =
          RGBA8(color[2], color[1], color[0], color[3]);
    }
  }

  free(temp);

  return texture;
}

int isDeviceAndroidTV() { return 1; }

stbtt_fontinfo *info = NULL;
unsigned char *fontBuffer = NULL;

void initFont() {

  long size;

  if (info != NULL)
    return;
  
  char font_path[256];
  switch (getCurrentLanguage()) {
  case 6:
  case 7:
    strcpy(font_path, "app0:/NotoSansSC-Regular.ttf");
    break;
  case 8:
    strcpy(font_path, "app0:/NotoSansKR-Regular.ttf");
    break;
  default:
    strcpy(font_path, "app0:/NotoSansJP-Regular.ttf");
    break;
  }
  FILE *fontFile = fopen(font_path, "rb");
  
  // Debug
  if (!fontFile)
    fontFile = fopen("ux0:/data/ff4/NotoSansJP-Regular.ttf", "rb");

  fseek(fontFile, 0, SEEK_END);
  size = ftell(fontFile);       /* how long is the file ? */
  fseek(fontFile, 0, SEEK_SET); /* reset */

  fontBuffer = malloc(size);

  fread(fontBuffer, size, 1, fontFile);
  fclose(fontFile);

  info = malloc(sizeof(stbtt_fontinfo));

  /* prepare font */
  if (!stbtt_InitFont(info, fontBuffer, 0)) {
    printf("failed\n");
  }
}

static inline uint32_t utf8_decode_unsafe_2(const char *data)
{
  uint32_t codepoint;
  codepoint = ((data[0] & 0x1F) << 6);
  codepoint |= (data[1] & 0x3F);
  return codepoint;
}

static inline uint32_t utf8_decode_unsafe_3(const char *data)
{
  uint32_t codepoint;
  codepoint = ((data[0] & 0x0F) << 12);
  codepoint |= (data[1] & 0x3F) << 6;
  codepoint |= (data[2] & 0x3F);
  return codepoint;
}

static inline uint32_t utf8_decode_unsafe_4(const char *data)
{
  uint32_t codepoint;
  codepoint = ((data[0] & 0x07) << 18);
  codepoint |= (data[1] & 0x3F) << 12;
  codepoint |= (data[2] & 0x3F) << 6;
  codepoint |= (data[3] & 0x3F);
  return codepoint;
}

jni_intarray *drawFont(char *word, int size, int i2, int i3) {
  jni_intarray *texture = malloc(sizeof(jni_intarray));
  texture->size = size * size + 5;
  texture->elements = malloc(texture->size * sizeof(int));

  int b_w = size; /* bitmap width */
  int b_h = size; /* bitmap height */

  /* calculate font scaling */
  float scale = stbtt_ScaleForPixelHeight(info, size);

  int ascent, descent, lineGap;
  stbtt_GetFontVMetrics(info, &ascent, &descent, &lineGap);

  int i = 0;
  while (word[i]) {
    i++;
    if (i == 4)
      break;
  }
  
  int codepoint;
  switch (i) {
  case 0: // This should never happen
    codepoint = 32;
    break;
  case 2:
    codepoint = utf8_decode_unsafe_2(word);
    break;
  case 3:
    codepoint = utf8_decode_unsafe_3(word);
    break;
  case 4:
    codepoint = utf8_decode_unsafe_4(word);
    break;
  default:
    codepoint = word[0];
    break;
  }

  int ax;
  int lsb;
  stbtt_GetCodepointHMetrics(info, codepoint, &ax, &lsb);

  if (codepoint == 32) {
    texture->elements[0] = roundf(ax * scale);
    return texture;
  }
  
  /* create a bitmap for the phrase */
  unsigned char *bitmap = calloc(b_w * b_h, sizeof(unsigned char));

  /* get bounding box for character (may be offset to account for chars that dip above or below the line) */
  int c_x1, c_y1, c_x2, c_y2;
  stbtt_GetCodepointBitmapBox(info, codepoint, scale, scale, &c_x1, &c_y1, &c_x2, &c_y2);

  /* compute y (different characters have different heights) */
  int y = roundf(ascent * scale) + c_y1 - (200 * scale);

  /* render character (stride and offset is important here) */
  int byteOffset = roundf(lsb * scale) + (y * b_w);

  stbtt_MakeCodepointBitmap(info, bitmap + byteOffset, c_x2 - c_x1, c_y2 - c_y1, b_w, scale, scale, codepoint);

  texture->elements[0] = (c_x2 - c_x1 + roundf(lsb * scale));
  texture->elements[1] = 0;
  texture->elements[2] = 0;

  for (int n = 0; n < size * size; n++) {
    texture->elements[5 + n] = RGBA8(bitmap[n], bitmap[n], bitmap[n], bitmap[n]);
  }

  free(bitmap);
  return texture;
}

int editText = -1;

void createEditText(char *str) { editText = init_ime_dialog("", str); }

extern GLuint fb;
char *getEditText() {
  char *result = NULL;
  if (!result && editText != -1) {
    result = get_ime_dialog_result();
  }
  if (result) {
    editText = -1;
  }
  return result;
}

int getCurrentLanguage() {
  if (options.lang)
    return (options.lang - 1);

  int lang = -1;
  sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_LANG, &lang);
  switch (lang) {
  case SCE_SYSTEM_PARAM_LANG_JAPANESE:
    return 0;
  case SCE_SYSTEM_PARAM_LANG_FRENCH:
    return 2;
  case SCE_SYSTEM_PARAM_LANG_GERMAN:
    return 3;
  case SCE_SYSTEM_PARAM_LANG_ITALIAN:
    return 4;
  case SCE_SYSTEM_PARAM_LANG_SPANISH:
    return 5;
  case SCE_SYSTEM_PARAM_LANG_CHINESE_S:
    return 6;
  case SCE_SYSTEM_PARAM_LANG_CHINESE_T:
    return 7;
  case SCE_SYSTEM_PARAM_LANG_KOREAN:
    return 8;
  case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR:
  case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT:
    return 9;
  case SCE_SYSTEM_PARAM_LANG_RUSSIAN:
    return 10;
  default:
    return 1;
  }
}
