/*
 * SYNTIANT CONFIDENTIAL
 * _____________________
 *
 *   Copyright (c) 2018 Syntiant Corporation
 *   All Rights Reserved.
 *
 *  NOTICE:  All information contained herein is, and remains the property of
 *  Syntiant Corporation and its suppliers, if any.  The intellectual and
 *  technical concepts contained herein are proprietary to Syntiant Corporation
 *  and its suppliers and may be covered by U.S. and Foreign Patents, patents in
 *  process, and are protected by trade secret or copyright law.  Dissemination
 *  of this information or reproduction of this material is strictly forbidden
 *  unless prior written permission is obtained from Syntiant Corporation.
 */

#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#define JIOPHONE
#ifndef JIOPHONE
#include "../evb/ndp9101_pi/include/ndp9101_pi/ndp9101_pi_i2c_clock.h"
#endif
#include "ndp10x_app_utils.h"
#include "ndp10x_ioctl.h"
#include "syntiant_ilib/ndp10x_spi_regs.h"
#include "syntiant_ilib/syntiant_ndp.h"
#include "syntiant_ilib/syntiant_ndp10x.h"
#include "syntiant_ilib/syntiant_ndp_error.h"

#ifndef JIOPHONE
extern int ndp9101_pi_i2c_clock_init(char* i2c_dev, int i2c_addr);
#endif

#define BYTES_PER_AUDIO_SAMPLE (2)
#define BYTES_PER_MILLISECOND (16000 * BYTES_PER_AUDIO_SAMPLE / 1000)

#define MAX(a, b) ((a) > (b) ? (a) : (b))

typedef struct input_event {
    struct timeval time;
    __u16 type;
    __u16 code;
    __s32 value;
} input_event;

int transfer(int ndpfd, int mcu, uint32_t addr, uint8_t* out, uint8_t* in, int count) {
  struct ndp10x_transfer_s transfer;
  transfer.mcu = mcu;
  transfer.count = count;
  transfer.addr = addr;
  transfer.out = out;
  transfer.in = in;
  if (ioctl(ndpfd, TRANSFER, &transfer) < 0) {
    perror("ndp10x transfer out ioctl failed");
    return 1;
  }

  return 0;
}

void set_config(struct syntiant_ndp10x_config_s* ndp10x_config, int spi) {
#ifdef JIOPHONE
  int pdm_freq = 832000;
  int input_freq = 32768;
  int pdm_ndp = 1;
  int pdm_in_shift = 11;
  int pdm_out_shift = 7;
  int pdm_dc_offset = 0;
  int power_offset = 63;
  int mic = 0;
#else
  int pdm_freq = 1600000;
  int input_freq = 16000000;
  int pdm_ndp = 1;
  int pdm_in_shift = 7;
  int pdm_out_shift = 7;
  int pdm_dc_offset = 0;
  int power_offset = 52;
  int mic = 0;
#endif
  int i;

  memset(ndp10x_config, 0, sizeof(struct syntiant_ndp10x_config_s));
  ndp10x_config->set =
      SYNTIANT_NDP10X_CONFIG_SET_INPUT_CLOCK_RATE | SYNTIANT_NDP10X_CONFIG_SET_PDM_CLOCK_RATE |
      SYNTIANT_NDP10X_CONFIG_SET_PDM_CLOCK_NDP | SYNTIANT_NDP10X_CONFIG_SET_PDM_OUT_SHIFT |
      SYNTIANT_NDP10X_CONFIG_SET_PDM_IN_SHIFT | SYNTIANT_NDP10X_CONFIG_SET_PDM_DC_OFFSET |
      SYNTIANT_NDP10X_CONFIG_SET_POWER_OFFSET | SYNTIANT_NDP10X_CONFIG_SET_DNN_INPUT |
      SYNTIANT_NDP10X_CONFIG_SET_TANK_INPUT | SYNTIANT_NDP10X_CONFIG_SET_TANK_SIZE;
  ndp10x_config->input_clock_rate = input_freq;
  ndp10x_config->pdm_clock_rate = pdm_freq;
  ndp10x_config->pdm_clock_ndp = pdm_ndp;
  for (i = 0; i < 2; i++) {
    ndp10x_config->pdm_in_shift[i] = pdm_in_shift;
    ndp10x_config->pdm_out_shift[i] = pdm_out_shift;
    ndp10x_config->pdm_dc_offset[i] = pdm_dc_offset;
  }
  ndp10x_config->power_offset = power_offset;
  if (spi) {
    ndp10x_config->dnn_input = SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI;
    ndp10x_config->tank_input = SYNTIANT_NDP10X_CONFIG_TANK_INPUT_SPI;
  } else {
    ndp10x_config->dnn_input =
        mic ? SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM1 : SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM0;
    ndp10x_config->tank_input =
        mic ? SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM1 : SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM0;
  }
  ndp10x_config->tank_size = 64 * 1024;
}

void load_pkg(int ndpfd, char* path) {
  struct ndp10x_load_s load;
  struct stat file_state;
  int pfd, r1;
  void* pkg;
  int s = SYNTIANT_NDP_ERROR_NONE;
  uint32_t chunk_size;
  uint8_t* begin_ptr;
  uint8_t* end_ptr;

  if (stat(path, &file_state) < 0) {
    perror("unable to stat synpkg file");
    exit(1);
  }
  printf("package: %s\n", path);
  printf("file Size: %lld bytes\n", file_state.st_size);

  pkg = malloc(file_state.st_size);
  pfd = open(path, O_RDONLY);

  if (pfd < 0) {
    perror("unable to open synpkg file");
    exit(1);
  }

  r1 = read(pfd, pkg, file_state.st_size);
  if (r1 < 0) {
    perror("unable to read synpkg file");
    exit(1);
  }

  if (r1 < file_state.st_size) {
    perror("truncated synpkg file read");
    exit(1);
  }

  chunk_size = CHUNK_SIZE;
  begin_ptr = pkg;
  end_ptr = (uint8_t*)pkg + file_state.st_size;
  load.length = 0;
  load.package = NULL;

  do {
    if (ioctl(ndpfd, LOAD, &load) < 0) {
      perror("ndp10x load ioctl failed");
      exit(1);
    }
    begin_ptr = load.length ? (begin_ptr + load.length) : pkg;
    if ((begin_ptr + chunk_size) > end_ptr) chunk_size = end_ptr - begin_ptr;
    load.length = chunk_size;
    load.package = begin_ptr;
    s = load.error_code;
  } while (s == SYNTIANT_NDP_ERROR_MORE);

  if (s != SYNTIANT_NDP_ERROR_NONE) {
    fprintf(stderr, "error loading package: %d\n", s);
    exit(1);
  }

  s = 0;

  free(pkg);
  if (close(pfd) < 0) {
    perror("package file close failed");
    exit(1);
  }
}

void get_config(int ndpfd, char*** labelsp, int* classes) {
  struct ndp10x_ndp_config_s config;
  char device_type[STRING_LEN] = { 0 };
  char fwver[STRING_LEN] = { 0 };
  char paramver[STRING_LEN] = { 0 };
  char packagever[STRING_LEN] = { 0 };
  char** labels;
  char* label_data;
  int i, j;

  label_data = malloc(STRING_LEN);
  if (!label_data) {
    fprintf(stderr, "unable to allocate label data\n");
    exit(1);
  }

  /* get config data */
  memset(&config, 0, sizeof(struct syntiant_ndp_config_s));
  config.device_type = device_type;
  config.device_type_len = sizeof(device_type);
  config.firmware_version = fwver;
  config.firmware_version_len = sizeof(fwver);
  config.parameters_version = paramver;
  config.parameters_version_len = sizeof(paramver);
  config.labels = label_data;
  config.labels_len = STRING_LEN;
  config.pkg_version = packagever;
  config.pkg_version_len = sizeof(packagever);
  if (ioctl(ndpfd, NDP_CONFIG, &config) < 0) {
    perror("ndp10x get_config ioctl failed");
    exit(1);
  }

  if (STRING_LEN < config.firmware_version_len) {
    fprintf(stderr, "firmware version string too long");
    exit(1);
  }
  if (STRING_LEN < config.parameters_version_len) {
    fprintf(stderr, "parameter version string too long");
    exit(1);
  }

  if (STRING_LEN < config.labels_len) {
    fprintf(stderr, "labels strings too long");
    exit(1);
  }

  labels = malloc(config.classes * sizeof(*classes));
  if (!label_data) {
    fprintf(stderr, "unable to allocate labels array\n");
    exit(1);
  }

  *labelsp = labels;

  *classes = config.classes;

  j = 0;
  for (i = 0; i < config.classes; i++) {
    labels[i] = &label_data[j];
    for (; label_data[j]; j++)
      ;
    j++;
  }

  if (strlen(fwver)) {
    printf("firmware: %s ", fwver);
  }

  if (strlen(paramver)) {
    printf("parameters: %s ", paramver);
  }

  if (strlen(packagever)) {
    printf("package: %s ", packagever);
  }

  printf("device: %s\n", device_type);

  printf("labels:");
  for (i = 0; i < config.classes; i++) {
    printf(" %s", labels[i]);
  }
  printf("\n");
}

void initialization(int ndpfd, int spi) {
  int s = 0;
#ifndef JIOPHONE
  char* i2c_file = "/dev/i2c-1";
  int i2c_addr = 0x60;
#endif
  struct syntiant_ndp10x_config_s ndp10x_config;

#ifndef JIOPHONE
  /* Initialize the clocks to 1.6 MHz PDM, 16 MHz core */
  s = ndp9101_pi_i2c_clock_init(i2c_file, i2c_addr);
  if (s) {
    perror("cannot initialize i2c clock");
    exit(1);
  }
  /* allow the mcu/pdm clock to be stable */
  usleep(200 * 1000);
#endif

  if (ioctl(ndpfd, INIT, NULL) < 0) {
    perror("ndp10x default init ioctl failed");
    exit(1);
  }

  set_config(&ndp10x_config, spi);
  if (ioctl(ndpfd, NDP10X_CONFIG, &ndp10x_config) < 0) {
    perror("ndp10x default config ioctl failed");
    exit(1);
  }
}

void set_agc(int ndpfd, int agc) {
  struct syntiant_ndp10x_config_s ndp10x_config;

  memset(&ndp10x_config, 0, sizeof(struct syntiant_ndp10x_config_s));
  ndp10x_config.set = SYNTIANT_NDP10X_CONFIG_SET_AGC_ON;
  ndp10x_config.agc_on = agc;
  if (agc) {
    ndp10x_config.set |= SYNTIANT_NDP10X_CONFIG_SET_PDM_IN_SHIFT;
    ndp10x_config.pdm_in_shift[0] = 11;
    ndp10x_config.pdm_in_shift[1] = 11;
    ndp10x_config.set1 = SYNTIANT_NDP10X_CONFIG_SET1_AGC_MAX_ADJ;
    ndp10x_config.agc_max_adj[0] = 4;
    ndp10x_config.agc_max_adj[1] = 4;
  }
  if (ioctl(ndpfd, NDP10X_CONFIG, &ndp10x_config) < 0) {
    perror("ndp10x set agc ioctl failed");
    exit(1);
  }
  printf("agc on: %d\n", ndp10x_config.agc_on);
}

int send_event(int fd ,int type, int code, int value)
{
    int ret;

    input_event _event;
    memset(&_event,0,sizeof(_event));
    _event.type = type;
    _event.code = code;
    _event.value = value;
    ret = write(fd,&_event,sizeof(_event));
    if(ret < sizeof(_event)) 
        return -1;

    memset(&_event,0,sizeof(_event));
    _event.type = 0;
    _event.code = 0;
    _event.value = 0;
    ret = write(fd,&_event,sizeof(_event));
    if(ret < sizeof(_event))
        return -1;

    return 0;
}

int send_keycode(int keycode)
{
    int input_fd;
    int ret;
    
    input_fd = open("/dev/input/event2",O_RDWR);
    if (input_fd < 0) 
        return -1;
    
    ret = send_event(input_fd, 1, keycode, 1);
    if(ret < 0) {
        printf("xraov: send_event error\n");
        return -1;
    }
    ret = send_event(input_fd, 1, keycode, 0);
    if(ret < 0) {
        printf("xraov: send_event error\n");
        return -1;
    }
    close(input_fd);

    return 0;
}
void watch(int ndpfd, int num_matches, int print_stats, int extract_mode, int extract_len_ms,
           int post_keyword_len_ms) {
  struct ndp10x_watch_s watch;
  char** labels;
  struct tm* tm;
  char time[128];
  int classes;
  uint8_t* audio_buffer = NULL;
  struct ndp10x_pcm_extract_s extract;
  FILE* fptr;
  char buf[256];
  int wrt_size;
  int cnt = 0;
  int audio_buf_len = MAX(extract_len_ms, post_keyword_len_ms) * BYTES_PER_MILLISECOND;
    int ret = -1;

  set_agc(ndpfd, 0);

  if (extract_mode) {
    audio_buffer = malloc(audio_buf_len);
    if (!audio_buffer) {
      fprintf(stderr, "couldn't allocate buffer for audio data\n");
      exit(1);
    }

    memset(audio_buffer, 0, audio_buf_len);
  }

  memset(&watch, 0, sizeof(watch));
  get_config(ndpfd, &labels, &classes);
  classes--;
  ret = send_keycode(59);
  if (ret < 0)
      printf("xraov: send_keycode error\n");
  else
      printf("xraov: send_keycode success\n");

  printf("\nwatching for %d keyword%s\n", classes, classes ? "s" : "");
  while (1) {
    watch.timeout = -1;
    watch.classes = (1 << classes) - 1;

    watch.extract_match_mode = extract_mode;
    watch.extract_before_match = extract_len_ms;

    if (ioctl(ndpfd, WATCH, &watch) < 0) {
      perror("ndp10x watch ioctl failed");
      exit(1);
    }

    if (extract_mode) {
      memset(&extract, 0, sizeof(extract));
      extract.flush = 0;
      extract.buffer = audio_buffer;
      extract.buffer_length = extract_len_ms * BYTES_PER_MILLISECOND;

      printf("extract %d bytes of pre-roll + keyword audio\n", extract.buffer_length);
      if (ioctl(ndpfd, PCM_EXTRACT, &extract) < 0) {
        perror("ndp10x extract ioctl failed");
        exit(1);
      }
      printf("save data to a file\n");
      memset(buf, 0, sizeof(unsigned char) * 32);
      snprintf(buf, 32, "/data/%s%d%s", "pcm_audio", (unsigned int)cnt++, ".raw");
      fptr = fopen(buf, "wb");
      if (!fptr) {
        perror("Couldn't open file for saving audio data");
        exit(1);
      }
      wrt_size = fwrite(audio_buffer, 1, extract.extracted_length, fptr);
      if (ferror(fptr)) {
        perror("PCM data write failed");
      }
      printf("%d bytes written to %s\n", wrt_size, buf);
      fclose(fptr);
    }

    assert(watch.match);
    assert(watch.class < classes);
    tm = localtime(&watch.ts.tv_sec);
    strftime(time, sizeof(time), "%Y-%m-%d %H:%M:%S", tm);
    printf("%s match -> %s\n", time, labels[watch.class_index]);
        ret = send_keycode(60);
        if (ret < 0)
            printf("xraov: send_keycode error\n");
        else
            printf("xraov: send_keycode success\n");
    if (print_stats) {
      stats(ndpfd, 1, 1, NULL);
    }

    if (extract_mode && (post_keyword_len_ms > 0)) {
      fptr = fopen(buf, "ab");
      if (!fptr) {
        perror("Couldn't open file for appending audio data");
        exit(1);
      }

      wrt_size = 0;

      memset(&extract, 0, sizeof(extract));
      extract.buffer = audio_buffer;
      extract.buffer_length = post_keyword_len_ms * BYTES_PER_MILLISECOND;

      printf("extract %d bytes of post-keyword audio\n", extract.buffer_length);
      if (ioctl(ndpfd, PCM_EXTRACT, &extract) < 0) {
        perror("ndp10x extract ioctl failed");
        exit(1);
      }

      wrt_size = fwrite(audio_buffer, 1, extract.extracted_length, fptr);
      if (ferror(fptr)) {
        perror("PCM data append failed");
      }

      printf("extracted %d bytes of post-keyword audio\n", extract.extracted_length);
      fclose(fptr);
    }

    if (num_matches > 0) {
      num_matches--;
      if (!num_matches) return;
    }
  }
}

void stats(int ndpfd, int print, int clear, struct demo_statistics_s* stats) {
  struct ndp10x_statistics_s ndp_stats;

  memset(&ndp_stats, 0, sizeof(struct ndp10x_statistics_s));
  ndp_stats.clear = clear;
  if (ioctl(ndpfd, STATS, &ndp_stats) < 0) {
    perror("stats ioctl failed");
    exit(1);
  }

  if (print) {
    printf("isrs: %" PRIu64 ", polls: %" PRIu64 ", frames: %" PRIu64 "\n", ndp_stats.isrs,
           ndp_stats.polls, ndp_stats.frames);
    printf("results: %" PRIu64 ", dropped: %" PRIu64 ", ring used: %d\n", ndp_stats.results,
           ndp_stats.results_dropped, ndp_stats.result_ring_used);
    printf("extracts: %" PRIu64 ", bytes: %" PRIu64 ", bytes dropped: %" PRIu64 ", ring used: %d\n",
           ndp_stats.extracts, ndp_stats.extract_bytes, ndp_stats.extract_bytes_dropped,
           ndp_stats.extract_ring_used);
    printf("sends: %" PRIu64 ", bytes: %" PRIu64 ", ring used: %d\n", ndp_stats.sends,
           ndp_stats.send_bytes, ndp_stats.send_ring_used);
  }

  if (stats) {
    stats->isrs = ndp_stats.isrs;
    stats->polls = ndp_stats.polls;
    stats->frames = ndp_stats.frames;
    stats->results = ndp_stats.results;
    stats->results_dropped = ndp_stats.results_dropped;
    stats->result_ring_used = ndp_stats.result_ring_used;
    stats->extracts = ndp_stats.extracts;
    stats->extract_bytes = ndp_stats.extract_bytes;
    stats->extract_bytes_dropped = ndp_stats.extract_bytes_dropped;
    stats->extract_ring_used = ndp_stats.extract_ring_used;
    stats->sends = ndp_stats.sends;
    stats->send_bytes = ndp_stats.send_bytes;
    stats->send_ring_used = ndp_stats.send_ring_used;
  }
}
