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
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <unistd.h>
#include "ndp10x_app_utils.h"
#include "ndp10x_ioctl.h"
#include "syntiant_ilib/ndp10x_spi_regs.h"
#include "syntiant_ilib/syntiant_ndp.h"
#include "syntiant_ilib/syntiant_ndp_error.h"

#define PLAY_BUFFER_SIZE (768 * 10)
#define PLAY_FILE_LENGTH (256)

struct play_data {
  int devfd;
  char* name;
};

void* play_pcm(struct play_data* play) {
  int ret = 0;
  int read_len = 0;
  int sent = 0;
  FILE* fptr = NULL;
  uint8_t* play_buf = NULL;
  char* buf = "/data/alexa_21554158_16k.raw";
  struct ndp10x_pcm_send_s send;

  if (play->name) {
    buf = play->name;
  }
  fptr = fopen(buf, "rb");
  if (!fptr) {
    perror("couldn't PCM audio data file");
    ret = -1;
    goto err;
  }

  play_buf = malloc(PLAY_BUFFER_SIZE);
  if (!play_buf) {
    fprintf(stderr, "unable to allocate play buffer\n");
    ret = -1;
    goto err;
  }

  memset(&send, 0, sizeof(send));
  send.buffer = play_buf;
  while (!feof(fptr)) {
    read_len = fread(play_buf, 1, PLAY_BUFFER_SIZE, fptr);
    if (ferror(fptr)) {
      perror("error reading pcm audio data file");
      ret = -1;
      goto err;
    }
    send.buffer_length = read_len;
    if (ioctl(play->devfd, PCM_SEND, &send) < 0) {
      perror("ndp10x send ioctl failed\n");
      ret = -1;
      goto err;
    }
    sent += send.sent_length;
  }

err:
  if (fptr) {
    fclose(fptr);
  }
  if (play_buf) {
    free(play_buf);
  }
  printf("sent %d PCM bytes total\n", sent);

  return (void*)ret;
}

int ndp10x_play(char* play_file, char* fw_name, char* package_name) {
  int c, ndpfd;
  struct syntiant_ndp10x_config_s ndp10x_config;
#if 0
    pthread_t play_tid, watch_tid;
#endif
  struct play_data play;
  struct demo_statistics_s read_stats;

  memset(&play, 0, sizeof(play));

  play.name = malloc(sizeof(char) * PLAY_FILE_LENGTH);
  if (!play.name) {
    fprintf(stderr, "unable to allocate memory for file name\n");
    exit(1);
  }
  strncpy(play.name, play_file, PLAY_FILE_LENGTH);

  ndpfd = open("/dev/ndp10x", 0);
  if (ndpfd < 0) {
    perror("ndp10x open failed\n");
    exit(1);
  }

  stats(ndpfd, 0, 1, NULL);

  initialization(ndpfd, 1);
  printf("device initialized successfully!\n");

  load_synpkgs(ndpfd, fw_name, package_name);
  printf("synpkg loaded successfully!\n");

  /* configure for SPI input */
  memset(&ndp10x_config, 0, sizeof(struct syntiant_ndp10x_config_s));
  ndp10x_config.set = SYNTIANT_NDP10X_CONFIG_SET_DNN_INPUT;
  ndp10x_config.dnn_input = SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI;
  if (ioctl(ndpfd, NDP10X_CONFIG, &ndp10x_config) < 0) {
    perror("ndp10x_config set_config ioctl failed\n");
    exit(1);
  }
  play.devfd = ndpfd;
  play_pcm(&play);
  stats(ndpfd, 0, 0, &read_stats);
  if (read_stats.results) {
    watch(ndpfd, 1, 0, 0, 0, 0);
  }
  stats(ndpfd, 1, 0, NULL);

  return 0;
}
