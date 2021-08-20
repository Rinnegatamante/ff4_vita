#ifndef __CONFIG_H__
#define __CONFIG_H__

#define LOAD_ADDRESS 0x98000000

#define MEMORY_NEWLIB_MB 280
#define MEMORY_VITAGL_THRESHOLD_MB 8
#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_SAMPLES_PER_BUF 8192

#define DATA_PATH "ux0:data/ff4"
#define SO_PATH DATA_PATH "/" "libff4.so"

#define SCREEN_W 960
#define SCREEN_H 544

#endif
