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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "ndp10x_app_utils.h"
#include "ndp10x_ioctl.h"
#include "syntiant_ilib/ndp10x_spi_regs.h"
#include "syntiant_ilib/syntiant_ndp.h"
#include "syntiant_ilib/syntiant_ndp_error.h"

void load_synpkgs(int ndpfd, char* __fw_name, char* __package_name) {
  /* load fw pkg */
  if (__fw_name && strlen(__fw_name)) {
    load_pkg(ndpfd, __fw_name);
  }

  /* load model pkg */
  if (__package_name && strlen(__package_name)) {
    load_pkg(ndpfd, __package_name);
  }
}

void __set_config(struct syntiant_ndp10x_config_s* ndp10x_config, int spi) {
  int pdm_freq = 832000;
  int input_freq = 32768;
  int pdm_ndp = 1;
  int pdm_in_shift = 11;
  int pdm_out_shift = 7;
  int pdm_dc_offset = 0;
  int power_offset = 63;
  int mic = 0;

  int i;

  memset(ndp10x_config, 0, sizeof(struct syntiant_ndp10x_config_s));
  ndp10x_config->set =
      SYNTIANT_NDP10X_CONFIG_SET_PDM_CLOCK_RATE |
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


int model_recognize(char* fw_name, char* package_name, int extract_mode, int extract_len,
                    int post_keyword_len) {
  int ndpfd;
  struct syntiant_ndp10x_config_s ndp10x_config;

  ndpfd = open("/dev/ndp10x", 0);
  if (ndpfd < 0) {
    perror("ndp10x open failed");
    return EXIT_FAILURE;
  }

  if (ioctl(ndpfd, INIT, NULL) < 0) {
    perror("ndp10x default init ioctl failed");
    exit(1);
  }

  memset(&ndp10x_config, 0, sizeof(struct syntiant_ndp10x_config_s));
  ndp10x_config.set = SYNTIANT_NDP10X_CONFIG_SET_INPUT_CLOCK_RATE;
  ndp10x_config.input_clock_rate = 32768;
  if (ioctl(ndpfd, NDP10X_CONFIG, &ndp10x_config) < 0) {
    perror("ndp10x default config ioctl failed");
  }

  printf("device initialized successfully!\n");

  load_synpkgs(ndpfd, fw_name, package_name);
  printf("synpkg loaded successfully!\n");


  memset(&ndp10x_config, 0, sizeof(struct syntiant_ndp10x_config_s));
  ndp10x_config.get_all = 1;
  if (ioctl(ndpfd, NDP10X_CONFIG, &ndp10x_config) < 0) {
    perror("ndp10x config ioctl get failed");
  }

  if (ndp10x_config.dnn_input == SYNTIANT_NDP10X_CONFIG_DNN_INPUT_NONE) {
    __set_config(&ndp10x_config, 0);
    if (ioctl(ndpfd, NDP10X_CONFIG, &ndp10x_config) < 0) {
      perror("ndp10x set config ioctl failed");
    }
  }

  watch(ndpfd, -1, 1, extract_mode, extract_len, post_keyword_len);

  if (close(ndpfd) < 0) {
    perror("ndp10x close failed");
    exit(1);
  }

  return 0;
}

void show_usage() {
  printf(
      "\nusage:\n\n"
      "ndp10x_driver_test [option] \n\n"
      "Options:\n\n"
      "Recognize:\n"
      "\t-w -f [firmware] -m [model]\n\n"
      "Recognize & record (~1.5 secs of pre-roll & match data) + n sec of audio post-match:\n"
      "\t-w -r -t [rec_sec] -f [firmware] -m [model]\n"
      "play:\n"
      "\t-p [play file name] -f [firmware] -m [model]\n\n"
      "Record\n"
      "\t-r -t [rec_sec] -n [number of recordings] -f [firmware] -m [model]\n");
}

int main(int argc, char** args) {
  char* fw_name = NULL;
  char* package_name = NULL;

  char* play_file = NULL;

  int terms = 1, rec_time = 3;

  int c = 0, opt = 0;
  int flag_recognize = 0, flag_recognize_f = 0, flag_recognize_m = 0;
  int flag_play = 0;
  int flag_record = 0, flag_record_t = 0, flag_record_n = 0;

  if (argc < 2) {
    printf("invalid arguments");
    show_usage();
    return -1;
  }

  while ((c = getopt(argc, args, "f:m:n:p:rt:w")) != -1) {
    switch (c) {
      case 'w':
        flag_recognize = 1;
        break;

      case 'f':
        flag_recognize_f = 1;
        fw_name = optarg;
        break;

      case 'm':
        flag_recognize_m = 1;
        package_name = optarg;
        break;

      case 'p':
        flag_play = 1;
        play_file = optarg;
        break;

      case 'r':
        flag_record = 1;
        break;

      case 't':
        flag_record_t = 1;
        rec_time = atoi(optarg);
        break;

      case 'n':
        flag_record_n = 1;
        terms = atoi(optarg);
        break;

      default:
        show_usage();
        goto INVALID;
    }
  }

  if (!(flag_recognize_f && flag_recognize_m) && !flag_record) {
    printf("Error: no firmware and model specified\n");
    return -1;
  }

  if (flag_recognize) {
    if (flag_record) {
      /* record pre-roll & keyword & and optionally record port-keyword
       * audio as well */
      model_recognize(fw_name, package_name, 1, 1500, rec_time * 1000);
    } else {
      /* just recognize */
      model_recognize(fw_name, package_name, 0, 0, 0);
    }
  } else if (flag_play) {
    ndp10x_play(play_file, fw_name, package_name);
  } else if (flag_record & flag_record_t & flag_record_n) {
    ndp10x_record(rec_time, terms, fw_name, package_name);
  }

  return 0;

INVALID:
  printf("\nPlease try again.\n\n");

  return 0;
}
